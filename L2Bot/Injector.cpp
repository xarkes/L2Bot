#include "Injector.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <thread>

static bool seedSet = false;

#ifdef _DEBUG
void Injector::Log(QString message)
{
}

void Injector::LogError(QString message)
{
}
#else
#define Log(x)
#define LogError(x)
#endif

void Injector::InjectDLL(DWORD PID, const char* DLL)
{
    SIZE_T nb = 0;

    /// Do the injection
    // Open process and allocate memory
    hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, PID);
    if (hProcess) {
        // Allocate space in remote process
        lpLibraryBuffer = VirtualAllocEx(hProcess, NULL, MAX_PATH, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (lpLibraryBuffer) {
            // Write DLL name
            WriteProcessMemory(hProcess, lpLibraryBuffer, DLL, strlen(DLL), &nb);

            // Create a thread in the target process with entry point LoadLibraryW and parameter FileNameBufferRemote.
            HMODULE hMod = GetModuleHandleW(L"kernel32.dll");
            if (!hMod) {
                LogError("Cannot find kernel32.dll!");
            }

            // Get proc addr
            LPVOID startAddress = GetProcAddress(hMod, "LoadLibraryA");
            hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)startAddress, lpLibraryBuffer, 0, NULL);
            if (hThread) {
                Log("Remote thread started!");
                injected = true;
                return;
            }

            // Only do this if you are certain the thread has exited
            VirtualFreeEx(hProcess, lpLibraryBuffer, 0, MEM_RELEASE);
        }

        // Close handle for process
        CloseHandle(hProcess);
    }
    Log("DLL injection complete!");
}

void Injector::CleanupInjection()
{
    if (!injected) {
        return;
    }
    Log("Waiting for DLL to complete its job...");
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    // Call FreeLibrary
    HMODULE hMod = GetModuleHandleW(L"kernel32.dll");
    if (!hMod) {
        LogError("Cannot find kernel32.dll!");
    }
    LPVOID startAddress = GetProcAddress(hMod, "FreeLibrary");
    hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)startAddress, lpLibraryBuffer, 0, NULL);
    if (hThread) {
        // Wait again for it to complete
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    // Free memory
    VirtualFreeEx(hProcess, lpLibraryBuffer, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    Log("DLL removed from target process.");
}

bool Injector::IsInjected()
{
    return injected;
}


/******************
  Non class functions
  *******************/


void GenRandomString(char* s, const int len) {
    if (!seedSet) {
        srand(time(NULL));
        seedSet = true;
    }

    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

DWORD GetProcessIdFromName(wchar_t* targetProcess)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if (_wcsicmp(entry.szExeFile, targetProcess) == 0)
            {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        }
    }
    CloseHandle(snapshot);
    return 0;
}

LPWSTR CopyExecutableSomewhere(WCHAR* fileName, WCHAR* suffix)
{
    // Make sure the extension is something like .exe or .dll
    if (lstrlenW(suffix) != 4) {
        return nullptr;
    }

    // Copy the executable and execute the new one
    // Get new folder location (temporary path)
    const int EXE_NAME_LEN = 10;
    WCHAR tempFolder[MAX_PATH - EXE_NAME_LEN] = { 0 };
    GetTempPathW(MAX_PATH - EXE_NAME_LEN, tempFolder);

    // Copy the DLL somewhere else
    WCHAR* newFile = (WCHAR*) malloc(sizeof(WCHAR) * MAX_PATH);
    if (!newFile) {
        return nullptr;
    }
    memset(newFile, 0, sizeof(WCHAR) * MAX_PATH);
    lstrcatW(newFile, tempFolder);
    const int SUFFIX_LEN = 4;
    CHAR randomName[EXE_NAME_LEN - SUFFIX_LEN + 1] = { 0 };
    GenRandomString(randomName, EXE_NAME_LEN - SUFFIX_LEN - 3);
    WCHAR wRandomName[EXE_NAME_LEN - SUFFIX_LEN + 1] = { 0 };
    mbstowcs(wRandomName, randomName, EXE_NAME_LEN - SUFFIX_LEN);
    wRandomName[EXE_NAME_LEN - SUFFIX_LEN] = 0;
    lstrcatW(newFile, DLL_PREFIX_NAME);
    lstrcatW(newFile, wRandomName);
    lstrcatW(newFile, suffix);


    if (!CopyFile(fileName, newFile, false)) {
        return nullptr;
    }

    return newFile;
}

DWORD DeleguateInjection(DWORD targetProcess)
{
    // 1. Copy our executable that will be responsible for injection
    WCHAR exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    WCHAR* NewEXE = CopyExecutableSomewhere(exe, L".exe");

    // 2. Copy the DLL we want to inject
    WCHAR DLL[] = DLL_FULL_PATH; // TODO: Inject the .dll next to the .exe
    WCHAR* NewDLL = CopyExecutableSomewhere(DLL, L".dll");

    if (!NewEXE || !NewDLL) {
        return -1;
    }

    // 3. Start the process with a different working directory and a new name
    // so this one does not get detected.
    // As the above conditions were not sufficient, let's spawn it from explorer.exe as if someone
    // double clicked the icon (PPID spoofing)

    // Open svchost.exe
    auto PID = GetProcessIdFromName(PPID_SPOOFED);
    if (!PID) {
        return -1;
    }
    HANDLE hExplorer = OpenProcess(PROCESS_ALL_ACCESS, false, PID);
    if (!hExplorer) {
        return -1;
    }

    /// Initialize the startupinfo and PPID
    STARTUPINFOEXW sInfo;
    PROCESS_INFORMATION pInfo;
    SIZE_T size = 0;
    ZeroMemory(&sInfo, sizeof(sInfo));
    ZeroMemory(&pInfo, sizeof(pInfo));
    
    // Get needed space for attribute list
    bool bRet = InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    if (bRet || size == 0) {
        return -1;
    }
    // Allocate memory
    sInfo.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (!sInfo.lpAttributeList) {
        return -1;
    }
    // Initialize attribute list
    bRet = InitializeProcThreadAttributeList(sInfo.lpAttributeList, 1, 0, &size);
    if (!bRet || !sInfo.lpAttributeList) {
        return -1;
    }
    // Put in the parent process attribute
    bRet = UpdateProcThreadAttribute(sInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hExplorer, sizeof(hExplorer), NULL, NULL);
    if (!bRet) {
        return -1;
    }
    sInfo.StartupInfo.cb = sizeof(sInfo);
    
    // Initialize temp folder and command line
    WCHAR tempFolder[MAX_PATH];
    GetTempPathW(MAX_PATH, tempFolder);

    // Command line is of the form
    // Soft.exe -kDLLTOINJECT.dll -pPID
    LPWSTR swCmdLine = (LPWSTR)malloc(MAX_PATH * 4);
    if (!swCmdLine) {
        return -1;
    }
    memset(swCmdLine, 0, MAX_PATH * 4);

    lstrcatW(swCmdLine, NewEXE);
    lstrcatW(swCmdLine, L" "  DLL_INJECTION_CMD_PREFIX_L);

    lstrcatW(swCmdLine, NewDLL);
    lstrcatW(swCmdLine, L" " DLL_INJECTION_CMDP_PREFIX_L);

    LPWSTR swPID = (LPWSTR)malloc(MAX_PATH);
    if (!swPID) {
        return -1;
    }
    memset(swPID, 0, MAX_PATH);
    _itow(targetProcess, swPID, 10);
    lstrcatW(swCmdLine, swPID);

    // Spawn the process
    auto ret = CreateProcessW(NULL, swCmdLine, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, tempFolder, (LPSTARTUPINFOW)&sInfo, &pInfo);
    if (!ret) {
        ret = GetLastError();
    }
    else {
        ret = 0;
    }

    free(swPID);
    free(swCmdLine);
    free(NewDLL);
    free(NewEXE);

    CloseHandle(pInfo.hProcess);
    CloseHandle(pInfo.hThread);

    return ret;
}
