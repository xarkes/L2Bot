#pragma once

#include "IPCSocket.h"
#include "GameLogic.h"
#include "Protocol.h"

enum PAPERDOLL_ORDER
{
	PAPERDOLL_UNDER,
	PAPERDOLL_HEAD,
	PAPERDOLL_RHAND,
	PAPERDOLL_LHAND,
	PAPERDOLL_GLOVES,
	PAPERDOLL_CHEST,
	PAPERDOLL_LEGS,
	PAPERDOLL_FEET,
	PAPERDOLL_CLOAK,
	PAPERDOLL_RHAND2,
	PAPERDOLL_HAIR,
	PAPERDOLL_HAIR2,

	LAST
};

class PacketParser {
private:
	// Game Logic
	GameLogic* GL = nullptr;
	IPCSocket* IPC = nullptr;

	// Chosen protocol
	uint8_t ProtocolVersion = 0;
	const ServerPacketProtocol* SPP = nullptr;
	const ServerPacketProtocolSpecial* SPPP = nullptr;
	const ClientPacketProtocol* CPP = nullptr;

	// Debugging
	void PrintPacket(uint16_t PacketID, const char* packet, uint16_t length);

public:
	PacketParser(GameLogic* GL, IPCSocket* IPC);
	bool Setup(int ProtocolVersion);
	void ParseLineage2Packet(const char* packet, size_t length);
	const ClientPacketProtocol* GetClientProtocol();
};