////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#ifndef __CONST__
#define __CONST__
#include "definitions.h"

enum MagicEffect_t
{
	MAGIC_EFFECT_DRAW_BLOOD	= 0x00,
	MAGIC_EFFECT_LOSE_ENERGY	= 0x01,
	MAGIC_EFFECT_POFF		= 0x02,
	MAGIC_EFFECT_BLOCKHIT		= 0x03,
	MAGIC_EFFECT_EXPLOSION_AREA	= 0x04,
	MAGIC_EFFECT_EXPLOSION_DAMAGE	= 0x05,
	MAGIC_EFFECT_FIRE_AREA		= 0x06,
	MAGIC_EFFECT_YELLOW_RINGS	= 0x07,
	MAGIC_EFFECT_POISON_RINGS	= 0x08,
	MAGIC_EFFECT_HIT_AREA		= 0x09,
	MAGIC_EFFECT_TELEPORT		= 0x0A, //10
	MAGIC_EFFECT_ENERGY_DAMAGE	= 0x0B, //11
	MAGIC_EFFECT_WRAPS_BLUE	= 0x0C, //12
	MAGIC_EFFECT_WRAPS_RED	= 0x0D, //13
	MAGIC_EFFECT_WRAPS_GREEN	= 0x0E, //14
	MAGIC_EFFECT_HITBY_FIRE	= 0x0F, //15
	MAGIC_EFFECT_POISON		= 0x10, //16
	MAGIC_EFFECT_MORT_AREA		= 0x11, //17
	MAGIC_EFFECT_SOUND_GREEN	= 0x12, //18
	MAGIC_EFFECT_SOUND_RED		= 0x13, //19
	MAGIC_EFFECT_POISON_AREA	= 0x14, //20
	MAGIC_EFFECT_SOUND_YELLOW	= 0x15, //21
	MAGIC_EFFECT_SOUND_PURPLE	= 0x16, //22
	MAGIC_EFFECT_SOUND_BLUE	= 0x17, //23
	MAGIC_EFFECT_SOUND_WHITE	= 0x18, //24
	MAGIC_EFFECT_BUBBLES		= 0x19, //25
	MAGIC_EFFECT_CRAPS		= 0x1A, //26
	MAGIC_EFFECT_GIFT_WRAPS	= 0x1B, //27
	MAGIC_EFFECT_FIREWORK_YELLOW	= 0x1C, //28
	MAGIC_EFFECT_FIREWORK_RED	= 0x1D, //29
	MAGIC_EFFECT_FIREWORK_BLUE	= 0x1E, //30
	MAGIC_EFFECT_STUN		= 0x1F, //31
	MAGIC_EFFECT_SLEEP		= 0x20, //32
	MAGIC_EFFECT_WATERCREATURE	= 0x21, //33
	MAGIC_EFFECT_GROUNDSHAKER	= 0x22, //34
	MAGIC_EFFECT_HEARTS		= 0x23, //35
	MAGIC_EFFECT_FIREATTACK	= 0x24, //36
	MAGIC_EFFECT_ENERGY_AREA	= 0x25, //37
	MAGIC_EFFECT_SMALLCLOUDS	= 0x26, //38
	MAGIC_EFFECT_HOLYDAMAGE	= 0x27, //39
	MAGIC_EFFECT_BIGCLOUDS		= 0x28, //40
	MAGIC_EFFECT_ICEAREA		= 0x29, //41
	MAGIC_EFFECT_ICETORNADO	= 0x2A, //42
	MAGIC_EFFECT_ICEATTACK		= 0x2B, //43
	MAGIC_EFFECT_STONES		= 0x2C, //44
	MAGIC_EFFECT_SMALLPLANTS	= 0x2D, //45
	MAGIC_EFFECT_CARNIPHILA	= 0x2E, //46
	MAGIC_EFFECT_PURPLEENERGY	= 0x2F, //47
	MAGIC_EFFECT_YELLOWENERGY	= 0x30, //48
	MAGIC_EFFECT_HOLYAREA		= 0x31, //49
	MAGIC_EFFECT_BIGPLANTS		= 0x32, //50
	MAGIC_EFFECT_CAKE		= 0x33, //51
	MAGIC_EFFECT_GIANTICE		= 0x34, //52
	MAGIC_EFFECT_WATERSPLASH	= 0x35, //53
	MAGIC_EFFECT_PLANTATTACK	= 0x36, //54
	MAGIC_EFFECT_TUTORIALARROW	= 0x37, //55
	MAGIC_EFFECT_TUTORIALSQUARE	= 0x38, //56
	MAGIC_EFFECT_MIRRORHORIZONTAL	= 0x39, //57
	MAGIC_EFFECT_MIRRORVERTICAL	= 0x3A, //58
	MAGIC_EFFECT_SKULLHORIZONTAL	= 0x3B, //59
	MAGIC_EFFECT_SKULLVERTICAL	= 0x3C, //60
	MAGIC_EFFECT_ASSASSIN		= 0x3D, //61
	MAGIC_EFFECT_STEPSHORIZONTAL	= 0x3E, //62
	MAGIC_EFFECT_BLOODYSTEPS	= 0x3F, //63
	MAGIC_EFFECT_STEPSVERTICAL	= 0x40, //64
	MAGIC_EFFECT_YALAHARIGHOST	= 0x41, //65
	MAGIC_EFFECT_BATS		= 0x42, //66
	MAGIC_EFFECT_SMOKE		= 0x43, //67
	MAGIC_EFFECT_INSECTS		= 0x44, //68
	MAGIC_EFFECT_DRAGONHEAD		= 0x45, //69
	MAGIC_EFFECT_LAST		= MAGIC_EFFECT_DRAGONHEAD,

	//for internal use, dont send to client
	MAGIC_EFFECT_NONE		= 0xFF,
	MAGIC_EFFECT_UNKNOWN		= 0xFFFF
};

enum ShootEffect_t
{
	SHOOT_EFFECT_SPEAR		= 0x00,
	SHOOT_EFFECT_BOLT		= 0x01,
	SHOOT_EFFECT_ARROW		= 0x02,
	SHOOT_EFFECT_FIRE		= 0x03,
	SHOOT_EFFECT_ENERGY		= 0x04,
	SHOOT_EFFECT_POISONARROW	= 0x05,
	SHOOT_EFFECT_BURSTARROW	= 0x06,
	SHOOT_EFFECT_THROWINGSTAR	= 0x07,
	SHOOT_EFFECT_THROWINGKNIFE	= 0x08,
	SHOOT_EFFECT_SMALLSTONE	= 0x09,
	SHOOT_EFFECT_DEATH		= 0x0A, //10
	SHOOT_EFFECT_LARGEROCK	= 0x0B, //11
	SHOOT_EFFECT_SNOWBALL	= 0x0C, //12
	SHOOT_EFFECT_POWERBOLT	= 0x0D, //13
	SHOOT_EFFECT_POISONFIELD	= 0x0E, //14
	SHOOT_EFFECT_INFERNALBOLT	= 0x0F, //15
	SHOOT_EFFECT_HUNTINGSPEAR	= 0x10, //16
	SHOOT_EFFECT_ENCHANTEDSPEAR	= 0x11, //17
	SHOOT_EFFECT_REDSTAR	= 0x12, //18
	SHOOT_EFFECT_GREENSTAR	= 0x13, //19
	SHOOT_EFFECT_ROYALSPEAR	= 0x14, //20
	SHOOT_EFFECT_SNIPERARROW	= 0x15, //21
	SHOOT_EFFECT_ONYXARROW	= 0x16, //22
	SHOOT_EFFECT_PIERCINGBOLT	= 0x17, //23
	SHOOT_EFFECT_WHIRLWINDSWORD	= 0x18, //24
	SHOOT_EFFECT_WHIRLWINDAXE	= 0x19, //25
	SHOOT_EFFECT_WHIRLWINDCLUB	= 0x1A, //26
	SHOOT_EFFECT_ETHEREALSPEAR	= 0x1B, //27
	SHOOT_EFFECT_ICE		= 0x1C, //28
	SHOOT_EFFECT_EARTH		= 0x1D, //29
	SHOOT_EFFECT_HOLY		= 0x1E, //30
	SHOOT_EFFECT_SUDDENDEATH	= 0x1F, //31
	SHOOT_EFFECT_FLASHARROW	= 0x20, //32
	SHOOT_EFFECT_FLAMMINGARROW	= 0x21, //33
	SHOOT_EFFECT_SHIVERARROW	= 0x22, //34
	SHOOT_EFFECT_ENERGYBALL	= 0x23, //35
	SHOOT_EFFECT_SMALLICE	= 0x24, //36
	SHOOT_EFFECT_SMALLHOLY	= 0x25, //37
	SHOOT_EFFECT_SMALLEARTH	= 0x26, //38
	SHOOT_EFFECT_EARTHARROW	= 0x27, //39
	SHOOT_EFFECT_EXPLOSION	= 0x28, //40
	SHOOT_EFFECT_CAKE		= 0x29, //41
	SHOOT_EFFECT_LAST		= SHOOT_EFFECT_CAKE,

	//for internal use, dont send to client
	SHOOT_EFFECT_WEAPONTYPE	= 0xFE, //254
	SHOOT_EFFECT_NONE		= 0xFF,
	SHOOT_EFFECT_UNKNOWN	= 0xFFFF
};

enum SpeakClasses
{
	SPEAK_CLASS_NONE	= 0x00,
	SPEAK_CLASS_FIRST 	= 0x01,
	SPEAK_SAY			= SPEAK_CLASS_FIRST,
	SPEAK_WHISPER		= 0x02,
	SPEAK_YELL			= 0x03,
	SPEAK_PRIVATE_PN	= 0x04,
	SPEAK_PRIVATE_NP	= 0x05,
	SPEAK_PRIVATE		= 0x06,
	SPEAK_CHANNEL_Y		= 0x07,
	SPEAK_CHANNEL_W		= 0x08,
	SPEAK_BROADCAST		= 0x09,
	SPEAK_CHANNEL_RN	= 0x0A,
	SPEAK_PRIVATE_RED	= 0x0B,
	SPEAK_CHANNEL_O		= 0x0C,
	SPEAK_MONSTER_SAY	= 0x0D,
	SPEAK_MONSTER_YELL	= 0x0E,
	SPEAK_CLASS_LAST 	= SPEAK_MONSTER_YELL,

	// removed from game
	SPEAK_CHANNEL_RA	= 0xFF + 4
};

enum MessageClasses
{
	MSG_CLASS_FIRST				= 0x0D,
	MSG_EVENT_ORANGE			= MSG_CLASS_FIRST, /*Orange message in the console*/
	MSG_STATUS_CONSOLE_ORANGE	= 0x0E, /*Orange message in the console*/
	MSG_STATUS_WARNING			= 0x0F, /*Red message in game window and in the console*/
	MSG_EVENT_ADVANCE			= 0x10, /*White message in game window and in the console*/
	MSG_EVENT_DEFAULT			= 0x11, /*White message at the bottom of the game window and in the console*/
	MSG_STATUS_DEFAULT			= 0x12, /*White message at the bottom of the game window and in the console*/
	MSG_INFO_DESCR				= 0x13, /*Green message in game window and in the console*/
	MSG_STATUS_SMALL			= 0x14, /*White message at the bottom of the game window"*/
	MSG_STATUS_CONSOLE_BLUE		= 0x15, /*Blue message in the console*/
	MSG_STATUS_CONSOLE_RED		= 0x16, /*Red message in the console*/
	MSG_CLASS_LAST				= MSG_STATUS_CONSOLE_RED
};

enum MapMarks_t
{
	MAPMARK_TICK		= 0x00,
	MAPMARK_QUESTION	= 0x01,
	MAPMARK_EXCLAMATION	= 0x02,
	MAPMARK_STAR		= 0x03,
	MAPMARK_CROSS		= 0x04,
	MAPMARK_TEMPLE		= 0x05,
	MAPMARK_KISS		= 0x06,
	MAPMARK_SHOVEL		= 0x07,
	MAPMARK_SWORD		= 0x08,
	MAPMARK_FLAG		= 0x09,
	MAPMARK_LOCK		= 0x0A,
	MAPMARK_BAG		= 0x0B,
	MAPMARK_SKULL		= 0x0C,
	MAPMARK_DOLLAR		= 0x0D,
	MAPMARK_REDNORTH	= 0x0E,
	MAPMARK_REDSOUTH	= 0x0F,
	MAPMARK_REDEAST		= 0x10,
	MAPMARK_REDWEST		= 0x11,
	MAPMARK_GREENNORTH	= 0x12,
	MAPMARK_GREENSOUTH	= 0x13
};

enum FluidColors_t
{
	FLUID_EMPTY	= 0x00,
	FLUID_BLUE	= 0x01,
	FLUID_RED	= 0x02,
	FLUID_BROWN	= 0x03,
	FLUID_GREEN	= 0x04,
	FLUID_YELLOW	= 0x05,
	FLUID_WHITE	= 0x06,
	FLUID_PURPLE	= 0x07
};

enum FluidTypes_t
{
	FLUID_NONE		= FLUID_EMPTY,
	FLUID_WATER		= FLUID_BLUE,
	FLUID_BLOOD		= FLUID_RED,
	FLUID_BEER		= FLUID_BROWN,
	FLUID_SLIME		= FLUID_GREEN,
	FLUID_LEMONADE		= FLUID_YELLOW,
	FLUID_MILK		= FLUID_WHITE,
	FLUID_MANA		= FLUID_PURPLE,

	FLUID_LIFE		= FLUID_RED + 8,
	FLUID_OIL		= FLUID_BROWN + 8,
	FLUID_URINE		= FLUID_YELLOW + 8,
	FLUID_COCONUTMILK	= FLUID_WHITE + 8,
	FLUID_WINE		= FLUID_PURPLE + 8,

	FLUID_MUD		= FLUID_BROWN + 16,
	FLUID_FRUITJUICE	= FLUID_YELLOW + 16,

	FLUID_LAVA		= FLUID_RED + 24,
	FLUID_RUM		= FLUID_BROWN + 24,
	FLUID_SWAMP		= FLUID_GREEN + 24,

	FLUID_TEA               = FLUID_BROWN + 32,
	FLUID_MEAD              = FLUID_BROWN + 40
};

const uint8_t reverseFluidMap[] =
{
	FLUID_EMPTY,
	FLUID_WATER,
	FLUID_MANA,
	FLUID_BEER,
	FLUID_EMPTY,
	FLUID_BLOOD,
	FLUID_SLIME,
	FLUID_EMPTY,
	FLUID_LEMONADE,
	FLUID_MILK
};

enum ClientFluidTypes_t
{
	CLIENTFLUID_EMPTY	= 0x00,
	CLIENTFLUID_BLUE	= 0x01,
	CLIENTFLUID_PURPLE	= 0x02,
	CLIENTFLUID_BROWN_1	= 0x03,
	CLIENTFLUID_BROWN_2	= 0x04,
	CLIENTFLUID_RED		= 0x05,
	CLIENTFLUID_GREEN	= 0x06,
	CLIENTFLUID_BROWN	= 0x07,
	CLIENTFLUID_YELLOW	= 0x08,
	CLIENTFLUID_WHITE	= 0x09
};

const uint8_t fluidMap[] =
{
	CLIENTFLUID_EMPTY,
	CLIENTFLUID_BLUE,
	CLIENTFLUID_RED,
	CLIENTFLUID_BROWN_1,
	CLIENTFLUID_GREEN,
	CLIENTFLUID_YELLOW,
	CLIENTFLUID_WHITE,
	CLIENTFLUID_PURPLE
};

enum Color_t
{
	COLOR_BLACK		= 0,
	COLOR_BLUE		= 5,
	COLOR_GREEN		= 18,
	COLOR_LIGHTGREEN	= 66,
	COLOR_DARKBROWN		= 78,
	COLOR_LIGHTBLUE		= 89,
	COLOR_DARKRED		= 108,
	COLOR_DARKPURPLE	= 112,
	COLOR_BROWN		= 120,
	COLOR_GREY		= 129,
	COLOR_TEAL		= 143,
	COLOR_DARKPINK		= 152,
	COLOR_PURPLE		= 154,
	COLOR_DARKORANGE	= 156,
	COLOR_RED		= 180,
	COLOR_PINK		= 190,
	COLOR_ORANGE		= 192,
	COLOR_DARKYELLOW	= 205,
	COLOR_YELLOW		= 210,
	COLOR_WHITE		= 215,

	COLOR_NONE		= 255,
	COLOR_UNKNOWN		= 256
};

enum Icons_t
{
	ICON_NONE = 0,
	ICON_POISON = 1 << 0,
	ICON_BURN = 1 << 1,
	ICON_ENERGY = 1 << 2,
	ICON_DRUNK = 1 << 3,
	ICON_MANASHIELD = 1 << 4,
	ICON_PARALYZE = 1 << 5,
	ICON_HASTE = 1 << 6,
	ICON_SWORDS = 1 << 7,
	ICON_DROWNING = 1 << 8,
	ICON_FREEZING = 1 << 9,
	ICON_DAZZLED = 1 << 10,
	ICON_CURSED = 1 << 11,
	ICON_BUFF = 1 << 12,
	ICON_PZBLOCK = 1 << 13,
	ICON_PZ = 1 << 14,
	ICON_BLEED = 1 << 15
};

enum WeaponType_t
{
	WEAPON_NONE = 0,
	WEAPON_SWORD,
	WEAPON_CLUB,
	WEAPON_AXE,
	WEAPON_DIST,
	WEAPON_SHIELD,
	WEAPON_FIST,
	WEAPON_WAND,
	WEAPON_AMMO
};

enum Ammo_t
{
	AMMO_NONE = 0,
	AMMO_BOLT,
	AMMO_ARROW,
	AMMO_SPEAR,
	AMMO_THROWINGSTAR,
	AMMO_THROWINGKNIFE,
	AMMO_STONE,
	AMMO_SNOWBALL
};

enum AmmoAction_t
{
	AMMOACTION_NONE = 0,
	AMMOACTION_REMOVECOUNT,
	AMMOACTION_REMOVECHARGE,
	AMMOACTION_MOVE,
	AMMOACTION_MOVEBACK
};

enum WieldInfo_t
{
	WIELDINFO_LEVEL = 1,
	WIELDINFO_MAGLV = 2,
	WIELDINFO_VOCREQ = 4,
	WIELDINFO_PREMIUM = 8
};

enum Skulls_t
{
	SKULL_NONE = 0,
	SKULL_YELLOW,
	SKULL_GREEN,
	SKULL_WHITE,
	SKULL_RED,
	SKULL_BLACK,
	SKULL_ORANGE,
	SKULL_LAST = SKULL_ORANGE
};

enum PartyShields_t
{
	SHIELD_NONE = 0,
	SHIELD_WHITEYELLOW,
	SHIELD_WHITEBLUE,
	SHIELD_BLUE,
	SHIELD_YELLOW,
	SHIELD_BLUE_SHAREDEXP,
	SHIELD_YELLOW_SHAREDEXP,
	SHIELD_BLUE_NOSHAREDEXP_BLINK,
	SHIELD_YELLOW_NOSHAREDEXP_BLINK,
	SHIELD_BLUE_NOSHAREDEXP,
	SHIELD_YELLOW_NOSHAREDEXP,
	SHIELD_LAST = SHIELD_YELLOW_NOSHAREDEXP
};

enum GuildEmblems_t
{
	EMBLEM_NONE = 0,
	EMBLEM_GREEN,
	EMBLEM_RED,
	EMBLEM_BLUE
};

enum Spells_t
{
	SPELL_NONE                    = 0x00,
	SPELL_LIGHT_HEALING           = 0x01,
	SPELL_INTENSE_HEALING         = 0x02,
	SPELL_ULTIMATE_HEALING        = 0x03,
	SPELL_INTENSE_HEALING_RUNE    = 0x04,
	SPELL_ULTIMATE_HEALING_RUNE   = 0x05,
	SPELL_HASTE                   = 0x06,
	SPELL_LIGHT_MAGIC_MISSILE     = 0x07,
	SPELL_HEAVY_MAGIC_MISSILE     = 0x08,
	SPELL_SUMMON_CREATURE         = 0x09,
	SPELL_LIGHT                   = 0x0A,
	SPELL_GREAT_LIGHT             = 0x0B,
	SPELL_CONVINCE_CREATURE       = 0x0C,
	SPELL_ENERGY_WAVE             = 0x0D,
	SPELL_CHAMELEON               = 0x0E,
	SPELL_FIREBALL                = 0x0F,
	SPELL_GREAT_FIREBALL          = 0x10,
	SPELL_FIREBOMB                = 0x11,
	SPELL_EXPLOSION               = 0x12,
	SPELL_FIRE_WAVE               = 0x13,
	SPELL_FIND_PERSON             = 0x14,
	SPELL_SUDDEN_DEATH            = 0x15,
	SPELL_ENERGY_BEAM             = 0x16,
	SPELL_GREAT_ENERGY_BEAM       = 0x17,
	SPELL_HELLS_CORE              = 0x18,
	SPELL_FIRE_FIELD              = 0x19,
	SPELL_POISON_FIELD            = 0x1A,
	SPELL_ENERGY_FIELD            = 0x1B,
	SPELL_FIRE_WALL               = 0x1C,
	SPELL_CURE_POISON             = 0x1D,
	SPELL_DESTROY_FIELD           = 0x1E,
	SPELL_ANTIDOTE_RUNE           = 0x1F,
	SPELL_POISON_WALL             = 0x20,
	SPELL_ENERGY_WALL             = 0x21,
	SPELL_UNKNOWN_1               = 0x22,
	SPELL_UNKNOWN_2               = 0x23,
	SPELL_SALVATION               = 0x24,
	SPELL_MOVE                    = 0x25,
	SPELL_CREATURE_ILLUSION       = 0x26,
	SPELL_STRONG_HASTE            = 0x27,
	SPELL_UNKNOWN_3               = 0x28,
	SPELL_UNKNOWN_4               = 0x29,
	SPELL_FOOD                    = 0x2A,
	SPELL_STRONG_ICE_WAVE         = 0x2B,
	SPELL_MAGIC_SHIELD            = 0x2C,
	SPELL_INVISIBLE               = 0x2D,
	SPELL_UNKNOWN_5               = 0x2E,
	SPELL_UNKNOWN_6               = 0x2F,
	SPELL_POISONED_ARROW          = 0x30,
	SPELL_EXPLOSIVE_ARROW         = 0x31,
	SPELL_SOULFIRE                = 0x32,
	SPELL_CONJURE_ARROW           = 0x33,
	SPELL_RETRIEVE_FRIEND         = 0x34,
	SPELL_UNKNOWN_7               = 0x35,
	SPELL_PARALYZE                = 0x36,
	SPELL_ENERGYBOMB              = 0x37,
	SPELL_WRATH_OF_NATURE         = 0x38,
	SPELL_STRONG_ETHEREAL_SPEAR   = 0x39,
	SPELL_UNKNOWN_8               = 0x3A,
	SPELL_FRONT_SWEEP             = 0x3B,
	SPELL_UNKNOWN_9               = 0x3C,
	SPELL_BRUTAL_STRIKE           = 0x3D,
	SPELL_ANNIHILATION            = 0x3E,
	SPELL_UNKNOWN_10              = 0x3F,
	SPELL_UNKNOWN_11              = 0x40,
	SPELL_UNKNOWN_12              = 0x41,
	SPELL_UNKNOWN_13              = 0x42,
	SPELL_UNKNOWN_14              = 0x43,
	SPELL_UNKNOWN_15              = 0x44,
	SPELL_UNKNOWN_16              = 0x45,
	SPELL_UNKNOWN_17              = 0x46,
	SPELL_INVITE_GUESTS           = 0x47,
	SPELL_INVITE_SUBOWNERS        = 0x48,
	SPELL_KICK_GUEST              = 0x49,
	SPELL_EDIT_DOOR               = 0x4A,
	SPELL_ULTIMATE_LIGHT          = 0x4B,
	SPELL_MAGIC_ROPE              = 0x4C,
	SPELL_STALAGMITE              = 0x4D,
	SPELL_DESINTEGRATE            = 0x4E,
	SPELL_CONJURE_BOLT            = 0x4F,
	SPELL_BERSERK                 = 0x50,
	SPELL_LEVITATE                = 0x51,
	SPELL_MASS_HEALING            = 0x52,
	SPELL_ANIMATE_DEAD            = 0x53,
	SPELL_HEAL_FRIEND             = 0x54,
	SPELL_UNDEAD_LEGION           = 0x55,
	SPELL_MAGIC_WALL              = 0x56,
	SPELL_DEATH_STRIKE            = 0x57,
	SPELL_ENERGY_STRIKE           = 0x58,
	SPELL_FLAME_STRIKE            = 0x59,
	SPELL_CANCEL_INVISIBILITY     = 0x5A,
	SPELL_POISONBOMB              = 0x5B,
	SPELL_ENCHANT_STAFF           = 0x5C,
	SPELL_CHALLENGE               = 0x5D,
	SPELL_WILD_GROWTH             = 0x5E,
	SPELL_POWER_BOLT              = 0x5F,
	SPELL_UNKNOWN_18              = 0x60,
	SPELL_UNKNOWN_19              = 0x61,
	SPELL_UNKNOWN_20              = 0x62,
	SPELL_UNKNOWN_21              = 0x63,
	SPELL_UNKNOWN_22              = 0x64,
	SPELL_UNKNOWN_23              = 0x65,
	SPELL_UNKNOWN_24              = 0x66,
	SPELL_UNKNOWN_25              = 0x67,
	SPELL_UNKNOWN_26              = 0x68,
	SPELL_FIERCE_BERSERK          = 0x69,
	SPELL_GROUNDSHAKER            = 0x6A,
	SPELL_WHIRLWIND_THROW         = 0x6B,
	SPELL_SNIPER_ARROW            = 0x6C,
	SPELL_PIERCING_BOLT           = 0x6D,
	SPELL_ENCHANT_SPEAR           = 0x6E,
	SPELL_ETHEREAL_SPEAR          = 0x6F,
	SPELL_ICE_STRIKE              = 0x70,
	SPELL_TERRA_STRIKE            = 0x71,
	SPELL_ICICLE                  = 0x72,
	SPELL_AVALANCHE               = 0x73,
	SPELL_STONE_SHOWER            = 0x74,
	SPELL_THUNDERSTORM            = 0x75,
	SPELL_ETERNAL_WINTER          = 0x76,
	SPELL_RAGE_OF_THE_SKIES       = 0x77,
	SPELL_TERRA_WAVE              = 0x78,
	SPELL_ICE_WAVE                = 0x79,
	SPELL_DIVINE_MISSILE          = 0x7A,
	SPELL_WOUND_CLEANSING         = 0x7B,
	SPELL_DIVINE_CALDERA          = 0x7C,
	SPELL_DIVINE_HEALING          = 0x7D,
	SPELL_TRAIN_PARTY             = 0x7E,
	SPELL_PROTECT_PARTY           = 0x7F,
	SPELL_HEAL_PARTY              = 0x80,
	SPELL_ENCHANT_PARTY           = 0x81,
	SPELL_HOLY_MISSILE            = 0x82,
	SPELL_CHARGE                  = 0x83,
	SPELL_PROTECTOR               = 0x84,
	SPELL_BLOOD_RAGE              = 0x85,
	SPELL_SWIFT_FOOT              = 0x86,
	SPELL_SHARPSHOOTER            = 0x87,
	SPELL_UNKNOWN_27              = 0x88,
	SPELL_UNKNOWN_28              = 0x89,
	SPELL_IGNITE                  = 0x8A,
	SPELL_CURSE                   = 0x8B,
	SPELL_ELECTRIFY               = 0x8C,
	SPELL_INFLICT_WOUND           = 0x8D,
	SPELL_ENVENOM                 = 0x8E,
	SPELL_HOLY_FLASH              = 0x8F,
	SPELL_CURE_BLEEDING           = 0x90,
	SPELL_CURE_BURNING            = 0x91,
	SPELL_CURE_ELECTRIFICATION    = 0x92,
	SPELL_CURE_CURSE              = 0x93,
	SPELL_PHYSICAL_STRIKE         = 0x94,
	SPELL_LIGHTNING               = 0x95,
	SPELL_STRONG_FLAME_STRIKE     = 0x96,
	SPELL_STRONG_ENERGY_STRIKE    = 0x97,
	SPELL_STRONG_ICE_STRIKE       = 0x98,
	SPELL_STRONG_TERRA_STRIKE     = 0x99,
	SPELL_ULTIMATE_FLAME_STRIKE   = 0x9A,
	SPELL_ULTIMATE_ENERGY_STRIKE  = 0x9B,
	SPELL_ULTIMATE_ICE_STRIKE     = 0x9C,
	SPELL_ULTIMATE_TERRA_STRIKE   = 0x9D,
	SPELL_INTENSE_WOUND_CLEANSING = 0x9E,
	SPELL_RECOVERY                = 0x9F,
	SPELL_INTENSE_RECOVERY        = 0xA0
};

enum item_t
{
	ITEM_FIREFIELD		= 1492,
	ITEM_FIREFIELD_SAFE	= 1500,

	ITEM_POISONFIELD	= 1496,
	ITEM_POISONFIELD_SAFE	= 1503,

	ITEM_ENERGYFIELD	= 1495,
	ITEM_ENERGYFIELD_SAFE	= 1504,

	ITEM_MAGICWALL		= 1497,
	ITEM_MAGICWALL_SAFE	= 11098,

	ITEM_WILDGROWTH		= 1499,
	ITEM_WILDGROWTH_SAFE	= 11099,

	ITEM_DEPOT		= 2594,
	ITEM_LOCKER		= 2589,

	ITEM_MALE_CORPSE	= 3058,
	ITEM_FEMALE_CORPSE	= 3065,

	ITEM_FULLSPLASH		= 2016,
	ITEM_SMALLSPLASH	= 2019,

	ITEM_PARCEL		= 2595,
	ITEM_PARCEL_STAMPED	= 2596,
	ITEM_LETTER		= 2597,
	ITEM_LETTER_STAMPED	= 2598,
	ITEM_LABEL		= 2599,

	ITEM_WATERBALL_SPLASH	= 7711,
	ITEM_WATERBALL		= 7956,

	ITEM_STEALTH_RING = 2202,
	ITEM_HOUSE_TRANSFER	= 1968 //read-only
};

enum PlayerFlags
{
	PlayerFlag_CannotUseCombat = 0,			//2^0 = 1
	PlayerFlag_CannotAttackPlayer,			//2^1 = 2
	PlayerFlag_CannotAttackMonster,			//2^2 = 4
	PlayerFlag_CannotBeAttacked,			//2^3 = 8
	PlayerFlag_CanConvinceAll,				//2^4 = 16
	PlayerFlag_CanSummonAll,				//2^5 = 32
	PlayerFlag_CanIllusionAll,				//2^6 = 64
	PlayerFlag_CanSenseInvisibility,		//2^7 = 128
	PlayerFlag_IgnoredByMonsters,			//2^8 = 256
	PlayerFlag_NotGainInFight,				//2^9 = 512
	PlayerFlag_HasInfiniteMana,				//2^10 = 1024
	PlayerFlag_HasInfiniteSoul,				//2^11 = 2048
	PlayerFlag_HasNoExhaustion,				//2^12 = 4096
	PlayerFlag_CannotUseSpells,				//2^13 = 8192
	PlayerFlag_CannotPickupItem,			//2^14 = 16384
	PlayerFlag_CanAlwaysLogin,				//2^15 = 32768
	PlayerFlag_CanBroadcast,				//2^16 = 65536
	PlayerFlag_CanEditHouses,				//2^17 = 131072
	PlayerFlag_CannotBeBanned,				//2^18 = 262144
	PlayerFlag_CannotBePushed,				//2^19 = 524288
	PlayerFlag_HasInfiniteCapacity,			//2^20 = 1048576
	PlayerFlag_CanPushAllCreatures,			//2^21 = 2097152
	PlayerFlag_CanTalkRedPrivate,			//2^22 = 4194304
	PlayerFlag_CanTalkRedChannel,			//2^23 = 8388608
	PlayerFlag_TalkOrangeHelpChannel,		//2^24 = 16777216
	PlayerFlag_NotGainExperience,			//2^25 = 33554432
	PlayerFlag_NotGainMana,					//2^26 = 67108864
	PlayerFlag_NotGainHealth,				//2^27 = 134217728
	PlayerFlag_NotGainSkill,				//2^28 = 268435456
	PlayerFlag_SetMaxSpeed,					//2^29 = 536870912
	PlayerFlag_SpecialVIP,					//2^30 = 1073741824
	PlayerFlag_NotGenerateLoot,				//2^31 = 2147483648
	PlayerFlag_CanTalkRedChannelAnonymous,	//2^32 = 4294967296
	PlayerFlag_IgnoreProtectionZone,		//2^33 = 8589934592
	PlayerFlag_IgnoreSpellCheck,			//2^34 = 17179869184
	PlayerFlag_IgnoreEquipCheck,			//2^35 = 34359738368
	PlayerFlag_CannotBeMuted,				//2^36 = 68719476736
	PlayerFlag_IsAlwaysPremium,				//2^37 = 137438953472
	PlayerFlag_38,							//2^38 = 274877906944
	PlayerFlag_39,							//2^39 = 549755813888 //not used by us
	PlayerFlag_ShowGroupNameInsteadOfVocation,	//2^40 = 1099511627776
	PlayerFlag_HasInfiniteStamina,			//2^41 = 2199023255552
	PlayerFlag_CannotMoveItems,				//2^42 = 4398046511104
	PlayerFlag_CannotMoveCreatures,			//2^43 = 8796093022208
	PlayerFlag_CanReportBugs,				//2^44 = 17592186044416
	PlayerFlag_45,							//2^45 = 35184372088832 //not used by us
	PlayerFlag_CannotBeSeen,				//2^46 = 70368744177664
	PlayerFlag_HideHealth,					//2^47 = 140737488355328
	PlayerFlag_CanPassThroughAllCreatures,	//2^48 = 281474976710656

	PlayerFlag_LastFlag
};

enum PlayerCustomFlags
{
	PlayerCustomFlag_AllowIdle = 0,				//2^0 = 1
	PlayerCustomFlag_CanSeePosition,			//2^1 = 2
	PlayerCustomFlag_CanSeeItemDetails,			//2^2 = 4
	PlayerCustomFlag_CanSeeCreatureDetails,			//2^3 = 8
	PlayerCustomFlag_NotSearchable,				//2^4 = 16
	PlayerCustomFlag_GamemasterPrivileges,			//2^5 = 32
	PlayerCustomFlag_CanThrowAnywhere,			//2^6 = 64
	PlayerCustomFlag_CanPushAllItems,			//2^7 = 128
	PlayerCustomFlag_CanMoveAnywhere,			//2^8 = 256
	PlayerCustomFlag_CanMoveFromFar,			//2^9 = 512
	PlayerCustomFlag_CanLoginMultipleCharacters,		//2^10 = 1024 (account flag)
	PlayerCustomFlag_HasFullLight,				//2^11 = 2048
	PlayerCustomFlag_CanLogoutAnytime,			//2^12 = 4096 (account flag)
	PlayerCustomFlag_HideLevel,				//2^13 = 8192
	PlayerCustomFlag_IsProtected,				//2^14 = 16384
	PlayerCustomFlag_IsImmune,				//2^15 = 32768
	PlayerCustomFlag_NotGainSkull,				//2^16 = 65536
	PlayerCustomFlag_NotGainUnjustified,			//2^17 = 131072
	PlayerCustomFlag_IgnorePacification,			//2^18 = 262144
	PlayerCustomFlag_IgnoreLoginDelay,			//2^19 = 524288
	PlayerCustomFlag_CanStairhop,				//2^20 = 1048576
	PlayerCustomFlag_CanTurnhop,				//2^21 = 2097152
	PlayerCustomFlag_IgnoreHouseRent,			//2^22 = 4194304
	PlayerCustomFlag_CanWearAllAddons,			//2^23 = 8388608
	PlayerCustomFlag_IsWalkable,				//2^24 = 16777216
	PlayerCustomFlag_CanUseAllMounts,			//2^25 = 33554432

	PlayerCustomFlag_LastFlag
};

//Reserved player storage key ranges
//[10000000 - 20000000]
#define PSTRG_RESERVED_RANGE_START	10000000
#define PSTRG_RESERVED_RANGE_SIZE	10000000

//[1000 - 1500]
#define PSTRG_OUTFITS_RANGE_START	(PSTRG_RESERVED_RANGE_START + 1000)
#define PSTRG_OUTFITS_RANGE_SIZE	500

//[1500 - 2000]
#define PSTRG_OUTFITSID_RANGE_START	(PSTRG_RESERVED_RANGE_START + 1500)
#define PSTRG_OUTFITSID_RANGE_SIZE	500

//[2000 - 2010]
#define PSTRG_MOUNTS_RANGE_START   (PSTRG_RESERVED_RANGE_START + 2000)
#define PSTRG_MOUNTS_RANGE_SIZE    10
#define PSTRG_MOUNTS_CURRENTMOUNT (PSTRG_MOUNTS_RANGE_START + PSTRG_MOUNTS_RANGE_SIZE)

#define NETWORK_CRYPTOHEADER_SIZE 8
#define NETWORK_RETRY_TIMEOUT 5000
#define NETWORK_DEFAULT_SIZE 4096
#define NETWORK_HEADER_SIZE 2
#define NETWORK_MAX_SIZE 15360

#define IPBAN_FLAG 128
#define LOCALHOST 2130706433

#define IS_IN_KEYRANGE(key, range) (key >= PSTRG_##range##_START && ((key - PSTRG_##range##_START) < PSTRG_##range##_SIZE))
#endif
