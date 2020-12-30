#ifndef _L2BOT_BOTLOGIC_H
#define _L2BOT_BOTLOGIC_H

#include "GameLogic.h"

#include <QThread>

#include <chrono>
#include <set>

#define VecContains(v, x) std::find(v.begin(), v.end(), x) != v.end()
#define VecNotContains(v, x) std::find(v.begin(), v.end(), x) == v.end()

// The time the choice loop should be taking at most, in milliseconds
// Using a low value permits to know if our code is resiliant to events or not
#define LOOP_DELAY 10
// Macro used to get the timed passed thanks to trycount
#define TRY_WAIT(x) trycount * LOOP_DELAY >= x

enum class BotState {
	NONE = 0,
	START,
	CHOOSING,
	TARGET,
	TARGETED,
	ENGAGE,
	ENGAGED,
	TARGET_DEAD,
	SWEEP,
	PICKUP,
	PICKING,
	REST
};

static const char* BotStateString[] = {
	"NONE",
	"START",
	"CHOOSING",
	"TARGET",
	"TARGETED",
	"ENGAGE",
	"ENGAGED",
	"TARGET_DEAD",
	"SWEEP",
	"PICKUP",
	"PICKING",
	"REST",
	NULL
};

enum class Condition {
	ALWAYS = 0,
	PARTY_MEMBER_HP_LOWER_THAN_PERCENT,
	LAST
};

static const char* ConditionString[] = {
	"Always",
	"Party member HP < XX%",
};

enum ActionState {
	FIRST_TRY = 0,
	WAITING = 1,
	SUCCESS = 0xDEAD
};

struct Action {
	enum Type {
		SKILL = 1,
		ACTION = 2
	};
	uint32_t Type = 0;
	uint32_t ID = 0;
	bool EngageOnly = 0;
	uint32_t Delay = 0;

	Condition condition;
	uint32_t condval;

	uint64_t LastUse = 0;
};

struct RestOptions {
	bool HPRest = true;
	uint32_t HPRestMin = 60;
	uint32_t HPRestMax = 100;
	bool MPRest = true;
	uint32_t MPRestMin = 30;
	uint32_t MPRestMax = 100;
};

enum class TargetingType {
	ANYTHING = 0,
	CENTER,
	ASSIST,
};

struct TargetingInfo {
	TargetingType targetingType = TargetingType::ANYTHING;

	Position targetingCenter = { 0 };
	int targetingCenterRadius = 0;
	
	bool assistPlayer = false;
	WCHAR assistPlayerName[32] = { 0 };
};

static inline uint64_t GetCurrentMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

class BotLogic : public QThread
{
    Q_OBJECT

public:
	BotLogic(QObject *parent, GameLogic* GL);
	~BotLogic();
	void SetPlayer() { player = GL->GetPlayer(); }
	void CleanUp();

	// Thread related
	bool isBotRunning() { return botRunning; }
	void Start();
	void Stop();

	// Combat action methods
	void AddCombatAction(uint32_t actionType, uint32_t actionID, bool engageOnly=0, uint32_t delay=300);
	void RemoveCombatAction(uint32_t actionType, uint32_t actionID);
	std::vector<Action> GetRegisteredCombatActions();

	// Party action methods
	void AddPartyAction(uint32_t actionType, uint32_t actionID, uint32_t delay, Condition condition, uint32_t conditionValue);
	void RemovePartyAction(uint32_t actionType, uint32_t actionID);
	std::vector<Action> GetRegisteredPartyActions();

	// Rest methods
	void SetRestOptions(bool HPRest, uint32_t HPRestMin, uint32_t HPRestMax, bool MPRest, uint32_t MPRestMin, uint32_t MPRestMax);
	RestOptions GetRestOptions() { return restOptions; }


	// UI related
	uint8_t GetState() { return (uint8_t) state; }
	Position SetTargetingCenter(int radius);
	void SetTargetingType(TargetingType);
	void SetTargetingAssist(WCHAR* name);
	Position GetTargetingCenter() { return targetingOptions.targetingCenter; }
	TargetingType GetTargetingType() { return targetingOptions.targetingType; }
	int GetTargetingCenterRadius() { return targetingOptions.targetingCenterRadius; }
	void EnableSpoilAndSweep(bool enable);

	// Configuration
	void LoadConfiguration(uint8_t* config);
	uint8_t* GetConfiguration(uint32_t* size);

signals:
	void stateChanged(QString state);

protected:
	void run();

private:
	// Internal bot logic
	std::vector<uint32_t> targetBlacklist;
	std::vector<uint32_t> itemBlacklist;
	std::set<uint32_t> killedTargets;
	bool botRunning = false;
	GameLogic* GL = nullptr;
	BotState state = BotState::NONE;
	uint32_t lockedTarget = 0;
	uint32_t lockedAssist = 0;
	Action* assistAction = nullptr;
	uint32_t lockedItem = 0;
	MainPlayer* player = nullptr;
	uint32_t trycount = 0;
	uint64_t curtime = 0;

	bool spoilAndSweep = false;
	std::set<uint32_t> spoiledTargets;
	Action spoilAction = { 0 };

	// Actions timeout
	uint64_t lastActionSent = 0;
	bool ActionConditionValid(Condition cond);

	// Events catching
	static const int NCON = 13;
	QMetaObject::Connection con[NCON] = { };

	// Shitty variables that should be removed...
	uint32_t skillsEngageOnlyCount = 0;

	// User defined properties - basically what's in our configuration
	std::vector<Action> registeredActions;
	std::vector<Action> registeredSkills;
	std::vector<Action> partyActions;
	RestOptions restOptions;
	TargetingInfo targetingOptions;

	// Bot states
	void BotStart();
	void BotChoosing();
	void BotTarget();
	void BotTargeted();
	void BotEngage();
	void BotEngaged();
	void BotTargetDead();
	void BotSweep();
	void BotPickup();
	void BotPicking();
	void BotRest();
	inline void SS(BotState state) {
		this->state = state;
		trycount = 0;
		emit stateChanged(QString(BotStateString[(uint8_t)state]));
	}

	// Helpers
	bool AttackEntityTargetingUs();
	bool UseRandomSkill(bool engage);
	bool UseRandomAction();
	bool DoAttackTarget(uint32_t ms_max = 0);
	bool IsEntityInZone(GameEntity* entity);
	AliveEntity* GetEntityTargetingUs();
	void BlackListAndSwitchTarget();

private slots:
	// Game logic slots
	void playerTargetChanged();
	void actionFailed();
	void skillCast(quint32 caster, quint32 target);
	void skillCanceled(quint32 objid);
	void skillLaunched(quint32 caster);
	void entityMoveToPawn(quint32 source, quint32 target);
	void autoAttackStart(quint32 target);
	void entityDies(quint32 entity);
	void somethingTargetedSomething(quint32 who, quint32 target);
	void systemMessageReceived(quint16 msgID);
	void attack(quint32 attacker, quint32 target, quint32 damage, quint32 flags);
	void getItem(quint32 PlayerID, quint32 ItemID);
	void extraPlayerStatusChanged(quint32 PlayerID);
};

#endif // _L2BOT_BOTLOGIC_H
