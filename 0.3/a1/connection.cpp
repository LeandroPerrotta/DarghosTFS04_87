//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"

#include "connection.h"
#include "protocol.h"
#include "outputmessage.h"
#include "protocolgame.h"
#include "protocollogin.h"
#include "admin.h"
#include "status.h"
#include "tasks.h"
#include "scheduler.h"
#include "configmanager.h"

#include <boost/bind.hpp>

extern ConfigManager g_config;
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t Connection::connectionCount = 0;
#endif

ConnectionManager::ConnectionManager()
{
	OTSYS_THREAD_LOCKVARINIT(m_connectionManagerLock);

	maxLoginTries = (uint32_t)g_config.getNumber(ConfigManager::LOGIN_TRIES);
	retryTimeout = (uint32_t)g_config.getNumber(ConfigManager::RETRY_TIMEOUT) / 1000;
	loginTimeout = (uint32_t)g_config.getNumber(ConfigManager::LOGIN_TIMEOUT) / 1000;
}

Connection* ConnectionManager::createConnection(boost::asio::io_service& io_service)
{
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Create new Connection" << std::endl;
	#endif
	OTSYS_THREAD_LOCK_CLASS(m_connectionManagerLock, "");
	Connection* connection = new Connection(io_service);
	m_connections.push_back(connection);
	return connection;
}

void ConnectionManager::releaseConnection(Connection* connection)
{
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Releasing connection" << std::endl;
	#endif
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionManagerLock);
	std::list<Connection*>::iterator it =
		std::find(m_connections.begin(), m_connections.end(), connection);

	if(it != m_connections.end())
		m_connections.erase(it);
	else
		std::cout << "Error: [ConnectionManager::releaseConnection] Connection not found" << std::endl;
}

void ConnectionManager::closeAll()
{
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Closing all connections" << std::endl;
	#endif
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionManagerLock);
	std::list<Connection*>::iterator it = m_connections.begin();
	while(it != m_connections.end())
	{
		boost::system::error_code error;
		(*it)->m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
		(*it)->m_socket.close(error);
		++it;
	}
	m_connections.clear();
}

bool ConnectionManager::isDisabled(uint32_t clientip)
{
	if(maxLoginTries == 0)
		return false;

	if(clientip != 0)
	{
		uint32_t currentTime = time(NULL);
		IpLoginMap::const_iterator it = ipLoginMap.find(clientip);
		if(it != ipLoginMap.end())
		{
			if((it->second.numberOfLogins >= maxLoginTries) && (currentTime < it->second.lastLoginTime + loginTimeout))
				return true;
		}
	}

	return false;
}

void ConnectionManager::addLoginAttempt(uint32_t clientip, bool isSuccess)
{
	if(clientip != 0)
	{
		uint32_t currentTime = time(NULL);

		IpLoginMap::iterator it = ipLoginMap.find(clientip);
		if(it == ipLoginMap.end())
		{
			LoginBlock lb;
			lb.lastLoginTime = 0;
			lb.numberOfLogins = 0;

			ipLoginMap[clientip] = lb;
			it = ipLoginMap.find(clientip);
		}

		if(it->second.numberOfLogins >= maxLoginTries)
			it->second.numberOfLogins = 0;

		if(!isSuccess || (currentTime < it->second.lastLoginTime + retryTimeout))
			++it->second.numberOfLogins;
		else
			it->second.numberOfLogins = 0;

		it->second.lastLoginTime = currentTime;
	}
}

//*****************

void Connection::closeConnection()
{
	//any thread
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Connection::closeConnection" << std::endl;
	#endif
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	if(m_closeState != CLOSE_STATE_NONE)
		return;

	m_closeState = CLOSE_STATE_REQUESTED;

	Dispatcher::getDispatcher().addTask(
		createTask(boost::bind(&Connection::closeConnectionTask, this)));
}

void Connection::closeConnectionTask()
{
	//dispatcher thread
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Connection::closeConnectionTask" << std::endl;
	#endif

	OTSYS_THREAD_LOCK(m_connectionLock, "");
	if(m_closeState != CLOSE_STATE_REQUESTED)
	{
		std::cout << "Error: [Connection::closeConnectionTask] m_closeState = " << m_closeState << std::endl;
		OTSYS_THREAD_UNLOCK(m_connectionLock, "");
		return;
	}

	m_closeState = CLOSE_STATE_CLOSING;

	if(m_protocol)
	{
		Dispatcher::getDispatcher().addTask(
			createTask(boost::bind(&Protocol::deleteProtocolTask, m_protocol)));
		m_protocol->setConnection(NULL);
		m_protocol = NULL;
	}

	if(!closingConnection())
		OTSYS_THREAD_UNLOCK(m_connectionLock, "");
}

void Connection::acceptConnection()
{
	// Read size of te first packet
	m_pendingRead++;
	boost::asio::async_read(m_socket,
		boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::header_length),
		boost::bind(&Connection::parseHeader, this, boost::asio::placeholders::error));
}

void Connection::parseHeader(const boost::system::error_code& error)
{
	OTSYS_THREAD_LOCK(m_connectionLock, "");
	m_pendingRead--;
	if(m_closeState == CLOSE_STATE_CLOSING)
	{
		if(!closingConnection())
			OTSYS_THREAD_UNLOCK(m_connectionLock, "");
		return;
	}

	int32_t size = m_msg.decodeHeader();
	if(!error && size > 0 && size < NETWORKMESSAGE_MAXSIZE - 16)
	{
		// Read packet content
		m_pendingRead++;
		m_msg.setMessageLength(size + NetworkMessage::header_length);
		boost::asio::async_read(m_socket, boost::asio::buffer(m_msg.getBodyBuffer(), size),
			boost::bind(&Connection::parsePacket, this, boost::asio::placeholders::error));
	}
	else
		handleReadError(error);

	OTSYS_THREAD_UNLOCK(m_connectionLock, "");
}

void Connection::parsePacket(const boost::system::error_code& error)
{
	OTSYS_THREAD_LOCK(m_connectionLock, "");
	m_pendingRead--;
	if(m_closeState == CLOSE_STATE_CLOSING)
	{
		if(!closingConnection())
			OTSYS_THREAD_UNLOCK(m_connectionLock, "");
		return;
	}

	if(!error)
	{
		// Protocol selection
		if(!m_protocol)
		{
			// Protocol depends on the first byte of the packet
			uint8_t protocolId = m_msg.GetByte();
			switch(protocolId)
			{
				case 0x01: // Login server protocol
					m_protocol = new ProtocolLogin(this);
					break;
				case 0x0A: // World server protocol
					m_protocol = new ProtocolGame(this);
					break;
				case 0xFE: // Admin protocol
					m_protocol = new ProtocolAdmin(this);
					break;
				case 0xFF: // Status protocol
					m_protocol = new ProtocolStatus(this);
					break;
				default:
					// No valid protocol
					closeConnection();
					OTSYS_THREAD_UNLOCK(m_connectionLock, "");
					return;
					break;
			}
			m_protocol->onRecvFirstMessage(m_msg);
		}
		else
		{
			// Send the packet to the current protocol
			m_protocol->onRecvMessage(m_msg);
		}

		// Wait to the next packet
		m_pendingRead++;
		boost::asio::async_read(m_socket,
			boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::header_length),
			boost::bind(&Connection::parseHeader, this, boost::asio::placeholders::error));
	}
	else
		handleReadError(error);

	OTSYS_THREAD_UNLOCK(m_connectionLock, "");
}

void Connection::handleReadError(const boost::system::error_code& error)
{
	#ifdef __DEBUG_NET_DETAIL__
	PRINT_ASIO_ERROR("Reading - detail");
	#endif
	if(error == boost::asio::error::operation_aborted)
	{
		//Operation aborted because connection will be closed
		//Do NOT call closeConnection() from here
	}
	else if(error == boost::asio::error::eof)
	{
		//No more to read
		closeConnection();
	}
	else if(error == boost::asio::error::connection_reset ||
			error == boost::asio::error::connection_aborted)
	{
		//Connection closed remotely
		closeConnection();
	}
	else
	{
		PRINT_ASIO_ERROR("Reading");
		closeConnection();
	}
	m_readError = true;
}

bool Connection::send(OutputMessage* msg)
{
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Connection::send init" << std::endl;
	#endif

	OTSYS_THREAD_LOCK(m_connectionLock, "");
	if(m_closeState == CLOSE_STATE_CLOSING || m_writeError)
	{
		OTSYS_THREAD_UNLOCK(m_connectionLock, "");
		return false;
	}

	msg->getProtocol()->onSendMessage(msg);

	if(m_pendingWrite == 0)
	{
		#ifdef __DEBUG_NET_DETAIL__
		std::cout << "Connection::send " << msg->getMessageLength() << std::endl;
		#endif
		internalSend(msg);
	}
	else
	{
		#ifdef __DEBUG_NET__
		std::cout << "Connection::send Adding to queue " << msg->getMessageLength() << std::endl;
		#endif
		m_outputQueue.push_back(msg);
		m_pendingWrite++;
		if(m_pendingWrite > 500 && g_config.getBool(ConfigManager::FORCE_CLOSE_SLOW_CONNECTION))
		{
			std::cout << "[Notice] Forcing slow connection to disconnect" << std::endl;
			closeConnection();
		}
	}
	OTSYS_THREAD_UNLOCK(m_connectionLock, "");
	return true;
}

void Connection::internalSend(OutputMessage* msg)
{
	m_pendingWrite++;
	boost::asio::async_write(m_socket,
		boost::asio::buffer(msg->getOutputBuffer(), msg->getMessageLength()),
		boost::bind(&Connection::onWriteOperation, this, msg, boost::asio::placeholders::error));
}

uint32_t Connection::getIP() const
{
	//Ip is expressed in network byte order
	boost::system::error_code error;
	const boost::asio::ip::tcp::endpoint endpoint = m_socket.remote_endpoint(error);
	if(!error)
		return htonl(endpoint.address().to_v4().to_ulong());
	else
	{
		PRINT_ASIO_ERROR("Getting remote ip");
		return 0;
	}
}

void Connection::onWriteOperation(OutputMessage* msg, const boost::system::error_code& error)
{
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "onWriteOperation" << std::endl;
	#endif

	OutputMessagePool::getInstance()->releaseMessage(msg, true);

	OTSYS_THREAD_LOCK(m_connectionLock, "");

	if(!error)
	{
		if(m_pendingWrite > 0)
		{
			if(!m_outputQueue.empty())
			{
				OutputMessage* msg = m_outputQueue.front();
				m_outputQueue.pop_front();
				m_pendingWrite--;
				internalSend(msg);
				#ifdef __DEBUG_NET_DETAIL__
				std::cout << "Connection::onWriteOperation send " << msg->getMessageLength() << std::endl;
				#endif
			}
			m_pendingWrite--;
		}
		else
		{
			std::cout << "Error: [Connection::onWriteOperation] Getting unexpected notification!" << std::endl;
			// Error. Pending operations counter is 0, but we are getting a
			// notification!!
		}
	}
	else
	{
		m_pendingWrite--;
		handleWriteError(error);
	}

	if(m_closeState == CLOSE_STATE_CLOSING)
	{
		if(!closingConnection())
			OTSYS_THREAD_UNLOCK(m_connectionLock, "");
		return;
	}

	OTSYS_THREAD_UNLOCK(m_connectionLock, "");
}

void Connection::handleWriteError(const boost::system::error_code& error)
{
	#ifdef __DEBUG_NET_DETAIL__
	PRINT_ASIO_ERROR("Writing - detail");
	#endif
	if(error == boost::asio::error::operation_aborted)
	{
		//Operation aborted because connection will be closed
		//Do NOT call closeConnection() from here
	}
	else if(error == boost::asio::error::eof)
	{
		//No more to read
		closeConnection();
	}
	else if(error == boost::asio::error::connection_reset ||
			error == boost::asio::error::connection_aborted)
	{
		//Connection closed remotely
		closeConnection();
	}
	else
	{
		PRINT_ASIO_ERROR("Writting");
		closeConnection();
	}
	m_writeError = true;
}

bool Connection::closingConnection()
{
	//any thread
	#ifdef __DEBUG_NET_DETAIL__
	std::cout << "Connection::closingConnection" << std::endl;
	#endif

	if(m_pendingWrite == 0 || m_writeError == true)
	{
		if(!m_socketClosed)
		{
			#ifdef __DEBUG_NET_DETAIL__
			std::cout << "Closing socket" << std::endl;
			#endif
			boost::system::error_code error;
			m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			if(error)
			{
				if(error == boost::asio::error::not_connected)
				{
					//Transport endpoint is not connected.
				}
				else
					PRINT_ASIO_ERROR("Shutdown");
			}
			m_socket.close(error);
			m_socketClosed = true;
			if(error)
				PRINT_ASIO_ERROR("Close");
		}

		if(m_pendingRead == 0)
		{
			#ifdef __DEBUG_NET_DETAIL__
			std::cout << "Deleting Connection" << std::endl;
			#endif

			OTSYS_THREAD_UNLOCK(m_connectionLock, "");

			Dispatcher::getDispatcher().addTask(
				createTask(boost::bind(&Connection::deleteConnectionTask, this)));

			return true;
		}
	}
	return false;
}

void Connection::deleteConnectionTask()
{
	//dispather thread
	while(!m_outputQueue.empty())
	{
		OutputMessagePool::getInstance()->releaseMessage(m_outputQueue.back(), true);
		m_outputQueue.pop_back();
		--m_pendingWrite;
	}
	delete this;
}
