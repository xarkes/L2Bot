#pragma once

#include "GameLogic.h"
#include "BotLogic.h"
#include "Glob.h"

#include <QtWidgets/QMainWindow>
#include "ui_BotWindow.h"

class Injector;
class IPCSocket;

#define UI_INGAME() if (!GL || GL->GetState() != GameLogic::GameState::IN_GAME) { return; }

class BotWindow : public QWidget
{
	Q_OBJECT

public:
	BotWindow(QWidget *parent = Q_NULLPTR);
	~BotWindow();
	void LogMessage(QString msg);
#ifdef _DEBUG
	void LogMessageDebug(QString msg);
#endif
	void hook();
	int GetZoomLevel();
	void SetL2Proc(L2Proc* l2proc);

private:
	Ui::BotWindowClass ui;
	IPCSocket* Socket = nullptr;
	GameLogic* GL = nullptr;
	BotLogic* BL = nullptr;
	L2Proc* l2proc = nullptr;

	QMetaObject::Connection con1, con2;
#ifdef _DEBUG
	// Connection used to be log debug messages
	QMetaObject::Connection con3;
#endif
	
	// Configuration related functions
	std::wstring getConfigurationPath();
	void loadConfiguration();
	void saveConfiguration();

private slots:
	// Main UI slots
	void initializeGameUI();
	void closeGameUI();
	void clearButtonClicked();
	void socketClosed();
	void bottingButtonClicked();
	void updatePlayerStatus();
	void updateTargetStatus();
	void botStateChanged(QString state);

	// Map slots
	void refreshMap();
	void mapShowTargetingArea();

	// Entities list slots
	void addNPC(NPC* npc);
	void removeNPC(NPC* npc);

	// Combat action list slots
	void comboCombatActionTypeChanged(int index);
	void buttonCombatActionAddClicked();
	void refreshCombatActionTable();
	void showCombatActionsContextMenu(const QPoint& pos);

	// Party action list slots
	void comboPartyActionTypeChanged(int index);
	void buttonPartyActionAddClicked();
	void refreshPartyActionTable();
	void showPartyActionsContextMenu(const QPoint& pos);

	// Targeting slots
	void setBotCenter();
	void setBotTargeting();
	void enableSpoilAndSweep();

	// Rest slots
	void refreshRestOptions();
	void setRestOptions();
};
