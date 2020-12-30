#pragma once

#include <QMainWindow>
#include "ui_MainWindow.h"

#include "L2ProcessLister.h"
#include "BotWindow.h"

#include "Glob.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = Q_NULLPTR);
	~MainWindow();

private:
	Ui::MainWindow ui;
	L2ProcessLister* L2Lister = nullptr;
	std::vector<L2Proc*> l2proclist;
	QDockWidget* lastInsertedDock = nullptr;

	// Helpers
	static int DisplayError(unsigned int code);
	L2Proc* getSelectedL2Proc(QListWidgetItem* item = nullptr);

	// Main logic
	void UpdateProcessList();
	void EraseBottingSession(L2Proc* l2proc);

private slots:
	void processCreated(QString procname, unsigned int pid);
	void l2ProcSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void attachBot(bool checked = false);
};
