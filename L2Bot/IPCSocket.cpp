#include "IPCSocket.h"
#include "GameLogic.h"
#include "PacketParser.h"
#include "Glob.h"

#ifdef _DEBUG
#define DEBUG_LOG(x) do { emit sendMessageDebug(x); } while (false);
#else
#define DEBUG_LOG(x) do { } while (false);
#endif

static WCHAR RecvSocketName[MAX_PATH] = { 0 };
static WCHAR SendSocketName[MAX_PATH] = { 0 };

IPCSocket::IPCSocket(GameLogic* GL, DWORD pid)
{
	Parser = new PacketParser(GL, this);
	this->TargetPID = pid;
}

bool IPCSocket::CreatePipe()
{
	// Initialize dynamic socket name
	memset(RecvSocketName, 0, MAX_PATH);
	lstrcatW(RecvSocketName, PIPE_RECV_PACKET_NAME);
	_itow_s(TargetPID, RecvSocketName + lstrlenW(RecvSocketName), 5, 16);

	memset(SendSocketName, 0, MAX_PATH);
	lstrcatW(SendSocketName, PIPE_SEND_PACKET_NAME);
	_itow_s(TargetPID, SendSocketName + lstrlenW(SendSocketName), 5, 16);

	IPCPipeR = CreateNamedPipeW(RecvSocketName, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		1, 0, 1 << 16, NMPWAIT_USE_DEFAULT_WAIT, NULL);
	if (IPCPipeR == INVALID_HANDLE_VALUE) {
		return false;
	}
	IPCPipeW = CreateNamedPipeW(SendSocketName, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		1, 1 << 16, 0, NMPWAIT_USE_DEFAULT_WAIT, NULL);
	if (IPCPipeW == INVALID_HANDLE_VALUE) {
		CloseHandle(IPCPipeR);
		return false;
	}
	return true;
}

IPCSocket::~IPCSocket()
{
	if (Parser) {
		delete Parser;
	}
	if (IPCPipeR) {
		// Disconnect if we were connected
		DisconnectNamedPipe(IPCPipeR);
		CloseHandle(IPCPipeR);
	}
	if (IPCPipeW) {
		// Disconnect if we were connected
		DisconnectNamedPipe(IPCPipeW);
		CloseHandle(IPCPipeW);
	}
}

IPCState IPCSocket::GetState()
{
	return state;
}

void IPCSocket::SayGoodBye()
{
	DWORD dwWritten;
	WriteFile(IPCPipeW, "BYHE", 4, &dwWritten, NULL);

	// Now terminate the thread
	terminate();
}

void IPCSocket::SendPacket(const char* format, DWORD* parameters, DWORD nParam)
{
	ASSERT(IPCPipeW);

	auto bufSize = (nParam + 1) + nParam * sizeof(DWORD);
	memcpy(sendBuffer, format, nParam + 1);
	memcpy(sendBuffer + nParam + 1, parameters, nParam * sizeof(DWORD));

	DWORD dwWritten;
	WriteFile(IPCPipeW, sendBuffer, bufSize, &dwWritten, NULL);
	FlushFileBuffers(IPCPipeW);
	if (bufSize != dwWritten) {
		DEBUG_LOG("Error while sending message...")
	}
}

void IPCSocket::run()
{
	DWORD dwRead = 0;
	DWORD BUF_SIZE = 1 << 16;
	char* buffer = (char*) malloc(BUF_SIZE);
	if (!buffer) {
		DEBUG_LOG("Allocation failure, socket aborting");
		return;
	}

	// while (IPCPipe != INVALID_HANDLE_VALUE) {
	while (true) {
		// Wait for someone to connect
		if (ConnectNamedPipe(IPCPipeR, NULL) != FALSE) {
			// Read data
			while (ReadFile(IPCPipeR, buffer, BUF_SIZE, &dwRead, NULL) != FALSE) {
				if (state == IPCState::NOT_INITIALIZED && !strncmp(buffer, "EHLO", 4)) {
					DEBUG_LOG("Received hello from client!");
					srand(time(NULL));
					challenge = rand();
					SendPacket("c", &challenge, 1);
					state = IPCState::READ_OK;
				}
				else if (state == IPCState::READ_OK) {
					DWORD receivedChallenge = *(DWORD*)buffer;
					if (receivedChallenge == challenge) {
						DEBUG_LOG("Received correct challenge!");
						state = IPCState::INITIALIZED;
						// TODO: Dynamically read protocol version
						if (!Parser->Setup(PROTOCOL_VERSION_140)) {
							// TODO: Alert the user that this protocol version is not
							// supported, and exit nicely.
						}
					}
					else {
						DEBUG_LOG("There was an error while loading pipes...");
					}
				}
				else if (state == IPCState::INITIALIZED && !strncmp(buffer, "BYHE", 4)) {
					DEBUG_LOG("Received bye from client!");
					state = IPCState::CLOSED;
					break;
				}
				else {
					DWORD idx = 0;
					DWORD remainingBytes = dwRead;
					while (idx < dwRead) {
						uint16_t pktLength = *(uint16_t*)(buffer + idx);
						
						if (pktLength == 0) {
							DEBUG_LOG("Something went wrong!");
							break;
						}

						if (pktLength > remainingBytes) {
							// packet is bigger than what remains in the buffer and that sucks
							// memcpy(buffer, buffer + idx, dwRead - idx);
							// recv_offset = dwRead - idx;
							DEBUG_LOG("Well that sucks...");
							break;
						}
						Parse(buffer + idx, pktLength);
						idx += pktLength;
						remainingBytes -= pktLength;
					}
				}
			}
		}

		if (state >= IPCState::INITIALIZED) {
			break;
		}

		// Disconnect if we were connected
		// DisconnectNamedPipe(IPCPipe);
	}

	free(buffer);
}

/* Parsing */

void IPCSocket::Parse(const char* data, size_t length)
{
	Parser->ParseLineage2Packet(data, length);
	pktidx++;
}

