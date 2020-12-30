#include "PacketParser.h"

#ifdef _DEBUG
#define DEBUG_LOG(x) do { emit IPC->sendMessageDebug(x); } while (false);
#else
#define DEBUG_LOG(x) do { } while (false);
#endif

PacketParser::PacketParser(GameLogic* GL, IPCSocket* IPC)
{
	assert(GL);
	assert(IPC);

	this->GL = GL;
	this->IPC = IPC;
}

bool PacketParser::Setup(int ProtocolVersion)
{
	this->ProtocolVersion = ProtocolVersion;
	switch (ProtocolVersion) {
	case PROTOCOL_VERSION_28:
		SPP = &ServerPacketP28;
		SPPP = &ServerPacketSpecialP28;
		CPP = &ClientPacketP28;
		break;
	case PROTOCOL_VERSION_140:
		SPP = &ServerPacketP140;
		SPPP = &ServerPacketSpecialP140;
		CPP = &ClientPacketP140;
		break;
	default:
		SPP = nullptr;
		SPPP = nullptr;
		CPP = nullptr;
		return false;
	}

	return true;
}

const ClientPacketProtocol* PacketParser::GetClientProtocol()
{
	return CPP;
}

#ifdef _DEBUG
#define PRINT_PACKET() PrintPacket(PacketID, packet, length);
void PacketParser::PrintPacket(uint16_t PacketID, const char* packet, uint16_t length)
{
	QByteArray bytes(packet, length);
	QString idAndBytes = QString("ID:%1 > ")
		.arg(PacketID, 2, 16, QChar('0'));
	idAndBytes += bytes.toHex();
	DEBUG_LOG(idAndBytes);
}
#else
#define PRINT_PACKET() do { } while (false);
#endif

#define RD8()  *(uint8_t*) (packet + pidx); pidx += 1;
#define RD16() *(uint16_t*)(packet + pidx); pidx += 2;
#define RD32() *(uint32_t*)(packet + pidx); pidx += 4;
#define RD64() *(uint64_t*)(packet + pidx); pidx += 8;
#define RDStr(x) WCHAR* x = (WCHAR*)(packet + pidx);  pidx += (wcslen(x) + 1) * sizeof(wchar_t);
#define RDStrL(name, length) WCHAR name[256] = { 0 }; memcpy(name, packet + pidx, length * sizeof(wchar_t)); pidx += length * sizeof(wchar_t);

static void ExUserInfo(GameLogic* GL, const char* packet)
{
	size_t pidx = 0x03;
	auto EntityID = RD32();
	auto initSize = RD32();
	auto someSize = RD16();
	auto mask = RD32();

	auto _relation = RD32();
	auto wut = RD8();
	auto strlen = RD16();
	RDStrL(charName, strlen);
	auto GM = RD8();
	auto Race = RD8();
	auto Sex = RD8();
	auto classId = RD32();
	auto classId2 = RD32();
	auto level = RD8();

	// Skip STR CON etc.
	auto blockSize = RD16();
	pidx += blockSize - 2;

	// Parse Max data
	blockSize = RD16();
	auto maxHP = RD32();
	auto maxMP = RD32();
	auto maxCP = RD32();

	// Parse Cur data
	blockSize = RD16();
	auto curHP = RD32();
	auto curMP = RD32();
	auto curCP = RD32();

	AliveEntity* entity = nullptr;
	if (GL->GetState() == GameLogic::GameState::CHAR_SELECTED) {
		entity = GL->GetPlayer();
		GL->SetMainPlayerContext(EntityID);
	}
	else {
		auto E = GL->GetEntity(EntityID);
		if (!E || !E->isAliveEntity()) {
			return;
		}
		entity = static_cast<AliveEntity*>(E);
	}
	entity->SetLevel(level);
	entity->SetHPMax(maxHP);
	entity->SetHP(curHP);
	entity->SetMPMax(maxMP);
	entity->SetMP(curMP);
	emit GL->playerStatusChanged();
}

void PacketParser::ParseLineage2Packet(const char* packet, size_t length)
{
	// Fetch packet ID
	uint8_t PacketID = packet[2];

	if (GL->GetState() == GameLogic::GameState::NOT_CONNECTED) {
		if (PacketID == SPP->CHAR_SELECTED) {
			size_t pidx = 0x03;

			RDStr(charName);
			auto objectId = RD32();
			RDStr(charTitle);
			auto sessionId = RD32();
			auto clandId = RD32();
			auto unk = RD32();
			auto sex = RD32();
			auto race = RD32();
			auto classId = RD32();
			auto unk2 = RD32();
			auto posX = RD32();
			auto posY = RD32();
			auto posZ = RD32();

			// TODO Parse game time etc.
			//uint32_t unk = *(uint32_t*)(packet + idx + 0x08);
			//uint32_t sex = *(uint32_t*)(packet + idx + 0x0c);
			//uint32_t race = *(uint32_t*)(packet + idx + 0x10);
			//uint32_t classId = *(uint32_t*)(packet + idx + 0x14);
			//uint32_t unk2 = *(uint32_t*)(packet + idx + 0x18);
			// unk2 == 1
			GL->SetMainPlayer(charName, posX, posY, posZ);
		}
	}
	else if (GL->GetState() == GameLogic::GameState::CHAR_SELECTED) {
		if (PacketID == SPP->EX_USER_INFO) {
			ExUserInfo(GL, packet);
		}
	}
	else if (GL->GetState() == GameLogic::GameState::IN_GAME) {

		// Big switch case on packet ID
		if (PacketID == SPP->DIE)
		{
			size_t pidx = 0x03;
			auto objectID = RD32();
			GL->KillEntity(objectID);
		}
		else if (PacketID == SPP->DELETE_OBJECT)
		{
			size_t pidx = 0x03;
			auto objectID = RD32();
			GL->DeleteObject(objectID);
		}
		else if (PacketID == SPP->EX_NPC_INFO)
		{
			uint16_t pidx = 0x03;
			auto ObjID = RD32();
			auto summon = RD8();
			auto Mask = RD16();
			if (Mask != 0x0025) {
				DEBUG_LOG("Received weird mask...");
			}
			BYTE Mask2[5]; memcpy(Mask2, packet + pidx, sizeof(Mask2)); pidx += sizeof(Mask2);

			auto initSize = RD8();
			auto attackable = RD8();
			auto unk = RD32();
			RDStr(npcTitle);

			auto bsize = RD16();
			if ((ProtocolVersion == PROTOCOL_VERSION_28 && bsize != 0x44) ||
				(ProtocolVersion == PROTOCOL_VERSION_140 && bsize != 0x49)) {
				DEBUG_LOG("There might be an issue parsing NPC...");
			}
			auto npcID = RD32();
			npcID -= 1000000;
			auto posX = RD32();
			auto posY = RD32();
			auto posZ = RD32();
			auto unk2 = RD32();
			auto atkSpeed = RD32();
			auto cstSpeed = RD32();
			auto speedMul1 = RD32();
			auto speedMul2 = RD32();
			auto eqRH = RD32();
			auto eqArmor = RD32();
			auto eqLH = RD32();

			auto alive = RD8();
			auto running = RD8();
			auto swimOrFly = RD8();
			if (ProtocolVersion == PROTOCOL_VERSION_28) {
				auto team = RD8();
			}
			else if (ProtocolVersion == PROTOCOL_VERSION_140) {
				auto unk4 = RD32();
			}
			auto clone = RD32();
			auto HPMax = RD32();
			auto HPCur = RD32();
			auto unk3 = RD32();

			// TODO: Parse the mask to parse everything correctly...

			NPC* npc = static_cast<NPC*>(GL->GetEntity(ObjID));
			if (!npc) {
				npc = GL->AddNPC(ObjID, posX, posY, posZ, HPMax, HPCur, npcID);
			}
			npc->SetLocation(posX, posY, posZ);
			npc->SetAttackable(attackable);
			npc->SetHP(HPMax, HPCur);
		}
		else if (PacketID == SPP->ITEM_LIST)
		{
			// TODO if we want to pickup already existing items on the ground.
		}
		else if (PacketID == SPP->EX_USER_INFO)
		{
			ExUserInfo(GL, packet);
		}
		else if (PacketID == SPP->DROP_ITEM)
		{
			uint32_t pidx = 0x03;
			auto dropper = RD32();
			auto itemObjID = RD32();
			auto itemDisplayID = RD32(); // The actual item ID
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			auto stackable = RD8();
			auto count = RD64();
			GL->DropItem(itemObjID, X, Y, Z, itemDisplayID, count, dropper);
		}
		else if (PacketID == SPP->GET_ITEM)
		{
			uint32_t pidx = 0x03;
			auto PlayerID = RD32();
			auto ItemID = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			emit GL->getItem(PlayerID, ItemID);
			GL->SetLocation(PlayerID, X, Y, Z);
		}
		else if (PacketID == SPP->STATUS_UPDATE)
		{
			uint32_t pidx = 0x03;
			auto objID = RD32();
			auto unk = RD32();
			auto visible = RD8();
			auto count = RD8();
			std::vector<Status> status;
			for (size_t i = 0; i < count; i++) {
				auto type = RD8();
				auto value = RD32();
				status.push_back(Status(type, value));
			}
			GL->UpdateStatus(objID, status);
		}
		else if (PacketID == SPP->ACTION_FAIL)
		{
			emit GL->actionFailed();
		}
		else if (PacketID == SPP->TARGET_SELECTED)
		{
			uint32_t pidx = 0x03;
			auto ObjID = RD32();
			auto targetObjID = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			GL->SetTarget(ObjID, targetObjID);
			GL->SetLocation(targetObjID, X, Y, Z);
		}
		else if (PacketID == SPP->TARGET_UNSELECTED)
		{
			uint32_t pidx = 0x03;
			auto ObjID = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			GL->SetTarget(ObjID, GameEntity::ENTITY_NONE);
			GL->SetLocation(ObjID, X, Y, Z);
		}
		else if (PacketID == SPP->AUTO_ATTACK_START)
		{
			uint32_t pidx = 0x03;
			auto TargetID = RD32();
			emit GL->autoAttackStart(TargetID);
		}
		else if (PacketID == SPP->CHANGE_WAIT_TYPE)
		{
			uint32_t pidx = 0x03;
			auto ObjID = RD32();
			auto waitType = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			GL->SetWaitType(ObjID, waitType);
			GL->SetLocation(ObjID, X, Y, Z);
		}
		else if (PacketID == SPP->MOVE_TO_LOCATION)
		{
			uint32_t EntityID = *(uint32_t*)(packet + 3);
			uint32_t DestX = *(uint32_t*)(packet + 7);
			uint32_t DestY = *(uint32_t*)(packet + 11);
			uint32_t DestZ = *(uint32_t*)(packet + 15);
			uint32_t FromX = *(uint32_t*)(packet + 19);
			uint32_t FromY = *(uint32_t*)(packet + 23);
			uint32_t FromZ = *(uint32_t*)(packet + 27);
			GL->MoveToLocation(EntityID, DestX, DestY, DestZ, FromX, FromY, FromZ);
		}
		else if (PacketID == SPP->CHARINFO)
		{
			size_t pidx = 0x03;
			if (ProtocolVersion == PROTOCOL_VERSION_140) {
				pidx = 0x04;
			}
			auto x = RD32();
			auto y = RD32();
			auto z = RD32();
			auto vehicleID = RD32();
			auto objID = RD32();
			RDStr(charName);
			auto race = RD16();
			auto sex = RD8();
			auto baseClass = RD32();

			for (int paperdoll = PAPERDOLL_ORDER::PAPERDOLL_UNDER; paperdoll != PAPERDOLL_ORDER::LAST; paperdoll++) {
				// TODO: Parse that?
				auto itemDisplay = RD32();
			}
			for (int i = 0; i < 3; i++) {
				// TODO: Parse that?
				auto augment1 = RD32();
				auto augment2 = RD32();
			}
			auto armorEnchant = RD8();
			for (int i = 0; i < 9; i++) {
				auto itemDisplay = RD32();
			}

			auto pvp = RD8();
			auto reputation = RD32();

			auto mAtkSpd = RD32();
			auto pAtkSpd = RD32();
			
			auto runSpd = RD16();
			auto walkSpd = RD16();
			auto swimRunSpd = RD16();
			auto swimWalkSpd = RD16();
			auto flyRunSpd = RD16();
			auto flyWalkSpd = RD16();
			auto flyRunSpd2 = RD16();
			auto flyWalkSpd2 = RD16();
			auto moveMult = RD64();
			auto atkSpeedMult = RD64();

			auto collisionRadius = RD64();
			auto collisionHeight = RD64();

			auto visualHair = RD32();
			auto visualHairColor = RD32();
			auto visualFace = RD32();

			RDStr(title);

			auto clanId = RD32();
			auto clanCrestId = RD32();
			auto allyId = RD32();
			auto allyCrestId = RD32();

			auto sitting = RD8();
			auto running = RD8();
			auto combat = RD8();

			auto olympiad = RD8();

			auto invisible = RD8();

			auto mountType = RD8();
			auto privateStore = RD8();

			auto nCubics = RD16();
			for (int i = 0; i < nCubics; i++) {
				auto cubic = RD16();
			}

			auto matchingRoom = RD8();
			auto inZone = RD8(); // fly, water, ...
			auto recomHave = RD16();
			auto mountNpcID = RD32();

			auto classID = RD32();
			auto unk = RD32();
			auto mounted = RD8();

			auto teamID = RD8();

			auto clanCrestLargeID = RD32();
			auto noble = RD8();
			auto hero = RD8();

			auto fishing = RD8();
			auto baitX = RD32();
			auto baitY = RD32();
			auto baitZ = RD32();

			auto nameColor = RD32();
			auto heading = RD32();
			auto pledgeClass = RD8();
			auto pledgeType = RD16();

			auto titleColor = RD32();
			auto cursedWeapon = RD8();
			auto clanReput = RD32();
			auto transformationID = RD32();
			auto agathionID = RD32();

			auto unk2 = RD8();

			auto curCP = RD32();
			auto maxHP = RD32();
			auto curHP = RD32();
			auto maxMP = RD32();
			auto curMP = RD32();

			auto unk3 = RD8();
			auto nEffects = RD32();
			for (int i = 0; i < nEffects; i++) {
				auto effect = RD16();
			}
			auto unk4 = RD8();
			auto accessoryEnabled = RD8();
			auto abilityPoint = RD8();

			auto player = static_cast<Player*>(GL->GetEntity(objID));
			if (!player) {
				GL->AddPlayer(objID, charName, x, y, z);
				player = static_cast<Player*>(GL->GetEntity(objID));
			}
			GL->SetLocation(objID, x, y, z);
			player->SetHPMax(maxHP);
			player->SetHP(curHP);
			player->SetMPMax(maxMP);
			player->SetMP(curMP);
		}
		else if (PacketID == SPP->ATTACK)
		{
			size_t pidx = 0x03;
			auto attackerObjID = RD32();
			auto targetObjID = RD32();
			auto SS = RD32();
			auto damage = RD32();
			auto flags = RD32();
			auto grade = RD32();

			auto attackerX = RD32();
			auto attackerY = RD32();
			auto attackerZ = RD32();
			GL->Attack(attackerObjID, targetObjID, damage, flags);
			GL->SetLocation(attackerObjID, attackerX, attackerY, attackerZ);

			auto nbHit = RD16();
			for (size_t i = 0; i < nbHit; i++) {
				targetObjID = RD32();
				damage = RD32();
				flags = RD32();
				grade = RD32();
				GL->Attack(attackerObjID, targetObjID, damage, flags);
			}

			auto targetX = RD32();
			auto targetY = RD32();
			auto targetZ = RD32();
			GL->SetLocation(targetObjID, targetX, targetY, targetZ);
		}
		else if (PacketID == SPP->STOP_MOVE)
		{
			size_t pidx = 0x03;
			auto ObjID = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			auto heading = RD32();
			auto E = GL->GetEntity(ObjID);
			if (E) {
				E->SetLocation(X, Y, Z);
				E->SetHeading(heading);
			}
		}
		else if (PacketID == SPP->MAGIC_SKILL_USE)
		{
			size_t pidx = 0x03;
			auto barType = RD32();
			auto caster = RD32();
			auto target = RD32();
			auto skillID = RD32();
			auto skillLevel = RD32();
			auto hitTime = RD32();
			auto reuseGroup = RD32();
			auto reuseDelay = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			// Unk that?
			auto unk_size = RD16();
			for (int i = 0; i < unk_size; i++) {
				auto unk = RD16();
			}
			// Ground locations, what is it?
			// TODO

			auto E = GL->GetEntity(caster);
			if (E) {
				E->SetLocation(X, Y, Z);
				GL->SkillCast(caster, target);
			}
		}
		else if (PacketID == SPP->MAGIC_SKILL_CANCELED)
		{
			size_t pidx = 0x03;
			auto ObjID = RD32();
			GL->SkillCanceled(ObjID);
		}
		else if (PacketID == SPP->MAGIC_SKILL_LAUNCHED)
		{
			size_t pidx = 0x03;
			auto barType = RD32();
			auto caster = RD32();
			auto skillID = RD32();
			auto skillLevel = RD32();
			// TODO Parse targets?
			GL->SkillLaunched(caster);
		}
		else if (PacketID == SPP->SKILL_LIST)
		{
			size_t pidx = 0x03;
			auto nSkills = RD32();
			for (int i = 0; i < nSkills; i++) {
				auto passive = RD32();
				auto level = RD16();
				auto subLevel = RD16();
				auto skillID = RD32();
				auto reuseDelayGroup = RD32();
				auto disabled = RD8();
				auto enchanted = RD8();
				GL->AddSkill(skillID, level, passive, disabled);
			}
			auto lastLearnedSkillID = RD32();
		}
		else if (PacketID == SPP->SYSTEM_MESSAGE)
		{
			PRINT_PACKET();
			size_t pidx = 0x03;
			auto msgID = RD16();
			emit GL->systemMessage(msgID);
		}
		else if (PacketID == SPP->RESTART_RESPONSE)
		{
			size_t pidx = 0x03;
			PRINT_PACKET();
			GL->Restart();
		}
		else if (PacketID == SPP->MOVE_TO_PAWN)
		{
			size_t pidx = 0x03;
			auto ObjID = RD32();
			auto TargetID = RD32();
			auto dist = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			auto TX = RD32();
			auto TY = RD32();
			auto TZ = RD32();

			auto TE = GL->GetEntity(TargetID);
			if (TE) {
				TE->SetLocation(TX, TY, TZ);
			}
			emit GL->entityMoveToPawn(ObjID, TargetID);
		}
		else if (PacketID == SPP->MY_TARGET_SELECTED)
		{
			size_t pidx = 0x03;
			uint32_t target, levelDiff;

			// TODO: That's not very clean...
			if (ProtocolVersion == 140)
			{
				levelDiff = RD32();
				target = RD32();
			}
			else
			{
				target = RD32();
				levelDiff = RD16();
			}

			auto E = GL->GetEntity(target);
			if (E && E->isAliveEntity()) {
				auto AE = static_cast<AliveEntity*>(E);
				GL->GetPlayer()->SetTarget(AE);
				uint16_t playerLevel = GL->GetPlayer()->GetLevel();
				uint16_t targetLevel = playerLevel - levelDiff;
				if (AE->isNPC()) {
					AE->SetLevel(targetLevel);
				}
				emit(GL->playerTargetChanged());
			}
		}
		else if (PacketID == SPP->S_VALIDATE_LOCATION)
		{
			size_t pidx = 0x03;
			auto ObjID = RD32();
			auto X = RD32();
			auto Y = RD32();
			auto Z = RD32();
			auto heading = RD32();
			auto E = GL->GetEntity(ObjID);
			if (E) {
				E->SetLocation(X, Y, Z);
				E->SetHeading(heading);
			}
		}
		else if (PacketID == SPP->SOCIAL_ACTION ||
			PacketID == SPP->EX_NPC_INFO_ABNORMAL_VISUAL_EFFECT ||
			PacketID == SPP->NET_PING ||
			PacketID == SPP->QUEST_LIST ||
			PacketID == SPP->EX_ACQUIRABLE_SKILL_LIST_BY_CLASS ||
			PacketID == SPP->MACRO_LIST ||
			PacketID == SPP->DUMMY_7D)
		{
			// Ignore packets, do nothing
		}
		// Special packet (those starting with EX_)
		else if (PacketID == SPP->SPKT_FE)
		{
			uint8_t PacketID_2 = packet[3];
			if (PacketID_2 == SPPP->EX_EVENT_MATCH_TEAM_UNLOCKED ||
				PacketID_2 == SPPP->EX_EVENT_MATCH_CREATE ||
				PacketID_2 == SPPP->EX_SEND_MANOR_LIST ||
				PacketID_2 == SPPP->PLEDGE_RECEIVE_POWER_INFO ||
				PacketID_2 == SPPP->EX_GET_BOOKMARK_INFO ||
				PacketID_2 == SPPP->EX_QUEST_ITEM_LIST ||
				PacketID_2 == SPPP->EXBR_AGATHION_ENERGY_INFO ||
				PacketID_2 == SPPP->EX_SUBJOBINFO) {
				// Ignored packets, do nothing
			}
			else
			{
				PRINT_PACKET();
			}
		}
		else
		{
			PRINT_PACKET();
		}
	}
}