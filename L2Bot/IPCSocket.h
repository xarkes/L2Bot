#pragma once

#include <qthread.h>
#include <Windows.h>
#include <string>
#include <stdint.h>

#define PIPE_RECV_PACKET_NAME L"\\\\.\\pipe\\APipeR"
#define PIPE_SEND_PACKET_NAME L"\\\\.\\pipe\\APipeS"

class PacketParser;
class GameLogic;

const int MAX_BUF_SIZE = 10 * sizeof(DWORD) + 10 + 1;

enum class IPCState : uint8_t {
	NOT_INITIALIZED = 0,
	READ_OK = 1,
	INITIALIZED = 2,
	CLOSED = 3,
};

class IPCSocket : public QThread
{
	Q_OBJECT
private:
	// IPC attributes
	HANDLE IPCPipeR = INVALID_HANDLE_VALUE;
	HANDLE IPCPipeW = INVALID_HANDLE_VALUE;
	DWORD challenge = -1;
	uint64_t pktidx = 0;
	BYTE sendBuffer[MAX_BUF_SIZE] = { 0 };
	IPCState state = IPCState::NOT_INITIALIZED;
	DWORD TargetPID = 0;

	// Parser
	PacketParser* Parser;

	// Private methods
	void Parse(const char* data, size_t length);

public:
	IPCSocket(GameLogic* GL, DWORD pid);
	~IPCSocket();

	bool CreatePipe();
	IPCState GetState();
	void SendPacket(const char* format, DWORD* parameters, DWORD nParam);
	void SayGoodBye();
	PacketParser* GetPacketParser() { return Parser; }

protected:
	void run();

signals:
	void sendMessage(QString);
	void sendMessageDebug(QString);
};

