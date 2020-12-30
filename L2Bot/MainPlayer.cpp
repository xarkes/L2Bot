#include "GameLogic.h"
#include "IPCSocket.h"
#include "PacketParser.h"


/******************************************
   Main player methods (sending packets)
   *****************************************/



MainPlayer::MainPlayer(uint32_t ID, WCHAR* name, uint32_t posX, uint32_t posY, uint32_t posZ)
	: Player(ID, name, posX, posY, posZ)
{
}

void MainPlayer::SetSocket(IPCSocket* socket)
{
	this->socket = socket;
	this->protocol = socket->GetPacketParser()->GetClientProtocol();
}

void MainPlayer::SitOrStand()
{
	if (!socket) {
		return;
	}

	const char format[] = "cddc";
	const int nParam = 4;
	DWORD parameters[nParam] = { 0 };
	parameters[0] = protocol->REQUEST_ACTION_USE;
	parameters[1] = 0;
	parameters[2] = 0;
	parameters[3] = 0;
	socket->SendPacket(format, parameters, nParam);
}

void MainPlayer::ValidateLocation(Position pos)
{
	if (!socket) {
		return;
	}

	const char format[] = "cdddddc";
	const int nParam = 7;
	DWORD parameters[nParam] = { 0 };
	parameters[0] = protocol->VALIDATE_LOCATION;
	parameters[1] = pos.x;
	parameters[2] = pos.y;
	parameters[3] = pos.z;
	parameters[4] = pos.heading;
	parameters[5] = 0; // ?
	parameters[6] = 0; // ?
	socket->SendPacket(format, parameters, nParam);
}

void MainPlayer::GoTo(Position pos)
{
	// TODO This packet is not even used it seems...
	/*const char format[] = "cddddddc";
	const int nParam = 8;
	DWORD parameters[nParam] = { 0 };
	parameters[0] = protocol->MOVE_BACKWARD_TO_LOCATION;
	parameters[1] = pos.x;
	parameters[2] = pos.y;
	parameters[3] = pos.z;
	parameters[4] = this->pos.x;
	parameters[5] = this->pos.y;
	parameters[6] = this->pos.z;
	parameters[7] = 1; // 1 - mouse | 0 - keyboard ?
	*/
}

void MainPlayer::TargetEntity(GameEntity* entity, bool waitForTarget)
{
	if (!socket || !entity) {
		return;
	}
	AliveEntity* oldtarget = nullptr;
	if (waitForTarget) {
		oldtarget = target;
		target = nullptr;
	}

	const char format[] = "cddddc";
	const int nParam = 6;
	DWORD parameters[nParam] = { 0 };
	parameters[0] = protocol->ACTION;
	parameters[1] = entity->GetID();
	parameters[2] = pos.x;
	parameters[3] = pos.y;
	parameters[4] = pos.z;
	parameters[5] = 0; // Action type: 0 = normal, 1 = shift click
	socket->SendPacket(format, parameters, nParam);

	if (waitForTarget) {
		int i = 0;
		while (target == nullptr && i < 1000) {
			Sleep(5);
			i++;
		}
	}
}

void MainPlayer::PickUp(Item* item)
{
	// Same as target, but with different position
	if (!socket || !item) {
		return;
	}

	const char format[] = "cddddc";
	const int nParam = 6;
	DWORD parameters[nParam] = { 0 };
	parameters[0] = protocol->ACTION;
	parameters[1] = item->GetID();
	parameters[2] = item->GetPos().x;
	parameters[3] = item->GetPos().y;
	parameters[4] = item->GetPos().z;
	parameters[5] = 0; // Action type: 0 = normal, 1 = shift click
	socket->SendPacket(format, parameters, nParam);
}

void MainPlayer::UseSkill(uint32_t ID)
{
	const char format[] = "cddc";
	const int nParam = 4;
	DWORD parameters[nParam] = { 0 };
	auto ctrlPressed = 0;
	auto shiftPressed = 0;

	parameters[0] = protocol->REQUEST_MAGIC_SKILL_USE;
	parameters[1] = ID;
	parameters[2] = ctrlPressed;
	parameters[3] = shiftPressed;
	socket->SendPacket(format, parameters, nParam);
}