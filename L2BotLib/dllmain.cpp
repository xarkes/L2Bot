#include "Windows.h"
#include <stdio.h>


/// Globals used for hooking
DWORD SendPacketFP;
DWORD(_stdcall* sendPacket)(DWORD FP, const char* format, ...);
DWORD(_stdcall* realDataRecv)(BYTE* packet, DWORD length);
DWORD(_stdcall* sendMovementPacket)(double X, double Y, double Z, double XF, double XY, double XZ);

/// Address to the allocated space for instructions
LPVOID oldInstructions;

/// Pipe used for IPC
HANDLE IPCPipeR;
HANDLE IPCPipeS;
#define PIPE_RECV_PACKET_NAME L"\\\\.\\pipe\\APipeR"
#define PIPE_SEND_PACKET_NAME L"\\\\.\\pipe\\APipeS"
WCHAR RecvSocketName[MAX_PATH] = { 0 };
WCHAR SendSocketName[MAX_PATH] = { 0 };

/// Buffer used to store packet parameters
const int MAX_PARAM = 10;
DWORD parameters[MAX_PARAM];

/// Hooking parameters
auto const HOOK_METHOD = 1;


DWORD _stdcall dataReceiveToText(BYTE* packet, DWORD length)
{
    // First decrypt the packets
    DWORD result = realDataRecv(packet, length);

    // Check a console is allocated
    if (!GetConsoleWindow()) {
        AllocConsole();
        FILE* stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        printf("Console initialized!\n");
    }

    // Now print them
    printf("Packet received!\n");
    for (DWORD i = 0; i < length; i++) printf("%02X ", packet[i]);
    printf("\n");

    return result;
}

DWORD _stdcall dataReceiveIPC(BYTE* packet, DWORD length)
{
    // First decrypt the packets
    DWORD result = realDataRecv(packet, length);

    // Send data if possible
    if (IPCPipeR != INVALID_HANDLE_VALUE) {
        DWORD dwWritten;
        WriteFile(IPCPipeR, packet, length, &dwWritten, NULL);
        // FlushFileBuffers(IPCPipeR);
    }

    return result;
}

void _stdcall printPacketIPC(BYTE* packet, DWORD length)
{
    // Send data if possible
    if (IPCPipeR != INVALID_HANDLE_VALUE) {
        DWORD dwWritten;
        WriteFile(IPCPipeR, packet, length, &dwWritten, NULL);
    }
}

LPVOID Hook(LPVOID functionToHook, LPVOID myFunction, size_t size)
{
    DWORD old;
    DWORD old2;

    // Allocate memory and copy the old bytes there
    oldInstructions = malloc(5 + size);
    VirtualProtect(oldInstructions, size + 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(oldInstructions, functionToHook, size);

    // Add a jump after the copied bytes from the hooked function
    // to jump back to the rest of that hooked function.
    // This allows that when someone calls oldInstructions, it acts as the original functionToHook
    *(BYTE*)((DWORD)oldInstructions + size) = 0xE9;
    *(DWORD*)((DWORD)oldInstructions + size + 1) = (DWORD)((DWORD)functionToHook + size) - (DWORD)((DWORD)oldInstructions + size) - 5;

    // Patch the function to hook in order to jump to our own function
    VirtualProtect(functionToHook, 5, PAGE_EXECUTE_READWRITE, &old);
    *(BYTE*)functionToHook = 0xE9;
    *(DWORD*)((DWORD)functionToHook + 1) = (DWORD)myFunction - (DWORD)functionToHook - 5;
    VirtualProtect(functionToHook, 5, old, &old2);
    return oldInstructions;
}

void HookCleanup(LPVOID hookedFunction)
{
    // Repatch our main function with original bytes
    DWORD prot, _osef;
    VirtualProtect(hookedFunction, 5, PAGE_EXECUTE_READWRITE, &prot);
    memcpy(hookedFunction, oldInstructions, 5);
    VirtualProtect(hookedFunction, 5, prot, &_osef);

    // Free the allocated space
    free(oldInstructions);
}

void IPCSetup()
{
    // Initialize dynamic socket name
    DWORD PID = GetCurrentProcessId();

    memset(RecvSocketName, 0, MAX_PATH);
    lstrcatW(RecvSocketName, PIPE_RECV_PACKET_NAME);
    _itow_s(PID, RecvSocketName + lstrlenW(RecvSocketName), 5, 16);

    memset(SendSocketName, 0, MAX_PATH);
    lstrcatW(SendSocketName, PIPE_SEND_PACKET_NAME);
    _itow_s(PID, SendSocketName + lstrlenW(SendSocketName), 5, 16);

    // Open the socket - Send/Receive is from the view of the bot and game (packets from/to the server)
    IPCPipeR = CreateFileW(RecvSocketName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    IPCPipeS = CreateFileW(SendSocketName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (IPCPipeR != INVALID_HANDLE_VALUE && IPCPipeS != INVALID_HANDLE_VALUE) {
        // Send hello
        DWORD nBytes;
        WriteFile(IPCPipeR, "EHLO", 4, &nBytes, NULL);
        // Wait for challenge, expect 6 bytes: "0x63 0x00 0xde 0xad 0xc0 0xfe"
        BYTE buffer[6] = { 0 };
        auto r = ReadFile(IPCPipeS, buffer, 8, &nBytes, NULL);
        // Send back challenge
        WriteFile(IPCPipeR, buffer + 2, sizeof(DWORD), &nBytes, NULL);
    }
}

void IPCCleanup()
{
    if (IPCPipeR != INVALID_HANDLE_VALUE) {
        DWORD dwWritten;
        WriteFile(IPCPipeR, "BYHE", 4, &dwWritten, NULL);
        CloseHandle(IPCPipeR);
    }
    if (IPCPipeS != INVALID_HANDLE_VALUE) {
        CloseHandle(IPCPipeS);
    }
}

void PacketSend(BYTE* buffer)
{
    char* format = (char*) buffer;
    size_t formatLength = strlen(format);
    auto pidx = formatLength + 1;

    if (formatLength > MAX_PARAM) {
        // We can only handle at most 10 parameters
        return;
    }

    // Setup parameters buffer
    for (size_t i = 0; i < formatLength; i++)
    {
        parameters[i] = *(DWORD*)(buffer + pidx + i * 4);
    }

    if (formatLength == 1) {
        sendPacket(SendPacketFP, format, parameters[0]);
    }
    else if (formatLength == 2) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1]);
    }
    else if (formatLength == 3) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2]);
    }
    else if (formatLength == 4) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3]);
    }
    else if (formatLength == 5) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3],
            parameters[4]);
    }
    else if (formatLength == 6) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3],
            parameters[4], parameters[5]);
    }
    else if (formatLength == 7) {
        // TODO: Handle special packet here
        if (parameters[0] == 0xdeadbeef) {
            sendMovementPacket(parameters[1], parameters[2], parameters[3],
                parameters[4], parameters[5], parameters[6]);
        }
        else {
            sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3],
                parameters[4], parameters[5], parameters[6]);
        }
    }
    else if (formatLength == 8) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3],
            parameters[4], parameters[5], parameters[6], parameters[7]);
    }
    else if (formatLength == 9) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3],
            parameters[4], parameters[5], parameters[6], parameters[7], parameters[8]);
    }
    else if (formatLength == 10) {
        sendPacket(SendPacketFP, format, parameters[0], parameters[1], parameters[2], parameters[3],
            parameters[4], parameters[5], parameters[6], parameters[7], parameters[8], parameters[9]);
    }
}

bool SetOffsetsAccordingToGameVersion(DWORD* DATA_RECV_ADDR, DWORD* DATA_SEND_ADDR, DWORD* DATA_SEND_SOCKET_INFO, DWORD* DATA_SEND_MOVEMENT_ADDR, DWORD* HOOK_SIZE)
{
	/// Here are the 4 values we need to know:
    /// realDataRecv addr
    /// realDataRecv numBytes
    /// sendPacket addr
    /// sendPacket first parameter structure address

	if (false) {
		// Offsets for other protocol version Underground (l2 classic 1.5/2.5?)
        // TODO Use offsets instead
		if (HOOK_METHOD == 0) {
			*DATA_RECV_ADDR = 0x20653840;
		}
		else {
			*DATA_RECV_ADDR = 0x206539DF;
		}
		*DATA_SEND_ADDR = 0x20609810;
		*DATA_SEND_SOCKET_INFO = 0xFD8B0000;
        *DATA_SEND_MOVEMENT_ADDR = 0x0; // TODO
		*HOOK_SIZE = 6; 
        return false;
	}
	else {
        // Offsets for l2 classic club (Zaken)
		*DATA_RECV_ADDR = 0x2F4627;
		*DATA_SEND_ADDR = 0x2F2D90;
		*DATA_SEND_SOCKET_INFO = 0xFD890000;
        *DATA_SEND_MOVEMENT_ADDR = 0x2E2EA0;
		*HOOK_SIZE = 6;
        return true;
	}
}

void ProcessAttach()
{
    /// Setup IPC and initialize parameters buffer
    IPCSetup();
    memset(parameters, 0, sizeof(DWORD) * MAX_PARAM);

    /// First get the info we need about the target in order to hook
	DWORD EngineDLLBase = (DWORD)GetModuleHandle(L"engine.dll");
    DWORD DATA_RECV_ADDR, DATA_SEND_ADDR, DATA_SEND_SOCKET_INFO, DATA_SEND_MOVEMENT_ADDR, HOOK_SIZE;
    if (SetOffsetsAccordingToGameVersion(&DATA_RECV_ADDR, &DATA_SEND_ADDR, &DATA_SEND_SOCKET_INFO, &DATA_SEND_MOVEMENT_ADDR, &HOOK_SIZE)) {
        DATA_RECV_ADDR += EngineDLLBase;
        DATA_SEND_ADDR += EngineDLLBase;
        DATA_SEND_MOVEMENT_ADDR += EngineDLLBase;
    }

    /// Hook our recv function
    // Assign the values now and hook
    sendPacket = (DWORD(_stdcall *)(DWORD, const char*, ...)) DATA_SEND_ADDR;
    SendPacketFP = DATA_SEND_SOCKET_INFO;

    if (HOOK_METHOD == 0) {
        // Method 1: Hook at the beginning of the function
        // and then call the function to decrypt the packet
        // and finally print the decrypted packet
        realDataRecv = (DWORD(_stdcall*)(BYTE*, DWORD)) DATA_RECV_ADDR;
        realDataRecv = (DWORD(_stdcall*)(BYTE*, DWORD)) Hook((LPVOID)realDataRecv, dataReceiveIPC, HOOK_SIZE);
    }
    else {
        // Method 2: Hook the ret of the receive packet function
        // and print the decrypted packet.
        // This works because there is some space after the function and we do not erase anything after the 'ret' instruction
        //    206539DE | 5D      | pop ebp |
        //    206539DF | C2 0800 | ret 8   |
        //    206539E2 | CC      | int3    |
        Hook((LPVOID)DATA_RECV_ADDR, printPacketIPC, HOOK_SIZE);
    }

    /// Function is hooked, wait for some keypress to play a bit!
    BYTE* buffer = (BYTE*) malloc(1 << 16);
    while (true) {
        /*if (GetKeyState(VK_F1) & 0x8000) {
            wchar_t msg[] = L"Hello :)";
            sendPacket(SendPacketFP, "cSd", 0x49, msg, 0);
        }*/
        if (GetKeyState(VK_F12) & 0x8000) {
            printf("F12 Pressed, let's exit this!");
            break;
        }

        // Wait and check if anything must be sent
        DWORD dwBytesRead = 0;
        DWORD dwTotalBytes = 0;
        DWORD dwBytesLeft = 0;
        PeekNamedPipe(IPCPipeS, buffer, 1 << 16, &dwBytesRead, &dwTotalBytes, &dwBytesLeft);
        if (dwBytesRead) {
            auto r = ReadFile(IPCPipeS, buffer, dwTotalBytes, &dwBytesRead, NULL);
            if (r) {
                if (!strncmp((char*) buffer, "BYHE", 4)) {
                    // The bot sent "bye" so we should just clean ourselves
                    break;
                }
                else {
                    // Otherwise we want to send the received packet to the server
                    PacketSend(buffer);
                }
            }
        }
    }

    /// Cleanup
    free(buffer);
    HookCleanup((LPVOID) DATA_RECV_ADDR);
    IPCCleanup();
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessAttach, 0, 0, NULL);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

