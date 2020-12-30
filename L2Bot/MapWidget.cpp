#include "MapWidget.h"
#include "BotWindow.h"

#include <QPainter.h>


MapWidget::MapWidget(QWidget *parent)
	: QFrame(parent)
{
	// Set background color
	QPalette colors;
	colors.setColor(QPalette::Background, QColor("#99ccff"));
	setAutoFillBackground(true);
	setPalette(colors);
}

void MapWidget::SetGameLogic(BotWindow* botWindow, GameLogic* gl)
{
	this->botWindow = botWindow;
	GL = gl;
}

void MapWidget::LoadMapData(Position p)
{
	// 0 -> 32768, 65536+ = 20_20
	// -32768 -> 0, 65536+ = 19_20
	const int radareZeroX = 20;
	const int radareZeroY = 18;
	int RadareX = radareZeroX + p.x / GAME_TILE_LOC_SIZE;
	int RadareY = radareZeroY + p.y / GAME_TILE_LOC_SIZE;

	// Put NTILES*NTILES tiles in cache (the higher, the more memory usage)
	const int NTILES = 3;
	mapPixmap = new QPixmap(TILE_SIZE * NTILES, TILE_SIZE * NTILES);
	QPainter cachePainter(mapPixmap);
	auto startx = RadareX - NTILES / 2;
	auto starty = RadareY - NTILES / 2;
	for (auto sy = 0; sy < NTILES; sy++) {
		for (auto sx = 0; sx < NTILES; sx++) {
			uint32_t ox = startx + sx;
			uint32_t oy = starty + sy;
			if (ox < 0 || oy < 0) {
				continue;
			}

			QPixmap pixmap;
			QString mapName("D:\\Sources\\L2Bot\\Maps\\%1_%2.jpg");
			mapName = mapName.arg(ox).arg(oy);
			pixmap.load(mapName);
			cachePainter.drawPixmap(sx * TILE_SIZE, sy * TILE_SIZE, pixmap.width(), pixmap.height(), pixmap);
		}
	}
}

void MapWidget::DrawMap(QPainter* painter, Position p, int zoom)
{
	if (!mapPixmap) {
		LoadMapData(p);
	}
	qreal const pixel_ratio = ((qreal) GAME_TILE_LOC_SIZE / (qreal) TILE_SIZE);
	qreal z = zoom;

	/// Let's say it's a problem in a 2d space where
	/// P is player position
	/// V is viewport position (top left)
	/// M is map position (top left)
	///
	///
	///    M
	///     +------+------+------+
	///     |      |      |      |
	///     |  V___|______|______|____
	///     +---|--+------+------+   |
	///     |   |  |      |      |   |
	///     |   |  |      P      |   |
	///     +---|--+------+------+   |
	///     |   |__|______|______|___|
	///     |      |      |      |
	///     +------+------+------+
	///
	/// The map is positioned relatively to the top left of the viewport (painter->drawPixmap)
	/// So we need to compute VM s.t. VM = VP + PM
	/// P is always in the middle of the viewport, so VP is trivial to compute
	/// PM can be computed using the game coordinates

	auto locx_pixel = (p.x % GAME_TILE_LOC_SIZE) / pixel_ratio;
	auto locy_pixel = (p.y % GAME_TILE_LOC_SIZE) / pixel_ratio;
	auto PMx = - TILE_SIZE - locx_pixel;
	auto PMy = - TILE_SIZE - locy_pixel;
	PMx /= (z / 50);
	PMy /= (z / 50);
	auto VPx = width() / 2;
	auto VPy = height() / 2;
	
	auto VMx = PMx + VPx;
	auto VMy = PMy + VPy;
	auto w = mapPixmap->width() / (z / 50);
	auto h = mapPixmap->height() / (z / 50);

	painter->drawPixmap(VMx, VMy, w, h, *mapPixmap);
}

void MapWidget::paintEvent(QPaintEvent*)
{
	// To continue drawing, the game logic must be set!
	if (!GL || !botWindow) {
		return;
	}

	const int EWIDTH = 2;
	auto ZOOM = botWindow->GetZoomLevel();

	auto wMid = width() / 2;
	auto hMid = height() / 2;

	QPoint topLeft(wMid - EWIDTH, hMid - EWIDTH);
	QPoint bottomRight(wMid + EWIDTH, hMid + EWIDTH);
	QRect playerRect(topLeft, bottomRight);
	QPainter painter(this);

	auto player = GL->GetPlayer();
	QColor pcolor = player ? Qt::red : Qt::white;

	// First, draw map
	DrawMap(&painter, player->GetPos(), ZOOM);

	// Player at the center
	painter.setPen(QPen(pcolor, 1));
	painter.drawRect(playerRect);

	// To continue drawing, the player must be valid.
	if (!player) {
		return;
	}
	auto playerPos = player->GetPos();

	// Draw targeting zone
	if (showTargetZone) {
		auto zoneColor = QColor(210, 61, 227, 200);
		painter.setPen(QPen(zoneColor, 1));
		if (targetingZoneShape == TargetingType::CENTER) {
			// Draw the circle
			auto zoomedRadius = zoneRadius / ZOOM;
			auto centerMapX = (targetingCenter.x - playerPos.x - zoneRadius) / ZOOM;
			auto centerMapY = (targetingCenter.y - playerPos.y - zoneRadius) / ZOOM;

			painter.drawEllipse(centerMapX + wMid, centerMapY + hMid, zoomedRadius * 2, zoomedRadius * 2);
		}
	}

	// Entities around
	painter.setPen(QPen(Qt::black, 1));
	auto gameEntities = GL->GetEntities();
	for (auto& entity : gameEntities) {
		if (!entity.second || entity.second == player) {
			continue;
		}

		auto E = entity.second;

		auto entityPos = E->GetPos();
		auto entityMapX = (entityPos.x - playerPos.x) / ZOOM;
		auto entityMapY = (entityPos.y - playerPos.y) / ZOOM;

		if (E->isPlayer()) {
			pcolor = QColor("#FF5D00");
		}
		else if (E->isNPC()) {
			NPC* npc = static_cast<NPC*>(E);
			if (npc->IsAttackable()) {
				pcolor = QColor("#0021B6");
			}
			else {
				pcolor = QColor("#E42194");
			}
		}
		else if (E->isItem()) {
			pcolor = Qt::yellow;
		}
		else {
			// Unknown entity type?
			pcolor = Qt::black;
		}
		
		QPoint tL(entityMapX - EWIDTH + wMid, entityMapY - EWIDTH + hMid);
		QPoint bR(entityMapX + EWIDTH + wMid, entityMapY + EWIDTH + hMid);
		QRect entityRect(tL, bR);
		painter.setPen(QPen(pcolor, 1));
		if (E == player->GetTarget()) {
			painter.fillRect(entityRect, pcolor);
		}
		else {
			painter.drawRect(entityRect);
		}
	}
}

MapWidget::~MapWidget()
{
}

void MapWidget::showTargetingArea(BotLogic* BL, bool show)
{
	targetingCenter = BL->GetTargetingCenter();
	showTargetZone = show;

	zoneRadius = BL->GetTargetingCenterRadius();
	targetingZoneShape = BL->GetTargetingType();
}
