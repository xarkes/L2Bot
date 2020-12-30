#pragma once

#include "GameLogic.h";
#include "BotLogic.h";

#include <QFrame>

class BotWindow;

class MapWidget : public QFrame
{
	Q_OBJECT

public:
	MapWidget(QWidget *parent = Q_NULLPTR);
	~MapWidget();
	void SetGameLogic(BotWindow* botWindow, GameLogic* gl);
	void showTargetingArea(BotLogic* BL, bool show);

protected:
	void paintEvent(QPaintEvent*) override;

private:
	GameLogic* GL = nullptr;
	BotWindow* botWindow = nullptr;

	// Targeting zone
	Position targetingCenter = { 0 };
	bool showTargetZone = true;
	TargetingType targetingZoneShape = TargetingType::ANYTHING;
	int zoneRadius = 0;

	// Drawing functions
	const int TILE_SIZE = 900;
	const int GAME_TILE_LOC_SIZE = 1 << 15; // A game tile is 2**16 = 32768
	QPixmap* mapPixmap = nullptr;
	void LoadMapData(Position p);
	void DrawMap(QPainter* painter, Position p, int zoom);
};
