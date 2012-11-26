/*
 * terrainview.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2012, Manu Evans <turkeyman@gmail.com>
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

#include "terrainview.h"

#include "map.h"
#include "mapdocument.h"
#include "preferences.h"
#include "propertiesdialog.h"
#include "tmxmapwriter.h"
#include "tile.h"
#include "tileset.h"
#include "terrain.h"
#include "terrainmodel.h"
#include "utils.h"
#include "zoomable.h"

#include <QAbstractItemDelegate>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QUndoCommand>
#include <QWheelEvent>

using namespace Tiled;
using namespace Tiled::Internal;

namespace {

/**
 * The delegate for drawing terrain types in the terrain view.
 */
class TerrainDelegate : public QAbstractItemDelegate
{
public:
    TerrainDelegate(TerrainView *terrainView, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mTerrainView(terrainView)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

private:
    TerrainView *mTerrainView;
};

void TerrainDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    // Draw the terrain image
    const QVariant display = index.model()->data(index, Qt::DecorationRole);
    const QPixmap terrainImage = display.value<QPixmap>();
    const int extra = 1;

    if (mTerrainView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->drawPixmap(option.rect.adjusted(0, 0, -extra, -extra), terrainImage);

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(0, 0, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
}

QSize TerrainDelegate::sizeHint(const QStyleOptionViewItem & /* option */,
                                const QModelIndex & /* index */) const
{
//    const TerrainModel *m = static_cast<const TerrainModel*>(index.model());
//    const Tileset *tileset = m->tileset();
//    const qreal zoom = mTerrainView->zoomable()->scale();

//    return QSize(tileset->tileWidth() * zoom + extra,
//                 tileset->tileHeight() * zoom + extra);
    return QSize(32, 32);
}



} // anonymous namespace

TerrainView::TerrainView(QWidget *parent)
    : QTreeView(parent)
    , mZoomable(new Zoomable(this))
    , mMapDocument(0)
{
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setRootIsDecorated(false);
    setIndentation(0);
    setItemsExpandable(false);
//    setItemDelegate(new TerrainDelegate(this, this));

//    setFlow(QTreeView::LeftToRight);
//    setResizeMode(QTreeView::Adjust);
//    setWrapping(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale()));
}

void TerrainView::setMapDocument(MapDocument *mapDocument)
{
    mMapDocument = mapDocument;
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void TerrainView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTreeView::wheelEvent(event);
}

/**
 * Allow changing terrain properties through a context menu.
 */
void TerrainView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    const TerrainModel *m = terrainModel();
    Terrain *terrain = m->terrainAt(index);

    const bool isExternal = terrain->tileset()->isExternal();
    QMenu menu;

    QIcon propIcon(QLatin1String(":images/16x16/document-properties.png"));

    if (terrain) {
        QAction *terrainProperties = menu.addAction(propIcon,
                                                 tr("Terrain &Properties..."));
        terrainProperties->setEnabled(!isExternal);
        Utils::setThemeIcon(terrainProperties, "document-properties");
        menu.addSeparator();

        connect(terrainProperties, SIGNAL(triggered()),
                SLOT(editTerrainProperties()));
    }

    menu.exec(event->globalPos());
}

void TerrainView::editTerrainProperties()
{
    const TerrainModel *m = terrainModel();
    Terrain *terrain = m->terrainAt(selectionModel()->currentIndex());
    if (!terrain)
        return;

    PropertiesDialog propertiesDialog(tr("Terrain"),
                                      terrain,
                                      mMapDocument->undoStack(),
                                      this);
    propertiesDialog.exec();
}

void TerrainView::adjustScale()
{
//    terrainModel()->tilesetChanged();
}
