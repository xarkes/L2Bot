#include "GameLogic.h"
#include "BotWindow.h"
#include "Glob.h"

/******************************************
   Game Entities methods
   *****************************************/

/// GameEntity

GameEntity::GameEntity(uint32_t ID) : ID(ID)
{
}

GameEntity::GameEntity(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ) 
	: ID(ID)
{
	SetLocation(posX, posY, posZ);
}

static double ComputeDistance(Position p1, Position p2)
{
	auto dx = p1.x - p2.x;
	auto dy = p1.y - p2.y;
	auto dx2 = dx * dx;
	auto dy2 = dy * dy;
	auto res = sqrt(dx2 + dy2);
	return res;
}

double GameEntity::GetDistance(const Position& targetPos)
{
	return ComputeDistance(this->pos, targetPos);
}

void GameEntity::SetLocation(uint32_t posX, uint32_t posY, uint32_t posZ)
{
	pos.x = posX;
	pos.y = posY;
	pos.z = posZ;
}

void GameEntity::Move(uint32_t x, uint32_t y, uint32_t z)
{
	// TODO
}

/// ITEM

Item::Item(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t itemID, uint64_t count, uint32_t dropper)
	: GameEntity(ID, posX, posY, posZ)
{
	this->_TYPE = EntityType::ET_ITEM;
	this->ItemID = itemID;
	this->amount = count;
	this->dropper = dropper;
}

/// ALIVE ENTITY

AliveEntity::AliveEntity(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ)
	: GameEntity(ID, posX, posY, posZ)
{
	SetLocation(posX, posY, posZ);
}

void AliveEntity::SetTarget(AliveEntity* target)
{
	this->target = target;
}

void AliveEntity::Kill()
{
	hpCur = 0;
	dead = true;
}

/// NPC

NPC::NPC(uint32_t ID, uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t NpcID) :
	AliveEntity(ID, posX, posY, posZ), NpcID(NpcID)
{
	this->_TYPE = EntityType::ET_NPC;
}

void NPC::SetHP(uint32_t HPMax, uint32_t HPCur)
{
	this->hpMax = HPMax;
	this->hpCur = HPCur;
}

/// Player

Player::Player(uint32_t ID, WCHAR* name, uint32_t posX, uint32_t posY, uint32_t posZ) : AliveEntity(ID, posX, posY, posZ)
{
	this->_TYPE = EntityType::ET_PLAYER;
	wcscpy_s(this->name, name);
}


/******************************************
   Game Logic methods (receiving packets) 
   *****************************************/

// Instant move
void GameLogic::SetLocation(uint32_t EntityID, uint32_t X, uint32_t Y, uint32_t Z)
{
	if (!entities[EntityID]) {
		emit sendMessage(QString("Entity %1 does not exist!").arg(EntityID, 4, 16));
		return;
	}
	entities[EntityID]->SetLocation(X, Y, Z);

	// Tell the UI the environment has changed
	emit environmentChanged();
}

// Animated move (move query)
void GameLogic::MoveToLocation(uint32_t EntityID, uint32_t DestX, uint32_t DestY, uint32_t DestZ, uint32_t FromX, uint32_t FromY, uint32_t FromZ)
{
	if (state != IN_GAME) {
		return;
	}
	if (!entities[EntityID]) {
		emit sendMessage(QString("Entity %1 does not exist!").arg(EntityID, 4, 16));
		return;
	}
	entities[EntityID]->SetLocation(FromX, FromY, FromZ);
	if (DestX != DestY != DestZ != 0) {
		entities[EntityID]->Move(DestX, DestY, DestZ);
	}

	// Tell the UI the environment has changed
	emit environmentChanged();
}

void GameLogic::SetMainPlayer(WCHAR* charName, uint32_t posX, uint32_t posY, uint32_t posZ)
{
	player = new MainPlayer(0, charName, posX, posY, posZ);
	player->SetSocket(socket);
	state = CHAR_SELECTED;
}

void GameLogic::SetMainPlayerContext(uint32_t EntityID)
{
	if (state != CHAR_SELECTED) {
		return;
	}
	player->SetID(EntityID);
	entities[EntityID] = player;
	state = IN_GAME;
	emit gameInitialized();
}

void GameLogic::AddPlayer(uint32_t EntityID, WCHAR* charName, uint32_t posX, uint32_t posY, uint32_t posZ)
{
	assert(entities[EntityID] == nullptr);
	Player* player = new Player(EntityID, charName, posX, posY, posZ);
	entities[EntityID] = player;
}

NPC* GameLogic::AddNPC(uint32_t ObjID, uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t HPMax, uint32_t HPCur, uint32_t npcID)
{
	assert(entities[ObjID] == nullptr);
	NPC* npc = new NPC(ObjID, posX, posY, posZ, npcID);
	npc->SetHP(HPMax, HPCur);
	entities[ObjID] = npc;

	emit npcAdded(npc);

	return npc;
}

void GameLogic::DeleteObject(uint32_t ObjID)
{
	auto ent = entities[ObjID];
	if (ent) {
		// Remove from NPC list
		if (ent->isNPC()) {
			emit npcRemoved(static_cast<NPC*>(ent));
		}
		// Remove from entities list
		entities.erase(ObjID);
		delete ent;
	}
}

void GameLogic::KillEntity(uint32_t ObjID)
{
	auto ent = entities[ObjID];
	if (!ent || !ent->isAliveEntity()) {
		return;
	}

	auto E = static_cast<AliveEntity*>(ent);
	if (E == player) {
		// XXX: Might be wrong
		entitiesTargetingPlayer.clear();
	}
	else {
		// Remove target from entities targeting player
		remove_target(E->GetID());
	}
	E->Kill();
	
	// Emit signals
	auto target = player->GetTarget();
	if (target == ent) {
		// TODO: Isn't UpdateStatus doing that already?
		emit playerTargetStatusChanged();
	}
	emit entityDies(ObjID);
}

void GameLogic::DropItem(uint32_t ObjID, uint32_t X, uint32_t Y, uint32_t Z, uint32_t itemDisplayID, uint64_t count, uint32_t dropper)
{
	auto item = new Item(ObjID, X, Y, Z, itemDisplayID, count, dropper);
	entities[ObjID] = item;
}

void GameLogic::UpdateStatus(uint32_t objID, std::vector<Status> status)
{
	auto E = entities[objID];
	if (!E) {
		return;
	}

	if (!E->isNPC() && !E->isPlayer()) {
		return;
	}

	auto AE = static_cast<AliveEntity*>(E);

	for (auto& s : status)
	{
		switch (s.type) {
		case Status::StatusType::CUR_HP:
			AE->SetHP(s.value);
			break;
		case Status::StatusType::MAX_HP:
			AE->SetHPMax(s.value);
			break;
		case Status::StatusType::CUR_MP:
			AE->SetMP(s.value);
			break;
		case Status::StatusType::MAX_MP:
			AE->SetMPMax(s.value);
			break;
		case Status::StatusType::CUR_CP:
			AE->SetCP(s.value);
			break;
		case Status::StatusType::MAX_CP:
			AE->SetCPMax(s.value);
			break;
		case Status::StatusType::LEVEL:
			AE->SetLevel(s.value);
			break;
		}
	}

	if (AE == player) {
		emit playerStatusChanged();
	}
	else if (AE == player->GetTarget()) {
		emit playerTargetStatusChanged();
	}
	else if (AE->isPlayer()) {
		emit extraPlayerStatusChanged(AE->GetID());
	}
}

void GameLogic::SetWaitType(uint32_t ObjID, uint32_t WaitType)
{
	if (!entities[ObjID] || !entities[ObjID]->isAliveEntity()) {
		return;
	}
	AliveEntity* AE = static_cast<AliveEntity*>(entities[ObjID]);
	AE->SetState(WaitType);
}

void GameLogic::Attack(uint32_t attacker, uint32_t target, uint32_t damage, uint32_t flags)
{
	if (entities[attacker] && entities[target]) {
		auto AETarget = static_cast<AliveEntity*>(entities[target]);
		AETarget->SetHP(AETarget->GetHPCur() - damage);
	}
	if (entities[target] == player) {
		emit playerStatusChanged();
	}
	emit attack(attacker, target, damage, flags);
}

void GameLogic::SetTarget(uint32_t obj, uint32_t target)
{
	if (!entities[obj] || !entities[obj]->isAliveEntity()) {
		return;
	}
	auto aeSource = static_cast<AliveEntity*>(entities[obj]);

	// Check if we want to untarget
	if (target == GameEntity::ENTITY_NONE) {
		aeSource->SetTarget(nullptr);
		if (aeSource == player) {
			emit playerTargetChanged();
		}
		// Check if we should update entitiesTargetingPlayer
		remove_target(obj);
		return;
	}

	// Handle the targeting
	if (!entities[target]) {
		return;
	}
	auto aeTarget = static_cast<AliveEntity*>(entities[target]);
	aeSource->SetTarget(aeTarget);

	// Check if we should update entitiesTargetingPlayer
	if (target == player->GetID()) {
		entitiesTargetingPlayer.insert(obj);
	}

	// Emit signals if required
	if (aeSource == player) {
		emit playerTargetChanged();
	} else {
		emit somethingTargetedSomething(obj, target);
	}
}

void GameLogic::AddSkill(uint32_t ID, uint16_t level, uint32_t passive, uint8_t disabled)
{
	ASSERT(state == GameState::IN_GAME);
	ASSERT(player);

	if (!disabled && !passive) {
		skills.push_back(Skill{ ID, level });
	}
}

void GameLogic::SkillCast(uint32_t caster, uint32_t target)
{
	auto E = entities[caster];
	if (!E) {
		return;
	}
	auto AE = static_cast<AliveEntity*>(E);
	AE->SetState(P_CAST);
	emit skillCast(caster, target);
}

void GameLogic::SkillCanceled(uint32_t objid)
{
	auto E = entities[objid];
	if (!E) {
		return;
	}
	auto AE = static_cast<AliveEntity*>(E);
	AE->SetState(P_STAND);
	emit skillCanceled(objid);
}

void GameLogic::SkillLaunched(uint32_t caster)
{
	auto E = entities[caster];
	if (!E) {
		return;
	}
	auto AE = static_cast<AliveEntity*>(E);
	AE->SetState(P_STAND);
	emit skillLaunched(caster);
}

void GameLogic::Restart()
{
	// First, stop the bot if it was running and refresh the UI
	state = NOT_CONNECTED;
	emit playerDisconnected();
}

void GameLogic::Cleanup()
{
	delete player;
	player = nullptr;
	entities.clear();
	entitiesTargetingPlayer.clear();
	skills.clear();
}

const GameLogic::entity_map_t& GameLogic::GetEntities()
{
	return entities;
}

const std::set<uint32_t>& GameLogic::GetEntitiesTargetingPlayer()
{
	return entitiesTargetingPlayer;
}

GameEntity* GameLogic::GetEntity(uint32_t id)
{
	return entities[id];
}

MainPlayer* GameLogic::GetPlayer()
{
	return player;
}

void GameLogic::Reset()
{
	if (player) {
		delete player;
	}
	player = nullptr;
	entities.clear();
	state = NOT_CONNECTED;
}

Player* GameLogic::GetPlayerByName(WCHAR* name)
{
	for (auto& ent : entities) {
		if (!ent.second || !ent.second->isPlayer()) {
			continue;
		}
		Player* player = static_cast<Player*>(ent.second);
		if (!lstrcmpiW(player->GetName(), name)) {
			return player;
		}
	}

	return nullptr;
}

AliveEntity* GameLogic::GetNearestAttackableEntity(GameEntity* entity, std::vector<uint32_t> blacklist)
{
	if (!entity || !entities[entity->GetID()]) {
		return nullptr;
	}

	double minDistance = DBL_MAX;
	NPC* npcEntity = nullptr;
	for (auto& ent : entities) {
		if (!ent.second || !ent.second->isNPC() || ent.second == player) {
			continue;
		}
		// TODO: Use a better formula to check that the monster is not in a tree
		if (abs(ent.second->GetPos().z - entity->GetPos().z) >= 250) {
			continue;
		}
		auto npc = static_cast<NPC*>(ent.second);
		if (!npc->IsAttackable() || npc->GetHPCur() <= 0 || npc->IsDead()) {
			// Our target is not attackable (e.g. NPC Guard) or our target is already dead
			continue;
		}
		if (std::find(blacklist.begin(), blacklist.end(), npc->GetID()) != blacklist.end()) {
			// Our target is in the blacklist, continue
			continue;
		}

		auto dist = player->GetDistance(npc->GetPos());
		if (dist < minDistance) {
			minDistance = dist;
			npcEntity = npc;
		}
	}

	return npcEntity;
}

std::vector<Item*> GameLogic::GetGroundItems()
{
	std::vector<Item*> items;
	for (auto entity : entities) {
		if (!entity.second || !entity.second->isItem()) {
			continue;
		}
		auto I = static_cast<Item*>(entity.second);
		items.push_back(I);
	}
	return items;
}

std::vector<Item*> GameLogic::GetNearestPickableItems(std::set<uint32_t> killedTargets, std::vector<uint32_t> itemBlacklist)
{
	std::vector<Item*> items = GetGroundItems();
	int i = 0;
	for (auto item : items) {
		auto dropper = item->GetDropper();
		if (std::find(itemBlacklist.begin(), itemBlacklist.end(), item->GetID()) != itemBlacklist.end() ||
			killedTargets.find(dropper) == killedTargets.end()) {
			// The dropper is not a target that we killed...
			// Or the item is blacklisted
			// Do not pick up that item
			items[i] = items.back();
			items.pop_back();
		}
	}

	// Now sort it, nearest first
	std::sort(items.begin(), items.end(), [this](Item* A, Item* B) {
		return player->GetDistance(A->GetPos()) < player->GetDistance(B->GetPos());
	});

	return items;
}