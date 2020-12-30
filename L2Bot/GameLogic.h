#pragma once

#include "IPCSocket.h"
#include "Protocol.h"

#include <Windows.h>
#include <qobject.h>
#include <stdint.h>
#include <map>
#include <set>

enum EntityType {
	ET_UNK = 0,
	ET_NPC = 1,
	ET_PLAYER = 2,
	ET_ITEM = 3,
};


enum WaitType {
	P_SIT = 0,
	P_STAND = 1,
	P_CAST = 2,
	P_ATTACK = 3,
};


struct Position
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	int32_t heading = 0;
};


struct Status
{
	uint8_t type = 0;
	uint32_t value = 0;
	Status(uint8_t type, uint32_t value) : type(type), value(value) {}

	enum StatusType
	{
		LEVEL = 0x01,
		EXP = 0x02,
		STR = 0x03,
		DEX = 0x04,
		CON = 0x05,
		INT = 0x06,
		WIT = 0x07,
		MEN = 0x08,

		CUR_HP = 0x09,
		MAX_HP = 0x0A,
		CUR_MP = 0x0B,
		MAX_MP = 0x0C,

		P_ATK = 0x11,
		ATK_SPD = 0x12,
		P_DEF = 0x13,
		EVASION = 0x14,
		ACCURACY = 0x15,
		CRITACAL = 0x16,
		M_ATK = 0x17,
		CAST_SPD = 0x18,
		M_DEF = 0x19,
		PVP_FLAG = 0x1A,
		REPUTATION = 0x1B,

		CUR_CP = 0x21,
		MAX_CP = 0x22

	};
};


struct Skill {
	uint32_t ID;
	uint16_t Level;
};

class GameEntity
{
protected:
	uint32_t ID = 0;
	EntityType _TYPE = EntityType::ET_UNK;

	Position pos = { 0 };

public:
	GameEntity(uint32_t ID);
	GameEntity(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ);

	bool isNPC() { return _TYPE == ET_NPC; }
	bool isPlayer() { return _TYPE == ET_PLAYER; }
	bool isItem() { return _TYPE == ET_ITEM; }
	bool isAliveEntity() { return _TYPE == ET_PLAYER || _TYPE == ET_NPC; }
	uint32_t GetID() { return ID; }
	Position GetPos() { return pos; }
	double GetDistance(const Position& targetPos);

	void SetLocation(uint32_t x, uint32_t y, uint32_t z);
	void SetHeading(uint32_t heading) { pos.heading = heading; }
	void Move(uint32_t x, uint32_t y, uint32_t z);

	// Used to specify no entity
	static const uint32_t ENTITY_NONE = -1;
};

class Item : public GameEntity
{
public:
	Item(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t itemID, uint64_t count, uint32_t dropper);
	uint32_t GetDropper() { return dropper; }

protected:
	uint32_t ItemID = 0;
	uint32_t amount = 0;
	uint32_t dropper = 0;
};

class AliveEntity : public GameEntity
{
protected:
	bool dead = false;
	bool attackable = true;

	uint32_t hpCur = 0;
	uint32_t hpMax = 0;
	uint32_t mpCur = 0;
	uint32_t mpMax = 0;
	uint32_t cpCur = 0;
	uint32_t cpMax = 0;

	uint16_t level = 0;
	uint32_t state = P_STAND;

	AliveEntity* target = nullptr;

public:
	AliveEntity(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ);

	// Setters
	void SetHP(uint32_t val) { hpCur = val; }
	void SetHPMax(uint32_t val) { hpMax = val; }
	void SetMP(uint32_t val) { mpCur = val; }
	void SetMPMax(uint32_t val) { mpMax = val; }
	void SetCP(uint32_t val) { cpCur = val; }
	void SetCPMax(uint32_t val) { cpMax = val; }
	void SetState(uint32_t state) { this->state = state; }
	void SetTarget(AliveEntity* target);
	void SetLevel(uint16_t level) { this->level = level; }

	// Getters
	uint32_t GetHPCur() { return hpCur; }
	uint32_t GetHPMax() { return hpMax; }
	uint32_t GetHPPercent() { return hpCur ? hpCur * 100 / hpMax : 0; }

	uint32_t GetMPCur() { return mpCur; }
	uint32_t GetMPMax() { return mpMax; }
	uint32_t GetMPPercent() { return mpCur ? mpCur * 100 / mpMax : 0; }

	bool IsSitting() { return state == P_SIT; }
	bool IsStanding() { return state != P_SIT; }
	AliveEntity* GetTarget() { return target; }
	uint16_t GetLevel() { return level; }

	bool IsAttackable() { return attackable; }
	bool IsDead() { return dead; }
	void SetAttackable(bool attackable) { this->attackable = attackable; }
	void Kill();
};


class NPC : public AliveEntity
{
private:
	uint32_t NpcID = 0;

public:
	NPC(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t NpcID);
	void SetHP(uint32_t HPMax, uint32_t HPCur);

	uint32_t GetNPCID() { return NpcID; }
};


class Player : public AliveEntity
{
private:
	WCHAR name[32];

public:
	Player(uint32_t ID, WCHAR* name, uint32_t posX, uint32_t posY, uint32_t posZ);
	WCHAR* GetName() { return name; }
};


class MainPlayer : public Player
{
public:
	MainPlayer(uint32_t ID, WCHAR* name, uint32_t posX, uint32_t posY, uint32_t posZ);
	void ValidateLocation(Position pos);
	void GoTo(Position pos);
	void TargetEntity(GameEntity* entity, bool waitForTarget=false);
	void PickUp(Item* item);
	void SetSocket(IPCSocket* socket);
	void SitOrStand();
	void SetID(uint32_t ID) { this->ID = ID; }
	void UseSkill(uint32_t ID);

private:
	IPCSocket* socket = nullptr;
	const ClientPacketProtocol* protocol;
	uint8_t level = 0;

};


class GameLogic : public QObject
{
	Q_OBJECT

public:
	typedef std::map<uint32_t, GameEntity*> entity_map_t;

	// Game Logic related packet events
	void SetLocation(uint32_t EntityID, uint32_t DestX, uint32_t DestY, uint32_t DestZ);
	void MoveToLocation(uint32_t EntityID, uint32_t DestX, uint32_t DestY, uint32_t DestZ, uint32_t FromX, uint32_t FromY, uint32_t FromZ);
	void SetMainPlayer(WCHAR* charName, uint32_t posX, uint32_t posY, uint32_t posZ);
	void SetMainPlayerContext(uint32_t EntityID);
	void AddPlayer(uint32_t EntityID, WCHAR* charName, uint32_t posX, uint32_t posY, uint32_t posZ);
	NPC* AddNPC(uint32_t ObjID, uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t HPMax, uint32_t HPCur, uint32_t npcID);
	void DeleteObject(uint32_t ObjID);
	void KillEntity(uint32_t ObjID);
	void DropItem(uint32_t objID, uint32_t X, uint32_t Y, uint32_t Z, uint32_t itemDisplayID, uint64_t count, uint32_t dropper);
	void UpdateStatus(uint32_t objID, std::vector<Status> status);
	void SetWaitType(uint32_t ObjID, uint32_t WaitType);
	void Attack(uint32_t attacker, uint32_t target, uint32_t damage, uint32_t flags);
	void SetTarget(uint32_t obj, uint32_t target);
	void AddSkill(uint32_t ID, uint16_t level, uint32_t passive, uint8_t disabled);
	void SkillCast(uint32_t caster, uint32_t target);
	void SkillCanceled(uint32_t objid);
	void SkillLaunched(uint32_t caster);
	void Restart();
	void Cleanup();

	// Utils
	void SetSocket(IPCSocket* socket) { this->socket = socket; }
	void Reset();
	const entity_map_t& GetEntities();
	const std::set<uint32_t>& GetEntitiesTargetingPlayer();
	GameEntity* GetEntity(uint32_t id);
	MainPlayer* GetPlayer();
	uint8_t GetState() { return state; }
	inline void remove_target(uint32_t target) {
		auto it = std::find(entitiesTargetingPlayer.begin(), entitiesTargetingPlayer.end(), target);
		if (it != entitiesTargetingPlayer.end()) {
			entitiesTargetingPlayer.erase(it);
		}
	}

	// Bot logic related getter
	Player* GetPlayerByName(WCHAR* name);
	AliveEntity* GetNearestAttackableEntity(GameEntity* entity, std::vector<uint32_t> blacklist);
	std::vector<Skill> GetSkills() { return skills; };
	std::vector<Item*> GameLogic::GetGroundItems();
	std::vector<Item*> GetNearestPickableItems(std::set<uint32_t> killedTargets, std::vector<uint32_t> itemBlacklist);

	enum GameState {
		NOT_CONNECTED = 0,
		CHAR_SELECTED = 1,
		IN_GAME = 2,
	};

signals:
	void gameInitialized();
	void sendMessage(QString);
	void npcAdded(NPC* npc);
	void npcRemoved(NPC* npc);
	void environmentChanged();
	void playerStatusChanged();
	void playerTargetChanged();
	void playerTargetStatusChanged();
	void extraPlayerStatusChanged(quint32 entity);
	void playerDisconnected();
	void skillCast(quint32 caster, quint32 target);
	void skillCanceled(quint32 objid);
	void skillLaunched(quint32 caster);
	void actionFailed();
	void entityMoveToPawn(quint32 source, quint32 target);
	void autoAttackStart(quint32 target);
	void entityDies(quint32 entity);
	void somethingTargetedSomething(quint32 source, quint32 target);
	void systemMessage(quint16 msgID);
	void attack(quint32 attacker, quint32 target, quint32 damage, quint32 flags);
	void getItem(quint32 PlayerID, quint32 ItemID);

private:
	IPCSocket* socket = nullptr;
	entity_map_t entities;
	std::vector<Skill> skills;
	MainPlayer* player = nullptr;
	uint8_t state = NOT_CONNECTED;
	std::set<uint32_t> entitiesTargetingPlayer;
};