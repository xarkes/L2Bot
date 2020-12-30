#include "MainWindow.h"

#include <Windows.h>
#include <qmessagebox.h>
#include <WbemCli.h>
#include <comdef.h>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// Initialize listening of process creation
	L2Lister = new L2ProcessLister;

	// Connect signals
	connect(L2Lister, &L2ProcessLister::processCreated, this, &MainWindow::processCreated);
	connect(ui.listWidgetProcess, &QListWidget::currentItemChanged, this, &MainWindow::l2ProcSelectionChanged);
	connect(ui.actionAttach_Bot, &QAction::triggered, this, &MainWindow::attachBot);

	// Start listening
	L2Lister->start();
}

MainWindow::~MainWindow()
{
	if (L2Lister) {

		L2Lister->stop();
		delete L2Lister;
	}
}

// Helpers

int MainWindow::DisplayError(unsigned int code)
{
	QMessageBox msgBox;
	msgBox.setText("Error");
	msgBox.setInformativeText(QString("An error occurred while initializing the program (code %1)").arg(code, 0, 16));
	msgBox.setStandardButtons(QMessageBox::Close);
	return msgBox.exec();
}

L2Proc* MainWindow::getSelectedL2Proc(QListWidgetItem* item)
{
	if (!item) {
		item = ui.listWidgetProcess->currentItem();
	}

	for (auto& l2proc : l2proclist) {
		if (l2proc->witem == item) {
			return l2proc;
		}
	}
	return nullptr;
}

// Main logic

void MainWindow::UpdateProcessList()
{
	int idx = 0;
	ui.listWidgetProcess->clear();
	for (L2Proc* l2proc : l2proclist) {
		int running = GetProcessVersion(l2proc->pid);
		if (!running) {
			// Delete the non existant process
			EraseBottingSession(l2proclist[idx]);
			// Replace it with the last item of the list, and remove the last item of the list
			l2proclist[idx] = l2proclist.back();
			l2proclist.pop_back();
			continue;
		}

		QString itemstring = QString("%1 - %2 - %3")
			.arg(l2proc->name)
			.arg(l2proc->pid)
			.arg(l2proc->attached? "Bot loaded" : "Bot not loaded");
		QListWidgetItem* item = new QListWidgetItem(itemstring, ui.listWidgetProcess);
		l2proc->witem = item;
		ui.listWidgetProcess->addItem(item);
		idx++;
	}
}

void MainWindow::EraseBottingSession(L2Proc* l2proc)
{
	if (!l2proc) {
		return;
	}
	if (l2proc->window) {
		delete l2proc->window;
	}
	delete l2proc;
}

// Slots


void MainWindow::processCreated(QString procname, UINT pid)
{
	L2Proc* l2proc = new L2Proc;
	l2proc->name = procname;
	l2proc->pid = pid;
	l2proc->witem = nullptr;
	l2proc->window = nullptr;
	l2proc->attached = false;
	l2proclist.push_back(l2proc);
	UpdateProcessList();
}

void MainWindow::l2ProcSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (!current) {
		ui.actionAttach_Bot->setEnabled(false);
		return;
	}

	L2Proc* l2proc = getSelectedL2Proc(current);
	if (!l2proc) {
		ui.actionAttach_Bot->setEnabled(false);
		return;
	}
	
	ui.actionAttach_Bot->setEnabled(!l2proc->attached);
}

void MainWindow::attachBot(bool checked)
{
	L2Proc* l2proc = getSelectedL2Proc();
	if (!l2proc || !GetProcessVersion(l2proc->pid)) {
		UpdateProcessList();
		return;
	}

	auto leftSize = ui.dockWidget_left->size();

	// Hide useless central widget
	ui.centralWidget->setMaximumWidth(5);
	ui.centralWidget->setMinimumWidth(5);
	
	// Create bot window on the right
	QDockWidget* dockWindow = new QDockWidget(this);
	dockWindow->setWindowTitle(QString("Bot - %1").arg(l2proc->pid));

	l2proc->window = new BotWindow(dockWindow);
	l2proc->window->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	l2proc->window->resize(QSize(600, 400));
	dockWindow->setWidget(l2proc->window);

	if (lastInsertedDock) {
		tabifyDockWidget(lastInsertedDock, dockWindow);
		dockWindow->show();
		dockWindow->raise();
	}
	else {
		addDockWidget(Qt::RightDockWidgetArea, dockWindow);
	}

	// Resize left window
	ui.dockWidget_left->resize(leftSize);
	lastInsertedDock = dockWindow;

	// Start hooking
	l2proc->window->SetL2Proc(l2proc);
	l2proc->window->hook();
	l2proc->attached = true;
	UpdateProcessList();
}