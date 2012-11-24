/*
 * terraindock.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 * Copyright 2012, Stefan Beller <stefanbeller@googlemail.com>
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

#include "terraindock.h"

#include "documentmanager.h"
//#include "map.h"
//#include "mapdocument.h"
#include "terrainmodel.h"
//#include "terrainview.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QTreeView>

using namespace Tiled;
using namespace Tiled::Internal;

TerrainDock::TerrainDock(QWidget *parent):
    QDockWidget(parent),
    mMapDocument(0),
    mCurrentTerrain(0)
{
    setObjectName(QLatin1String("TerrainDock"));

    QWidget *w = new QWidget(this);

    mTerrainView = new QTreeView;
    mTerrainView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mTerrainView->setRootIsDecorated(false);

    QHBoxLayout *horizontal = new QHBoxLayout(w);
    horizontal->setSpacing(5);
    horizontal->setMargin(5);
    horizontal->addWidget(mTerrainView);

    connect(DocumentManager::instance(), SIGNAL(documentCloseRequested(int)),
            SLOT(documentCloseRequested(int)));

    setWidget(w);
    retranslateUi();
}

TerrainDock::~TerrainDock()
{
}

void TerrainDock::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    // TODO: Remember selected terrain
//    if (mMapDocument)
//        mCurrentTilesets.insert(mMapDocument,
//                                mTabBar->tabText(mTabBar->currentIndex()));

    // Clear all connections to the previous document
    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;

    QItemSelectionModel *oldSelectionModel = mTerrainView->selectionModel();
    if (mMapDocument) {
        mTerrainView->setModel(mMapDocument->terrainModel());
        mTerrainView->expandAll();

        connect(mTerrainView->selectionModel(),
                SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                SLOT(currentRowChanged(QModelIndex)));

//        Map *map = mMapDocument->map();

//        QString cacheName = mCurrentTilesets.take(mMapDocument);
//        for (int i = 0; i < mTabBar->count(); ++i) {
//            if (mTabBar->tabText(i) == cacheName) {
//                mTabBar->setCurrentIndex(i);
//                break;
//            }
//        }
    } else {
        mTerrainView->setModel(0);
    }
    delete oldSelectionModel;
}

void TerrainDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void TerrainDock::currentRowChanged(const QModelIndex &index)
{
    if (Terrain *terrain = mMapDocument->terrainModel()->terrainAt(index))
        setCurrentTerrain(terrain);
}

void TerrainDock::setCurrentTerrain(Terrain *terrain)
{
    if (mCurrentTerrain == terrain)
        return;

    mCurrentTerrain = terrain;
    emit currentTerrainChanged(mCurrentTerrain);
}

void TerrainDock::retranslateUi()
{
    setWindowTitle(tr("Terrains"));
}

void TerrainDock::documentCloseRequested(int index)
{
    DocumentManager *documentManager = DocumentManager::instance();
    mCurrentTilesets.remove(documentManager->documents().at(index));
}
