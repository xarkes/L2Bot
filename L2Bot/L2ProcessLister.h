#pragma once

#include <qthread.h>

#include "EventSink.h"

class L2ProcessLister : public QThread
{
	Q_OBJECT

private:
	bool running = false;
	WMIEventListener* ProcListener = nullptr;

	void run();

public:
	void sendMessage(WCHAR* proc, UINT pid);
	void stop();

signals:
	void processCreated(QString procname, unsigned int pid);
};