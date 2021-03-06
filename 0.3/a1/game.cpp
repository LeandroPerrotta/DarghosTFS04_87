//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// class representing the gamestate
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

#include "definitions.h"

#include <string>
#include <map>

#include <boost/config.hpp>
#include <boost/bind.hpp>

#include "otsystem.h"
#include "tasks.h"
#include "items.h"
#include "commands.h"
#include "creature.h"
#include "player.h"
#include "monster.h"
#include "game.h"
#include "tile.h"
#include "house.h"
#include "actions.h"
#include "combat.h"
#include "iologindata.h"
#include "chat.h"
#include "luascript.h"
#include "talkaction.h"
#include "spells.h"
#include "configmanager.h"
#include "ioban.h"
#include "raids.h"
#include "database.h"
#include "server.h"
#include "ioguild.h"
#include "quests.h"

#ifdef __EXCEPTION_TRACER__
#include "exception.h"
extern OTSYS_THREAD_LOCKVAR maploadlock;
#endif

extern ConfigManager g_config;
extern Server* g_server;
extern Actions* g_actions;
extern Commands commands;
extern Chat g_chat;
extern TalkActions* g_talkActions;
extern Spells* g_spells;
extern Vocations g_vocations;

Game::Game()
{
	gameState = GAME_STATE_NORMAL;
	worldType = WORLD_TYPE_PVP;
	map = NULL;
	lastStageLevel = 0;
	lastPlayersRecord = 0;
	useLastStageLevel = false;
	for(int16_t i = 0; i < 3; i++)
		globalSaveMessage[i] = false;

	OTSYS_THREAD_LOCKVARINIT(AutoID::autoIDLock);

	#ifdef __EXCEPTION_TRACER__
	OTSYS_THREAD_LOCKVARINIT(maploadlock);
	#endif

	int32_t daycycle = 3600;
	//(1440 minutes/day)/(3600 seconds/day)*10 seconds event interval
	light_hour_delta = 1440 * 10 / daycycle;
	light_hour = SUNRISE + (SUNSET - SUNRISE) / 2;
	lightlevel = LIGHT_LEVEL_DAY;
	light_state = LIGHT_STATE_DAY;

	Scheduler::getScheduler().addEvent(createSchedulerTask(EVENT_LIGHTINTERVAL,
		boost::bind(&Game::checkLight, this)));

	checkCreatureLastIndex = 0;
	Scheduler::getScheduler().addEvent(createSchedulerTask(EVENT_CREATURE_THINK_INTERVAL,
		boost::bind(&Game::checkCreatures, this)));

	Scheduler::getScheduler().addEvent(createSchedulerTask(EVENT_DECAYINTERVAL,
		boost::bind(&Game::checkDecay, this)));
}

Game::~Game()
{
	if(map)
		delete map;
}

GameState_t Game::getGameState()
{
	return gameState;
}

void Game::setWorldType(WorldType_t type)
{
	worldType = type;
}

void Game::setGameState(GameState_t newState)
{
	if(gameState == GAME_STATE_SHUTDOWN)
		return; //this cannot be stopped

	if(gameState != newState)
	{
		gameState = newState;

		switch(newState)
		{
			case GAME_STATE_INIT:
			{
				Spawns::getInstance()->startup();

				Raids::getInstance()->loadFromXml();
				Raids::getInstance()->startup();

				Quests::getInstance()->loadFromXml();

				loadGameState();
				timedHighscoreUpdate();

				if(g_config.getBool(ConfigManager::REMOVE_PREMIUM_ON_INIT))
					IOLoginData::getInstance()->updatePremiumDays();
				break;
			}

			case GAME_STATE_SHUTDOWN:
			{
				//kick all players that are still online
				AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
				while(it != Player::listPlayer.list.end())
				{
					(*it).second->kickPlayer(true);
					it = Player::listPlayer.list.begin();
				}

				Houses::getInstance().payHouses();
				IOBan::getInstance()->clearTemporials();
				saveGameState(false);

				Dispatcher::getDispatcher().addTask(
					createTask(boost::bind(&Game::shutdown, this)));
				break;
			}

			case GAME_STATE_CLOSED:
			{
				//kick all players who not allowed to stay
				AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
				while(it != Player::listPlayer.list.end())
				{
					if(!(*it).second->hasFlag(PlayerFlag_CanAlwaysLogin))
					{
						(*it).second->kickPlayer(true);
						it = Player::listPlayer.list.begin();
					}
					else
						++it;
				}

				Houses::getInstance().payHouses();
				IOBan::getInstance()->clearTemporials();
				saveGameState(false);
				break;
			}

			case GAME_STATE_MAINTAIN:
			case GAME_STATE_STARTUP:
			case GAME_STATE_CLOSING:
			case GAME_STATE_NORMAL:
			default:
				break;
		}
	}
}

void Game::saveGameState(bool savePlayers)
{
	std::cout << "> Saving server..." << std::endl;

	if(savePlayers)
	{
		for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
		{
			(*it).second->loginPosition = (*it).second->getPosition();
			IOLoginData::getInstance()->savePlayer((*it).second, false);
		}
	}

	map->saveMap();
	if(g_config.getBool(ConfigManager::SAVE_GLOBAL_STORAGE))
		ScriptEnviroment::saveGameState();
}

void Game::loadGameState()
{
	loadMotd();
	loadPlayersRecord();

	ScriptEnviroment::loadGameState();
}

int32_t Game::loadMap(std::string filename)
{
	if(!map)
		map = new Map;

	maxPlayers = g_config.getNumber(ConfigManager::MAX_PLAYERS);
	inFightTicks = g_config.getNumber(ConfigManager::PZ_LOCKED);
	Player::maxMessageBuffer = g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER);
	Monster::despawnRange = g_config.getNumber(ConfigManager::DEFAULT_DESPAWNRANGE);
	Monster::despawnRadius = g_config.getNumber(ConfigManager::DEFAULT_DESPAWNRADIUS);
	return map->loadMap("data/world/" + filename + ".otbm");
}

void Game::refreshMap()
{
	Tile* tile;
	Item* item;

	for(Map::TileMap::iterator it = map->refreshTileMap.begin(); it != map->refreshTileMap.end(); ++it)
	{
		tile = it->first;

		//remove garbage
		int32_t downItemSize = tile->downItems.size();
		for(int32_t i = downItemSize - 1; i >= 0; --i)
		{
			item = tile->downItems[i];
			if(item)
			{
				#ifndef __DEBUG__
				internalRemoveItem(item);
				#else
				ReturnValue ret = internalRemoveItem(item);
				if(ret != RET_NOERROR)
					std::cout << "Could not refresh item: " << item->getID() << "pos: " << tile->getPosition() << std::endl;
				#endif
			}
		}

		cleanup();

		//restore to original state
		ItemVector list = it->second.list;
		for(ItemVector::reverse_iterator it = list.rbegin(); it != list.rend(); ++it)
		{
			Item* item = (*it)->clone();
			ReturnValue ret = internalAddItem(tile, item , INDEX_WHEREEVER, FLAG_NOLIMIT);
			if(ret == RET_NOERROR)
			{
				if(item->getUniqueId() != 0)
					ScriptEnviroment::addUniqueThing(item);
				startDecay(item);
			}
			else
			{
				std::cout << "Could not refresh item: " << item->getID() << "pos: " << tile->getPosition() << std::endl;
				delete item;
			}
		}
	}
}

Cylinder* Game::internalGetCylinder(Player* player, const Position& pos)
{
	if(pos.x != 0xFFFF)
		return getTile(pos.x, pos.y, pos.z);
	else
	{
		//container
		if(pos.y & 0x40)
		{
			uint8_t from_cid = pos.y & 0x0F;
			return player->getContainer(from_cid);
		}
		//inventory
		else
			return player;
	}
}

Thing* Game::internalGetThing(Player* player, const Position& pos, int32_t index, uint32_t spriteId /*= 0*/, stackPosType_t type /*= STACKPOS_NORMAL*/)
{
	if(pos.x != 0xFFFF)
	{
		Tile* tile = getTile(pos.x, pos.y, pos.z);
		if(tile)
		{
			/*look at*/
			if(type == STACKPOS_LOOK)
			{
				Creature* c;
				if(tile->getTopThing() && (c = tile->getTopThing()->getCreature()))
				{
					if(c && c->isInGhostMode() && !player->canSeeGhost(c))
					{
						CreatureVector v;
						CreatureVector::iterator it;

						Creature* ghostCreature = NULL;
						for(it = tile->creatures.begin(); it != tile->creatures.end(); ++it)
						{
							if(!(*it)->isInGhostMode() || player->canSeeGhost((*it)))
							{
								ghostCreature = (*it);
								break;
							}
						}

						if(ghostCreature)
							return ghostCreature;

						if(tile->getTopDownItem())
							return tile->getTopDownItem();

						if(tile->getTopTopItem())
							return tile->getTopTopItem();

						return tile->ground;
					}
					return c;
				}
				return tile->getTopThing();
			}

			Thing* thing = NULL;
			/*for move operations*/
			if(type == STACKPOS_MOVE)
			{
				Item* item = tile->getTopDownItem();
				if(item && !item->isNotMoveable())
					thing = item;
				else
					thing = tile->getTopCreature();
			}
			/*use item*/
			else if(type == STACKPOS_USE)
				thing = tile->getTopDownItem();
			else
				thing = tile->__getThing(index);

			if(player)
			{
				//do extra checks here if the thing is accessable
				if(thing && thing->getItem())
				{
					if(tile->hasProperty(ISVERTICAL))
					{
						if(player->getPosition().x + 1 == tile->getPosition().x)
							thing = NULL;
					}
					else if(tile->hasProperty(ISHORIZONTAL))
					{
						if(player->getPosition().y + 1 == tile->getPosition().y)
							thing = NULL;
					}
				}
			}
			return thing;
		}
	}
	else
	{
		//container
		if(pos.y & 0x40)
		{
			uint8_t fromCid = pos.y & 0x0F;
			uint8_t slot = pos.z;

			Container* parentcontainer = player->getContainer(fromCid);
			if(!parentcontainer)
				return NULL;

			return parentcontainer->getItem(slot);
		}
		else if(pos.y == 0 && pos.z == 0)
		{
			const ItemType& it = Item::items.getItemIdByClientId(spriteId);
			if(it.id == 0)
				return NULL;

			int32_t subType = -1;
			if(it.isFluidContainer())
			{
				int32_t maxFluidType = sizeof(reverseFluidMap) / sizeof(uint32_t);
				if(index < maxFluidType)
					subType = reverseFluidMap[index];
			}
			return findItemOfType(player, it.id, true, subType);
		}
		//inventory
		else
		{
			slots_t slot = (slots_t)static_cast<unsigned char>(pos.y);
			return player->getInventoryItem(slot);
		}
	}
	return NULL;
}

void Game::internalGetPosition(Item* item, Position& pos, uint8_t& stackpos)
{
	pos.x = 0;
	pos.y = 0;
	pos.z = 0;
	stackpos = 0;

	Cylinder* topParent = item->getTopParent();
	if(topParent)
	{
		if(Player* player = dynamic_cast<Player*>(topParent))
		{
			pos.x = 0xFFFF;

			Container* container = dynamic_cast<Container*>(item->getParent());
			if(container)
			{
				pos.y = ((uint16_t) ((uint16_t)0x40) | ((uint16_t)player->getContainerID(container)) );
				pos.z = container->__getIndexOfThing(item);
				stackpos = pos.z;
			}
			else
			{
				pos.y = player->__getIndexOfThing(item);
				stackpos = pos.y;
			}
		}
		else if(Tile* tile = topParent->getTile())
		{
			pos = tile->getPosition();
			stackpos = tile->__getIndexOfThing(item);
		}
	}
}

void Game::setTile(Tile* newTile)
{
	return map->setTile(newTile->getPosition(), newTile);
}

Tile* Game::getTile(uint32_t x, uint32_t y, uint32_t z)
{
	return map->getTile(x, y, z);
}

QTreeLeafNode* Game::getLeaf(uint32_t x, uint32_t y)
{
	return map->getLeaf(x, y);
}

Creature* Game::getCreatureByID(uint32_t id)
{
	if(id == 0)
		return NULL;

	AutoList<Creature>::listiterator it = listCreature.list.find(id);
	if(it != listCreature.list.end())
	{
		if(!(*it).second->isRemoved())
			return (*it).second;
	}
	return NULL; //just in case the player doesnt exist
}

Player* Game::getPlayerByID(uint32_t id)
{
	if(id == 0)
		return NULL;

	AutoList<Player>::listiterator it = Player::listPlayer.list.find(id);
	if(it != Player::listPlayer.list.end())
	{
		if(!(*it).second->isRemoved())
			return (*it).second;
	}
	return NULL; //just in case the player doesnt exist
}

Creature* Game::getCreatureByName(const std::string& s)
{
	std::string txt1 = asUpperCaseString(s);
	for(AutoList<Creature>::listiterator it = listCreature.list.begin(); it != listCreature.list.end(); ++it)
	{
		if(!(*it).second->isRemoved())
		{
			std::string txt2 = asUpperCaseString((*it).second->getName());
			if(txt1 == txt2)
				return it->second;
		}
	}
	return NULL; //just in case the creature doesnt exist
}

Player* Game::getPlayerByName(const std::string& s)
{
	std::string txt1 = asUpperCaseString(s);
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if(!(*it).second->isRemoved())
		{
			std::string txt2 = asUpperCaseString((*it).second->getName());
			if(txt1 == txt2)
				return it->second;
		}
	}
	return NULL; //just in case the player doesnt exist
}

ReturnValue Game::getPlayerByNameWildcard(const std::string& s, Player*& player)
{
	player = NULL;

	if(s.empty())
		return RET_PLAYERWITHTHISNAMEISNOTONLINE;

	if((*s.rbegin()) != '~')
	{
		player = getPlayerByName(s);
		if(!player)
			return RET_PLAYERWITHTHISNAMEISNOTONLINE;

		return RET_NOERROR;
	}

	Player* lastFound = NULL;
	std::string txt1 = asUpperCaseString(s.substr(0, s.length()-1));
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if(!(*it).second->isRemoved())
		{
			std::string txt2 = asUpperCaseString((*it).second->getName());
			if(txt2.substr(0, txt1.length()) == txt1)
			{
				if(lastFound == NULL)
					lastFound = (*it).second;
				else
					return RET_NAMEISTOOAMBIGIOUS;
			}
		}
	}

	if(lastFound != NULL)
	{
		player = lastFound;
		return RET_NOERROR;
	}
	return RET_PLAYERWITHTHISNAMEISNOTONLINE;
}

Player* Game::getPlayerByAccount(uint32_t acc)
{
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if(!it->second->isRemoved())
		{
			if(it->second->getAccount() == acc)
				return it->second;
		}
	}
	return NULL;
}

PlayerVector Game::getPlayersByAccount(uint32_t acc)
{
	PlayerVector players;
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if(!it->second->isRemoved())
		{
			if(it->second->getAccount() == acc)
				players.push_back(it->second);
		}
	}
	return players;
}

PlayerVector Game::getPlayersByIP(uint32_t ipadress, uint32_t mask)
{
	PlayerVector players;
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if(!it->second->isRemoved())
		{
			if((it->second->getIP() & mask) == (ipadress & mask))
				players.push_back(it->second);
		}
	}
	return players;
}

bool Game::internalPlaceCreature(Creature* creature, const Position& pos, bool forced /*= false*/)
{
	if(creature->getParent() != NULL)
		return false;

	if(!map->placeCreature(pos, creature, forced))
		return false;

	creature->useThing2();
	creature->setID();
	listCreature.addList(creature);
	creature->addList();
	return true;
}

bool Game::placeCreature(Creature* creature, const Position& pos, bool forced /*= false*/)
{
	if(!internalPlaceCreature(creature, pos, forced))
		return false;

	SpectatorVec list;
	SpectatorVec::iterator it;
	getSpectators(list, creature->getPosition(), false, true);

	Player* tmpPlayer = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureAppear(creature, true);
	}

	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureAppear(creature, true);

	int32_t newStackPos = creature->getParent()->__getIndexOfThing(creature);
	creature->getParent()->postAddNotification(creature, newStackPos);

	Player* player = creature->getPlayer();
	if(player)
	{
		Condition* conditionMuted = player->getCondition(CONDITION_MUTED, CONDITIONID_DEFAULT);
		if(conditionMuted && conditionMuted->getTicks() > 0)
		{
			conditionMuted->setTicks(conditionMuted->getTicks() - (time(NULL) - player->getLastLogout()) * 1000);
			if(conditionMuted->getTicks() <= 0)
				player->removeCondition(conditionMuted);
			else
				player->addCondition(conditionMuted->clone());
		}

		Condition* conditionTrade = player->getCondition(CONDITION_TRADETICKS, CONDITIONID_DEFAULT);
		if(conditionTrade && conditionTrade->getTicks() > 0)
		{
			conditionTrade->setTicks(conditionTrade->getTicks() - (time(NULL) - player->getLastLogout()) * 1000);
			if(conditionTrade->getTicks() <= 0)
				player->removeCondition(conditionTrade);
			else
				player->addCondition(conditionTrade->clone());
		}

		Condition* conditionYell = player->getCondition(CONDITION_YELLTICKS, CONDITIONID_DEFAULT);
		if(conditionYell && conditionYell->getTicks() > 0)
		{
			conditionYell->setTicks(conditionYell->getTicks() - (time(NULL) - player->getLastLogout()) * 1000);
			if(conditionYell->getTicks() <= 0)
				player->removeCondition(conditionYell);
			else
				player->addCondition(conditionYell->clone());
		}

		if(player->isPromoted() && g_vocations.getPromotedVocation(player->getVocationId()) != 0)
			player->setVocation(((player->isPremium() || !g_config.getBool(ConfigManager::PREMIUM_FOR_PROMOTION)) ? g_vocations.getPromotedVocation(player->vocation->getFromVocation()) : player->vocation->getFromVocation()));
	}

	addCreatureCheck(creature);
	creature->onPlacedCreature();
	return true;
}

bool Game::removeCreature(Creature* creature, bool isLogout /*= true*/)
{
	if(creature->isRemoved())
		return false;

#ifdef __DEBUG__
	std::cout << "removing creature " << std::endl;
#endif

	Cylinder* cylinder = creature->getTile();

	SpectatorVec list;
	SpectatorVec::iterator it;
	getSpectators(list, creature->getPosition(), false, true);

	int32_t index = cylinder->__getIndexOfThing(creature);
	if(!map->removeCreature(creature))
		return false;

	//send to client
	Player* player = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendCreatureDisappear(creature, index, isLogout);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureDisappear(creature, index, isLogout);

	creature->getParent()->postRemoveNotification(creature, index, true);

	listCreature.removeList(creature->getID());
	creature->removeList();
	creature->setRemoved();
	FreeThing(creature);

	removeCreatureCheck(creature);

	for(std::list<Creature*>::iterator cit = creature->summons.begin(); cit != creature->summons.end(); ++cit)
	{
		(*cit)->setLossSkill(false);
		removeCreature(*cit);
	}

	creature->onRemovedCreature();
	return true;
}

bool Game::playerMoveThing(uint32_t playerId, const Position& fromPos,
	uint16_t spriteId, uint8_t fromStackPos, const Position& toPos, uint8_t count)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint8_t fromIndex = 0;
	if(fromPos.x == 0xFFFF)
	{
		if(fromPos.y & 0x40)
			fromIndex = static_cast<uint8_t>(fromPos.z);
		else
			fromIndex = static_cast<uint8_t>(fromPos.y);
	}
	else
		fromIndex = fromStackPos;

	Thing* thing = internalGetThing(player, fromPos, fromIndex, spriteId, STACKPOS_MOVE);

	Cylinder* toCylinder = internalGetCylinder(player, toPos);

	if(!thing || !toCylinder)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(Creature* movingCreature = thing->getCreature())
	{
		if(Position::areInRange<1,1,0>(movingCreature->getPosition(), player->getPosition()))
		{
			SchedulerTask* task = createSchedulerTask(2000,
				boost::bind(&Game::playerMoveCreature, this, player->getID(),
				movingCreature->getID(), movingCreature->getPosition(), toCylinder->getPosition()));
			player->setNextActionTask(task);
		}
		else
			playerMoveCreature(playerId, movingCreature->getID(), movingCreature->getPosition(), toCylinder->getPosition());
	}
	else if(thing->getItem())
		playerMoveItem(playerId, fromPos, spriteId, fromStackPos, toPos, count);

	return true;
}

bool Game::playerMoveCreature(uint32_t playerId, uint32_t movingCreatureId,
	const Position& movingCreatureOrigPos, const Position& toPos)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();
		SchedulerTask* task = createSchedulerTask(delay, boost::bind(&Game::playerMoveCreature,
			this, playerId, movingCreatureId, movingCreatureOrigPos, toPos));
		player->setNextActionTask(task);
		return false;
	}

	player->setNextActionTask(NULL);

	Creature* movingCreature = getCreatureByID(movingCreatureId);
	if(!movingCreature || movingCreature->isRemoved())
		return false;

	if(movingCreature->getPlayer())
	{
		if(movingCreature->getPlayer()->getNoMove())
			return false;
	}

	if(!Position::areInRange<1,1,0>(movingCreatureOrigPos, player->getPosition()) && !player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar))
	{
		//need to walk to the creature first before moving it
		std::list<Direction> listDir;
		if(getPathToEx(player, movingCreatureOrigPos, listDir, 0, 1, true, true))
		{
			Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			SchedulerTask* task = createSchedulerTask(1500, boost::bind(&Game::playerMoveCreature, this,
				playerId, movingCreatureId, movingCreatureOrigPos, toPos));

			player->setNextWalkActionTask(task);
			return true;
		}
		else
		{
			player->sendCancelMessage(RET_THEREISNOWAY);
			return false;
		}
	}

	Tile* toTile = map->getTile(toPos);
	const Position& movingCreaturePos = movingCreature->getPosition();

	if(!toTile)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if((!movingCreature->isPushable() && !player->hasFlag(PlayerFlag_CanPushAllCreatures)) ||
		(movingCreature->isInGhostMode() && !player->canSeeGhost(movingCreature)))
	{
		player->sendCancelMessage(RET_NOTMOVEABLE);
		return false;
	}

	//check throw distance
	if((std::abs(movingCreaturePos.x - toPos.x) > movingCreature->getThrowRange()) || (std::abs(movingCreaturePos.y - toPos.y) > movingCreature->getThrowRange()) || (std::abs(movingCreaturePos.z - toPos.z) * 4 > movingCreature->getThrowRange()))
	{
		player->sendCancelMessage(RET_DESTINATIONOUTOFREACH);
		return false;
	}

	if(player != movingCreature)
	{
		if(toTile->hasProperty(BLOCKPATH))
		{
			player->sendCancelMessage(RET_NOTENOUGHROOM);
			return false;
		}
		else if((movingCreature->getZone() == ZONE_PROTECTION || movingCreature->getZone() == ZONE_NOPVP)
			&& !toTile->hasFlag(TILESTATE_NOPVPZONE) && !toTile->hasFlag(TILESTATE_PROTECTIONZONE) &&
			!player->hasFlag(PlayerFlag_CanPushAllCreatures) && !player->hasFlag(PlayerFlag_IgnoreProtectionZone))
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			return false;
		}
	}

	ReturnValue ret = internalMoveCreature(movingCreature, movingCreature->getTile(), toTile);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		return false;
	}
	return true;
}

ReturnValue Game::internalMoveCreature(Creature* creature, Direction direction, uint32_t flags /*= 0*/)
{
	Cylinder* fromTile = creature->getTile();
	Cylinder* toTile = NULL;

	creature->setLastPosition(creature->getPosition());
	const Position& currentPos = creature->getPosition();
	Position destPos = currentPos;
	destPos = getNextPosition(direction, destPos);

	if(creature->getPlayer())
	{
		//try go up
		if(currentPos.z != 8 && creature->getTile()->hasHeight(3))
		{
			Tile* tmpTile = map->getTile(currentPos.x, currentPos.y, currentPos.z - 1);
			if(tmpTile == NULL || (tmpTile->ground == NULL && !tmpTile->hasProperty(BLOCKSOLID)))
			{
				tmpTile = map->getTile(destPos.x, destPos.y, destPos.z - 1);
				if(tmpTile && tmpTile->ground && !tmpTile->hasProperty(BLOCKSOLID))
				{
					flags = flags | FLAG_IGNOREBLOCKITEM | FLAG_IGNOREBLOCKCREATURE;
					destPos.z--;
				}
			}
		}
		else
		{
			//try go down
			Tile* tmpTile = map->getTile(destPos);
			if(currentPos.z != 7 && (tmpTile == NULL || (tmpTile->ground == NULL && !tmpTile->hasProperty(BLOCKSOLID))))
			{
				tmpTile = map->getTile(destPos.x, destPos.y, destPos.z + 1);
				if(tmpTile && tmpTile->hasHeight(3))
				{
					flags = flags | FLAG_IGNOREBLOCKITEM | FLAG_IGNOREBLOCKCREATURE;
					destPos.z++;
				}
			}
		}
	}

	toTile = map->getTile(destPos);
	ReturnValue ret = RET_NOTPOSSIBLE;
	if(toTile != NULL)
		ret = internalMoveCreature(creature, fromTile, toTile, flags);

	if(ret != RET_NOERROR)
	{
		if(Player* player = creature->getPlayer())
		{
			player->sendCancelMessage(ret);
			player->sendCancelWalk();
		}
	}
	return ret;
}

ReturnValue Game::internalMoveCreature(Creature* creature, Cylinder* fromCylinder, Cylinder* toCylinder, uint32_t flags /*= 0*/)
{
	//check if we can move the creature to the destination
	ReturnValue ret = toCylinder->__queryAdd(0, creature, 1, flags);
	if(ret != RET_NOERROR)
		return ret;

	fromCylinder->getTile()->moveCreature(creature, toCylinder);

	int32_t index = 0;
	Item* toItem = NULL;
	Cylinder* subCylinder = NULL;

	uint32_t n = 0;
	while((subCylinder = toCylinder->__queryDestination(index, creature, &toItem, flags)) != toCylinder)
	{
		toCylinder->getTile()->moveCreature(creature, subCylinder);
		toCylinder = subCylinder;
		flags = 0;

		//to prevent infinite loop
		if(++n >= MAP_MAX_LAYERS)
			break;
	}
	return RET_NOERROR;
}

bool Game::playerMoveItem(uint32_t playerId, const Position& fromPos,
	uint16_t spriteId, uint8_t fromStackPos, const Position& toPos, uint8_t count)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();
		SchedulerTask* task = createSchedulerTask(delay, boost::bind(&Game::playerMoveItem, this,
			playerId, fromPos, spriteId, fromStackPos, toPos, count));
		player->setNextActionTask(task);
		return false;
	}

	player->setNextActionTask(NULL);

	Cylinder* fromCylinder = internalGetCylinder(player, fromPos);
	uint8_t fromIndex = 0;

	if(fromPos.x == 0xFFFF)
	{
		if(fromPos.y & 0x40)
			fromIndex = static_cast<uint8_t>(fromPos.z);
		else
			fromIndex = static_cast<uint8_t>(fromPos.y);
	}
	else
		fromIndex = fromStackPos;

	Thing* thing = internalGetThing(player, fromPos, fromIndex, spriteId, STACKPOS_MOVE);
	if(!thing || !thing->getItem())
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();

	Cylinder* toCylinder = internalGetCylinder(player, toPos);
	uint8_t toIndex = 0;

	if(toPos.x == 0xFFFF)
	{
		if(toPos.y & 0x40)
			toIndex = static_cast<uint8_t>(toPos.z);
		else
			toIndex = static_cast<uint8_t>(toPos.y);
	}

	if(fromCylinder == NULL || toCylinder == NULL || item == NULL || item->getClientID() != spriteId)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(!item->isPushable() || (item->getUniqueId() != 0 && !player->hasCustomFlag(PlayerCustomFlag_CanPushAllItems)))
	{
		player->sendCancelMessage(RET_NOTMOVEABLE);
		return false;
	}

	const Position& playerPos = player->getPosition();
	const Position& mapFromPos = fromCylinder->getTile()->getPosition();
	const Position& mapToPos = toCylinder->getTile()->getPosition();

	if(playerPos.z > mapFromPos.z && !player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
		return false;
	}

	if(playerPos.z < mapFromPos.z && !player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
		return false;
	}

	if(!Position::areInRange<1,1,0>(playerPos, mapFromPos) && !player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar))
	{
		//need to walk to the item first before using it
		std::list<Direction> listDir;
		if(getPathToEx(player, item->getPosition(), listDir, 0, 1, true, true))
		{
			Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerMoveItem, this,
				playerId, fromPos, spriteId, fromStackPos, toPos, count));

			player->setNextWalkActionTask(task);
			return true;
		}
		else
		{
			player->sendCancelMessage(RET_THEREISNOWAY);
			return false;
		}
	}

	//hangable item specific code
	if(item->isHangable() && toCylinder->getTile()->hasProperty(SUPPORTHANGABLE))
	{
		//destination supports hangable objects so need to move there first
		if(toCylinder->getTile()->hasProperty(ISVERTICAL))
		{
			if(player->getPosition().x + 1 == mapToPos.x)
			{
				player->sendCancelMessage(RET_NOTPOSSIBLE);
				return false;
			}
		}
		else if(toCylinder->getTile()->hasProperty(ISHORIZONTAL))
		{
			if(player->getPosition().y + 1 == mapToPos.y)
			{
				player->sendCancelMessage(RET_NOTPOSSIBLE);
				return false;
			}
		}

		if(!Position::areInRange<1,1,0>(playerPos, mapToPos) && !player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar))
		{
			Position walkPos = mapToPos;
			if(toCylinder->getTile()->hasProperty(ISVERTICAL))
				walkPos.x -= -1;

			if(toCylinder->getTile()->hasProperty(ISHORIZONTAL))
				walkPos.y -= -1;

			Position itemPos = fromPos;
			uint8_t itemStackPos = fromStackPos;

			if(fromPos.x != 0xFFFF && Position::areInRange<1,1,0>(mapFromPos, player->getPosition())
				&& !Position::areInRange<1,1,0>(mapFromPos, walkPos))
			{
				//need to pickup the item first
				Item* moveItem = NULL;

				ReturnValue ret = internalMoveItem(fromCylinder, player, INDEX_WHEREEVER, item, count, &moveItem);
				if(ret != RET_NOERROR)
				{
					player->sendCancelMessage(ret);
					return false;
				}

				//changing the position since its now in the inventory of the player
				internalGetPosition(moveItem, itemPos, itemStackPos);
			}

			std::list<Direction> listDir;
			if(map->getPathTo(player, walkPos, listDir))
			{
				Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerMoveItem, this,
					playerId, itemPos, spriteId, itemStackPos, toPos, count));

				player->setNextWalkActionTask(task);
				return true;
			}
			else
			{
				player->sendCancelMessage(RET_THEREISNOWAY);
				return false;
			}
		}
	}

	if(!player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		if((std::abs(playerPos.x - mapToPos.x) > item->getThrowRange()) ||
				(std::abs(playerPos.y - mapToPos.y) > item->getThrowRange()) ||
				(std::abs(mapFromPos.z - mapToPos.z) * 4 > item->getThrowRange()))
		{
			player->sendCancelMessage(RET_DESTINATIONOUTOFREACH);
			return false;
		}
	}

	if(!canThrowObjectTo(mapFromPos, mapToPos) && !player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		player->sendCancelMessage(RET_CANNOTTHROW);
		return false;
	}

	ReturnValue ret = internalMoveItem(fromCylinder, toCylinder, toIndex, item, count, NULL);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		return false;
	}
	return true;
}

ReturnValue Game::internalMoveItem(Cylinder* fromCylinder, Cylinder* toCylinder,
	int32_t index, Item* item, uint32_t count, Item** _moveItem, uint32_t flags /*= 0*/)
{
	if(!toCylinder)
		return RET_NOTPOSSIBLE;

	Item* toItem = NULL;
	toCylinder = toCylinder->__queryDestination(index, item, &toItem, flags);

	//destination is the same as the source?
	if(item == toItem)
		return RET_NOERROR; //silently ignore move

	//check if we can add this item
	ReturnValue ret = toCylinder->__queryAdd(index, item, count, flags);
	if(ret == RET_NEEDEXCHANGE)
	{
		//check if we can add it to source cylinder
		int32_t fromIndex = fromCylinder->__getIndexOfThing(item);

		ret = fromCylinder->__queryAdd(fromIndex, toItem, toItem->getItemCount(), 0);
		if(ret == RET_NOERROR)
		{
			//check how much we can move
			uint32_t maxExchangeQueryCount = 0;
			ReturnValue retExchangeMaxCount = fromCylinder->__queryMaxCount(-1, toItem, toItem->getItemCount(), maxExchangeQueryCount, 0);

			if(retExchangeMaxCount != RET_NOERROR && maxExchangeQueryCount == 0)
				return retExchangeMaxCount;

			if((toCylinder->__queryRemove(toItem, toItem->getItemCount()) == RET_NOERROR) && ret == RET_NOERROR)
			{
				int32_t oldToItemIndex = toCylinder->__getIndexOfThing(toItem);
				toCylinder->__removeThing(toItem, toItem->getItemCount());
				fromCylinder->__addThing(toItem);

				if(oldToItemIndex != -1)
					toCylinder->postRemoveNotification(toItem, oldToItemIndex, true);

				int32_t newToItemIndex = fromCylinder->__getIndexOfThing(toItem);
				if(newToItemIndex != -1)
					fromCylinder->postAddNotification(toItem, newToItemIndex);

				ret = toCylinder->__queryAdd(index, item, count, flags);
				toItem = NULL;
			}
		}
	}

	if(ret != RET_NOERROR)
		return ret;

	//check how much we can move
	uint32_t maxQueryCount = 0;
	ReturnValue retMaxCount = toCylinder->__queryMaxCount(index, item, count, maxQueryCount, flags);

	if(retMaxCount != RET_NOERROR && maxQueryCount == 0)
		return retMaxCount;

	uint32_t m = 0;
	uint32_t n = 0;

	if(item->isStackable())
		m = std::min((uint32_t)count, maxQueryCount);
	else
		m = maxQueryCount;

	Item* moveItem = item;

	//check if we can remove this item
	ret = fromCylinder->__queryRemove(item, m);
	if(ret != RET_NOERROR)
		return ret;

	//remove the item
	int32_t itemIndex = fromCylinder->__getIndexOfThing(item);
	Item* updateItem = NULL;
	fromCylinder->__removeThing(item, m);
	bool isCompleteRemoval = item->isRemoved();

	//update item(s)
	if(item->isStackable())
	{
		if(toItem && toItem->getID() == item->getID())
		{
			n = std::min((uint32_t)100 - toItem->getItemCount(), m);
			toCylinder->__updateThing(toItem, toItem->getID(), toItem->getItemCount() + n);
			updateItem = toItem;
		}

		if(m - n > 0)
			moveItem = Item::CreateItem(item->getID(), m - n);
		else
			moveItem = NULL;

		if(item->isRemoved())
			FreeThing(item);
	}

	//add item
	if(moveItem /*m - n > 0*/)
		toCylinder->__addThing(index, moveItem);

	if(itemIndex != -1)
		fromCylinder->postRemoveNotification(item, itemIndex, isCompleteRemoval);

	if(moveItem)
	{
		int32_t moveItemIndex = toCylinder->__getIndexOfThing(moveItem);
		if(moveItemIndex != -1)
			toCylinder->postAddNotification(moveItem, moveItemIndex);
	}

	if(updateItem)
	{
		int32_t updateItemIndex = toCylinder->__getIndexOfThing(updateItem);
		if(updateItemIndex != -1)
			toCylinder->postAddNotification(updateItem, updateItemIndex);
	}

	if(_moveItem)
	{
		if(moveItem)
			*_moveItem = moveItem;
		else
			*_moveItem = item;
	}

	//we could not move all, inform the player
	if(item->isStackable() && maxQueryCount < count)
		return retMaxCount;

	return ret;
}

ReturnValue Game::internalAddItem(Cylinder* toCylinder, Item* item, int32_t index /*= INDEX_WHEREEVER*/,
	uint32_t flags /*= 0*/, bool test /*= false*/)
{
	if(toCylinder == NULL || item == NULL)
		return RET_NOTPOSSIBLE;

	Item* toItem = NULL;
	toCylinder = toCylinder->__queryDestination(index, item, &toItem, flags);

	//check if we can add this item
	ReturnValue ret = toCylinder->__queryAdd(index, item, item->getItemCount(), flags);
	if(ret != RET_NOERROR)
		return ret;

	//check how much we can move
	uint32_t maxQueryCount = 0;
	ret = toCylinder->__queryMaxCount(index, item, item->getItemCount(), maxQueryCount, flags);

	if(ret != RET_NOERROR)
		return ret;

	uint32_t m = 0;
	uint32_t n = 0;

	if(item->isStackable())
		m = std::min((uint32_t)item->getItemCount(), maxQueryCount);
	else
		m = maxQueryCount;

	if(!test)
	{
		Item* moveItem = item;

		//update item(s)
		if(item->isStackable())
		{
			if(toItem && toItem->getID() == item->getID())
			{
				n = std::min((uint32_t)100 - toItem->getItemCount(), m);
				toCylinder->__updateThing(toItem, toItem->getID(), toItem->getItemCount() + n);
			}

			if(m - n > 0)
			{
				if(m - n != item->getItemCount())
					moveItem = Item::CreateItem(item->getID(), m - n);
			}
			else
			{
				moveItem = NULL;
				FreeThing(item);
			}
		}

		if(moveItem)
		{
			toCylinder->__addThing(index, moveItem);
			int32_t moveItemIndex = toCylinder->__getIndexOfThing(moveItem);
			if(moveItemIndex != -1)
				toCylinder->postAddNotification(moveItem, moveItemIndex);
		}
		else
		{
			int32_t itemIndex = toCylinder->__getIndexOfThing(item);
			if(itemIndex != -1)
				toCylinder->postAddNotification(item, itemIndex);
		}
	}
	return RET_NOERROR;
}

ReturnValue Game::internalRemoveItem(Item* item, int32_t count /*= -1*/, bool test /*= false*/)
{
	Cylinder* cylinder = item->getParent();
	if(cylinder == NULL)
		return RET_NOTPOSSIBLE;

	if(count == -1)
		count = item->getItemCount();

	//check if we can remove this item
	ReturnValue ret = cylinder->__queryRemove(item, count);
	if(ret != RET_NOERROR && ret != RET_NOTMOVEABLE)
		return ret;

	if(!item->canRemove())
		return RET_NOTPOSSIBLE;

	if(!test)
	{
		int32_t index = cylinder->__getIndexOfThing(item);

		//remove the item
		cylinder->__removeThing(item, count);
		bool isCompleteRemoval = false;
		if(item->isRemoved())
		{
			isCompleteRemoval = true;
			FreeThing(item);
		}
		cylinder->postRemoveNotification(item, index, isCompleteRemoval);
	}

	item->onRemoved();
	return RET_NOERROR;
}

ReturnValue Game::internalPlayerAddItem(Player* player, Item* item, bool dropOnMap /*= true*/)
{
	ReturnValue ret = internalAddItem(player, item);
	if(ret != RET_NOERROR && dropOnMap)
		ret = internalAddItem(player->getTile(), item, INDEX_WHEREEVER, FLAG_NOLIMIT);

	return ret;
}

Item* Game::findItemOfType(Cylinder* cylinder, uint16_t itemId,
	bool depthSearch /*= true*/, int32_t subType /*= -1*/)
{
	if(cylinder == NULL)
		return false;

	std::list<Container*> listContainer;
	Container* tmpContainer = NULL;
	Thing* thing = NULL;
	Item* item = NULL;

	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex();)
	{
		if((thing = cylinder->__getThing(i)) && (item = thing->getItem()))
		{
			if(item->getID() == itemId && (subType == -1 || subType == item->getSubType()))
				return item;
			else
			{
				++i;
				if(depthSearch && (tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
		else
			++i;
	}

	while(listContainer.size() > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(int32_t i = 0; i < (int32_t)container->size();)
		{
			Item* item = container->getItem(i);
			if(item->getID() == itemId && (subType == -1 || subType == item->getSubType()))
				return item;
			else
			{
				++i;
				if((tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
	}

	return NULL;
}

bool Game::removeItemOfType(Cylinder* cylinder, uint16_t itemId, int32_t count, int32_t subType /*= -1*/)
{
	if(cylinder == NULL || ((int32_t)cylinder->__getItemTypeCount(itemId, subType) < count))
		return false;

	if(count <= 0)
		return true;

	std::list<Container*> listContainer;
	Container* tmpContainer = NULL;
	Thing* thing = NULL;
	Item* item = NULL;

	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex() && count > 0;)
	{
		if((thing = cylinder->__getThing(i)) && (item = thing->getItem()))
		{
			if(item->getID() == itemId)
			{
				if(item->isStackable())
				{
					if(item->getItemCount() > count)
					{
						internalRemoveItem(item, count);
						count = 0;
					}
					else
					{
						count -= item->getItemCount();
						internalRemoveItem(item);
					}
				}
				else if(subType == -1 || subType == item->getSubType())
				{
					--count;
					internalRemoveItem(item);
				}
			}
			else
			{
				++i;
				if((tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
		else
			++i;
	}

	while(listContainer.size() > 0 && count > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(int32_t i = 0; i < (int32_t)container->size() && count > 0;)
		{
			Item* item = container->getItem(i);
			if(item->getID() == itemId)
			{
				if(item->isStackable())
				{
					if(item->getItemCount() > count)
					{
						internalRemoveItem(item, count);
						count = 0;
					}
					else
					{
						count-= item->getItemCount();
						internalRemoveItem(item);
					}
				}
				else if(subType == -1 || subType == item->getSubType())
				{
					--count;
					internalRemoveItem(item);
				}
			}
			else
			{
				++i;
				if((tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
	}
	return (count == 0);
}

uint32_t Game::getMoney(const Cylinder* cylinder)
{
	if(cylinder == NULL)
		return 0;

	std::list<Container*> listContainer;
	ItemList::const_iterator it;
	Container* tmpContainer = NULL;

	Thing* thing = NULL;
	Item* item = NULL;

	uint32_t moneyCount = 0;
	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex(); ++i)
	{
		if(!(thing = cylinder->__getThing(i)))
			continue;

		if(!(item = thing->getItem()))
			continue;

		if((tmpContainer = item->getContainer()))
			listContainer.push_back(tmpContainer);
		else if(item->getWorth() != 0)
			moneyCount += item->getWorth();
	}

	while(listContainer.size() > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();

		for(it = container->getItems(); it != container->getEnd(); ++it)
		{
			Item* item = *it;
			if((tmpContainer = item->getContainer()))
				listContainer.push_back(tmpContainer);
			else if(item->getWorth() != 0)
				moneyCount += item->getWorth();
		}
	}
	return moneyCount;
}

bool Game::removeMoney(Cylinder* cylinder, int32_t money, uint32_t flags /*= 0*/)
{
	if(cylinder == NULL)
		return false;
	if(money <= 0)
		return true;

	std::list<Container*> listContainer;
	Container* tmpContainer = NULL;

	typedef std::multimap<int, Item*, std::less<int> > MoneyMap;
	typedef MoneyMap::value_type moneymap_pair;
	MoneyMap moneyMap;
	Thing* thing = NULL;
	Item* item = NULL;
	int32_t moneyCount = 0;

	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex() && money > 0; ++i)
	{
		if(!(thing = cylinder->__getThing(i)))
			continue;

		if(!(item = thing->getItem()))
			continue;

		if((tmpContainer = item->getContainer()))
			listContainer.push_back(tmpContainer);
		else if(item->getWorth() != 0)
		{
			moneyCount += item->getWorth();
			moneyMap.insert(moneymap_pair(item->getWorth(), item));
		}
	}

	while(listContainer.size() > 0 && money > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();

		for(int32_t i = 0; i < (int32_t)container->size() && money > 0; i++)
		{
			Item* item = container->getItem(i);
			if(((tmpContainer = item->getContainer())))
				listContainer.push_back(tmpContainer);
			else if(item->getWorth() != 0)
			{
				moneyCount += item->getWorth();
				moneyMap.insert(moneymap_pair(item->getWorth(), item));
			}
		}
	}

	/*not enough money*/
	if(moneyCount < money)
		return false;

	MoneyMap::iterator mit;
	for(mit = moneyMap.begin(); mit != moneyMap.end() && money > 0; mit++)
	{
		Item* item = mit->second;
		internalRemoveItem(item);
		if(mit->first <= money)
			money = money - mit->first;
		else
		{
			/* Remove a monetary value from an item*/
			int32_t remaind = item->getWorth() - money;
			addMoney(cylinder, remaind, flags);
			money = 0;
		}

		mit->second = NULL;
	}

	moneyMap.clear();

	return (money == 0);
}

void Game::addMoney(Cylinder* cylinder, int32_t money, uint32_t flags /*= 0*/)
{
	int32_t crys = money / 10000;
	money -= crys * 10000;
	int32_t plat = money / 100;
	money -= plat * 100;
	int32_t gold = money;

	if(crys != 0)
	{
		do
		{
			Item* remaindItem = Item::CreateItem(ITEM_COINS_CRYSTAL, std::min(100, crys));

			ReturnValue ret = internalAddItem(cylinder, remaindItem, INDEX_WHEREEVER, flags);
			if(ret != RET_NOERROR)
				internalAddItem(cylinder->getTile(), remaindItem, INDEX_WHEREEVER, FLAG_NOLIMIT);

			crys -= std::min(100, crys);
		}
		while(crys > 0);
	}

	if(plat != 0)
	{
		Item* remaindItem = Item::CreateItem(ITEM_COINS_PLATINUM, plat);

		ReturnValue ret = internalAddItem(cylinder, remaindItem, INDEX_WHEREEVER, flags);
		if(ret != RET_NOERROR)
			internalAddItem(cylinder->getTile(), remaindItem, INDEX_WHEREEVER, FLAG_NOLIMIT);
	}

	if(gold != 0)
	{
		Item* remaindItem = Item::CreateItem(ITEM_COINS_GOLD, gold);

		ReturnValue ret = internalAddItem(cylinder, remaindItem, INDEX_WHEREEVER, flags);
		if(ret != RET_NOERROR)
			internalAddItem(cylinder->getTile(), remaindItem, INDEX_WHEREEVER, FLAG_NOLIMIT);
	}
}

Item* Game::transformItem(Item* item, uint16_t newId, int32_t newCount /*= -1*/)
{
	if(item->getID() == newId && (newCount == -1 || (newCount == item->getSubType() && newCount != 0)))
		return item;

	Cylinder* cylinder = item->getParent();
	if(cylinder == NULL)
		return NULL;

	int32_t itemIndex = cylinder->__getIndexOfThing(item);
	if(itemIndex == -1)
	{
#ifdef __DEBUG__
		std::cout << "Error: transformItem, itemIndex == -1" << std::endl;
#endif
		return item;
	}

	if(!item->canTransform())
		return item;

	const ItemType& curType = Item::items[item->getID()];
	const ItemType& newType = Item::items[newId];

	if(curType.alwaysOnTop != newType.alwaysOnTop)
	{
		//This only occurs when you transform items on tiles from a downItem to a topItem (or vice versa)
		//Remove the old, and add the new
		ReturnValue ret = internalRemoveItem(item);
		if(ret != RET_NOERROR)
			return item;

		Item* newItem = NULL;
		if(newCount == -1)
			newItem = Item::CreateItem(newId);
		else
			newItem = Item::CreateItem(newId, newCount);

		newItem->copyAttributes(item);

		ret = internalAddItem(cylinder, newItem, INDEX_WHEREEVER);
		if(ret != RET_NOERROR)
		{
			delete newItem;
			return NULL;
		}
		return newItem;
	}

	if(curType.type == newType.type)
	{
		//Both items has the same type so we can safely change id/subtype
		if(newCount == 0 && (item->isStackable() || item->hasCharges()))
		{
			if(item->isStackable())
			{
				internalRemoveItem(item);
				return NULL;
			}
			else
			{
				int32_t newItemId = newId;
				if(curType.id == newType.id)
					newItemId = curType.decayTo;

				if(newItemId != -1)
				{
					item = transformItem(item, newItemId);
					return item;
				}
				else
				{
					internalRemoveItem(item);
					return NULL;
				}
			}
		}
		else
		{
			cylinder->postRemoveNotification(item, itemIndex, false);
			uint16_t itemId = item->getID();
			int32_t count = item->getSubType();

			if(curType.id != newType.id)
			{
				if(newType.group != curType.group)
					item->setDefaultSubtype();

				itemId = newId;
			}

			if(newCount != -1 && newType.hasSubType())
				count = newCount;

			cylinder->__updateThing(item, itemId, count);
			cylinder->postAddNotification(item, itemIndex);
			return item;
		}
	}
	else
	{
		//Replacing the the old item with the new while maintaining the old position
		Item* newItem = NULL;
		if(newCount == -1)
			newItem = Item::CreateItem(newId);
		else
			newItem = Item::CreateItem(newId, newCount);

		if(newItem == NULL)
		{
			#ifdef __DEBUG__
			std::cout << "Error: [Game::transformItem] Item of type " << item->getID() << " transforming into invalid type " << newId << std::endl;
			#endif
			return NULL;
		}

		cylinder->__replaceThing(itemIndex, newItem);
		cylinder->postAddNotification(newItem, itemIndex);

		item->setParent(NULL);
		cylinder->postRemoveNotification(item, itemIndex, true);
		FreeThing(item);

		return newItem;
	}
	return NULL;
}

ReturnValue Game::internalTeleport(Thing* thing, const Position& newPos, bool pushMove)
{
	if(newPos == thing->getPosition())
		return RET_NOERROR;
	else if(thing->isRemoved())
		return RET_NOTPOSSIBLE;

	Tile* toTile = getTile(newPos.x, newPos.y, newPos.z);
	if(toTile)
	{
		if(Creature* creature = thing->getCreature())
		{
			if(Position::areInRange<1,1,0>(creature->getPosition(), newPos) && pushMove)
				creature->getTile()->moveCreature(creature, toTile, false);
			else
				creature->getTile()->moveCreature(creature, toTile, true);
			return RET_NOERROR;
		}
		else if(Item* item = thing->getItem())
			return internalMoveItem(item->getParent(), toTile, INDEX_WHEREEVER, item, item->getItemCount(), NULL);
	}

	return RET_NOTPOSSIBLE;
}

//Implementation of player invoked events
bool Game::playerMove(uint32_t playerId, Direction direction)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(player->getNoMove())
	{
		player->sendCancelWalk();
		return false;
	}

	player->stopWalk();
	int32_t delay = player->getWalkDelay(direction);
	if(delay > 0)
	{
		player->setNextAction(OTSYS_TIME() + player->getStepDuration());
		SchedulerTask* task = createSchedulerTask(((uint32_t)delay), boost::bind(&Game::playerMove, this, playerId, direction));
		player->setNextWalkTask(task);
		return false;
	}

	player->resetIdleTime();
	player->setFollowCreature(NULL);
	player->onWalk(direction);

	return (internalMoveCreature(player, direction) == RET_NOERROR);
}

bool Game::playerBroadcastMessage(Player* player, const std::string& text, SpeakClasses type)
{
	if(!player->hasFlag(PlayerFlag_CanBroadcast))
		return false;

	if(type >= SPEAK_CLASS_FIRST && type <= SPEAK_CLASS_LAST)
	{
		std::cout << "> " << player->getName() << " broadcasted: \"" << text << "\"." << std::endl;
		for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
			(*it).second->sendCreatureSay(player, type, text);
		return true;
	}
	return false;
}

bool Game::playerCreatePrivateChannel(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || !player->isPremium())
		return false;

	ChatChannel* channel = g_chat.createChannel(player, 0xFFFF);
	if(!channel)
		return false;

	if(!channel->addUser(player))
		return false;

	player->sendCreatePrivateChannel(channel->getId(), channel->getName());
	return true;
}

bool Game::playerChannelInvite(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	PrivateChatChannel* channel = g_chat.getPrivateChannel(player);
	if(!channel)
		return false;

	Player* invitePlayer = getPlayerByName(name);
	if(!invitePlayer)
		return false;

	channel->invitePlayer(player, invitePlayer);
	return true;
}

bool Game::playerChannelExclude(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	PrivateChatChannel* channel = g_chat.getPrivateChannel(player);
	if(!channel)
		return false;

	Player* excludePlayer = getPlayerByName(name);
	if(!excludePlayer)
		return false;

	channel->excludePlayer(player, excludePlayer);
	return true;
}

bool Game::playerRequestChannels(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->sendChannelsDialog();
	return true;
}

bool Game::playerOpenChannel(uint32_t playerId, uint16_t channelId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!g_chat.addUserToChannel(player, channelId))
		return false;

	ChatChannel* channel = g_chat.getChannel(player, channelId);
	if(!channel)
		return false;

	if(channel->getId() != 0x03)
		player->sendChannel(channel->getId(), channel->getName());
	else
		player->sendRuleViolationsChannel(channel->getId());
	return true;
}

bool Game::playerCloseChannel(uint32_t playerId, uint16_t channelId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	g_chat.removeUserFromChannel(player, channelId);
	return true;
}

bool Game::playerOpenPrivateChannel(uint32_t playerId, std::string& receiver)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint32_t guid;
	IOLoginData::getInstance()->getGuidByName(guid, receiver);
	if(IOLoginData::getInstance()->playerExists(receiver))
		player->sendOpenPrivateChannel(receiver);
	else
		player->sendCancel("A player with this name does not exist.");
	return true;
}

bool Game::playerProcessRuleViolation(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->hasFlag(PlayerFlag_CanAnswerRuleViolations))
		return false;

	Player* reporter = getPlayerByName(name);
	if(!reporter)
		return false;

	RuleViolationsMap::iterator it = ruleViolations.find(reporter->getID());
	if(it == ruleViolations.end())
		return false;

	RuleViolation& rvr = *it->second;
	if(!rvr.isOpen)
		return false;

	rvr.isOpen = false;
	rvr.gamemaster = player;

	ChatChannel* channel = g_chat.getChannelById(0x03);
	if(channel)
	{
		for(UsersMap::const_iterator it = channel->getUsers().begin(); it != channel->getUsers().end(); ++it)
		{
			if(it->second)
				it->second->sendRemoveReport(reporter->getName());
		}
	}
	return true;
}

bool Game::playerCloseRuleViolation(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* reporter = getPlayerByName(name);
	if(!reporter)
		return false;

	return closeRuleViolation(reporter);
}

bool Game::playerCancelRuleViolation(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	return cancelRuleViolation(player);
}

bool Game::playerCloseNpcChannel(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	SpectatorVec list;
	SpectatorVec::iterator it;
	getSpectators(list, player->getPosition());
	Npc* npc = NULL;

	for(it = list.begin(); it != list.end(); ++it)
	{
		if((npc = (*it)->getNpc()))
			npc->onPlayerCloseChannel(player);
	}
	return true;
}

bool Game::playerReceivePing(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->receivePing();
	return true;
}

bool Game::playerAutoWalk(uint32_t playerId, std::list<Direction>& listDir)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->resetIdleTime();
	player->setNextWalkTask(NULL);
	return player->startAutoWalk(listDir);
}

bool Game::playerStopAutoWalk(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->stopWalk();
	return true;
}

bool Game::playerUseItemEx(uint32_t playerId, const Position& fromPos, uint8_t fromStackPos, uint16_t fromSpriteId,
	const Position& toPos, uint8_t toStackPos, uint16_t toSpriteId, bool isHotkey)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(isHotkey && !g_config.getBool(ConfigManager::AIMBOT_HOTKEY_ENABLED))
		return false;

	Thing* thing = internalGetThing(player, fromPos, fromStackPos, fromSpriteId);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	if(!item || item->getClientID() != fromSpriteId || !item->isUseable())
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	Position walkToPos = fromPos;
	ReturnValue ret = g_actions->canUse(player, fromPos);
	if(ret == RET_NOERROR)
	{
		ret = g_actions->canUse(player, toPos, item);
		if(ret == RET_TOOFARAWAY)
			walkToPos = toPos;
	}

	if(ret != RET_NOERROR)
	{
		if(ret == RET_TOOFARAWAY)
		{
			Position itemPos = fromPos;
			uint8_t itemStackPos = fromStackPos;
			if(fromPos.x != 0xFFFF && toPos.x != 0xFFFF && Position::areInRange<1,1,0>(fromPos, player->getPosition()) &&
				!Position::areInRange<1,1,0>(fromPos, toPos))
			{
				Item* moveItem = NULL;

				ReturnValue ret = internalMoveItem(item->getParent(), player, INDEX_WHEREEVER,
					item, item->getItemCount(), &moveItem);
				if(ret != RET_NOERROR)
				{
					player->sendCancelMessage(ret);
					return false;
				}

				//changing the position since its now in the inventory of the player
				internalGetPosition(moveItem, itemPos, itemStackPos);
			}

			std::list<Direction> listDir;
			if(getPathToEx(player, walkToPos, listDir, 0, 1, true, true))
			{
				Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerUseItemEx, this,
					playerId, itemPos, itemStackPos, fromSpriteId, toPos, toStackPos, toSpriteId, isHotkey));

				player->setNextWalkActionTask(task);
				return true;
			}
			else
			{
				player->sendCancelMessage(RET_THEREISNOWAY);
				return false;
			}
		}

		player->sendCancelMessage(ret);
		return false;
	}

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();
		SchedulerTask* task = createSchedulerTask(delay, boost::bind(&Game::playerUseItemEx, this,
			playerId, fromPos, fromStackPos, fromSpriteId, toPos, toStackPos, toSpriteId, isHotkey));
		player->setNextActionTask(task);
		return false;
	}

	player->setNextActionTask(NULL);

	return g_actions->useItemEx(player, fromPos, toPos, toStackPos, item, isHotkey);
}

bool Game::playerUseItem(uint32_t playerId, const Position& pos, uint8_t stackPos,
	uint8_t index, uint16_t spriteId, bool isHotkey)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(isHotkey && !g_config.getBool(ConfigManager::AIMBOT_HOTKEY_ENABLED))
		return false;

	Thing* thing = internalGetThing(player, pos, stackPos, spriteId);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	if(!item || item->getClientID() != spriteId)
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	ReturnValue ret = g_actions->canUse(player, pos);
	if(ret != RET_NOERROR)
	{
		if(ret == RET_TOOFARAWAY)
		{
			std::list<Direction> listDir;
			if(getPathToEx(player, pos, listDir, 0, 1, true, true))
			{
				Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerUseItem, this,
					playerId, pos, stackPos, index, spriteId, isHotkey));

				player->setNextWalkActionTask(task);
				return true;
			}
			ret = RET_THEREISNOWAY;
		}
		player->sendCancelMessage(ret);
		return false;
	}

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();
		SchedulerTask* task = createSchedulerTask(delay, boost::bind(&Game::playerUseItem, this,
			playerId, pos, stackPos, index, spriteId, isHotkey));
		player->setNextActionTask(task);
		return false;
	}

	player->setNextActionTask(NULL);

	g_actions->useItem(player, pos, index, item, isHotkey);
	return true;
}

bool Game::playerUseBattleWindow(uint32_t playerId, const Position& fromPos, uint8_t fromStackPos,
	uint32_t creatureId, uint16_t spriteId, bool isHotkey)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Creature* creature = getCreatureByID(creatureId);
	if(!creature)
		return false;

	if(!Position::areInRange<7,5,0>(creature->getPosition(), player->getPosition()))
		return false;

	if(!g_config.getBool(ConfigManager::AIMBOT_HOTKEY_ENABLED))
	{
		if(creature->getPlayer() || isHotkey)
		{
			player->sendCancelMessage(RET_DIRECTPLAYERSHOOT);
			return false;
		}
	}

	Thing* thing = internalGetThing(player, fromPos, fromStackPos, spriteId, STACKPOS_USE);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	if(!item || item->getClientID() != spriteId)
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	ReturnValue ret = g_actions->canUse(player, fromPos);
	if(ret != RET_NOERROR)
	{
		if(ret == RET_TOOFARAWAY)
		{
			std::list<Direction> listDir;
			if(getPathToEx(player, item->getPosition(), listDir, 0, 1, true, true))
			{
				Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerUseBattleWindow, this,
					playerId, fromPos, fromStackPos, creatureId, spriteId, isHotkey));

				player->setNextWalkActionTask(task);
				return true;
			}
			ret = RET_THEREISNOWAY;
		}
		player->sendCancelMessage(ret);
		return false;
	}

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();
		SchedulerTask* task = createSchedulerTask(delay, boost::bind(&Game::playerUseBattleWindow, this,
			playerId, fromPos, fromStackPos, creatureId, spriteId, isHotkey));
		player->setNextActionTask(task);
		return false;
	}

	player->setNextActionTask(NULL);

	return g_actions->useItemEx(player, fromPos, creature->getPosition(), creature->getParent()->__getIndexOfThing(creature), item, isHotkey, creatureId);
}

bool Game::playerCloseContainer(uint32_t playerId, uint8_t cid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->closeContainer(cid);
	player->sendCloseContainer(cid);
	return true;
}

bool Game::playerMoveUpContainer(uint32_t playerId, uint8_t cid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Container* container = player->getContainer(cid);
	if(!container)
		return false;

	Container* parentContainer = dynamic_cast<Container*>(container->getParent());
	if(!parentContainer)
		return false;

	bool hasParent = (dynamic_cast<const Container*>(parentContainer->getParent()) != NULL);
	player->addContainer(cid, parentContainer);
	player->sendContainer(cid, parentContainer, hasParent);
	return true;
}

bool Game::playerUpdateTile(uint32_t playerId, const Position& pos)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(player->canSee(pos))
	{
		Tile* tile = getTile(pos.x, pos.y, pos.z);
		player->sendUpdateTile(tile, pos);
		return true;
	}
	return false;
}

bool Game::playerUpdateContainer(uint32_t playerId, uint8_t cid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Container* container = player->getContainer(cid);
	if(!container)
		return false;

	bool hasParent = (dynamic_cast<const Container*>(container->getParent()) != NULL);
	player->sendContainer(cid, container, hasParent);
	return true;
}

bool Game::playerRotateItem(uint32_t playerId, const Position& pos, uint8_t stackPos, const uint16_t spriteId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Thing* thing = internalGetThing(player, pos, stackPos);
	if(!thing)
		return false;

	Item* item = thing->getItem();
	if(!item || item->getClientID() != spriteId || !item->isRoteable() || item->getUniqueId() != 0)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(pos.x != 0xFFFF && !Position::areInRange<1,1,0>(pos, player->getPosition()))
	{
		std::list<Direction> listDir;
		if(getPathToEx(player, pos, listDir, 0, 1, true, true))
		{
			Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerRotateItem, this,
				playerId, pos, stackPos, spriteId));

			player->setNextWalkActionTask(task);
			return true;
		}
		else
		{
			player->sendCancelMessage(RET_THEREISNOWAY);
			return false;
		}
	}

	uint16_t newId = Item::items[item->getID()].rotateTo;
	if(newId != 0)
		transformItem(item, newId);

	return true;
}

bool Game::playerWriteItem(uint32_t playerId, uint32_t windowTextId, const std::string& text)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint16_t maxTextLength = 0;
	uint32_t internalWindowTextId = 0;
	Item* writeItem = player->getWriteItem(internalWindowTextId, maxTextLength);

	if(text.length() > maxTextLength || windowTextId != internalWindowTextId)
		return false;

	if(!writeItem || writeItem->isRemoved())
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Cylinder* topParent = writeItem->getTopParent();
	Player* owner = dynamic_cast<Player*>(topParent);
	if(owner && owner != player)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(!Position::areInRange<1,1,0>(writeItem->getPosition(), player->getPosition()))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(!text.empty())
	{
		if(writeItem->getText() != text)
		{
			writeItem->setText(text);
			writeItem->setWriter(player->getName());
			writeItem->setDate(std::time(NULL));
		}
	}
	else
	{
		writeItem->resetText();
		writeItem->resetWriter();
		writeItem->resetDate();
	}

	uint16_t newId = Item::items[writeItem->getID()].writeOnceItemId;
	if(newId != 0)
		transformItem(writeItem, newId);

	player->setWriteItem(NULL);
	return true;
}

bool Game::playerUpdateHouseWindow(uint32_t playerId, uint8_t listId, uint32_t windowTextId, const std::string& text)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint32_t internalWindowTextId;
	uint32_t internalListId;
	House* house = player->getEditHouse(internalWindowTextId, internalListId);
	if(house && internalWindowTextId == windowTextId && listId == 0)
	{
		house->setAccessList(internalListId, text);
		player->setEditHouse(NULL);
	}
	return true;
}

bool Game::playerRequestTrade(uint32_t playerId, const Position& pos, uint8_t stackPos,
	uint32_t tradePlayerId, uint16_t spriteId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* tradePartner = getPlayerByID(tradePlayerId);
	if(!tradePartner || tradePartner == player)
	{
		player->sendTextMessage(MSG_INFO_DESCR, "Sorry, not possible.");
		return false;
	}

	if(!Position::areInRange<2,2,0>(tradePartner->getPosition(), player->getPosition()))
	{
		std::stringstream ss;
		ss << tradePartner->getName() << " tells you to move closer.";
		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
		return false;
	}

	Item* tradeItem = dynamic_cast<Item*>(internalGetThing(player, pos, stackPos, spriteId, STACKPOS_USE));
	if(!tradeItem || tradeItem->getClientID() != spriteId || !tradeItem->isPickupable() || tradeItem->getUniqueId() != 0)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}
	else if(player->getPosition().z > tradeItem->getPosition().z)
	{
		player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
		return false;
	}
	else if(player->getPosition().z < tradeItem->getPosition().z)
	{
		player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
		return false;
	}
	else if(!Position::areInRange<1,1,0>(tradeItem->getPosition(), player->getPosition()))
	{
		std::list<Direction> listDir;
		if(getPathToEx(player, pos, listDir, 0, 1, true, true))
		{
			Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			SchedulerTask* task = createSchedulerTask(400, boost::bind(&Game::playerRequestTrade, this,
				playerId, pos, stackPos, tradePlayerId, spriteId));

			player->setNextWalkActionTask(task);
			return true;
		}
		else
		{
			player->sendCancelMessage(RET_THEREISNOWAY);
			return false;
		}
	}

	std::map<Item*, uint32_t>::const_iterator it;
	const Container* container = NULL;
	for(it = tradeItems.begin(); it != tradeItems.end(); it++)
	{
		if(tradeItem == it->first ||
			((container = dynamic_cast<const Container*>(tradeItem)) && container->isHoldingItem(it->first)) ||
			((container = dynamic_cast<const Container*>(it->first)) && container->isHoldingItem(tradeItem)))
		{
			player->sendTextMessage(MSG_INFO_DESCR, "This item is already being traded.");
			return false;
		}
	}

	Container* tradeContainer = tradeItem->getContainer();
	if(tradeContainer && tradeContainer->getItemHoldingCount() + 1 > 100)
	{
		player->sendTextMessage(MSG_INFO_DESCR, "You can not trade more than 100 items.");
		return false;
	}
	return internalStartTrade(player, tradePartner, tradeItem);
}

bool Game::internalStartTrade(Player* player, Player* tradePartner, Item* tradeItem)
{
	if(player->tradeState != TRADE_NONE && !(player->tradeState == TRADE_ACKNOWLEDGE && player->tradePartner == tradePartner))
	{
		player->sendCancelMessage(RET_YOUAREALREADYTRADING);
		return false;
	}
	else if(tradePartner->tradeState != TRADE_NONE && tradePartner->tradePartner != player)
	{
		player->sendCancelMessage(RET_THISPLAYERISALREADYTRADING);
		return false;
	}

	player->tradePartner = tradePartner;
	player->tradeItem = tradeItem;
	player->tradeState = TRADE_INITIATED;
	tradeItem->useThing2();
	tradeItems[tradeItem] = player->getID();

	player->sendTradeItemRequest(player, tradeItem, true);

	if(tradePartner->tradeState == TRADE_NONE)
	{
		char buffer[100];
		sprintf(buffer, "%s wants to trade with you", player->getName().c_str());
		tradePartner->sendTextMessage(MSG_INFO_DESCR, buffer);
		tradePartner->tradeState = TRADE_ACKNOWLEDGE;
		tradePartner->tradePartner = player;
	}
	else
	{
		Item* counterOfferItem = tradePartner->tradeItem;
		player->sendTradeItemRequest(tradePartner, counterOfferItem, false);
		tradePartner->sendTradeItemRequest(player, tradeItem, false);
	}
	return true;
}

bool Game::playerAcceptTrade(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!(player->getTradeState() == TRADE_ACKNOWLEDGE || player->getTradeState() == TRADE_INITIATED))
		return false;

	player->setTradeState(TRADE_ACCEPT);
	Player* tradePartner = player->tradePartner;
	if(tradePartner && tradePartner->getTradeState() == TRADE_ACCEPT)
	{
		Item* tradeItem1 = player->tradeItem;
		Item* tradeItem2 = tradePartner->tradeItem;

		player->setTradeState(TRADE_TRANSFER);
		tradePartner->setTradeState(TRADE_TRANSFER);

		std::map<Item*, uint32_t>::iterator it;

		it = tradeItems.find(tradeItem1);
		if(it != tradeItems.end())
		{
			FreeThing(it->first);
			tradeItems.erase(it);
		}

		it = tradeItems.find(tradeItem2);
		if(it != tradeItems.end())
		{
			FreeThing(it->first);
			tradeItems.erase(it);
		}

		bool isSuccess = false;

		ReturnValue ret1 = internalAddItem(tradePartner, tradeItem1, INDEX_WHEREEVER, 0, true);
		ReturnValue ret2 = internalAddItem(player, tradeItem2, INDEX_WHEREEVER, 0, true);

		if(ret1 == RET_NOERROR && ret2 == RET_NOERROR)
		{
			ret1 = internalRemoveItem(tradeItem1, tradeItem1->getItemCount(), true);
			ret2 = internalRemoveItem(tradeItem2, tradeItem2->getItemCount(), true);
			if(ret1 == RET_NOERROR && ret2 == RET_NOERROR)
			{
				Cylinder* cylinder1 = tradeItem1->getParent();
				Cylinder* cylinder2 = tradeItem2->getParent();

				internalMoveItem(cylinder1, tradePartner, INDEX_WHEREEVER, tradeItem1, tradeItem1->getItemCount(), NULL);
				internalMoveItem(cylinder2, player, INDEX_WHEREEVER, tradeItem2, tradeItem2->getItemCount(), NULL);

				tradeItem1->onTradeEvent(ON_TRADE_TRANSFER, tradePartner);
				tradeItem2->onTradeEvent(ON_TRADE_TRANSFER, player);

				isSuccess = true;
			}
		}

		if(!isSuccess)
		{
			std::string errorDescription = getTradeErrorDescription(ret1, tradeItem1);
			tradePartner->sendTextMessage(MSG_INFO_DESCR, errorDescription);
			tradePartner->tradeItem->onTradeEvent(ON_TRADE_CANCEL, tradePartner);

			errorDescription = getTradeErrorDescription(ret2, tradeItem2);
			player->sendTextMessage(MSG_INFO_DESCR, errorDescription);
			player->tradeItem->onTradeEvent(ON_TRADE_CANCEL, player);
		}

		player->setTradeState(TRADE_NONE);
		player->tradeItem = NULL;
		player->tradePartner = NULL;
		player->sendTradeClose();

		tradePartner->setTradeState(TRADE_NONE);
		tradePartner->tradeItem = NULL;
		tradePartner->tradePartner = NULL;
		tradePartner->sendTradeClose();
		return isSuccess;
	}
	return false;
}

std::string Game::getTradeErrorDescription(ReturnValue ret, Item* item)
{
	std::stringstream ss;
	if(ret == RET_NOTENOUGHCAPACITY)
	{
		ss << "You do not have enough capacity to carry";
		if(item->isStackable() && item->getItemCount() > 1)
			ss << " these objects.";
		else
			ss << " this object." ;
		ss << std::endl << " " << item->getWeightDescription();
	}
	else if(ret == RET_NOTENOUGHROOM || ret == RET_CONTAINERNOTENOUGHROOM)
	{
		ss << "You do not have enough room to carry";
		if(item->isStackable() && item->getItemCount() > 1)
			ss << " these objects.";
		else
			ss << " this object.";
	}
	else
		ss << "Trade could not be completed.";
	return ss.str().c_str();
}

bool Game::playerLookInTrade(uint32_t playerId, bool lookAtCounterOffer, int32_t index)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* tradePartner = player->tradePartner;
	if(!tradePartner)
		return false;

	Item* tradeItem = NULL;

	if(lookAtCounterOffer)
		tradeItem = tradePartner->getTradeItem();
	else
		tradeItem = player->getTradeItem();

	if(!tradeItem)
		return false;

	int32_t lookDistance = std::max(std::abs(player->getPosition().x - tradeItem->getPosition().x),
		std::abs(player->getPosition().y - tradeItem->getPosition().y));

	std::stringstream ss;
	ss << "You see ";
	if(index == 0)
	{
		ss << tradeItem->getDescription(lookDistance);
		if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
		{
			ss << std::endl << "ItemID: [" << tradeItem->getID() << "].";
			if(tradeItem->getActionId() > 0)
				ss << std::endl << "ActionID: [" << tradeItem->getActionId() << "].";
			if(tradeItem->getUniqueId() > 0)
				ss << std::endl << "UniqueID: [" << tradeItem->getUniqueId() << "].";
		}

		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
		return false;
	}

	Container* tradeContainer = tradeItem->getContainer();
	if(!tradeContainer || index > (int32_t)tradeContainer->getItemHoldingCount())
		return false;

	bool foundItem = false;
	std::list<const Container*> listContainer;
	ItemList::const_iterator it;
	Container* tmpContainer = NULL;

	listContainer.push_back(tradeContainer);
	while(!foundItem && listContainer.size() > 0)
	{
		const Container* container = listContainer.front();
		listContainer.pop_front();
		for(it = container->getItems(); it != container->getEnd(); ++it)
		{
			if((tmpContainer = (*it)->getContainer()))
				listContainer.push_back(tmpContainer);

			--index;
			if(index == 0)
			{
				tradeItem = *it;
				foundItem = true;
				break;
			}
		}
	}

	if(foundItem)
	{
		ss << tradeItem->getDescription(lookDistance);
		if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
		{
			ss << std::endl << "ItemID: [" << tradeItem->getID() << "].";
			if(tradeItem->getActionId() > 0)
				ss << std::endl << "ActionID: [" << tradeItem->getActionId() << "].";
			if(tradeItem->getUniqueId() > 0)
				ss << std::endl << "UniqueID: [" << tradeItem->getUniqueId() << "].";
		}

		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
	}
	return foundItem;
}

bool Game::playerCloseTrade(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	return internalCloseTrade(player);
}

bool Game::internalCloseTrade(Player* player)
{
	Player* tradePartner = player->tradePartner;
	if((tradePartner && tradePartner->getTradeState() == TRADE_TRANSFER) || player->getTradeState() == TRADE_TRANSFER)
	{
		std::cout << "Warning: [Game::playerCloseTrade] TradeState == TRADE_TRANSFER. " <<
			player->getName() << " " << player->getTradeState() << " , " <<
			tradePartner->getName() << " " << tradePartner->getTradeState() << std::endl;
		return true;
	}

	std::vector<Item*>::iterator it;
	if(player->getTradeItem())
	{
		std::map<Item*, uint32_t>::iterator it = tradeItems.find(player->getTradeItem());
		if(it != tradeItems.end())
		{
			FreeThing(it->first);
			tradeItems.erase(it);
		}
		player->tradeItem->onTradeEvent(ON_TRADE_CANCEL, player);
		player->tradeItem = NULL;
	}

	player->setTradeState(TRADE_NONE);
	player->tradePartner = NULL;

	player->sendTextMessage(MSG_STATUS_SMALL, "Trade cancelled.");
	player->sendTradeClose();

	if(tradePartner)
	{
		if(tradePartner->getTradeItem())
		{
			std::map<Item*, uint32_t>::iterator it = tradeItems.find(tradePartner->getTradeItem());
			if(it != tradeItems.end())
			{
				FreeThing(it->first);
				tradeItems.erase(it);
			}

			tradePartner->tradeItem->onTradeEvent(ON_TRADE_CANCEL, tradePartner);
			tradePartner->tradeItem = NULL;
		}

		tradePartner->setTradeState(TRADE_NONE);
		tradePartner->tradePartner = NULL;

		tradePartner->sendTextMessage(MSG_STATUS_SMALL, "Trade cancelled.");
		tradePartner->sendTradeClose();
	}
	return true;
}

bool Game::playerPurchaseItem(uint32_t playerId, uint16_t spriteId, uint8_t count, uint8_t amount)
{
	Player* player = getPlayerByID(playerId);
	if(player == NULL || player->isRemoved())
		return false;

	int32_t onBuy;
	int32_t onSell;

	Npc* merchant = player->getShopOwner(onBuy, onSell);
	if(merchant == NULL)
		return false;

	const ItemType& it = Item::items.getItemIdByClientId(spriteId);
	if(it.id == 0)
		return false;

	uint8_t subType = 0;
	if(it.isFluidContainer())
	{
		int32_t maxFluidType = sizeof(reverseFluidMap) / sizeof(uint32_t);
		if(count < maxFluidType)
			subType = (uint8_t)reverseFluidMap[count];
	}
	else
		subType = count;

	merchant->onPlayerTrade(player, SHOPEVENT_BUY, onBuy, it.id, subType, amount);
	return true;
}

bool Game::playerSellItem(uint32_t playerId, uint16_t spriteId, uint8_t count, uint8_t amount)
{
	Player* player = getPlayerByID(playerId);
	if(player == NULL || player->isRemoved())
		return false;

	int32_t onBuy;
	int32_t onSell;

	Npc* merchant = player->getShopOwner(onBuy, onSell);
	if(merchant == NULL)
		return false;

	const ItemType& it = Item::items.getItemIdByClientId(spriteId);
	if(it.id == 0)
		return false;

	uint8_t subType = 0;
	if(it.isFluidContainer())
	{
		int32_t maxFluidType = sizeof(reverseFluidMap) / sizeof(uint32_t);
		if(count < maxFluidType)
			subType = (uint8_t)reverseFluidMap[count];
	}
	else
		subType = count;

	merchant->onPlayerTrade(player, SHOPEVENT_SELL, onSell, it.id, subType, amount);
	return true;
}

bool Game::playerCloseShop(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(player == NULL || player->isRemoved())
		return false;

	player->closeShopWindow();
	return true;
}

bool Game::playerLookInShop(uint32_t playerId, uint16_t spriteId, uint8_t count)
{
	Player* player = getPlayerByID(playerId);
	if(player == NULL || player->isRemoved())
		return false;

	const ItemType& it = Item::items.getItemIdByClientId(spriteId);
	if(it.id == 0)
		return false;

	int32_t subType = 0;
	if(it.isFluidContainer())
	{
		int32_t maxFluidType = sizeof(reverseFluidMap) / sizeof(uint32_t);
		if(count < maxFluidType)
			subType = reverseFluidMap[count];
	}
	else
		subType = count;

	std::stringstream ss;
	ss << "You see " << Item::getDescription(it, 1, NULL, subType);
	if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
		ss << std::endl << "ItemID: [" << it.id << "].";

	player->sendTextMessage(MSG_INFO_DESCR, ss.str());
	return true;
}

bool Game::playerLookAt(uint32_t playerId, const Position& pos, uint16_t spriteId, uint8_t stackPos)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Thing* thing = internalGetThing(player, pos, stackPos, spriteId, STACKPOS_LOOK);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Position thingPos = thing->getPosition();
	if(!player->canSee(thingPos))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Position playerPos = player->getPosition();

	int32_t lookDistance = 0;
	if(thing == player)
		lookDistance = -1;
	else
	{
		lookDistance = std::max(std::abs(playerPos.x - thingPos.x), std::abs(playerPos.y - thingPos.y));
		if(playerPos.z != thingPos.z)
			lookDistance = lookDistance + 9 + 6;
	}

	std::stringstream ss;
	ss << "You see " << thing->getDescription(lookDistance);
	if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
	{
		Item* item = thing->getItem();
		if(item)
		{
			ss << std::endl << "ItemID: [" << item->getID() << "].";
			if(item->getActionId() > 0)
				ss << std::endl << "ActionID: [" << item->getActionId() << "].";
			if(item->getUniqueId() > 0)
				ss << std::endl << "UniqueID: [" << item->getUniqueId() << "].";
		}
	}

	if(player->hasCustomFlag(PlayerCustomFlag_CanSeeCreatureDetails))
	{
		if(Creature* creature = thing->getCreature())
		{
			ss << std::endl << "Health: [" << creature->getHealth() << " / " << creature->getMaxHealth() << "].";
			ss << std::endl << "Mana: [" << creature->getMana() << " / " << creature->getMaxMana() << "].";
		}
	}

	if(player->hasCustomFlag(PlayerCustomFlag_CanSeePosition))
		ss << std::endl << "Position: [X: " << thingPos.x << "] [Y: " << thingPos.y << "] [Z: " << thingPos.z << "].";

	player->sendTextMessage(MSG_INFO_DESCR, ss.str());

	//scripting event - onLook
	CreatureEvent* eventLook = player->getCreatureEvent(CREATURE_EVENT_LOOK);
	if(eventLook)
		eventLook->executeOnLook(player, thingPos, stackPos);

	return true;
}

bool Game::playerCancelAttackAndFollow(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	playerSetAttackedCreature(playerId, 0);
	playerFollowCreature(playerId, 0);
	player->stopWalk();
	return true;
}

bool Game::playerSetAttackedCreature(uint32_t playerId, uint32_t creatureId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(player->getAttackedCreature() && creatureId == 0)
	{
		player->setAttackedCreature(NULL);
		player->sendCancelTarget();
		return true;
	}

	Creature* attackCreature = getCreatureByID(creatureId);
	if(!attackCreature)
	{
		player->setAttackedCreature(NULL);
		player->sendCancelTarget();
		return false;
	}

	ReturnValue ret = Combat::canTargetCreature(player, attackCreature);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		player->sendCancelTarget();
		player->setAttackedCreature(NULL);
		return false;
	}

	player->setAttackedCreature(attackCreature);
	return true;
}

bool Game::playerFollowCreature(uint32_t playerId, uint32_t creatureId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->setAttackedCreature(NULL);
	Creature* followCreature = NULL;
	if(creatureId != 0)
		followCreature = getCreatureByID(creatureId);

	return player->setFollowCreature(followCreature);
}

bool Game::playerSetFightModes(uint32_t playerId, fightMode_t fightMode, chaseMode_t chaseMode, secureMode_t secureMode)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->setFightMode(fightMode);
	player->setChaseMode(chaseMode);
	player->setSecureMode(secureMode);
	return true;
}

bool Game::playerRequestAddVip(uint32_t playerId, const std::string& vip_name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	std::string real_name;
	real_name = vip_name;
	uint32_t guid;
	bool specialVip;

	if(!IOLoginData::getInstance()->getGuidByNameEx(guid, specialVip, real_name))
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "A player with that name does not exist.");
		return false;
	}

	if(specialVip && !player->hasFlag(PlayerFlag_SpecialVIP))
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "You can not add this player.");
		return false;
	}

	bool online = (getPlayerByName(real_name) != NULL);
	return player->addVIP(guid, real_name, online);
}

bool Game::playerRequestRemoveVip(uint32_t playerId, uint32_t guid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->removeVIP(guid);
	return true;
}

bool Game::playerTurn(uint32_t playerId, Direction dir)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->resetIdleTime();
	return internalCreatureTurn(player, dir);
}

bool Game::playerRequestOutfit(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->sendOutfitWindow();
	return true;
}

bool Game::playerChangeOutfit(uint32_t playerId, Outfit_t outfit)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(player->canWear(outfit.lookType, outfit.lookAddons) && player->hasRequestedOutfit())
	{
		player->hasRequestedOutfit(false);
		player->defaultOutfit = outfit;
		if(player->hasCondition(CONDITION_OUTFIT))
			return false;

		internalCreatureChangeOutfit(player, outfit);
	}
	return true;
}

bool Game::playerSay(uint32_t playerId, uint16_t channelId, SpeakClasses type,
	const std::string& receiver, const std::string& text)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint32_t muteTime = player->isMuted();
	if(muteTime > 0)
	{
		char buffer[75];
		sprintf(buffer, "You are still muted for %d seconds.", muteTime);
		player->sendTextMessage(MSG_STATUS_SMALL, buffer);
		return false;
	}

	if(playerSayCommand(player, type, text))
		return true;

	if(playerSayTalkAction(player, type, text))
		return true;

	if(playerSaySpell(player, type, text))
		return true;

	player->removeMessageBuffer();

	switch(type)
	{
		case SPEAK_SAY:
			return internalCreatureSay(player, SPEAK_SAY, text);
			break;
		case SPEAK_WHISPER:
			return playerWhisper(player, text);
			break;
		case SPEAK_YELL:
			return playerYell(player, text);
			break;
		case SPEAK_PRIVATE:
		case SPEAK_PRIVATE_RED:
		case SPEAK_RVR_ANSWER:
			return playerSpeakTo(player, type, receiver, text);
			break;
		case SPEAK_CHANNEL_O:
		case SPEAK_CHANNEL_Y:
		case SPEAK_CHANNEL_R1:
		case SPEAK_CHANNEL_R2:
			return playerTalkToChannel(player, type, text, channelId);
			break;
		case SPEAK_PRIVATE_PN:
			return playerSpeakToNpc(player, text);
			break;
		case SPEAK_BROADCAST:
			return playerBroadcastMessage(player, text, SPEAK_BROADCAST);
			break;
		case SPEAK_RVR_CHANNEL:
			return playerReportRuleViolation(player, text);
			break;
		case SPEAK_RVR_CONTINUE:
			return playerContinueReport(player, text);
			break;

		default:
			break;
	}
	return false;
}

bool Game::playerSayCommand(Player* player, SpeakClasses type, const std::string& text)
{
	if(player->getName() == "Account Manager")
		return internalCreatureSay(player, SPEAK_SAY, text);

	//First, check if this was a command
	for(uint32_t i = 0; i < commandTags.size(); i++)
	{
		if(commandTags[i] == text.substr(0,1))
		{
			if(commands.exeCommand(player, text))
				return true;
		}
	}
	return false;
}

bool Game::playerSayTalkAction(Player* player, SpeakClasses type, const std::string& text)
{
	if(player->getName() == "Account Manager")
		return internalCreatureSay(player, SPEAK_SAY, text);

	TalkActionResult_t result;
	result = g_talkActions->onPlayerSpeak(player, type, text);
	if(result == TALKACTION_BREAK)
		return true;

	return false;
}

bool Game::playerSaySpell(Player* player, SpeakClasses type, const std::string& text)
{
	if(player->getName() == "Account Manager")
		return internalCreatureSay(player, SPEAK_SAY, text);

	TalkActionResult_t result;
	result = g_spells->playerSaySpell(player, type, text);
	if(result == TALKACTION_BREAK)
	{
		//std::string _text = (g_config.getBool(ConfigManager::SPELL_NAME_INSTEAD_WORDS) ? g_spells->getInstantSpell(text)->getName() : text);
		return internalCreatureSay(player, SPEAK_SAY, text);
	}
	else if(result == TALKACTION_FAILED)
		return true;

	return false;
}

bool Game::playerWhisper(Player* player, const std::string& text)
{
	SpectatorVec list;
	getSpectators(list, player->getPosition());
	SpectatorVec::const_iterator it;

	//send to client
	Player* tmpPlayer = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
		{
			if(!Position::areInRange<1,1,0>(player->getPosition(), (*it)->getPosition()))
				tmpPlayer->sendCreatureSay(player, SPEAK_WHISPER, "Pssst");
			else
				tmpPlayer->sendCreatureSay(player, SPEAK_WHISPER, text);
		}
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureSay(player, SPEAK_WHISPER, text);

	return true;
}

bool Game::playerYell(Player* player, const std::string& text)
{
	if(player->getLevel() > 1)
	{
		if(!player->hasCondition(CONDITION_YELLTICKS))
		{
			if(!player->hasFlag(PlayerFlag_CannotBeMuted))
			{
				Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_YELLTICKS, 30000, 0);
				player->addCondition(condition);
			}
			internalCreatureSay(player, SPEAK_YELL, asUpperCaseString(text));
		}
		else
			player->sendCancelMessage(RET_YOUAREEXHAUSTED);
	}
	else
		player->sendTextMessage(MSG_STATUS_SMALL, "You may not yell as long as you are on level 1.");

	return true;
}

bool Game::playerSpeakTo(Player* player, SpeakClasses type, const std::string& receiver,
	const std::string& text)
{
	Player* toPlayer = getPlayerByName(receiver);
	if(!toPlayer || toPlayer->isRemoved())
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "A player with this name is not online.");
		return false;
	}

	if(!toPlayer->isInGhostMode() && toPlayer->isIgnoringPrivMsg() &&
		!player->canSeeGhost(toPlayer) && !player->hasFlag(PlayerFlag_CannotBeMuted))
	{
		char buffer[70];
		sprintf(buffer, "Sorry, %s is ignoring private messages.", toPlayer->getName().c_str());
		player->sendTextMessage(MSG_STATUS_SMALL, buffer);
		return false;
	}

	if(type == SPEAK_PRIVATE_RED && !player->hasFlag(PlayerFlag_CanTalkRedPrivate))
		type = SPEAK_PRIVATE;

	toPlayer->sendCreatureSay(player, type, text);
	toPlayer->onCreatureSay(player, type, text);

	if(toPlayer->isInGhostMode() && !player->canSeeGhost(toPlayer))
		player->sendTextMessage(MSG_STATUS_SMALL, "A player with this name is not online.");
	else
	{
		char buffer[80];
		sprintf(buffer, "Message sent to %s.", toPlayer->getName().c_str());
		player->sendTextMessage(MSG_STATUS_SMALL, buffer);
	}
	return true;
}

bool Game::playerTalkToChannel(Player* player, SpeakClasses type, const std::string& text, uint16_t channelId)
{
	switch(type)
	{
		case SPEAK_CHANNEL_Y:
		{
			if(channelId == 0x08 && player->hasFlag(PlayerFlag_TalkOrangeHelpChannel))
				type = SPEAK_CHANNEL_O;
			break;
		}

		case SPEAK_CHANNEL_O:
		{
			if(channelId != 0x08 || !player->hasFlag(PlayerFlag_TalkOrangeHelpChannel))
				type = SPEAK_CHANNEL_Y;
			break;
		}

		case SPEAK_CHANNEL_R1:
		{
			if(!player->hasFlag(PlayerFlag_CanTalkRedChannel))
				type = SPEAK_CHANNEL_Y;
			break;
		}

		case SPEAK_CHANNEL_R2:
		{
			if(!player->hasFlag(PlayerFlag_CanTalkRedChannel))
				type = SPEAK_CHANNEL_Y;
			break;
		}

		default:
			break;
	}

	g_chat.talkToChannel(player, type, text, channelId);
	return true;
}

bool Game::playerSpeakToNpc(Player* player, const std::string& text)
{
	SpectatorVec list;
	SpectatorVec::iterator it;
	getSpectators(list, player->getPosition());

	//send to npcs only
	Npc* tmpNpc = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpNpc = (*it)->getNpc()))
			(*it)->onCreatureSay(player, SPEAK_PRIVATE_PN, text);
	}
	return true;
}

void Game::npcSpeakToPlayer(Npc* npc, Player* player, const std::string& text, bool publicize)
{
	if(player != NULL)
	{
		player->sendCreatureSay(npc, SPEAK_PRIVATE_NP, text);
		player->onCreatureSay(npc, SPEAK_PRIVATE_NP, text);
	}

	if(publicize)
	{
		SpectatorVec list;
		SpectatorVec::iterator it;
		getSpectators(list, npc->getPosition());

		//send to client
		Player* tmpPlayer = NULL;
		for(it = list.begin(); it != list.end(); ++it)
		{
			tmpPlayer = (*it)->getPlayer();
			if((tmpPlayer != NULL) && (tmpPlayer != player))
				tmpPlayer->sendCreatureSay(npc, SPEAK_SAY, text);
		}

		//event method
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((*it) != player)
				(*it)->onCreatureSay(npc, SPEAK_SAY, text);
		}
	}
}

bool Game::playerReportRuleViolation(Player* player, const std::string& text)
{
	//Do not allow reports on multiclones worlds
	//Since reports are name-based
	if(g_config.getNumber(ConfigManager::ALLOW_CLONES))
	{
		player->sendTextMessage(MSG_INFO_DESCR, "Rule violation reports are disabled.");
		return false;
	}

	cancelRuleViolation(player);

	boost::shared_ptr<RuleViolation> rvr(new RuleViolation(
		player,
		text,
		std::time(NULL)
	));

	ruleViolations[player->getID()] = rvr;

	ChatChannel* channel = g_chat.getChannelById(0x03); //Rule Violations channel
	if(channel)
	{
		channel->talk(player, SPEAK_RVR_CHANNEL, text, rvr->time);
		return true;
	}
	return false;
}

bool Game::playerContinueReport(Player* player, const std::string& text)
{
	RuleViolationsMap::iterator it = ruleViolations.find(player->getID());
	if(it == ruleViolations.end())
		return false;

	RuleViolation& rvr = *it->second;
	Player* toPlayer = rvr.gamemaster;
	if(!toPlayer)
		return false;

	toPlayer->sendCreatureSay(player, SPEAK_RVR_CONTINUE, text);

	player->sendTextMessage(MSG_STATUS_SMALL, "Message sent to Gamemaster.");
	return true;
}

//--
bool Game::canThrowObjectTo(const Position& fromPos, const Position& toPos, bool checkLineOfSight /*= true*/,
	int32_t rangex /*= Map::maxClientViewportX*/, int32_t rangey /*= Map::maxClientViewportY*/)
{
	return map->canThrowObjectTo(fromPos, toPos, checkLineOfSight, rangex, rangey);
}

bool Game::isSightClear(const Position& fromPos, const Position& toPos, bool floorCheck)
{
	return map->isSightClear(fromPos, toPos, floorCheck);
}

bool Game::internalCreatureTurn(Creature* creature, Direction dir)
{
	if(creature->getDirection() != dir)
	{
		creature->setDirection(dir);

		int32_t stackpos = creature->getParent()->__getIndexOfThing(creature);

		const SpectatorVec& list = getSpectators(creature->getPosition());
		SpectatorVec::const_iterator it;

		//send to client
		Player* tmpPlayer = NULL;
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()))
			{
				int32_t i = 0;
				if(!creature->isInGhostMode() || tmpPlayer->canSeeGhost(creature))
				{
					Tile* t = creature->getTile();
					for(CreatureVector::iterator it = t->creatures.begin(); it != t->creatures.end(); ++it)
					{
						int32_t itIndex = t->__getIndexOfThing((*it));
						if(itIndex < stackpos)
						{
							if((*it)->isInGhostMode() && !tmpPlayer->canSeeGhost((*it)))
								i++;
						}
					}
				}
				tmpPlayer->sendCreatureTurn(creature, stackpos - i);
			}
		}

		//event method
		for(it = list.begin(); it != list.end(); ++it)
			(*it)->onCreatureTurn(creature, stackpos);

		return true;
	}
	return false;
}

bool Game::internalCreatureSay(Creature* creature, SpeakClasses type, const std::string& text)
{
	Player* player = creature->getPlayer();
	if(player && player->getName() == "Account Manager")
		player->manageAccount(text);
	else
	{
		// This somewhat complex construct ensures that the cached SpectatorVec
		// is used if available and if it can be used, else a local vector is
		// used. (Hopefully the compiler will optimize away the construction of
		// the temporary when it's not used.
		SpectatorVec list;
		SpectatorVec::const_iterator it;

		if(type == SPEAK_YELL || type == SPEAK_MONSTER_YELL)
			getSpectators(list, creature->getPosition(), false, true, 18, 18, 14, 14);
		else
			getSpectators(list, creature->getPosition(), false, false);

		//send to client
		Player* tmpPlayer = NULL;
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()))
				tmpPlayer->sendCreatureSay(creature, type, text);
		}

		//event method
		for(it = list.begin(); it != list.end(); ++it)
			(*it)->onCreatureSay(creature, type, text);
	}
	return true;
}

bool Game::getPathTo(const Creature* creature, const Position& destPos,
	std::list<Direction>& listDir, int32_t maxSearchDist /*= -1*/)
{
	return map->getPathTo(creature, destPos, listDir, maxSearchDist);
}

bool Game::getPathToEx(const Creature* creature, const Position& targetPos,
	std::list<Direction>& dirList, const FindPathParams& fpp)
{
	return map->getPathMatching(creature, dirList, FrozenPathingConditionCall(targetPos), fpp);
}

bool Game::getPathToEx(const Creature* creature, const Position& targetPos, std::list<Direction>& dirList,
	uint32_t minTargetDist, uint32_t maxTargetDist, bool fullPathSearch /*= true*/,
	bool clearSight /*= true*/, int32_t maxSearchDist /*= -1*/)
{
	FindPathParams fpp;
	fpp.fullPathSearch = fullPathSearch;
	fpp.maxSearchDist = maxSearchDist;
	fpp.clearSight = clearSight;
	fpp.minTargetDist = minTargetDist;
	fpp.maxTargetDist = maxTargetDist;
	return getPathToEx(creature, targetPos, dirList, fpp);
}

void Game::checkCreatureWalk(uint32_t creatureId)
{
	Creature* creature = getCreatureByID(creatureId);
	if(creature && creature->getHealth() > 0)
	{
		creature->onWalk();
		cleanup();
	}
}

void Game::updateCreatureWalk(uint32_t creatureId)
{
	Creature* creature = getCreatureByID(creatureId);
	if(creature && creature->getHealth() > 0)
		creature->getPathToFollowCreature();
}

void Game::checkCreatureAttack(uint32_t creatureId)
{
	Creature* creature = getCreatureByID(creatureId);
	if(creature && creature->getHealth() > 0)
		creature->onAttacking(0);
}

void Game::addCreatureCheck(Creature* creature)
{
	if(creature->checkCreatureVectorIndex != 0)
		return; //Already in a vector

	size_t min = std::numeric_limits<size_t>::max();
	size_t insertindex = 0;
	for(size_t i = 0; i < EVENT_CREATURECOUNT; ++i)
	{
		if(checkCreatureVectors[i].size() < min)
		{
			insertindex = i;
			min = checkCreatureVectors[i].size();
		}
	}
	checkCreatureVectors[insertindex].push_back(creature);
	creature->checkCreatureVectorIndex = insertindex + 1;
}

void Game::removeCreatureCheck(Creature* creature)
{
	if(creature->checkCreatureVectorIndex == 0)
		return;

	std::vector<Creature*>& checkCreatureVector = checkCreatureVectors[creature->checkCreatureVectorIndex - 1];

	std::vector<Creature*>::iterator cit = std::find(checkCreatureVector.begin(),
	checkCreatureVector.end(), creature);
	if(cit != checkCreatureVector.end())
	{
		std::swap(*cit, checkCreatureVector.back());
		checkCreatureVector.pop_back();
	}
	creature->checkCreatureVectorIndex = 0;
}

void Game::checkCreatures()
{
	Scheduler::getScheduler().addEvent(createSchedulerTask(
		EVENT_CHECK_CREATURE_INTERVAL, boost::bind(&Game::checkCreatures, this)));

	if(checkCreatureLastIndex == EVENT_CREATURECOUNT)
		checkCreatureLastIndex = 0;

	std::vector<Creature*>& checkCreatureVector = checkCreatureVectors[checkCreatureLastIndex];
	checkCreatureLastIndex++;

	Creature* creature;
	for(uint32_t i = 0; i < checkCreatureVector.size(); ++i)
	{
		creature = checkCreatureVector[i];
		if(creature->getHealth() > 0)
		{
			creature->onThink(EVENT_CREATURE_THINK_INTERVAL);
			//creature->onAttacking(EVENT_CREATURE_THINK_INTERVAL);
			//TODO: find out why does it cause crash in some cases
			creature->executeConditions(EVENT_CREATURE_THINK_INTERVAL);
		}
		else
			creature->onDeath();
	}
	cleanup();
}

void Game::changeSpeed(Creature* creature, int32_t varSpeedDelta)
{
	int32_t varSpeed = creature->getSpeed() - creature->getBaseSpeed();
	varSpeed += varSpeedDelta;

	creature->setSpeed(varSpeed);

	const SpectatorVec& list = getSpectators(creature->getPosition());
	SpectatorVec::const_iterator it;

	//send to client
	Player* tmpPlayer = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendChangeSpeed(creature, creature->getStepSpeed());
	}
}

void Game::internalCreatureChangeOutfit(Creature* creature, const Outfit_t& outfit)
{
	creature->setCurrentOutfit(outfit);
	if(!creature->isInvisible())
	{
		const SpectatorVec& list = getSpectators(creature->getPosition());
		SpectatorVec::const_iterator it;

		//send to client
		Player* tmpPlayer = NULL;
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()))
				tmpPlayer->sendCreatureChangeOutfit(creature, outfit);
		}

		//event method
		for(it = list.begin(); it != list.end(); ++it)
			(*it)->onCreatureChangeOutfit(creature, outfit);
	}
}

void Game::internalCreatureChangeVisible(Creature* creature, bool visible)
{
	const SpectatorVec& list = getSpectators(creature->getPosition());
	SpectatorVec::const_iterator it;

	//send to client
	Player* tmpPlayer = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureChangeVisible(creature, visible);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureChangeVisible(creature, visible);
}


void Game::changeLight(const Creature* creature)
{
	const SpectatorVec& list = getSpectators(creature->getPosition());

	//send to client
	Player* tmpPlayer = NULL;
	for(SpectatorVec::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureLight(creature);
	}
}

bool Game::combatBlockHit(CombatType_t combatType, Creature* attacker, Creature* target,
	int32_t& healthChange, bool checkDefense, bool checkArmor)
{
	if(healthChange > 0)
		return false;

	const Position& targetPos = target->getPosition();
	const SpectatorVec& list = getSpectators(targetPos);

	if(!target->isAttackable() || Combat::canDoCombat(attacker, target) != RET_NOERROR)
	{
		addMagicEffect(list, targetPos, NM_ME_POFF, target->isInGhostMode());
		return true;
	}

	int32_t damage = -healthChange;
	BlockType_t blockType = target->blockHit(attacker, combatType, damage, checkDefense, checkArmor);
	healthChange = -damage;

	if(blockType == BLOCK_DEFENSE)
	{
		addMagicEffect(list, targetPos, NM_ME_POFF);
		return true;
	}
	else if(blockType == BLOCK_ARMOR)
	{
		addMagicEffect(list, targetPos, NM_ME_BLOCKHIT);
		return true;
	}
	else if(blockType == BLOCK_IMMUNITY)
	{
		uint8_t hitEffect = 0;
		switch(combatType)
		{
			case COMBAT_UNDEFINEDDAMAGE:
				break;

			case COMBAT_ENERGYDAMAGE:
			case COMBAT_FIREDAMAGE:
			case COMBAT_PHYSICALDAMAGE:
			case COMBAT_ICEDAMAGE:
			case COMBAT_DEATHDAMAGE:
			{
				hitEffect = NM_ME_BLOCKHIT;
				break;
			}

			case COMBAT_EARTHDAMAGE:
			{
				hitEffect = NM_ME_POISON_RINGS;
				break;
			}

			case COMBAT_HOLYDAMAGE:
			{
				hitEffect = NM_ME_HOLYDAMAGE;
				break;
			}

			default:
			{
				hitEffect = NM_ME_POFF;
				break;
			}
		}
		addMagicEffect(list, targetPos, hitEffect);
		return true;
	}
	return false;
}

bool Game::combatChangeHealth(CombatType_t combatType, Creature* attacker, Creature* target, int32_t healthChange)
{
	const Position& targetPos = target->getPosition();
	if(healthChange > 0)
	{
		if(target->getHealth() <= 0)
			return false;

		if(attacker && target && attacker->defaultOutfit.lookFeet == target->defaultOutfit.lookFeet && g_config.getBool(ConfigManager::CANNOT_ATTACK_SAME_LOOKFEET) && combatType != COMBAT_HEALING)
			return false;

		target->gainHealth(attacker, healthChange);

		if(g_config.getBool(ConfigManager::SHOW_HEALING_DAMAGE))
		{
			const SpectatorVec& list = getSpectators(targetPos);
			char buffer[10];
			sprintf(buffer, "+%d", healthChange);
			if(combatType != COMBAT_HEALING)
				addMagicEffect(list, targetPos, NM_ME_MAGIC_ENERGY);
			addAnimatedText(list, targetPos, TEXTCOLOR_GREEN, buffer);
		}
	}
	else
	{
		const SpectatorVec& list = getSpectators(targetPos);
		if(!target->isAttackable() || Combat::canDoCombat(attacker, target) != RET_NOERROR)
		{
			addMagicEffect(list, targetPos, NM_ME_POFF);
			return true;
		}

		if(attacker && target && attacker->defaultOutfit.lookFeet == target->defaultOutfit.lookFeet && g_config.getBool(ConfigManager::CANNOT_ATTACK_SAME_LOOKFEET) && combatType != COMBAT_HEALING)
			return false;

		int32_t damage = -healthChange;
		if(damage != 0)
		{
			if(target->hasCondition(CONDITION_MANASHIELD) && combatType != COMBAT_UNDEFINEDDAMAGE)
			{
				int32_t manaDamage = std::min(target->getMana(), damage);
				damage = std::max((int32_t)0, damage - manaDamage);
				if(manaDamage != 0)
				{
					target->drainMana(attacker, manaDamage);
					char buffer[20];
					sprintf(buffer, "%d", manaDamage);
					addMagicEffect(list, targetPos, NM_ME_LOSE_ENERGY);
					addAnimatedText(list, targetPos, TEXTCOLOR_BLUE, buffer);
				}
			}

			damage = std::min(target->getHealth(), damage);
			if(damage > 0)
			{
				target->drainHealth(attacker, combatType, damage);
				addCreatureHealth(list, target);

				TextColor_t textColor = TEXTCOLOR_NONE;
				uint8_t hitEffect = 0;
				switch(combatType)
				{
					case COMBAT_PHYSICALDAMAGE:
					{
						Item* splash = NULL;
						switch(target->getRace())
						{
							case RACE_VENOM:
								textColor = TEXTCOLOR_LIGHTGREEN;
								hitEffect = NM_ME_POISON;
								splash = Item::CreateItem(ITEM_SMALLSPLASH, FLUID_GREEN);
								break;

							case RACE_BLOOD:
								textColor = TEXTCOLOR_RED;
								hitEffect = NM_ME_DRAW_BLOOD;
								splash = Item::CreateItem(ITEM_SMALLSPLASH, FLUID_BLOOD);
								break;

							case RACE_UNDEAD:
								textColor = TEXTCOLOR_LIGHTGREY;
								hitEffect = NM_ME_HIT_AREA;
								break;

							case RACE_FIRE:
								textColor = TEXTCOLOR_ORANGE;
								hitEffect = NM_ME_DRAW_BLOOD;
								break;

							default:
								break;
						}

						if(splash)
						{
							internalAddItem(target->getTile(), splash, INDEX_WHEREEVER, FLAG_NOLIMIT);
							startDecay(splash);
						}
						break;
					}

					case COMBAT_ENERGYDAMAGE:
					{
						textColor = TEXTCOLOR_PURPLE;
						hitEffect = NM_ME_ENERGY_DAMAGE;
						break;
					}

					case COMBAT_EARTHDAMAGE:
					{
						textColor = TEXTCOLOR_LIGHTGREEN;
						hitEffect = NM_ME_POISON_RINGS;
						break;
					}

					case COMBAT_DROWNDAMAGE:
					{
						textColor = TEXTCOLOR_LIGHTBLUE;
						hitEffect = NM_ME_LOSE_ENERGY;
						break;
					}

					case COMBAT_FIREDAMAGE:
					{
						textColor = TEXTCOLOR_ORANGE;
						hitEffect = NM_ME_HITBY_FIRE;
						break;
					}

					case COMBAT_ICEDAMAGE:
					{
						textColor = TEXTCOLOR_LIGHTBLUE;
						hitEffect = NM_ME_ICEATTACK;
						break;
					}

					case COMBAT_HOLYDAMAGE:
					{
						textColor = TEXTCOLOR_YELLOW;
						hitEffect = NM_ME_HOLYDAMAGE;
						break;
					}

					case COMBAT_DEATHDAMAGE:
					{
						textColor = TEXTCOLOR_DARKRED;
						hitEffect = NM_ME_SMALLCLOUDS;
						break;
					}

					case COMBAT_LIFEDRAIN:
					{
						textColor = TEXTCOLOR_RED;
						hitEffect = NM_ME_MAGIC_BLOOD;
						break;
					}

					default:
						break;
				}

				if(textColor != TEXTCOLOR_NONE)
				{
					char buffer[20];
					sprintf(buffer, "%d", damage);
					addMagicEffect(list, targetPos, hitEffect);
					addAnimatedText(list, targetPos, textColor, buffer);
				}
			}
		}
	}
	return true;
}

bool Game::combatChangeMana(Creature* attacker, Creature* target, int32_t manaChange)
{
	const Position& targetPos = target->getPosition();
	const SpectatorVec& list = getSpectators(targetPos);

	if(manaChange > 0)
	{
		if(attacker && target && attacker->defaultOutfit.lookFeet == target->defaultOutfit.lookFeet && g_config.getBool(ConfigManager::CANNOT_ATTACK_SAME_LOOKFEET))
			return false;
		target->changeMana(manaChange);
	}
	else
	{
		if(!target->isAttackable() || Combat::canDoCombat(attacker, target) != RET_NOERROR)
		{
			addMagicEffect(list, targetPos, NM_ME_POFF);
			return false;
		}

		if(attacker && target && attacker->defaultOutfit.lookFeet == target->defaultOutfit.lookFeet && g_config.getBool(ConfigManager::CANNOT_ATTACK_SAME_LOOKFEET))
			return false;

		int32_t manaLoss = std::min(target->getMana(), -manaChange);
		BlockType_t blockType = target->blockHit(attacker, COMBAT_MANADRAIN, manaLoss);
		if(blockType != BLOCK_NONE)
		{
			addMagicEffect(list, targetPos, NM_ME_POFF);
			return false;
		}

		if(manaLoss > 0)
		{
			target->drainMana(attacker, manaLoss);
			char buffer[20];
			sprintf(buffer, "%d", manaLoss);
			addAnimatedText(list, targetPos, TEXTCOLOR_BLUE, buffer);
		}
	}
	return true;
}

void Game::addCreatureHealth(const Creature* target)
{
	const SpectatorVec& list = getSpectators(target->getPosition());
	addCreatureHealth(list, target);
}

void Game::addCreatureHealth(const SpectatorVec& list, const Creature* target)
{
	Player* player = NULL;
	for(SpectatorVec::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendCreatureHealth(target);
	}
}

void Game::addAnimatedText(const Position& pos, uint8_t textColor,
	const std::string& text)
{
	const SpectatorVec& list = getSpectators(pos);
	addAnimatedText(list, pos, textColor, text);
}

void Game::addAnimatedText(const SpectatorVec& list, const Position& pos, uint8_t textColor,
	const std::string& text)
{
	Player* player = NULL;
	for(SpectatorVec::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendAnimatedText(pos, textColor, text);
	}
}

void Game::addMagicEffect(const Position& pos, uint8_t effect, bool ghostMode /* = false */)
{
	if(ghostMode)
		return;

	const SpectatorVec& list = getSpectators(pos);
	addMagicEffect(list, pos, effect);
}

void Game::addMagicEffect(const SpectatorVec& list, const Position& pos, uint8_t effect, bool ghostMode /* = false */)
{
	if(ghostMode)
		return;

	Player* player = NULL;
	for(SpectatorVec::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendMagicEffect(pos, effect);
	}
}

void Game::addDistanceEffect(const Position& fromPos, const Position& toPos,
	uint8_t effect)
{
	SpectatorVec list;

	getSpectators(list, fromPos, false, true);
	getSpectators(list, toPos, true, true);

	Player* player = NULL;
	for(SpectatorVec::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendDistanceShoot(fromPos, toPos, effect);
	}
}

void Game::startDecay(Item* item)
{
	if(item && item->canDecay())
	{
		uint32_t decayState = item->getDecaying();
		if(decayState == DECAYING_TRUE)
			 return;

		if(item->getDuration() > 0)
		{
			item->useThing2();
			item->setDecaying(DECAYING_TRUE);
			toDecayItems.push_back(item);
		}
		else
			internalDecayItem(item);
	}
}

void Game::internalDecayItem(Item* item)
{
	const ItemType& it = Item::items[item->getID()];
	if(it.decayTo != 0)
	{
		Item* newItem = transformItem(item, it.decayTo);
		startDecay(newItem);
	}
	else
	{
		ReturnValue ret = internalRemoveItem(item);
		if(ret != RET_NOERROR)
			std::cout << "DEBUG, internalDecayItem failed, error code: " << (int32_t) ret << "item id: " << item->getID() << std::endl;
	}
}

void Game::checkDecay()
{
	Scheduler::getScheduler().addEvent(createSchedulerTask(EVENT_DECAYINTERVAL,
		boost::bind(&Game::checkDecay, this)));

	Item* item = NULL;
	for(DecayList::iterator it = decayItems.begin(); it != decayItems.end();)
	{
		item = *it;
		item->decreaseDuration(EVENT_DECAYINTERVAL);

		if(!item->canDecay())
		{
			item->setDecaying(DECAYING_FALSE);
			FreeThing(item);
			it = decayItems.erase(it);
			continue;
		}

		if(item->getDuration() <= 0)
		{
			it = decayItems.erase(it);
			internalDecayItem(item);
			FreeThing(item);
		}
		else
			++it;
	}
	cleanup();
}

void Game::checkLight()
{
	Scheduler::getScheduler().addEvent(createSchedulerTask(EVENT_LIGHTINTERVAL,
		boost::bind(&Game::checkLight, this)));

	light_hour = light_hour + light_hour_delta;
	if(light_hour > 1440)
		light_hour = light_hour - 1440;

	if(std::abs(light_hour - SUNRISE) < 2*light_hour_delta)
		light_state = LIGHT_STATE_SUNRISE;
	else if(std::abs(light_hour - SUNSET) < 2*light_hour_delta)
		light_state = LIGHT_STATE_SUNSET;

	int32_t newlightlevel = lightlevel;
	bool lightChange = false;
	switch(light_state)
	{
		case LIGHT_STATE_SUNRISE:
		{
			newlightlevel += (LIGHT_LEVEL_DAY - LIGHT_LEVEL_NIGHT) / 30;
			lightChange = true;
			break;
		}
		case LIGHT_STATE_SUNSET:
		{
			newlightlevel -= (LIGHT_LEVEL_DAY - LIGHT_LEVEL_NIGHT) / 30;
			lightChange = true;
			break;
		}
		default:
			break;
	}

	if(newlightlevel <= LIGHT_LEVEL_NIGHT)
	{
		lightlevel = LIGHT_LEVEL_NIGHT;
		light_state = LIGHT_STATE_NIGHT;
	}
	else if(newlightlevel >= LIGHT_LEVEL_DAY)
	{
		lightlevel = LIGHT_LEVEL_DAY;
		light_state = LIGHT_STATE_DAY;
	}
	else
		lightlevel = newlightlevel;

	if(lightChange)
	{
		LightInfo lightInfo;
		getWorldLightInfo(lightInfo);
		for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
			(*it).second->sendWorldLight(lightInfo);
	}
}

void Game::getWorldLightInfo(LightInfo& lightInfo)
{
	lightInfo.level = lightlevel;
	lightInfo.color = 0xD7;
}

bool Game::cancelRuleViolation(Player* player)
{
	RuleViolationsMap::iterator it = ruleViolations.find(player->getID());
	if(it == ruleViolations.end())
		return false;

	Player* gamemaster = it->second->gamemaster;
	if(!it->second->isOpen && gamemaster)
	{
		//Send to the responser
		gamemaster->sendRuleViolationCancel(player->getName());
	}
	else
	{
		//Send to channel
		ChatChannel* channel = g_chat.getChannelById(0x03);
		if(channel)
		{
			for(UsersMap::const_iterator ut = channel->getUsers().begin(); ut != channel->getUsers().end(); ++ut)
			{
				if(ut->second)
					ut->second->sendRemoveReport(player->getName());
			}
		}
	}

	//Now erase it
	ruleViolations.erase(it);
	return true;
}

bool Game::closeRuleViolation(Player* player)
{
	RuleViolationsMap::iterator it = ruleViolations.find(player->getID());
	if(it == ruleViolations.end())
		return false;

	ruleViolations.erase(it);
	player->sendLockRuleViolation();

	ChatChannel* channel = g_chat.getChannelById(0x03);
	if(channel)
	{
		for(UsersMap::const_iterator ut = channel->getUsers().begin(); ut != channel->getUsers().end(); ++ut)
		{
			if(ut->second)
				ut->second->sendRemoveReport(player->getName());
		}
	}
	return true;
}

void Game::addCommandTag(std::string tag)
{
	bool found = false;
	for(uint32_t i = 0; i < commandTags.size() ;i++)
	{
		if(commandTags[i] == tag)
		{
			found = true;
			break;
		}
	}

	if(!found)
		commandTags.push_back(tag);
}

void Game::resetCommandTag()
{
	commandTags.clear();
}

void Game::shutdown()
{
	std::cout << "Preparing shutdown";
	Scheduler::getScheduler().stop();
	std::cout << ".";
	Dispatcher::getDispatcher().stop();
	std::cout << ".";
	Spawns::getInstance()->clear();
	std::cout << "." << std::endl;

	if(g_server)
		g_server->stop();

	cleanup();
	std::cout << "Exiting." << std::endl;
	exit(1);
}

void Game::cleanup()
{
	//free memory
	for(std::vector<Thing*>::iterator it = ToReleaseThings.begin(); it != ToReleaseThings.end(); ++it)
		(*it)->releaseThing2();

	ToReleaseThings.clear();

	for(DecayList::iterator it = toDecayItems.begin(); it != toDecayItems.end(); ++it)
		decayItems.push_back(*it);

	toDecayItems.clear();
}

void Game::FreeThing(Thing* thing)
{
	ToReleaseThings.push_back(thing);
}

bool Game::reloadHighscores()
{
	lastHSUpdate = time(NULL);
	for(int16_t i = 0; i <= 8; i++)
		highscoreStorage[i] = getHighscore(i);
	return true;
}

void Game::timedHighscoreUpdate()
{
	reloadHighscores();

	int32_t highscoreUpdateTime = g_config.getNumber(ConfigManager::HIGHSCORES_UPDATETIME) * 1000 * 60;
	if(highscoreUpdateTime <= 0)
		return;

	Scheduler::getScheduler().addEvent(createSchedulerTask(highscoreUpdateTime, boost::bind(&Game::timedHighscoreUpdate, this)));
}

std::string Game::getHighscoreString(uint16_t skill)
{
	Highscore hs = highscoreStorage[skill];
	std::stringstream ss;
	ss << "Highscore for " << getSkillName(skill) << "\n\nRank. Level - Player Name";
	for(uint32_t i = 0; i < hs.size(); i++)
		ss << "\n" << i+1 << ".  " << hs[i].second << "  -  " << hs[i].first;
	ss << "\n\nLast updated on:\n" << std::ctime(&lastHSUpdate);
	std::string highscores = ss.str();
	highscores.erase(highscores.length() - 1);
	return highscores;
}

bool Game::broadcastMessage(const std::string& text, MessageClasses type)
{
	if(type >= MSG_CLASS_FIRST && type <= MSG_CLASS_LAST)
	{
		std::cout << "> Broadcasted message: \"" << text << "\"." << std::endl;
		for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
			(*it).second->sendTextMessage(type, text);
		return true;
	}
	return false;
}

Highscore Game::getHighscore(uint16_t skill)
{
	Highscore hs;
	Database* db = Database::getInstance();
	DBQuery query;
	DBResult* result;
	int32_t highscoresTop = g_config.getNumber(ConfigManager::HIGHSCORES_TOP);
	if(skill >= 7)
	{
		if(skill == 7)
			query << "SELECT `maglevel`, `name` FROM `players` ORDER BY `maglevel` DESC, `manaspent` DESC LIMIT " << highscoresTop;
		else
			query << "SELECT `level`, `name` FROM `players` ORDER BY `level` DESC, `experience` DESC LIMIT " << highscoresTop;

		if((result = db->storeQuery(query.str())))
		{
			do
			{
				uint32_t level;
				if(skill == 7)
					level = result->getDataInt("maglevel");
				else
					level = result->getDataInt("level");

				std::string name = result->getDataString("name");
				if(name.length() > 0)
					hs.push_back(make_pair(name, level));
			}
			while(result->next());
			db->freeResult(result);
		}
	}
	else
	{
		query << "SELECT `player_skills`.`value`, `players`.`name` FROM `player_skills`,`players` WHERE `player_skills`.`skillid`=" << skill << " AND `player_skills`.`player_id`=`players`.`id` ORDER BY `player_skills`.`value` DESC, `player_skills`.`count` DESC LIMIT " << highscoresTop;
		if((result = db->storeQuery(query.str())))
		{
			do
			{
				uint32_t level = result->getDataInt("value");
				std::string name = result->getDataString("name");
				if(name.length() > 0)
					hs.push_back(make_pair(name, level));
			}
			while(result->next());
			db->freeResult(result);
		}
	}
	return hs;
}

void Game::updateCreatureSkull(Player* player)
{
	if(getWorldType() != WORLD_TYPE_PVP)
		return;

	const SpectatorVec& list = getSpectators(player->getPosition());

	//send to client
	Player* tmpPlayer = NULL;
	for(SpectatorVec::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		 if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureSkull(player);
	}
}

void Game::prepareGlobalSave()
{
	if(!globalSaveMessage[0])
	{
		globalSaveMessage[0] = true;
		broadcastMessage("Server is saving game in 5 minutes. Please logout.", MSG_STATUS_WARNING);
		Scheduler::getScheduler().addEvent(createSchedulerTask(120000, boost::bind(&Game::prepareGlobalSave, this)));
	}
	else if(!globalSaveMessage[1])
	{
		globalSaveMessage[1] = true;
		broadcastMessage("Server is saving game in 3 minutes. Please logout.", MSG_STATUS_WARNING);
		Scheduler::getScheduler().addEvent(createSchedulerTask(120000, boost::bind(&Game::prepareGlobalSave, this)));
	}
	else if(!globalSaveMessage[2])
	{
		globalSaveMessage[2] = true;
		broadcastMessage("Server is saving game in one minute. Please logout.", MSG_STATUS_WARNING);
		Scheduler::getScheduler().addEvent(createSchedulerTask(60000, boost::bind(&Game::prepareGlobalSave, this)));
	}
	else
		globalSave();
}

void Game::globalSave()
{
	if(g_config.getBool(ConfigManager::SHUTDOWN_AT_GLOBALSAVE))
	{
		//shutdown server
		Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::setGameState, this, GAME_STATE_SHUTDOWN)));
	}
	else
	{
		//close server
		Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::setGameState, this, GAME_STATE_CLOSED)));

		//clean map if configured to
		if(g_config.getBool(ConfigManager::CLEAN_MAP_AT_GLOBALSAVE))
			map->clean();

		//remove premium days from accounts
		if(g_config.getBool(ConfigManager::REMOVE_PREMIUM_ON_INIT))
			IOLoginData::getInstance()->updatePremiumDays();

		//reload highscores
		reloadHighscores();

		//reset variables
		for(int16_t i = 0; i < 3; i++)
			setGlobalSaveMessage(i, false);

		//prepare for next global save after 24 hours
		Scheduler::getScheduler().addEvent(createSchedulerTask(86100000, boost::bind(&Game::prepareGlobalSave, this)));

		//open server
		Dispatcher::getDispatcher().addTask(createTask(boost::bind(&Game::setGameState, this, GAME_STATE_NORMAL)));
	}
}

Position Game::getClosestFreeTile(Creature* creature, Position toPos)
{
	int32_t i;
	Tile* tile[9] =
	{
		getTile(toPos.x, toPos.y, toPos.z),
		getTile(toPos.x - 1, toPos.y - 1, toPos.z), getTile(toPos.x, toPos.y - 1, toPos.z),
		getTile(toPos.x + 1, toPos.y - 1, toPos.z), getTile(toPos.x - 1, toPos.y, toPos.z),
		getTile(toPos.x + 1, toPos.y, toPos.z), getTile(toPos.x - 1, toPos.y + 1, toPos.z),
		getTile(toPos.x, toPos.y + 1, toPos.z), getTile(toPos.x + 1, toPos.y + 1, toPos.z),
	};

	Player* player = creature->getPlayer();
	if(player)
	{
		for(i = 0; i < 9; i++)
		{
			if(tile[i] && tile[i]->creatures.empty() && (!tile[i]->hasProperty(IMMOVABLEBLOCKSOLID) || player->hasCustomFlag(PlayerCustomFlag_CanMoveAnywhere)))
				return tile[i]->getPosition();
		}
	}
	else
	{
		for(i = 0; i < 9; i++)
		{
			if(tile[i] && tile[i]->creatures.empty() && !tile[i]->hasProperty(IMMOVABLEBLOCKSOLID))
				return tile[i]->getPosition();
		}
	}
	return Position(0, 0, 0);
}

int32_t Game::getMotdNum()
{
	if(lastMotdText != g_config.getString(ConfigManager::MOTD))
	{
		Database* db = Database::getInstance();
		DBQuery query;

		lastMotdNum++;
		lastMotdText = g_config.getString(ConfigManager::MOTD);
		query << "INSERT INTO `server_motd` (`id`, `text`) VALUES (" << lastMotdNum << ", " << db->escapeString(lastMotdText) << ");";
		db->executeQuery(query.str());
		query.str("");
	}
	return lastMotdNum;
}

void Game::loadMotd()
{
	Database* db = Database::getInstance();
	DBResult* result;

	if(!(result = db->storeQuery("SELECT `id`, `text` FROM `server_motd` ORDER BY `id` DESC LIMIT 1")))
	{
		std::cout << "> ERROR: Failed to load motd!" << std::endl;
		lastMotdNum = random_range(5, 500);
		return;
	}

	lastMotdNum = result->getDataInt("id");
	lastMotdText = result->getDataString("text");
	db->freeResult(result);
}

void Game::checkPlayersRecord()
{
	if(getPlayersOnline() > lastPlayersRecord)
	{
		Database* db = Database::getInstance();
		DBQuery query;

		lastPlayersRecord = getPlayersOnline();
		query << "INSERT INTO `server_record` (`record`, `timestamp`) VALUES (" << lastPlayersRecord << ", " << time(NULL) << ");";
		db->executeQuery(query.str());
		query.str("");

		char buffer[60];
		sprintf(buffer, "New record: %d players are logged in.", lastPlayersRecord);
		broadcastMessage(buffer, MSG_STATUS_DEFAULT);
	}
}

void Game::loadPlayersRecord()
{
	Database* db = Database::getInstance();
	DBResult* result;

	if(!(result = db->storeQuery("SELECT `record` FROM `server_record` ORDER BY `timestamp` DESC LIMIT 1")))
	{
		std::cout << "> ERROR: Failed to load online record!" << std::endl;
		lastPlayersRecord = 0;
		return;
	}

	lastPlayersRecord = result->getDataInt("record");
	db->freeResult(result);
}

bool Game::violationWindow(uint32_t playerId, std::string targetPlayerName, int32_t reason, int32_t action, std::string banComment, std::string statement, bool IPBanishment)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if((0 == (violationActions[player->getViolationAccess()] & (1 << action)))
		|| reason > violationReasons[player->getViolationAccess()]
		|| (IPBanishment && (violationActions[player->getViolationAccess()] & Action_IpBan) != Action_IpBan))
	{
		player->sendCancel("You do not have authorization for this action.");
		return false;
	}

	uint32_t commentSize = g_config.getNumber(ConfigManager::MAX_VIOLATIONCOMMENT_SIZE);
	if(banComment.size() > commentSize)
	{
		char buffer[90];
		sprintf(buffer, "The comment may not exceed limit of %d characters.", commentSize);
		player->sendCancel(buffer);
		return false;
	}

	//Statements cannot be this long, player has most likely faked the message.
	if(statement.size() > 300)
		return false;

	bool playerExists = IOLoginData::getInstance()->playerExists(targetPlayerName);
	toLowerCaseString(targetPlayerName);
	if(!playerExists || targetPlayerName == "account manager")
	{
		player->sendCancel("A player with this name does not exist.");
		return false;
	}

	bool auth = false;
	Account account;
	uint32_t guid;
	uint32_t ip;

	Player* targetPlayer = getPlayerByName(targetPlayerName);
	if(targetPlayer)
	{
		auth = targetPlayer->hasFlag(PlayerFlag_CannotBeBanned);
		account = IOLoginData::getInstance()->loadAccount(targetPlayer->getAccount());
		guid = targetPlayer->getGUID();
		ip = targetPlayer->lastIP;
	}
	else
	{
		auth = IOLoginData::getInstance()->hasFlag(targetPlayerName, PlayerFlag_CannotBeBanned);
		account = IOLoginData::getInstance()->loadAccount(IOLoginData::getInstance()->getAccountNumberByName(targetPlayerName));
		IOLoginData::getInstance()->getGuidByName(guid, targetPlayerName);
		ip = IOLoginData::getInstance()->getLastIP(guid);
	}

	if(auth)
	{
		player->sendCancel("You do not have authorization for this action.");
		return false;
	}

	bool notation = false;
	switch(action)
	{
		case 0:
		{
			IOBan::getInstance()->addNotation(account.accnumber, reason, action, banComment, player->getGUID());
			if(IOBan::getInstance()->getNotationsCount(account.accnumber) >= (uint32_t)g_config.getNumber(ConfigManager::NOTATIONS_TO_BAN))
			{
				account.warnings++;
				if(account.warnings >= (g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION)))
				{
					action = 7;
					IOBan::getInstance()->addDeletion(account.accnumber, reason, action, banComment, player->getGUID());
				}
				else if(account.warnings >= g_config.getNumber(ConfigManager::WARNINGS_TO_FINALBAN))
					IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::FINALBAN_LENGTH)), reason, action, banComment, player->getGUID());
				else
					IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::BAN_LENGTH)), reason, action, banComment, player->getGUID());
			}
			else
				notation = true;
			break;
		}

		case 1:
		{
			IOBan::getInstance()->addNamelock(guid, reason, action, banComment, player->getGUID());
			break;
		}

		case 3:
		{
			if(account.warnings >= g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION))
			{
				action = 7;
				IOBan::getInstance()->addDeletion(account.accnumber, reason, action, banComment, player->getGUID());
			}
			else
			{
				account.warnings++;
				if(account.warnings >= g_config.getNumber(ConfigManager::WARNINGS_TO_FINALBAN))
					IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::FINALBAN_LENGTH)), reason, action, banComment, player->getGUID());
				else
					IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::BAN_LENGTH)), reason, action, banComment, player->getGUID());

				IOBan::getInstance()->addNamelock(guid, reason, action, banComment, player->getGUID());
			}
			break;
		}

		case 4:
		{
			if(account.warnings < g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION))
			{
				account.warnings = g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION);
				IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::FINALBAN_LENGTH)), reason, action, banComment, player->getGUID());
			}
			else
			{
				action = 7;
				IOBan::getInstance()->addDeletion(account.accnumber, reason, action, banComment, player->getGUID());
			}
			break;
		}

		case 5:
		{
			if(account.warnings < g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION))
			{
				account.warnings = g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION);
				IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::FINALBAN_LENGTH)), reason, action, banComment, player->getGUID());
				IOBan::getInstance()->addNamelock(guid, reason, action, banComment, player->getGUID());
			}
			else
			{
				action = 7;
				IOBan::getInstance()->addDeletion(account.accnumber, reason, action, banComment, player->getGUID());
			}
			break;
		}

		default:
		{
			account.warnings++;
			if(account.warnings >= g_config.getNumber(ConfigManager::WARNINGS_TO_DELETION))
			{
				action = 7;
				IOBan::getInstance()->addDeletion(account.accnumber, reason, action, banComment, player->getGUID());
			}
			else
			{
				if(account.warnings >= g_config.getNumber(ConfigManager::WARNINGS_TO_FINALBAN))
					IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::FINALBAN_LENGTH)), reason, action, banComment, player->getGUID());
				else
					IOBan::getInstance()->addBanishment(account.accnumber, (time(NULL) + g_config.getNumber(ConfigManager::BAN_LENGTH)), reason, action, banComment, player->getGUID());
			}
			break;
		}
	}

	if(IPBanishment && ip > 0)
		IOBan::getInstance()->addIpBanishment(ip, (time(NULL) + g_config.getNumber(ConfigManager::IPBANISHMENT_LENGTH)), banComment, player->getGUID());

	if(!notation)
		IOBan::getInstance()->removeNotations(account.accnumber);

	char buffer[800 + banComment.length()];
	if(g_config.getBool(ConfigManager::BROADCAST_BANISHMENTS))
	{
		if(action == 6)
			sprintf(buffer, "%s has taken the action \"%s\" for the statement: \"%s\" against: %s (Warnings: %d), with reason: \"%s\", and comment: \"%s\".", player->getName().c_str(), getAction(action, IPBanishment).c_str(), statement.c_str(), targetPlayerName.c_str(), account.warnings, getReason(reason).c_str(), banComment.c_str());
		else
			sprintf(buffer, "%s has taken the action \"%s\" against: %s (Warnings: %d), with reason: \"%s\", and comment: \"%s\".", player->getName().c_str(), getAction(action, IPBanishment).c_str(), targetPlayerName.c_str(), account.warnings, getReason(reason).c_str(), banComment.c_str());

		broadcastMessage(buffer, MSG_STATUS_WARNING);
	}
	else
	{
		if(action == 6)
			sprintf(buffer, "You have taken the action \"%s\" for the statement: \"%s\" against: %s (Warnings: %d), with reason: \"%s\", and comment: \"%s\".", getAction(action, IPBanishment).c_str(), statement.c_str(), targetPlayerName.c_str(), account.warnings, getReason(reason).c_str(), banComment.c_str());
		else
			sprintf(buffer, "You have taken the action \"%s\" against: %s (Warnings: %d), with reason: \"%s\", and comment: \"%s\".", getAction(action, IPBanishment).c_str(), targetPlayerName.c_str(), account.warnings, getReason(reason).c_str(), banComment.c_str());

		player->sendTextMessage(MSG_STATUS_CONSOLE_RED, buffer);
	}

	if(targetPlayer && !notation)
	{
		addMagicEffect(targetPlayer->getPosition(), NM_ME_MAGIC_POISON);

		uint32_t playerId = targetPlayer->getID();
		Scheduler::getScheduler().addEvent(createSchedulerTask(1000, boost::bind(&Game::kickPlayer, this, playerId, false)));
	}

	IOLoginData::getInstance()->saveAccount(account);
	return true;
}

uint64_t Game::getExperienceStage(uint32_t level)
{
	if(stagesEnabled)
	{
		if(useLastStageLevel && level >= lastStageLevel)
			return stages[lastStageLevel];
		else
			return stages[level];
	}
	else
		return g_config.getNumber(ConfigManager::RATE_EXPERIENCE);
}

bool Game::loadExperienceStages()
{
	std::string filename = "data/XML/stages.xml";
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if(doc)
	{
		xmlNodePtr root, p;
		int32_t intVal, low, high, mult;
		root = xmlDocGetRootElement(doc);
		if(xmlStrcmp(root->name,(const xmlChar*)"stages"))
		{
			xmlFreeDoc(doc);
			return false;
		}
		p = root->children;

		while(p)
		{
			if(!xmlStrcmp(p->name, (const xmlChar*)"config"))
			{
				if(readXMLInteger(p, "enabled", intVal))
					stagesEnabled = (intVal != 0);
			}
			else if(!xmlStrcmp(p->name, (const xmlChar*)"stage"))
			{
				if(readXMLInteger(p, "minlevel", intVal))
					low = intVal;
				else
					low = 1;

				if(readXMLInteger(p, "maxlevel", intVal))
					high = intVal;
				else
				{
					high = 0;
					lastStageLevel = low;
					useLastStageLevel = true;
				}

				if(readXMLInteger(p, "multiplier", intVal))
					mult = intVal;
				else
					mult = 1;

				if(useLastStageLevel)
					stages[lastStageLevel] = mult;
				else
				{
					for(int32_t iteratorValue = low; iteratorValue <= high; iteratorValue++)
						stages[iteratorValue] = mult;
				}
			}
			p = p->next;
		}
		xmlFreeDoc(doc);
	}
	return true;
}

bool Game::playerInviteToParty(uint32_t playerId, uint32_t invitedId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* invitedPlayer = getPlayerByID(invitedId);
	if(!invitedPlayer || invitedPlayer->isRemoved() || invitedPlayer->isInviting(player))
		return false;

	if(invitedPlayer->getParty())
	{
		char buffer[90];
		sprintf(buffer, "%s is already in a party.", invitedPlayer->getName().c_str());
		player->sendTextMessage(MSG_INFO_DESCR, buffer);
		return false;
	}

	Party* party = player->getParty();
	if(!party)
		party = new Party(player);
	else if(party->getLeader() != player)
		return false;

	return party->invitePlayer(invitedPlayer);
}

bool Game::playerJoinParty(uint32_t playerId, uint32_t leaderId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* leader = getPlayerByID(leaderId);
	if(!leader || leader->isRemoved() || !leader->isInviting(player))
		return false;

	if(!leader->getParty() || leader->getParty()->getLeader() != leader)
		return false;

	if(player->getParty())
	{
		player->sendTextMessage(MSG_INFO_DESCR, "You are already in a party.");
		return false;
	}
	return leader->getParty()->joinParty(player);
}

bool Game::playerRevokePartyInvitation(uint32_t playerId, uint32_t invitedId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || !player->getParty() || player->getParty()->getLeader() != player)
		return false;

	Player* invitedPlayer = getPlayerByID(invitedId);
	if(!invitedPlayer || invitedPlayer->isRemoved() || !player->isInviting(invitedPlayer))
		return false;

	player->getParty()->revokeInvitation(invitedPlayer);
	return true;
}

bool Game::playerPassPartyLeadership(uint32_t playerId, uint32_t newLeaderId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || !player->getParty() || player->getParty()->getLeader() != player)
		return false;

	Player* newLeader = getPlayerByID(newLeaderId);
	if(!newLeader || newLeader->isRemoved() || !player->isPartner(newLeader))
		return false;

	return player->getParty()->passPartyLeadership(newLeader);
}

bool Game::playerLeaveParty(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->getParty() || player->hasCondition(CONDITION_INFIGHT))
		return false;

	return player->getParty()->leaveParty(player);
}

bool Game::playerEnableSharedPartyExperience(uint32_t playerId, uint8_t sharedExpActive, uint8_t unknown)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->getParty() || player->hasCondition(CONDITION_INFIGHT))
		return false;

	return player->getParty()->setSharedExperience(player, sharedExpActive == 1);
}

void Game::sendGuildMotd(uint32_t playerId, uint32_t guildId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return;

	player->sendChannelMessage("Message of the Day", IOGuild::getInstance()->getMotd(guildId), SPEAK_CHANNEL_R1, 0x00);
}

void Game::kickPlayer(uint32_t playerId, bool displayEffect)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return;

	player->kickPlayer(displayEffect);
}

bool Game::playerReportBug(uint32_t playerId, std::string bug)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Database* db = Database::getInstance();
	DBQuery query;

	query << "INSERT INTO `server_reports` (`player_id`, `posx`, `posy`, `posz`, `timestamp`, `report`) VALUES (";
	query << player->getGUID() << ", ";
	query << player->getPosition().x << ", " << player->getPosition().y << ", " << player->getPosition().z << ", ";
	query << time(NULL) << ", " << db->escapeString(bug.c_str()) << ");";

	if(db->executeQuery(query.str()))
	{
		query.str("");
		player->sendTextMessage(MSG_EVENT_DEFAULT, "Your report has been sent to " + g_config.getString(ConfigManager::SERVER_NAME) + ".");
		return true;
	}

	query.str("");
	return false;
}
