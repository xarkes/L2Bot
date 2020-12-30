#include "L2ProcessLister.h"

void L2ProcessLister::run()
{
	ProcListener = new WMIEventListener();
	int ret = ProcListener->StartListening();
	if (ret) {
		return;
	}

	ProcListener->SetProxy(this);
	running = true;
	while (running) {
		Sleep(10000);
	}
}

void L2ProcessLister::stop()
{
	running = false;
	ProcListener->StopListening();
	delete ProcListener;
	this->terminate();
}

void L2ProcessLister::sendMessage(WCHAR* proc, UINT pid)
{
	QString procname = QString::fromWCharArray(proc);
	if (procname.contains("L2.exe") || procname.contains("l2.exe")) {
		emit this->processCreated(procname, pid);
	}
}