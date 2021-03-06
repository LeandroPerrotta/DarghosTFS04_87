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

#include "creature.h"
#include "player.h"
#include "tools.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "talkaction.h"

extern Game g_game;

TalkActions::TalkActions() :
m_scriptInterface("TalkAction Interface")
{
	m_scriptInterface.initState();
}

TalkActions::~TalkActions()
{
	clear();
}

void TalkActions::clear()
{
	TalkActionList::iterator it = wordsMap.begin();
	while(it != wordsMap.end())
	{
		delete it->second;
		wordsMap.erase(it);
		it = wordsMap.begin();
	}

	m_scriptInterface.reInitState();
}

LuaScriptInterface& TalkActions::getScriptInterface()
{
	return m_scriptInterface;
}

std::string TalkActions::getScriptBaseName()
{
	return "talkactions";
}

Event* TalkActions::getEvent(const std::string& nodeName)
{
	if(asLowerCaseString(nodeName) == "talkaction")
		return new TalkAction(&m_scriptInterface);
	else
		return NULL;
}

bool TalkActions::registerEvent(Event* event, xmlNodePtr p)
{
	TalkAction* talkAction = dynamic_cast<TalkAction*>(event);
	if(!talkAction)
		return false;

	wordsMap.push_back(std::make_pair(talkAction->getWords(), talkAction));
	return true;
}

TalkActionResult_t TalkActions::onPlayerSpeak(Player* player, SpeakClasses type, const std::string& words)
{
	if(type != SPEAK_SAY)
		return TALKACTION_CONTINUE;

	std::string cmdstring[TALKFILTER_LAST] = words, paramstring[TALKFILTER_LAST] = "";
	size_t loc;

	loc = words.find('"', 0 );
	if(loc != std::string::npos && loc >= 0)
	{
		cmdstring[TALKFILTER_QUOTATION] = std::string(words, 0, loc);
		paramstring[TALKFILTER_QUOTATION] = std::string(words, (loc + 1), (words.size() - loc - 1));
		trim_left(cmdstring[TALKFILTER_QUOTATION], " ");
		trim_right(cmdstring[TALKFILTER_QUOTATION], " ");
	}

	loc = words.find(' ', 0 );
	if(loc != std::string::npos && loc >= 0)
	{
		cmdstring[TALKFILTER_WORD] = std::string(words, 0, loc);
		paramstring[TALKFILTER_WORD] = std::string(words, (loc + 1), (words.size() - loc - 1));
	}

	for(TalkActionList::iterator it = wordsMap.begin(); it != wordsMap.end(); ++it)
	{
		if(it->first == cmdstring[it->second->getFilter()] || (!it->second->isSensitive() &&
			strcasecmp(it->first.c_str(), cmdstring[it->second->getFilter()].c_str()) == 0))
		{
			TalkAction* talkAction = it->second;
			if(talkAction->getAccess() > player->getAccessLevel() || player->getName() == "Account Manager")
			{
				if(player->getAccessLevel() > 0)
					player->sendTextMessage(MSG_STATUS_SMALL, "You can not execute this talkaction.");

				return TALKACTION_FAILED;
			}

			uint32_t ret = talkAction->executeSay(player, cmdstring[talkAction->getFilter()], paramstring[talkAction->getFilter()]);
			if(ret == 1)
			{
				if(player && talkAction->isLogged())
				{
					player->sendTextMessage(MSG_STATUS_CONSOLE_RED, words.c_str());

					char buf[21], buffer[85];
					formatDate(time(NULL), buf);
					sprintf(buffer, "data/logs/%s talkactions.log", player->getName().c_str());

					FILE* file = fopen(buffer, "a");
					if(file)
					{
						fprintf(file, "[%s] %s\n", buf, words.c_str());
						fclose(file);
					}
				}
				return TALKACTION_CONTINUE;
			}
			else
				return TALKACTION_BREAK;
		}
	}
	return TALKACTION_CONTINUE;
}

TalkAction::TalkAction(LuaScriptInterface* _interface) :
Event(_interface), m_filter(TALKFILTER_QUOTATION), m_access(0), m_logged(false), m_sensitive(true)
{
	//
}

bool TalkAction::configureEvent(xmlNodePtr p)
{
	int32_t intValue;
	std::string strValue;

	if(readXMLString(p, "words", strValue))
		m_words = strValue;
	else
	{
		std::cout << "[Error - TalkAction::configureEvent] No words for TalkAction." << std::endl;
		return false;
	}

	if(readXMLString(p, "filter", strValue))
	{
		strValue = asLowerCaseString(strValue);
		if(strValue == "quotation")
			m_filter = TALKFILTER_QUOTATION;
		else if(strValue == "word")
			m_filter = TALKFILTER_WORD;
	}

	if(readXMLInteger(p, "access", intValue))
		m_access = intValue;

	if(readXMLString(p, "log", strValue))
	{
		strValue = asLowerCaseString(strValue);
		m_sensitive = booleanString(strValue);
	}

	if(readXMLString(p, "case-sensitive", strValue))
	{
		strValue = asLowerCaseString(strValue);
		m_sensitive = booleanString(strValue);
	}

	return true;
}

std::string TalkAction::getScriptEventName()
{
	return "onSay";
}

int32_t TalkAction::executeSay(Creature* creature, const std::string& words, const std::string& param)
{
	//onSay(cid, words, param)
	if(m_scriptInterface->reserveScriptEnv())
	{
		ScriptEnviroment* env = m_scriptInterface->getScriptEnv();

		#ifdef __DEBUG_LUASCRIPTS__
		char desc[125];
		sprintf(desc, "%s - %s- %s", creature->getName().c_str(), words.c_str(), param.c_str());
		env->setEventDesc(desc);
		#endif

		env->setScriptId(m_scriptId, m_scriptInterface);
		env->setRealPos(creature->getPosition());

		uint32_t cid = env->addThing(creature);

		lua_State* L = m_scriptInterface->getLuaState();

		m_scriptInterface->pushFunction(m_scriptId);
		lua_pushnumber(L, cid);
		lua_pushstring(L, words.c_str());
		lua_pushstring(L, param.c_str());

		int32_t result = m_scriptInterface->callFunction(3);
		m_scriptInterface->releaseScriptEnv();

		return (result == LUA_TRUE);
	}
	else
	{
		std::cout << "[Error] Call stack overflow. TalkAction::executeSay" << std::endl;
		return 0;
	}
}
