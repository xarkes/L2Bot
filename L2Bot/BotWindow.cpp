#include "BotWindow.h"
#include "Injector.h"
#include "IPCSocket.h"
#include "Glob.h"

#include <QPushButton>
#include <QScrollBar>
#include <QMenu>
#include <qfontdatabase.h>
#include <qrunnable.h>
#include <fstream>

//#define UI_DEV
#ifdef UI_DEV
static void CreateFakeGL(GameLogic* GL, BotLogic* BL)
{
	/// Method used to do some testing for the UI that is meant to be used
	/// only when the game is running.
	GL->SetMainPlayer(L"John", 103 + 600, 65615 + 600, 1000);
	GL->AddNPC(12345, 1355, 64946, 1000, 123123, 123123, 123123);
	GL->SetMainPlayerContext(55555);
}
#endif

BotWindow::BotWindow(QWidget* parent)
	: QWidget(parent)
{
	// Setup UI
	ui.setupUi(this);
	const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	ui.textEditLog->setFont(fixedFont);

	// Setup required objects
	// TODO Fix the logic/software architecture in the future
	GL = new GameLogic();
	BL = new BotLogic(this, GL);
	connect(GL, &GameLogic::gameInitialized, this, &BotWindow::initializeGameUI);
	connect(GL, &GameLogic::sendMessage, this, &BotWindow::LogMessage);
	connect(GL, &GameLogic::npcAdded, this, &BotWindow::addNPC);
	connect(GL, &GameLogic::npcRemoved, this, &BotWindow::removeNPC);
	connect(GL, &GameLogic::environmentChanged, this, &BotWindow::refreshMap);
	connect(GL, &GameLogic::playerStatusChanged, this, &BotWindow::updatePlayerStatus);
	connect(GL, &GameLogic::playerTargetChanged, this, &BotWindow::updateTargetStatus);
	connect(GL, &GameLogic::playerTargetStatusChanged, this, &BotWindow::updateTargetStatus);
	connect(GL, &GameLogic::playerDisconnected, this, &BotWindow::closeGameUI);

	/// Connect all widgets
	connect(ui.buttonClear, &QPushButton::clicked, this, &BotWindow::clearButtonClicked);
	connect(ui.sliderZoom, &QAbstractSlider::valueChanged, this, &BotWindow::refreshMap);
	connect(ui.buttonBotting, &QPushButton::clicked, this, &BotWindow::bottingButtonClicked);
	connect(ui.checkboxMapViewzone, SIGNAL(clicked()), this, SLOT(mapShowTargetingArea()));
	// Combat action list
	connect(ui.comboCombatActionType, SIGNAL(currentIndexChanged(int)), this, SLOT(comboCombatActionTypeChanged(int)));
	connect(ui.buttonCombatActionAdd, SIGNAL(clicked()), this, SLOT(buttonCombatActionAddClicked()));
	connect(ui.tableCombatActions, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showCombatActionsContextMenu(const QPoint &)));
	ui.tableCombatActions->setContextMenuPolicy(Qt::CustomContextMenu);
	// Party action list
	connect(ui.comboPartyActionType, SIGNAL(currentIndexChanged(int)), this, SLOT(comboPartyActionTypeChanged(int)));
	connect(ui.buttonPartyActionAdd, SIGNAL(clicked()), this, SLOT(buttonPartyActionAddClicked()));
	connect(ui.tablePartyActions, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showPartyActionsContextMenu(const QPoint&)));
	ui.tablePartyActions->setContextMenuPolicy(Qt::CustomContextMenu);
	// Targeting
	connect(ui.buttonTargetingSetcenter, SIGNAL(clicked()), this, SLOT(setBotCenter()));
	connect(ui.radioTargetingAnything, SIGNAL(clicked()), this, SLOT(setBotTargeting()));
	connect(ui.radioTargetingCircle, SIGNAL(clicked()), this, SLOT(setBotTargeting()));
	connect(ui.radioTargetingAssist, SIGNAL(clicked()), this, SLOT(setBotTargeting()));
	connect(ui.checkboxTargetingSpoilSweep, SIGNAL(clicked()), this, SLOT(enableSpoilAndSweep()));

	// Rest options
	connect(ui.checkboxRestHP, SIGNAL(clicked()), this, SLOT(setRestOptions()));
	connect(ui.checkboxRestMP, SIGNAL(clicked()), this, SLOT(setRestOptions()));
	connect(ui.spinboxRestHPMax, SIGNAL(valueChanged(int)), this, SLOT(setRestOptions()));
	connect(ui.spinboxRestMPMax, SIGNAL(valueChanged(int)), this, SLOT(setRestOptions()));
	connect(ui.spinboxRestHPMin, SIGNAL(valueChanged(int)), this, SLOT(setRestOptions()));
	connect(ui.spinboxRestMPMin, SIGNAL(valueChanged(int)), this, SLOT(setRestOptions()));

	// Connect bot logic
	connect(BL, &BotLogic::stateChanged, this, &BotWindow::botStateChanged);

#ifdef UI_DEV
	CreateFakeGL(GL, BL);
#endif
}

BotWindow::~BotWindow()
{
	// Disconnect the signals
	disconnect(con1);
	disconnect(con2);
#ifdef _DEBUG
	disconnect(con3);
#endif

	// Delete allocated items
	if (GL) {
		delete GL;
	}
	if (BL) {
		if (BL->isBotRunning()) {
			BL->Stop();
		}
		delete BL;
	}
	if (Socket) {
		Socket->SayGoodBye();
		delete Socket;
	}
}

/* Utilitaries functions */

#ifdef _DEBUG
#define LogDebug(x) do { LogMessageDebug(x); } while (false);
#else
#define LogDebug(x) do { } while (false);
#endif

void BotWindow::LogMessage(QString msg)
{
	ui.textEditLog->append(msg);
	ui.textEditLog->verticalScrollBar()->setValue(ui.textEditLog->verticalScrollBar()->maximum());
}

#ifdef _DEBUG
void BotWindow::LogMessageDebug(QString msg)
{
	ui.textEditLog->append(QString("[DEBUG] %1").arg(msg));
	ui.textEditLog->verticalScrollBar()->setValue(ui.textEditLog->verticalScrollBar()->maximum());
}
#endif

void BotWindow::SetL2Proc(L2Proc* l2proc)
{
	this->l2proc = l2proc;
}

int BotWindow::GetZoomLevel()
{
	return ui.sliderZoom->value();
}

std::string GetLastErrorAsString(DWORD errorMessageID = 0xFFFFFFFF)
{
	// Get the error message, if any.
	if (errorMessageID == 0xFFFFFFFF) {
		errorMessageID = GetLastError();
	}

	if (errorMessageID == 0) {
		return std::string("No error"); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

/* Slots */

void BotWindow::initializeGameUI()
{
	ASSERT(GL);
	ASSERT(GL->GetPlayer());

	ui.map->SetGameLogic(this, GL);
	BL->SetPlayer();

	loadConfiguration();

	refreshCombatActionTable();
	refreshRestOptions();
}

void BotWindow::closeGameUI()
{
	// Avoid weird attempts at refreshing the map
	ui.map->SetGameLogic(nullptr, nullptr);

	// Stop any botting action
	BL->Stop();
	BL->SetPlayer();
	BL->CleanUp();

	// Cleanup GameLogic
	GL->Cleanup();

	// Refresh UI
	refreshMap();
	refreshCombatActionTable();
	refreshRestOptions();
	updatePlayerStatus();
	updateTargetStatus();
}

std::wstring BotWindow::getConfigurationPath()
{
	// TODO: Find game server ID or something like that in order not to rely only on the player name
	auto player = GL->GetPlayer();
	WCHAR FileNameBuffer[MAX_PATH] = { 0 };
	HMODULE hMod = GetModuleHandle(NULL);
	GetModuleFileNameW(hMod, FileNameBuffer, MAX_PATH);
	auto length = lstrlenW(FileNameBuffer);
	while (length) {
		if (FileNameBuffer[length] == '\\') {
			break;
		}
		length--;
	}
	memcpy(FileNameBuffer + length + 1, player->GetName(), (wcslen(player->GetName()) + 1) * sizeof(wchar_t));
	lstrcatW(FileNameBuffer, L".sav");
	return std::wstring(FileNameBuffer);
}

void BotWindow::loadConfiguration()
{
	std::wstring path = getConfigurationPath();
	auto infile = std::ifstream(path.c_str());
	if (infile.is_open()) {
		char* config = (char*) malloc(4096);
		infile.read(config, 4096);
		BL->LoadConfiguration((uint8_t*) config);
		free(config);
		infile.close();
	}
}

void BotWindow::saveConfiguration()
{
	std::wstring path = getConfigurationPath();
	uint32_t size = 0;
	auto config = BL->GetConfiguration(&size);
	auto outfile = std::ofstream(path.c_str());
	outfile.write((char*) config, size);
	outfile.close();
	free(config);
}

void BotWindow::hook()
{
	// Check if we have an associated l2 process...
	if (!l2proc) {
		LogDebug("Failure! No known l2 process bound to this window...");
		return;
	}

	// Listen on the named pipe if anything is incoming
	Socket = new IPCSocket(GL, l2proc->pid);
	if (!Socket->CreatePipe()) {
		LogDebug("Could not create the pipe... Stopping there sorry.");
		return;
	}
	GL->Reset();
	GL->SetSocket(Socket);

	con1 = connect(Socket, &IPCSocket::sendMessage, this, &BotWindow::LogMessage);
	con2 = connect(Socket, &QThread::finished, this, &BotWindow::socketClosed);
#ifdef _DEBUG
	con3 = connect(Socket, &IPCSocket::sendMessageDebug, this, &BotWindow::LogMessageDebug);
#endif
	Socket->start();

	// Now hook and connect our named pipe
	auto rCode = DeleguateInjection(l2proc->pid);
	if (rCode) {
		// Here the process could not be created, so let's just exit.
		LogDebug("Creation of injector failed, deleting the socket.");
		QString errorMessage(GetLastErrorAsString(rCode).c_str());
		LogDebug(errorMessage);
		Socket->terminate();
		return;
	}

	// The only thing we know is that the process creation was fine.
	// We have no idea if the injection actually worked.
	// Let's hope it did! And now wait on the socket :-)
	LogDebug("Hooking worked? Hmm we are not so sure... Let's wait.");
}

void BotWindow::clearButtonClicked()
{
	ui.textEditLog->clear();
	// Clear and reset NPC table, this will trigger map refresh as well
	ui.tableNPC->setRowCount(0);
	for (auto& ent : GL->GetEntities()) {
		if (ent.second && ent.second->isNPC()) {
			addNPC(static_cast<NPC*>(ent.second));
		}
	}
}

void BotWindow::socketClosed()
{
	// If state is closed, cleanup the socket
	LogDebug(QString("Socket was closed with status %1").arg((int) Socket->GetState()));
	delete Socket;
	Socket = nullptr;

	// Disconnect the signals
	disconnect(con1);
	disconnect(con2);
#ifdef _DEBUG
	disconnect(con3);
#endif

	ui.map->repaint();
}

void BotWindow::addNPC(NPC* npc)
{
	uint32_t rowIdx = ui.tableNPC->rowCount();
	ui.tableNPC->insertRow(rowIdx);
	
	auto entityID = QString("%1").arg(npc->GetID(), 0, 16);
	auto npcID = QString("%1").arg(npc->GetNPCID(), 0, 16);
	auto pos = npc->GetPos();
	auto X = QString::number(pos.x);
	auto Y = QString::number(pos.y);
	auto Z = QString::number(pos.z);

	ui.tableNPC->setItem(rowIdx, 0, new QTableWidgetItem(entityID));
	ui.tableNPC->setItem(rowIdx, 1, new QTableWidgetItem(npcID));
	ui.tableNPC->setItem(rowIdx, 2, new QTableWidgetItem(X));
	ui.tableNPC->setItem(rowIdx, 3, new QTableWidgetItem(Y));
	ui.tableNPC->setItem(rowIdx, 4, new QTableWidgetItem(Z));

	refreshMap();
}

void BotWindow::removeNPC(NPC* npc)
{
	auto tableModel = ui.tableNPC->model();
	auto rows = tableModel->match(tableModel->index(0, 0), 0, QString("%1").arg(npc->GetID(), 0, 16));
	for (const auto row : rows) {
		ui.tableNPC->removeRow(row.row());
	}
	refreshMap();
}

void BotWindow::refreshMap()
{
	ui.map->repaint();
}

void BotWindow::bottingButtonClicked()
{
	if (BL->isBotRunning()) {
		ui.buttonBotting->setText("Start botting");
		BL->Stop();
	}
	else {
		ui.buttonBotting->setText("Stop botting");
		// Start the botting thread
		BL->Start();
	}
}

static void updateBar(QProgressBar* bar, uint32_t val, uint32_t max)
{
	if (max > 0x7fffffff) {
		bar->setMaximum(max / 1000);
		bar->setValue(val/ 1000);
	}
	else {
		bar->setMaximum(max);
		bar->setValue(val);
	}
	bar->setFormat(QString::number(val) + "/" + QString::number(max) + " (%p%)");
}

void BotWindow::updatePlayerStatus()
{
	ASSERT(GL);
	auto player = GL->GetPlayer();
	if (player) {
		auto pname = QString::fromWCharArray(player->GetName());
		pname.append("- Lvl: %1");
		pname = pname.arg(player->GetLevel());
		ui.labelPlayerName->setText(pname);

		auto maxHP = player->GetHPMax();
		auto curHP = player->GetHPCur();
		auto maxMP = player->GetMPMax();
		auto curMP = player->GetMPCur();

		updateBar(ui.barHP, curHP, maxHP);
		updateBar(ui.barMP, curMP, maxMP);
	} else {
		ui.labelPlayerName->setText("");
		ui.barHP->reset();
		ui.barMP->reset();
	}
}

void BotWindow::updateTargetStatus()
{
	ASSERT(GL);
	auto player = GL->GetPlayer();
	if (player) {
		auto target = player->GetTarget();
		if (!target) {
			ui.labelTargetName->setText(" - ");
			updateBar(ui.barTargetHP, 0, 100);
			updateBar(ui.barTargetMP, 0, 100);
		}
		else {
			auto tname = QString("Unk");
			tname.append("- Lvl: %1");
			tname = tname.arg(target->GetLevel());
			ui.labelTargetName->setText(tname);

			auto curTargetHP = target->GetHPCur();
			auto maxTargetHP = target->GetHPMax();
			auto curTargetMP = target->GetMPCur();
			auto maxTargetMP = target->GetMPMax();
			if (maxTargetMP == 0) {
				maxTargetMP = 100;
				curTargetMP = 0;
			}

			updateBar(ui.barTargetHP, curTargetHP, maxTargetHP);
			updateBar(ui.barTargetMP, curTargetMP, maxTargetMP);
		}
	}
	else {
		ui.labelTargetName->setText("");
		ui.barTargetHP->reset();
		ui.barTargetMP->reset();
	}
}

/* Combat tab */

void BotWindow::comboCombatActionTypeChanged(int index)
{
	UI_INGAME();

	if (index == 0) {
		ui.comboCombatAction->clear();
	}
	else if (index == 1) {
		ui.comboCombatAction->clear();

		// Fill the list with skills
		auto skills = GL->GetSkills();
		for (auto& S : skills) {
			auto skillName = QString("%1 - %2");
			skillName = skillName.arg(S.ID).arg(S.Level);
			ui.comboCombatAction->addItem(skillName);
		}
	}
	else {
		ui.comboCombatAction->clear();
		ui.comboCombatAction->addItem("1 - Attack");
	}
}

void BotWindow::buttonCombatActionAddClicked()
{
	UI_INGAME();

	uint32_t actionID = ui.comboCombatAction->currentText().split(" ")[0].toUInt();
	auto engageOnly = ui.checkboxCombatActionEngage->isChecked();
	auto delay = ui.spinboxCombatActionReusedelay->value();
	BL->AddCombatAction(ui.comboCombatActionType->currentIndex(), actionID, engageOnly, delay);

	saveConfiguration();
	refreshCombatActionTable();
}

void BotWindow::refreshCombatActionTable()
{
	ui.tableCombatActions->setRowCount(0);

	if (!BL) {
		return;
	}

	auto actions = BL->GetRegisteredCombatActions();
	
	for (auto& action : actions) {
		uint32_t rowIdx = ui.tableCombatActions->rowCount();
		ui.tableCombatActions->insertRow(rowIdx);

		auto actionType = QString("%1").arg(action.Type, 0, 10);
		auto actionID = QString::number(action.ID);
		auto engage = action.EngageOnly ? QString("Yes") : QString("No");
		auto reuseDelay = QString::number(action.Delay);

		ui.tableCombatActions->setItem(rowIdx, 0, new QTableWidgetItem(actionType));
		ui.tableCombatActions->setItem(rowIdx, 1, new QTableWidgetItem(actionID));
		ui.tableCombatActions->setItem(rowIdx, 2, new QTableWidgetItem(engage));
		ui.tableCombatActions->setItem(rowIdx, 3, new QTableWidgetItem(reuseDelay));
	}
}

void BotWindow::showCombatActionsContextMenu(const QPoint& pos)
{
	UI_INGAME();

	auto item = ui.tableCombatActions->itemAt(pos);
	if (!item) {
		return;
	}

	QPoint globalPos = ui.tableCombatActions->mapToGlobal(pos);
	QMenu menu;
	menu.addAction("Delete action");
	QAction* selectedItem = menu.exec(globalPos);
	if (selectedItem) {
		auto r = item->row();
		auto tableModel = ui.tableCombatActions->model();
		auto ActionType = tableModel->index(r, 0).data().toUInt();
		auto ActionID = tableModel->index(r, 1).data().toUInt();
		BL->RemoveCombatAction(ActionType, ActionID);
		refreshCombatActionTable();
	}
}

/* Party tab */

void BotWindow::comboPartyActionTypeChanged(int index)
{
	UI_INGAME();

	if (index == 0) {
		ui.comboPartyAction->clear();
	}
	else if (index == 1) {
		ui.comboPartyAction->clear();

		// Fill the list with skills
		auto skills = GL->GetSkills();
		for (auto& S : skills) {
			auto skillName = QString("%1 - %2");
			skillName = skillName.arg(S.ID).arg(S.Level);
			ui.comboPartyAction->addItem(skillName);
		}
	}
	else {
		ui.comboPartyAction->clear();
		ui.comboPartyAction->addItem("1 - Attack");
	}

	// Fill the condition list
	for (int i = 0; i < sizeof(ConditionString) / sizeof(char*); i++) {
		auto condName = QString("%1").arg(ConditionString[i]);
		ui.comboPartyActionCondition->addItem(condName);
	}
}

void BotWindow::buttonPartyActionAddClicked()
{
	UI_INGAME();

	uint32_t actionID = ui.comboPartyAction->currentText().split(" ")[0].toUInt();
	auto delay = ui.spinboxPartyActionReusedelay->value();
	Condition condition = (Condition) ui.comboPartyActionCondition->currentIndex();
	auto condval = ui.lineEditPartyActionCondition->text().toUInt();
	BL->AddPartyAction(ui.comboPartyActionType->currentIndex(), actionID, delay, condition, condval);

	saveConfiguration();
	refreshPartyActionTable();
}

void BotWindow::refreshPartyActionTable()
{
	ui.tablePartyActions->setRowCount(0);

	if (!BL) {
		return;
	}

	auto actions = BL->GetRegisteredPartyActions();

	for (auto& action : actions) {
		uint32_t rowIdx = ui.tablePartyActions->rowCount();
		ui.tablePartyActions->insertRow(rowIdx);

		auto actionType = QString("%1").arg(action.Type, 0, 10);
		auto actionID = QString::number(action.ID);
		auto condition = QString("%1").arg(ConditionString[(uint8_t) action.condition]);
		auto reuseDelay = QString::number(action.Delay);

		ui.tablePartyActions->setItem(rowIdx, 0, new QTableWidgetItem(actionType));
		ui.tablePartyActions->setItem(rowIdx, 1, new QTableWidgetItem(actionID));
		ui.tablePartyActions->setItem(rowIdx, 2, new QTableWidgetItem(condition));
		ui.tablePartyActions->setItem(rowIdx, 3, new QTableWidgetItem(reuseDelay));
	}
}

void BotWindow::showPartyActionsContextMenu(const QPoint& pos)
{
	UI_INGAME();

	auto item = ui.tablePartyActions->itemAt(pos);
	if (!item) {
		return;
	}

	QPoint globalPos = ui.tablePartyActions->mapToGlobal(pos);
	QMenu menu;
	menu.addAction("Delete action");
	QAction* selectedItem = menu.exec(globalPos);
	if (selectedItem) {
		auto r = item->row();
		auto tableModel = ui.tablePartyActions->model();
		auto ActionType = tableModel->index(r, 0).data().toUInt();
		auto ActionID = tableModel->index(r, 1).data().toUInt();
		BL->RemoveCombatAction(ActionType, ActionID);
		refreshCombatActionTable();
	}
}

/***************************************************/

void BotWindow::botStateChanged(QString state)
{
	ui.labelBotState->setText("State: " + state);
}

void BotWindow::setBotCenter()
{
	UI_INGAME();

	int radius = ui.spinboxTargetingRadius->value();
	Position center = BL->SetTargetingCenter(radius);
	QString text = QString("X: %1 Y: %2 Z: %3").arg(center.x).arg(center.y).arg(center.z);
	ui.labelTargetingCenter->setText(text);

	setBotTargeting();
	saveConfiguration();
	mapShowTargetingArea();
}

void BotWindow::setBotTargeting()
{
	UI_INGAME();

	if (ui.radioTargetingAnything->isChecked()) {
		BL->SetTargetingType(TargetingType::ANYTHING);
	}
	else if (ui.radioTargetingCircle->isChecked()) {
		BL->SetTargetingType(TargetingType::CENTER);
	}
	else if (ui.radioTargetingAssist->isChecked()) {
		BL->SetTargetingType(TargetingType::ASSIST);
		WCHAR name[32] = { 0 };
		ui.lineEditTargetingAssistName->text().toWCharArray(name);
		BL->SetTargetingAssist(name);
	}
	saveConfiguration();
}

void BotWindow::enableSpoilAndSweep()
{
	UI_INGAME();

	auto enable = ui.checkboxTargetingSpoilSweep->isChecked();
	BL->EnableSpoilAndSweep(enable);
	saveConfiguration();
}

void BotWindow::mapShowTargetingArea()
{
	UI_INGAME();

	ui.map->showTargetingArea(BL, ui.checkboxMapViewzone->isChecked());
	refreshMap();
}

void BotWindow::refreshRestOptions()
{
	if (!BL) {
		return;
	}

	auto restOptions = BL->GetRestOptions();
	ui.checkboxRestHP->setChecked(restOptions.HPRest);
	ui.spinboxRestHPMin->setValue(restOptions.HPRestMin);
	ui.spinboxRestHPMax->setValue(restOptions.HPRestMax);
	ui.checkboxRestMP->setChecked(restOptions.MPRest);
	ui.spinboxRestMPMin->setValue(restOptions.MPRestMin);
	ui.spinboxRestMPMax->setValue(restOptions.MPRestMax);
}

void BotWindow::setRestOptions()
{
	UI_INGAME();

	auto restHPCond = ui.checkboxRestHP->isChecked();
	auto restHPMin = ui.spinboxRestHPMin->value();
	auto restHPMax = ui.spinboxRestHPMax->value();

	auto restMPCond = ui.checkboxRestMP->isChecked();
	auto restMPMin = ui.spinboxRestMPMin->value();
	auto restMPMax = ui.spinboxRestMPMax->value();

	BL->SetRestOptions(restHPCond, restHPMin, restHPMax, restMPCond, restMPMin, restMPMax);
	refreshRestOptions();
}