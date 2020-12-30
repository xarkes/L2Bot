#include "Injector.h"
#include "MainWindow.h"
#include <QtWidgets/QApplication>

#define DEBUG_CREATE_NEW_EXE 0

void PreventL2FromFuckingYouUp(int argc, char *argv[])
{
#if DEBUG_CREATE_NEW_EXE
	/// This was written because my first assumption was that l2.exe was detecting which process
	/// was calling OpenProcess on it.
	/// To fix that I did the following: copy the executable in another directory, change the working directory and changed the PPID when creating the process.
	/// Now it seems this assumption was not true.
	/// My new assumption is that l2.exe will just check every executed process in the system, and keep a handle on each file.
	/// So the idea was to *never* execute the result of the compilation (EXE_NAME) to only keep other processes executed.
	/// Result -> it does not work.
	/// So, what the fuck does l2.exe do?

	if (argc == 1) {
		// Retrieve basename of executable
		size_t fullpathLength = strlen(argv[0]);
		const char* basepath = argv[0] + fullpathLength;
		while (*basepath != '\\') {
			basepath--;
		}

		if (!strcmp(basepath + 1, EXE_NAME)) {
			// Copy current executable to some other location
			WCHAR swFileName[MAX_PATH] = { 0 };
			GetModuleFileNameW(NULL, swFileName, MAX_PATH);
			LPWSTR realExe = CopyExecutableSomewhere(swFileName, L".exe");

			// Initialize temp folder
			WCHAR tempFolder[MAX_PATH];
			GetTempPathW(MAX_PATH, tempFolder);

			// Execute it
			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));

			CreateProcessW(realExe, NULL, NULL, NULL, FALSE, NULL, NULL, tempFolder, (LPSTARTUPINFOW)&si, &pi);
			auto fu = GetLastError();
			
			// Free memory and exit
			free(realExe);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			exit(fu);
		}
	}
#endif
	/// Handle arguments
	// Soft.exe [-kDLLTOINJECT.dll -pPID]

	if (argc <= 1) {
		return;
	}

	if (!strncmp(argv[1], DLL_INJECTION_CMD_PREFIX, DLL_INJECTION_CMD_PREFIX_LEN)
		&& !strncmp(argv[2], DLL_INJECTION_CMDP_PREFIX, DLL_INJECTION_CMDP_PREFIX_LEN)) {
		/// We are an injector, so let's inject
		Injector injector;
		DWORD pid = atoi(argv[2] + DLL_INJECTION_CMDP_PREFIX_LEN);
		injector.InjectDLL(pid, argv[1] + DLL_INJECTION_CMD_PREFIX_LEN);
		
		// Cleaning up the injection will wait for the thread to complete
		injector.CleanupInjection();

		// And now everything shall be fine!
		exit(0);
	}
	else {
		// We didn't understand what happened, just execute normally
	}
}

int main(int argc, char *argv[])
{
	PreventL2FromFuckingYouUp(argc, argv);

#if DEBUG_CREATE_NEW_EXE
	__debugbreak();
#endif

	// Create application and main Window
	QApplication a(argc, argv);
	MainWindow mainWindow;
	mainWindow.show();
	return a.exec();
}
