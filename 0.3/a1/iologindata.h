//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// Base class for the LoginData loading/saving
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

#ifndef __IOLOGINDATA_H
#define __IOLOGINDATA_H

#include <string>
#include "account.h"
#include "player.h"
#include "database.h"

class PlayerGroup
{
	public:
		PlayerGroup(){}
		virtual ~PlayerGroup(){}

		std::string m_name;
		uint64_t m_flags;
		uint64_t m_customflags;
		uint16_t m_access;
		uint16_t m_violationaccess;
		uint32_t m_maxdepotitems;
		uint32_t m_maxviplist;
};

typedef std::pair<int32_t, Item*> itemBlock;
typedef std::list<itemBlock> ItemBlockList;

class IOLoginData
{
	public:
		IOLoginData(){}
		virtual ~IOLoginData(){}

		static IOLoginData* getInstance()
		{
			static IOLoginData instance;
			return &instance;
		}

		Account loadAccount(uint32_t accno, bool preLoad = false);
		bool saveAccount(Account acc);

		bool hasFlag(uint32_t accno, PlayerFlags value);
		bool hasCustomFlag(uint32_t accno, PlayerCustomFlags value);

		bool accountExists(uint32_t accno);
		bool getPassword(uint32_t accno, const std::string& name, std::string& password);
		bool setNewPassword(uint32_t accountId, std::string newPassword);
		bool validRecoveryKey(uint32_t accountNumber, const std::string recoveryKey);
		bool setRecoveryKey(uint32_t accountNumber, std::string recoveryKey);

		bool createAccount(uint32_t accountNumber, std::string newPassword);
		void removePremium(Account account);

		bool loadPlayer(Player* player, const std::string& name, bool preload = false);
		bool savePlayer(Player* player, bool preSave);
		bool updateOnlineStatus(uint32_t guid, bool login);

		const PlayerGroup* getPlayerGroup(uint32_t groupid);
		const PlayerGroup* getPlayerGroupByAccount(uint32_t accno);
		uint32_t getLastIPByName(std::string name);

		bool hasFlag(std::string name, PlayerFlags value);
		bool hasCustomFlag(std::string name, PlayerCustomFlags value);

		bool isPremium(uint32_t guid);

		bool playerExists(std::string name);
		bool playerExists(uint32_t guid);
		bool getNameByGuid(uint32_t guid, std::string& name);
		bool getGuidByName(uint32_t& guid, std::string& name);
		bool getGuidByNameEx(uint32_t &guid, bool& specialVip, std::string& name);
		uint32_t getAccountNumberByName(std::string name);

		bool changeName(uint32_t guid, std::string newName, std::string oldName);
		bool createCharacter(uint32_t accountNumber, std::string characterName, int32_t vocationId, PlayerSex_t sex);
		int16_t deleteCharacter(uint32_t accountNumber, const std::string characterName);

		uint32_t getLevel(uint32_t guid) const;
		uint32_t getLastIP(uint32_t guid) const;

		bool updatePremiumDays();
		bool resetOnlineStatus();
		bool resetGuildInformation(uint32_t guid);

	protected:
		struct StringCompareCase
		{
			bool operator()(const std::string& l, const std::string& r) const
			{
				return strcasecmp(l.c_str(), r.c_str()) < 0;
			}
		};

		typedef std::map<int,std::pair<Item*,int> > ItemMap;
		typedef std::map<uint32_t, std::string> NameCacheMap;
		typedef std::map<std::string, uint32_t, StringCompareCase> GuidCacheMap;
		typedef std::map<uint32_t, PlayerGroup*> PlayerGroupMap;

		void loadItems(ItemMap& itemMap, DBResult *result);
		bool saveItems(const Player* player, const ItemBlockList& itemList, DBInsert& query_insert);

		bool storeNameByGuid(Database &mysql, uint32_t guid);

		PlayerGroupMap playerGroupMap;
		NameCacheMap nameCacheMap;
		GuidCacheMap guidCacheMap;
};

#endif
