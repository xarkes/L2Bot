#pragma once
#include <Windows.h>
#include <qstring.h>

/// Variables depending on the project
#define EXE_NAME "BotWindow.exe"
#define DLL_PREFIX_NAME L"L2Bot"
#define DLL_FULL_PATH L"D:\\Sources\\L2Bot\\Release\\L2BotLib.dll"
#define PPID_SPOOFED L"svchost.exe"

#define DLL_INJECTION_CMD_PREFIX "-k"
#define DLL_INJECTION_CMD_PREFIX_L L"-k"
#define DLL_INJECTION_CMD_PREFIX_LEN 2
#define DLL_INJECTION_CMDP_PREFIX "-p"
#define DLL_INJECTION_CMDP_PREFIX_L L"-p"
#define DLL_INJECTION_CMDP_PREFIX_LEN 2

void GenRandomString(char* s, const int len);
LPWSTR CopyExecutableSomewhere(WCHAR* fileName, WCHAR* suffix);
DWORD GetProcessIdFromName(wchar_t* targetProcess);
DWORD DeleguateInjection(DWORD targetProcess);

class Injector
{
private:
	HANDLE hProcess = 0;
	LPVOID lpLibraryBuffer = 0;
	HANDLE hThread = 0;
	bool injected = false;


public:
	void Log(QString message);
	void LogError(QString message);
	void InjectDLL(DWORD PID, const char* DLL);
	void CleanupInjection();
	bool IsInjected();
};
