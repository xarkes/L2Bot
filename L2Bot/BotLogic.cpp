#include "BotLogic.h"
#include "Glob.h"

BotLogic::BotLogic(QObject *parent, GameLogic* GL)
	: QThread(parent), GL(GL)
{
}

BotLogic::~BotLogic()
{
	if (botRunning) {
		Stop();
	}
}

void BotLogic::CleanUp()
{
	registeredActions.clear();
	registeredSkills.clear();
}

void BotLogic::Stop()
{
	// Disconnect signals
	for (int i = 0; i < NCON; i++) {
		disconnect(con[i]);
	}

	// Stop the bot
	botRunning = false;
	// SS(BotState::NONE);
}

void BotLogic::Start()
{
	// Set values
	if (!player) {
		return;
	}
	botRunning = true;
	targetBlacklist.clear();
	itemBlacklist.clear();
	lockedTarget = 0;
	SS(BotState::START);

	// Connect signals
	con[0x00] = connect(GL, &GameLogic::playerTargetChanged, this, &BotLogic::playerTargetChanged);
	con[0x01] = connect(GL, &GameLogic::actionFailed, this, &BotLogic::actionFailed);
	con[0x02] = connect(GL, &GameLogic::skillCast, this, &BotLogic::skillCast);
	con[0x03] = connect(GL, &GameLogic::entityMoveToPawn, this, &BotLogic::entityMoveToPawn);
	con[0x04] = connect(GL, &GameLogic::autoAttackStart, this, &BotLogic::autoAttackStart);
	con[0x05] = connect(GL, &GameLogic::entityDies, this, &BotLogic::entityDies);
	con[0x06] = connect(GL, &GameLogic::somethingTargetedSomething, this, &BotLogic::somethingTargetedSomething);
	con[0x07] = connect(GL, &GameLogic::systemMessage, this, &BotLogic::systemMessageReceived);
	con[0x08] = connect(GL, &GameLogic::getItem, this, &BotLogic::getItem);
	con[0x09] = connect(GL, &GameLogic::skillCanceled, this, &BotLogic::skillCanceled);
	con[0x0a] = connect(GL, &GameLogic::skillLaunched, this, &BotLogic::skillLaunched);
	con[0x0b] = connect(GL, &GameLogic::attack, this, &BotLogic::attack);
	con[0x0c] = connect(GL, &GameLogic::extraPlayerStatusChanged, this, &BotLogic::extraPlayerStatusChanged);

	// Start the thread
	this->start();
}

/** EVENTS **/

void BotLogic::playerTargetChanged()
{
	if (state == BotState::TARGET) {
		auto target = player->GetTarget();
		if (target && target->GetID() == lockedTarget) {
			SS(BotState::TARGETED);
			return;
		}
		else {
			SS(BotState::START);
			return;
		}
	}

	else if (state == BotState::ENGAGE) {
		SS(BotState::START);
		return;
	}
}

void BotLogic::actionFailed()
{
}

void BotLogic::skillCast(quint32 caster, quint32 target)
{
	if (state == BotState::TARGETED) {
		auto TE = player->GetTarget();
		if (caster == player->GetID() && target && TE->GetID() == target) {
			SS(BotState::ENGAGE);
		}
	}
}

void BotLogic::skillCanceled(quint32 objid)
{
	if (objid == player->GetID()) {
		if (state == BotState::ENGAGE) {
			trycount = FIRST_TRY;
			return;
		}
	}
}

void BotLogic::skillLaunched(quint32 caster)
{
	if (caster == player->GetID()) {
		if (state == BotState::ENGAGE) {
			SS(BotState::ENGAGED);
			return;
		}
	}
}

void BotLogic::entityMoveToPawn(quint32 caster, quint32 target)
{
}

void BotLogic::autoAttackStart(quint32 target)
{
	/* Removed as it may overlap with attack()
	if (state == BotState::ENGAGE) {
		SS(BotState::ENGAGED);
		return;
	}*/
}

void BotLogic::entityDies(quint32 entity)
{
	if (state == BotState::ENGAGED || state == BotState::ENGAGE) {
		ASSERT(lockedTarget);
		// It is possible to one shot our target (from state ENGAGE)
		if (lockedTarget == entity) {
			SS(BotState::TARGET_DEAD);
			return;
		}
	}

	if (entity == player->GetID()) {
		// Our player died, stop botting
		Stop();
	}
}

void BotLogic::somethingTargetedSomething(quint32 source, quint32 target)
{
}

void BotLogic::systemMessageReceived(quint16 msgID)
{
	if (msgID == 0x006d) {
		// Invalid target
		if (state == BotState::ENGAGE) {
			BlackListAndSwitchTarget();
			return;
		}
	}
	else if (msgID == 0x02ec) {
		// Distance is too far to cast the skill
		if (state == BotState::ENGAGE) {
			BlackListAndSwitchTarget();
			return;
		}
	}
	else if (msgID == 0x00b5) {
		// Cannot see target
		if (state == BotState::ENGAGE) {
			BlackListAndSwitchTarget();
			return;
		}
	}
	else if (msgID == 0x0165 || msgID == 0x0264) {
		// Target already spoiled | Spoil conditions activated
		spoiledTargets.insert(lockedTarget);
	}
}

void BotLogic::attack(quint32 attacker, quint32 target, quint32 damage, quint32 flags)
{
	if (state == BotState::ENGAGE) {
		ASSERT(lockedTarget);
		if (attacker == player->GetID() && target == lockedTarget) {
			SS(BotState::ENGAGED);
		}
	}
}

bool BotLogic::ActionConditionValid(Condition cond)
{
	return true;
}

void BotLogic::extraPlayerStatusChanged(quint32 PlayerID)
{
	// TODO: Handle a whole party...
	Player* eventPlayer = static_cast<Player*>(GL->GetEntity(PlayerID));
	Player* assistPlayer = GL->GetPlayerByName(targetingOptions.assistPlayerName);
	if (eventPlayer != assistPlayer) {
		return;
	}

	// We already have an assistAction, so we must complete it first, ignore this
	if (assistAction) {
		return;
	}

	// So, here the status of a player we must assist changed.
	// So in any case, we will have to help them
	// TODO: Handle priority...
	for (auto& action : partyActions) {
		if (!ActionConditionValid(action.condition)) {
			continue;
		}

		lockedTarget = 0;
		lockedAssist = PlayerID;
		assistAction = &action;
		SS(BotState::TARGET);
		return;
	}
}

void BotLogic::getItem(quint32 PlayerID, quint32 ItemID)
{
	if (state == BotState::PICKING) {
		ASSERT(lockedItem);

		if (lockedItem == ItemID) {
			if (PlayerID == player->GetID()) {
				// We were picking up an item and we just picked it up
				SS(BotState::PICKUP);
			}
			else {
				// We were picking up an item but someone was faster than us
				SS(BotState::PICKUP);
			}
		}
	}
}

/** BOT STATES **/

void BotLogic::BotStart()
{
	// ETP > 0?
	if (AttackEntityTargetingUs()) {
		return;
	}

	// Should we rest?
	if ((restOptions.HPRest && player->GetHPPercent() <= restOptions.HPRestMin) ||
	    (restOptions.MPRest && player->GetMPPercent() <= restOptions.MPRestMin)) {
		SS(BotState::REST);
		return;
	}

	// Clear targeting blacklist
	targetBlacklist.clear();

	// End state
	SS(BotState::CHOOSING);
}

void BotLogic::BotChoosing()
{
	// ETP > 0?
	if (AttackEntityTargetingUs()) {
		return;
	}
	lockedTarget = 0;

	// 1. We target anything, or something in an area, so just pick the nearest attackable entity
	if (targetingOptions.targetingType == TargetingType::ANYTHING || targetingOptions.targetingType == TargetingType::CENTER) {
		// Pick the nearest attackable entity
		AliveEntity* entity = nullptr;
		entity = GL->GetNearestAttackableEntity(player, targetBlacklist);
		if (entity) {
			// Blacklist entities that are not allowed by the user
			if (entity->isNPC()) {
				auto npc = static_cast<NPC*>(entity);
				if (npc->GetNPCID() == 20204) {
					// Here we blacklister the bloody bee
					targetBlacklist.push_back(entity->GetID());
					return;
				}
			}

			if (targetingOptions.targetingType == TargetingType::CENTER) {
				// Blacklist entities not in the targeting area
				auto inZone = IsEntityInZone(entity);
				if (!inZone) {
					targetBlacklist.push_back(entity->GetID());
					return;
				}
			}

			// Set that entity as the one we want to target
			lockedTarget = entity->GetID();
			SS(BotState::TARGET);
			return;
		}
	}
	else if (targetingOptions.targetingType == TargetingType::ASSIST) {
		Player* player = GL->GetPlayerByName(targetingOptions.assistPlayerName);
		if (!player) {
			return;
		}
		AliveEntity* target = player->GetTarget();
		if (!target) {
			return;
		}

		// TODO. Right now we heal only when a certain event occurs, and we never reach the condition
		// So here we should check if any condition is satisfied (or not) and satisfy it.

		lockedTarget = target->GetID();
		SS(BotState::TARGET);
		return;
	}


	// No locked target
	// TODO: Move to center of zone
}

void BotLogic::BotTarget()
{
	// Priority for assists
	uint32_t target = lockedAssist ? lockedAssist : lockedTarget;

	// Only target if we are not already targeting it
	if (player->GetTarget() && player->GetTarget()->GetID() == target) {
		SS(BotState::TARGETED);
		return;
	}

	// Enter state
	if (trycount == FIRST_TRY) {
		player->TargetEntity(GL->GetEntity(target));
	}
	trycount++;
	if (TRY_WAIT(3000)) {
		SS(BotState::CHOOSING);
	}
}

void BotLogic::BotTargeted()
{
	// Priority for assists
	uint32_t target = lockedAssist ? lockedAssist : lockedTarget;

	if (player->GetTarget() && player->GetTarget()->GetID() != target) {
		// There is an error here...
	}

	// Enter state
	if (trycount == FIRST_TRY) {
		if (player->IsSitting()) {
			player->SitOrStand();
			trycount++;
		}
	}

	// Waiting to stand up
	if (player->IsSitting()) {
		return;
	}

	SS(BotState::ENGAGE);
}

void BotLogic::BotEngage()
{
	// Priority on locked assists
	if (lockedAssist) {
		// Do action
		if (!assistAction) {
			// TODO: Handle error
			return;
		}

		if (assistAction->Type == Action::Type::SKILL) {
			player->UseSkill(assistAction->ID);
			lockedAssist = 0;
			assistAction = nullptr;
			SS(BotState::CHOOSING);
		}
	}
	else {
		// ETP > 0?
		if (AttackEntityTargetingUs()) {
			return;
		}
		
		DoAttackTarget(100);
		trycount++;

		// If we are still there after 10s, switch target
		if (TRY_WAIT(10000)) {
			BlackListAndSwitchTarget();
		}
	}
}

void BotLogic::BotEngaged()
{
	// We are fighting, let's spoil it
	if (spoilAndSweep && VecNotContains(spoiledTargets, lockedTarget)) {
		// Spoil is enabled and the target is not spoiled yet, spoil it!
	 	if (curtime - spoilAction.LastUse > spoilAction.Delay) {
			player->UseSkill(spoilAction.ID);
			spoilAction.LastUse = curtime;
		}
	} else {
		// Just do a random attack
		DoAttackTarget(100);
	}
}

void BotLogic::BotTargetDead()
{
	ASSERT(lockedTarget);

	// Enter state
	killedTargets.insert(lockedTarget);

	// Sweep it if needed
	if (spoilAndSweep && VecContains(spoiledTargets, lockedTarget)) {
		SS(BotState::SWEEP);
		return;
	}

	// ETP > 0?
	if (AttackEntityTargetingUs()) {
		return;
	}

	// Pickup if needed
	lockedTarget = 0;
	SS(BotState::PICKUP);
}

void BotLogic::BotSweep()
{
	if (TRY_WAIT(500)) {
		// 500 ms have passed, we assume sweep worked

		// ETP > 0?
		if (AttackEntityTargetingUs()) {
			return;
		}

		// Pickup if needed
		lockedTarget = 0;
		SS(BotState::PICKUP);
	} else if (TRY_WAIT(100)) {
		// Wait 100 ms to make sure everything is alright
		player->UseSkill(42);
		spoiledTargets.erase(lockedTarget); // Cleanup spoiledTargets vector
	}

	trycount++;
}

void BotLogic::BotPickup()
{
	lockedItem = 0;

	// Enter state: pickup all the items we should pickup
	for (auto item : GL->GetNearestPickableItems(killedTargets, itemBlacklist)) {
		lockedItem = item->GetID();
		SS(BotState::PICKING);
		return;
	}

	// Go back to starting the battle
	SS(BotState::START);
}

void BotLogic::BotPicking()
{
	// Enter state: go pickup the locked item
	if (trycount == FIRST_TRY) {
		auto E = GL->GetEntity(lockedItem);
		if (E && E->isItem()) {
			auto I = static_cast<Item*>(E);
			player->PickUp(I);
		}
		else {
			SS(BotState::PICKUP);
		}
	}

	// ETP > 0 : We are picking and a monster wants to attack us
	// Also we are picking but stuck and a monster attacks us
	AttackEntityTargetingUs();
	trycount++;

	// If picking up took too long, go back to choosing an item
	if (TRY_WAIT(15000)) {
		itemBlacklist.push_back(lockedItem);
		SS(BotState::PICKUP);
	}
}

void BotLogic::BotRest()
{
	// Sit if needed
	if (trycount == FIRST_TRY) {
		if (player->IsStanding()) {
			player->SitOrStand();
		}
		trycount++;
		return;
	}

	// ETP > 0?
	if (AttackEntityTargetingUs()) {
		return;
	}

	// Check if rest condition is satisfied
	bool stopResting = false;
	if (restOptions.HPRest && restOptions.MPRest) {
		if (player->GetHPPercent() >= restOptions.HPRestMax && player->GetMPPercent() >= restOptions.MPRestMax) {
			stopResting = true;
		}
	}
	else if (restOptions.HPRest) {
		if (player->GetHPPercent() >= restOptions.HPRestMax) {
			stopResting = true;
		}
	}
	else if (restOptions.MPRest) {
		if (player->GetMPPercent() >= restOptions.MPRestMax) {
			stopResting = true;
		}
	}

	// If condition is satisfied, start over
	if (stopResting) {
		killedTargets.clear();
		SS(BotState::START);
	}
}

void BotLogic::run()
{
	while (botRunning) {
		curtime = GetCurrentMs();
		switch (state) {
		case BotState::START:
			BotStart();
			break;
		case BotState::CHOOSING:
			BotChoosing();
			break;
		case BotState::TARGET:
			BotTarget();
			break;
		case BotState::TARGETED:
			BotTargeted();
			break;
		case BotState::ENGAGE:
			BotEngage();
			break;
		case BotState::ENGAGED:
			BotEngaged();
			break;
		case BotState::TARGET_DEAD:
			BotTargetDead();
			break;
		case BotState::SWEEP:
			BotSweep();
			break;
		case BotState::PICKUP:
			BotPickup();
			break;
		case BotState::PICKING:
			BotPicking();
			break;
		case BotState::REST:
			BotRest();
			break;
		}
		uint64_t time_after = GetCurrentMs();
		uint64_t time_to_wait = time_after - curtime;
		if (time_to_wait < LOOP_DELAY) {
			time_to_wait = LOOP_DELAY - time_to_wait;
			Sleep(time_to_wait);
		}
	}
}

/** UI RELATED **/

void BotLogic::AddCombatAction(uint32_t actionType, uint32_t actionID, bool engageOnly, uint32_t delay)
{
	auto action = Action { actionType, actionID, engageOnly, delay, Condition::ALWAYS, 0, 0 };
	if (actionType == Action::Type::SKILL) {
		registeredSkills.push_back(action);
		if (engageOnly) {
			skillsEngageOnlyCount++;
		}
	}
	else {
		registeredActions.push_back(action);
	}
}

void BotLogic::RemoveCombatAction(uint32_t actionType, uint32_t actionID)
{
	if (actionType == Action::Type::SKILL) {
		for (auto it = registeredSkills.begin(); it != registeredSkills.end(); ++it) {
			if (it->ID == actionID) {
				if (it->EngageOnly) {
					skillsEngageOnlyCount--;
				}
				registeredSkills.erase(it);
				break;
			}
		}
	}
	else if (actionType == Action::Type::ACTION) {
		for (auto it = registeredActions.begin(); it != registeredActions.end(); ++it) {
			if (it->ID == actionID) {
				registeredActions.erase(it);
				break;
			}
		}
	}
}

std::vector<Action> BotLogic::GetRegisteredCombatActions()
{
	std::vector<Action> ret = registeredActions;
	ret.insert(ret.end(), registeredSkills.begin(), registeredSkills.end());
	return ret;
}

void BotLogic::AddPartyAction(uint32_t actionType, uint32_t actionID, uint32_t delay, Condition condition, uint32_t conditionValue)
{
	auto action = Action { actionType, actionID, 0, delay, condition, conditionValue, 0 };
	partyActions.push_back(action);
}

void BotLogic::RemovePartyAction(uint32_t actionType, uint32_t actionID)
{
	for (auto it = partyActions.begin(); it != partyActions.end(); ++it) {
		if (it->Type == actionType && it->ID == actionID) {
			partyActions.erase(it);
			break;
		}
	}
}

std::vector<Action> BotLogic::GetRegisteredPartyActions()
{
	return partyActions;
}

void BotLogic::SetRestOptions(bool HPRest, uint32_t HPRestMin, uint32_t HPRestMax, bool MPRest, uint32_t MPRestMin, uint32_t MPRestMax)
{
	restOptions.HPRest = HPRest;
	restOptions.HPRestMin = HPRestMin;
	restOptions.HPRestMax = HPRestMax;
	restOptions.MPRest = MPRest;
	restOptions.MPRestMin = MPRestMin;
	restOptions.MPRestMax = MPRestMax;
}

Position BotLogic::SetTargetingCenter(int radius)
{
	targetingOptions.targetingCenter = player->GetPos();
	targetingOptions.targetingCenterRadius = radius;
	return targetingOptions.targetingCenter;
}

void BotLogic::SetTargetingType(TargetingType type)
{
	targetingOptions.targetingType = type;
}

void BotLogic::SetTargetingAssist(WCHAR* name)
{
	memset(targetingOptions.assistPlayerName, 0, sizeof(targetingOptions.assistPlayerName));
	lstrcpyW(targetingOptions.assistPlayerName, name);
}

void BotLogic::EnableSpoilAndSweep(bool enable)
{
	// Check if we have the spoil skill
	auto skills = GL->GetSkills();
	Skill* spoil = nullptr;
	for (auto& skill : skills) {
		if (skill.ID == 254) {
			spoil = &skill;
			break;
		}
	}
	if (!spoil) {
		// Explicitely disable spoil action
		spoilAndSweep = false;
	}
	else {
		// Initialize spoilAction
		spoilAction.Type = Action::Type::SKILL;
		spoilAction.ID = spoil->ID;
		spoilAction.EngageOnly = false;
		spoilAction.Delay = 2500; // Try it every 2.5s
		spoilAndSweep = enable;
	}
}

/** HELPERS **/

bool BotLogic::AttackEntityTargetingUs()
{
	auto AE = GetEntityTargetingUs();
	if (!AE) {
		return false;
	}

	if (state == BotState::ENGAGE && lockedTarget == AE->GetID()) {
		// We are already engaging the right target
		return false;
	}

	lockedTarget = AE->GetID();
	SS(BotState::TARGET);
	return true;
}

bool BotLogic::UseRandomSkill(bool engage)
{
	if (registeredSkills.size() == 0) {
		return false;
	}

	Action* action = nullptr;
	for (Action& a : registeredSkills) {
		if (a.EngageOnly && !engage) {
			continue;
		}
		if (curtime - a.LastUse > a.Delay) {
			action = &a;
			break;
		}
	}

	if (!action) {
		return false;
	}

	action->LastUse = curtime;
	player->UseSkill(action->ID);
	return true;
}

bool BotLogic::UseRandomAction()
{
	if (registeredActions.size() == 0) {
		return false;
	}

	Action* action = nullptr;
	for (Action& a : registeredActions) {
		if (curtime - a.LastUse > a.Delay) {
			action = &a;
			break;
		}
	}

	if (!action) {
		return false;
	}

	action->LastUse = curtime;
	player->TargetEntity(GL->GetEntity(lockedTarget));
	return true;
}

bool BotLogic::DoAttackTarget(uint32_t ms_max)
{
	// If ms_max is non zero, make sure that amount of time has passed before trying to do
	// any kind of action
	if (ms_max) {
		if (curtime - lastActionSent <= ms_max) {
			return false;
		}
	}

	// Execute skill or action with a priority for skills
	auto used = UseRandomSkill(state == BotState::TARGETED);
	if (!used) {
		used = UseRandomAction();
	}

	if (used) {
		lastActionSent = curtime;
	}

	return used;
}

bool BotLogic::IsEntityInZone(GameEntity* entity)
{
	// This is a circle
	auto dx = abs(entity->GetPos().x - targetingOptions.targetingCenter.x);
	auto dy = abs(entity->GetPos().y - targetingOptions.targetingCenter.y);
	auto R = targetingOptions.targetingCenterRadius;

	if (dx + dy <= R) {
		return true;
	}
	if (dx > R || dy > R) {
		return false;
	}
	return dx * dx + dy * dy <= R * R;
}

/**
 * Get one *alive* entity (NPC) that is targeting us. If no entity
 * is targeting us, return nullptr.
 */
AliveEntity* BotLogic::GetEntityTargetingUs()
{
	for (auto& entity : GL->GetEntitiesTargetingPlayer()) {
		auto E = GL->GetEntity(entity);
		if (!E || !E->isNPC()) {
			continue;
		}
		auto AE = static_cast<AliveEntity*>(E);
		if (AE->IsDead() || AE->GetHPCur() <= 0) {
			continue;
		}
		return AE;
	}
	return nullptr;
}

void BotLogic::BlackListAndSwitchTarget()
{
	ASSERT(lockedTarget);

	targetBlacklist.push_back(lockedTarget);
	SS(BotState::CHOOSING);
}

/****** Configuration related functions ********/

const static int CONFIG_VERSION = 0xAF000000;

void BotLogic::LoadConfiguration(uint8_t* config)
{
	uint8_t* ptr = config;

	// A. Parse version
	uint32_t version = *(uint32_t*)ptr;
	ptr += 4;
	if (version != CONFIG_VERSION) {
		emit GL->sendMessage("Could not load configuration - version mismatch!");
		return;
	}

	// B. Get amount of actions and load them
	registeredActions.clear();
	uint32_t nactions = *(uint32_t*)ptr;
	if (nactions > 1000) {
		emit GL->sendMessage("Malformed configuration file.");
		return;
	}
	ptr += 4;
	for (auto i = 0; i < nactions; i++) {
		Action action;
		memcpy(&action, ptr, sizeof(Action));
		registeredActions.push_back(action);
		ptr += sizeof(Action);
	}

	// C. Get amount of skills and load them
	registeredSkills.clear();
	uint32_t nskills = *(uint32_t*)ptr;
	if (nskills > 1000) {
		emit GL->sendMessage("Malformed configuration file.");
		return;
	}
	ptr += 4;
	for (auto i = 0; i < nskills; i++) {
		Action action;
		memcpy(&action, ptr, sizeof(Action));
		registeredSkills.push_back(action);
		ptr += sizeof(Action);
	}

	// D. Read rest options
	memcpy(&restOptions, ptr, sizeof(RestOptions));
	ptr += sizeof(RestOptions);

	// E. Read targeting info
	memcpy(&targetingOptions, ptr, sizeof(TargetingInfo));
	ptr += sizeof(TargetingInfo);
}

uint8_t* BotLogic::GetConfiguration(uint32_t* size)
{
	// 1. Compute the size of our save config we'll need
	auto nactions = registeredActions.size();
	auto nskills = registeredSkills.size();
	auto confsize =
		sizeof(uint32_t) +                              // VERSION
		sizeof(uint32_t) + nactions * sizeof(Action) +  // NACTIONS + ACTIONS
		sizeof(uint32_t) + nskills * sizeof(Action) +   // NSKILLS + SKILLS
		sizeof(RestOptions) +                           // REST_OPTIONS
		sizeof(TargetingInfo);                          // TARGETING INFO

	// 2. Allocate memory and manually serialize everything
	uint8_t* config = (uint8_t*) malloc(confsize);
	if (!config) {
		*size = 0;
		return nullptr;
	}

	uint8_t* ptr = config;

	// A. Write version
	*(uint32_t*)(ptr) = CONFIG_VERSION;
	ptr += 4;

	// B. Write the amount of actions and every action
	*(uint32_t*)(ptr) = nactions;
	ptr += 4;
	for (auto i = 0; i < nactions; i++) {
		memcpy(ptr, &registeredActions[i], sizeof(Action));
		ptr += sizeof(Action);
	}

	// C. Write the amount of skills and every skill
	*(uint32_t*)(ptr) = nskills;
	ptr += 4;
	for (auto i = 0; i < nskills; i++) {
		memcpy(ptr, &registeredSkills[i], sizeof(Action));
		ptr += sizeof(Action);
	}
	
	// D. Write rest options
	memcpy(ptr, &restOptions, sizeof(RestOptions));
	ptr += sizeof(RestOptions);

	// E. Write targeting info
	memcpy(ptr, &targetingOptions, sizeof(TargetingInfo));
	ptr += sizeof(TargetingInfo);

	assert(ptr - config == confsize);
	*size = confsize;
	return config;
}