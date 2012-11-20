/*
 * tilesetview.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tilesetview.h"

#include "map.h"
#include "mapdocument.h"
#include "preferences.h"
#include "propertiesdialog.h"
#include "tmxmapwriter.h"
#include "tile.h"
#include "tileset.h"
#include "tilesetmodel.h"
#include "utils.h"
#include "zoomable.h"

#include <QAbstractItemDelegate>
#include <QCoreApplication>
#include <QFileDialog>
#include <QGesture>
#include <QGestureEvent>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QPinchGesture>
#include <QUndoCommand>
#include <QWheelEvent>

#include <QDebug>

using namespace Tiled;
using namespace Tiled::Internal;

namespace {

/**
 * The delegate for drawing tile items in the tileset view.
 */
class TileDelegate : public QAbstractItemDelegate
{
public:
    TileDelegate(TilesetView *tilesetView, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mTilesetView(tilesetView)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

private:
    TilesetView *mTilesetView;
};

void TileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const QVariant display = index.model()->data(index, Qt::DecorationRole);
    const QPixmap tileImage = display.value<QPixmap>();
    const int extra = mTilesetView->drawGrid() ? 1 : 0;
    const qreal zoom = mTilesetView->scale();

    // Compute rectangle to draw the image in: bottom- and left-aligned
    QRect targetRect = option.rect.adjusted(0, 0, -extra, -extra);
    targetRect.setTop(targetRect.top() + targetRect.height() - tileImage.height() * zoom);
    targetRect.setRight(targetRect.right() - targetRect.width() + tileImage.width() * zoom);

    // Draw the tile image
    if (Zoomable *zoomable = mTilesetView->zoomable())
        if (zoomable->smoothTransform())
            painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->drawPixmap(targetRect, tileImage);

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(targetRect, option.palette.highlight());
        painter->setOpacity(opacity);
    }

    if (mTilesetView->isEditTerrain()) {
        for (int corner = 0; corner < 4; ++corner) {
            if (static_cast<const TilesetModel*>(index.model())->tileAt(index)->cornerTerrainId(corner) == mTilesetView->terrainId()) {
                QPoint pos;
                switch (corner) {
                case 0: pos = option.rect.topLeft(); break;
                case 1: pos = option.rect.topRight(); break;
                case 2: pos = option.rect.bottomLeft(); break;
                case 3: pos = option.rect.bottomRight(); break;
                }

                painter->save();
                painter->setBrush(Qt::gray);
                painter->setPen(Qt::NoPen);
                painter->setClipRect(option.rect.adjusted(0, 0, -extra, -extra));
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setOpacity(0.5);
                painter->drawEllipse(pos, option.rect.width() / 2, option.rect.height() / 2);
                painter->setOpacity(1);
                QPen pen(Qt::darkGray);
                pen.setWidth(2);
                painter->setPen(pen);
                painter->drawEllipse(pos, option.rect.width() / 4, option.rect.height() / 4);
                painter->restore();
            }
        }

        // Overlay with terrain corner indication when hovered
        if (index == mTilesetView->hoveredIndex()) {
            QPoint pos;
            switch (mTilesetView->hoveredCorner()) {
            case 0: pos = option.rect.topLeft(); break;
            case 1: pos = option.rect.topRight(); break;
            case 2: pos = option.rect.bottomLeft(); break;
            case 3: pos = option.rect.bottomRight(); break;
            }

            painter->save();
            painter->setBrush(option.palette.highlight());
            painter->setPen(Qt::NoPen);
            painter->setClipRect(option.rect.adjusted(0, 0, -extra, -extra));
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setOpacity(0.5);
            painter->drawEllipse(pos, option.rect.width() / 2, option.rect.height() / 2);
            painter->setOpacity(1);
            QPen pen(option.palette.highlight().color().darker());
            pen.setWidth(2);
            painter->setPen(pen);
            painter->drawEllipse(pos, option.rect.width() / 4, option.rect.height() / 4);
            painter->restore();
        }
    }
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem & /* option */,
                             const QModelIndex &index) const
{
    const TilesetModel *m = static_cast<const TilesetModel*>(index.model());
    const Tileset *tileset = m->tileset();
    const qreal zoom = mTilesetView->scale();
    const int extra = mTilesetView->drawGrid() ? 1 : 0;

    return QSize(tileset->tileWidth() * zoom + extra,
                 tileset->tileHeight() * zoom + extra);
}



} // anonymous namespace

TilesetView::TilesetView(QWidget *parent)
    : QTableView(parent)
    , mZoomable(0)
    , mMapDocument(0)
    , mEditTerrain(false)
    , mTerrainId(-1)
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new TileDelegate(this, this));
    setShowGrid(false);

    QHeaderView *hHeader = horizontalHeader();
    QHeaderView *vHeader = verticalHeader();
    hHeader->hide();
    vHeader->hide();
#if QT_VERSION >= 0x050000
    hHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    vHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    hHeader->setResizeMode(QHeaderView::ResizeToContents);
    vHeader->setResizeMode(QHeaderView::ResizeToContents);
#endif
    hHeader->setMinimumSectionSize(1);
    vHeader->setMinimumSectionSize(1);

    // Hardcode this view on 'left to right' since it doesn't work properly
    // for 'right to left' languages.
    setLayoutDirection(Qt::LeftToRight);

    Preferences *prefs = Preferences::instance();
    mDrawGrid = prefs->showTilesetGrid();

    grabGesture(Qt::PinchGesture);

    connect(prefs, SIGNAL(showTilesetGridChanged(bool)),
            SLOT(setDrawGrid(bool)));
}

void TilesetView::setMapDocument(MapDocument *mapDocument)
{
    mMapDocument = mapDocument;
}

QSize TilesetView::sizeHint() const
{
    return QSize(130, 100);
}

int TilesetView::sizeHintForColumn(int column) const
{
    Q_UNUSED(column)
    const TilesetModel *model = tilesetModel();
    if (!model)
        return -1;

    const int tileWidth = model->tileset()->tileWidth();
    return tileWidth * scale() + (mDrawGrid ? 1 : 0);
}

int TilesetView::sizeHintForRow(int row) const
{
    Q_UNUSED(row)
    const TilesetModel *model = tilesetModel();
    if (!model)
        return -1;

    const int tileHeight = model->tileset()->tileHeight();
    return tileHeight * scale() + (mDrawGrid ? 1 : 0);
}

void TilesetView::setZoomable(Zoomable *zoomable)
{
    if (mZoomable)
        mZoomable->disconnect(this);

    if (zoomable)
        connect(zoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale()));

    mZoomable = zoomable;
    adjustScale();
}

qreal TilesetView::scale() const
{
    return mZoomable ? mZoomable->scale() : 1;
}

bool TilesetView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent *>(event);
        if (QGesture *gesture = gestureEvent->gesture(Qt::PinchGesture))
            mZoomable->handlePinchGesture(static_cast<QPinchGesture *>(gesture));
    }

    return QTableView::event(event);
}

void TilesetView::setEditTerrain(bool enabled)
{
    if (mEditTerrain == enabled)
        return;

    mEditTerrain = enabled;
    setMouseTracking(true);
    viewport()->update();
}

void TilesetView::setTerrainId(int terrainId)
{
    if (mTerrainId == terrainId)
        return;

    mTerrainId = terrainId;
    if (mEditTerrain)
        viewport()->update();
}

void TilesetView::mousePressEvent(QMouseEvent *event)
{
    if (!mEditTerrain) {
        QTableView::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton)
        applyTerrain();
}

void TilesetView::mouseMoveEvent(QMouseEvent *event)
{
    if (!mEditTerrain) {
        QTableView::mouseMoveEvent(event);
        return;
    }

    const QPoint pos = event->pos();
    const QModelIndex hoveredIndex = indexAt(pos);
    int hoveredCorner = 0;

    if (hoveredIndex.isValid()) {
        const QPoint center = visualRect(hoveredIndex).center();
        qDebug() << pos << center;

        if (pos.x() > center.x())
            hoveredCorner += 1;
        if (pos.y() > center.y())
            hoveredCorner += 2;
    }

    if (mHoveredIndex != hoveredIndex || mHoveredCorner != hoveredCorner) {
        const QModelIndex previousHoveredIndex = mHoveredIndex;
        mHoveredIndex = hoveredIndex;
        mHoveredCorner = hoveredCorner;

        if (previousHoveredIndex.isValid())
            update(previousHoveredIndex);
        if (previousHoveredIndex != mHoveredIndex && mHoveredIndex.isValid())
            update(mHoveredIndex);
    }

    if (event->buttons() & Qt::LeftButton)
        applyTerrain();
}

void TilesetView::leaveEvent(QEvent *event)
{
    if (mHoveredIndex.isValid()) {
        const QModelIndex previousHoveredIndex = mHoveredIndex;
        mHoveredIndex = QModelIndex();
        update(previousHoveredIndex);
    }

    QTableView::leaveEvent(event);
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void TilesetView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

/**
 * Allow changing tile properties through a context menu.
 */
void TilesetView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    const TilesetModel *model = tilesetModel();
    if (!model)
        return;

    Tile *tile = model->tileAt(index);

    const bool isExternal = model->tileset()->isExternal();
    QMenu menu;

    QIcon propIcon(QLatin1String(":images/16x16/document-properties.png"));

    if (tile) {
        // Select this tile to make sure it is clear that only the properties
        // of a single tile are being edited.
        selectionModel()->setCurrentIndex(index,
                                          QItemSelectionModel::SelectCurrent |
                                          QItemSelectionModel::Clear);

        QAction *tileProperties = menu.addAction(propIcon,
                                                 tr("Tile &Properties..."));
        tileProperties->setEnabled(!isExternal);
        Utils::setThemeIcon(tileProperties, "document-properties");
        menu.addSeparator();

        connect(tileProperties, SIGNAL(triggered()),
                SLOT(editTileProperties()));
    }


    menu.addSeparator();
    QAction *toggleGrid = menu.addAction(tr("Show &Grid"));
    toggleGrid->setCheckable(true);
    toggleGrid->setChecked(mDrawGrid);

    Preferences *prefs = Preferences::instance();
    connect(toggleGrid, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowTilesetGrid(bool)));

    menu.exec(event->globalPos());
}

void TilesetView::editTileProperties()
{
    const TilesetModel *m = tilesetModel();
    Tile *tile = m->tileAt(selectionModel()->currentIndex());
    if (!tile)
        return;

    PropertiesDialog propertiesDialog(tr("Tile"),
                                      tile,
                                      mMapDocument->undoStack(),
                                      this);
    propertiesDialog.exec();
}

void TilesetView::setDrawGrid(bool drawGrid)
{
    mDrawGrid = drawGrid;
    if (TilesetModel *model = tilesetModel())
        model->tilesetChanged();
}

void TilesetView::adjustScale()
{
    if (TilesetModel *model = tilesetModel())
        model->tilesetChanged();
}

void TilesetView::applyTerrain()
{
    if (!mHoveredIndex.isValid())
        return;

    // Modify the terrain of the tile (TODO: Undo (probably via the TilesetModel))
    Tile *tile = tilesetModel()->tileAt(mHoveredIndex);
    tile->setCornerTerrain(mHoveredCorner, mTerrainId);
}
