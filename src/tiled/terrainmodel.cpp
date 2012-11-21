/*
 * terrainmodel.cpp
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
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

#include "terrainmodel.h"

#include "map.h"
#include "mapdocument.h"
#include "terrain.h"
#include "tileset.h"
#include "tile.h"

#include <QUndoCommand>
#include <QCoreApplication>

using namespace Tiled;
using namespace Tiled::Internal;

namespace {

class RenameTerrain : public QUndoCommand
{
public:
    RenameTerrain(MapDocument *mapDocument,
                  Tileset *tileset,
                  int terrainId,
                  const QString &newName)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change Terrain Name"))
        , mMapDocument(mapDocument)
        , mTileset(tileset)
        , mTerrainId(terrainId)
        , mOldName(tileset->terrain(terrainId)->name())
        , mNewName(newName)
    {}

    void undo()
    { mMapDocument->setTerrainName(mTileset, mTerrainId, mOldName); }

    void redo()
    { mMapDocument->setTerrainName(mTileset, mTerrainId, mNewName); }

private:
    MapDocument *mMapDocument;
    Tileset *mTileset;
    int mTerrainId;
    QString mOldName;
    QString mNewName;
};

class SetTerrainImage : public QUndoCommand
{
public:
    SetTerrainImage(MapDocument *mapDocument,
                    Tileset *tileset,
                    int terrainId,
                    int tileId)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change Terrain Image"))
        , mMapDocument(mapDocument)
        , mTileset(tileset)
        , mTerrainId(terrainId)
        , mOldImageTileId(tileset->terrain(terrainId)->imageTileId())
        , mNewImageTileId(tileId)
    {}

    void undo()
    { mMapDocument->setTerrainImage(mTileset, mTerrainId, mOldImageTileId); }

    void redo()
    { mMapDocument->setTerrainImage(mTileset, mTerrainId, mNewImageTileId); }

private:
    MapDocument *mMapDocument;
    Tileset *mTileset;
    int mTerrainId;
    int mOldImageTileId;
    int mNewImageTileId;
};

} // anonymous namespace

TerrainModel::TerrainModel(MapDocument *mapDocument,
                           Tileset *tileset,
                           QObject *parent):
    QAbstractTableModel(parent),
    mMapDocument(mapDocument),
    mTileset(tileset)
{
    connect(mapDocument, SIGNAL(terrainAdded(Tileset*,int)),
            SLOT(terrainAdded(Tileset*,int)));
    connect(mapDocument, SIGNAL(terrainRemoved(Tileset*,int)),
            SLOT(terrainRemoved(Tileset*,int)));
    connect(mapDocument, SIGNAL(terrainChanged(Tileset*,int)),
            SLOT(terrainChanged(Tileset*,int)));
}

int TerrainModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return mTileset->terrainCount() + 1;
}

int TerrainModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

QVariant TerrainModel::data(const QModelIndex &index, int role) const
{
    if (index.column() == 0) {
        if (role == Qt::DecorationRole) {
            if (Terrain *terrain = terrainAt(index))
                if (Tile *imageTile = terrain->imageTile())
                    return imageTile->image();
        }
    } else if (index.column() == 1) {
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            if (index.row() == 0)
                return tr("No terrain");
            else if (Terrain *terrain = terrainAt(index))
                return terrain->name();
        }
    }

    return QVariant();
}

bool TerrainModel::setData(const QModelIndex &index,
                           const QVariant &value,
                           int role)
{
    if (index.row() < 1 || index.column() != 1)
        return false;

    if (role == Qt::EditRole) {
        const QString newName = value.toString();
        Terrain *terrain = terrainAt(index);
        if (terrain->name() != newName) {
            RenameTerrain *rename = new RenameTerrain(mMapDocument, mTileset,
                                                      terrain->id(), newName);
            mMapDocument->undoStack()->push(rename);
        }
        return true;
    }

    return false;
}

Qt::ItemFlags TerrainModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractTableModel::flags(index);
    if (index.column() == 1)
        rc |= Qt::ItemIsEditable;
    return rc;
}

QVariant TerrainModel::headerData(int /* section */,
                                  Qt::Orientation /* orientation */,
                                  int role) const
{
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

Terrain *TerrainModel::terrainAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    const int i = index.row() - 1;
    return i == -1 || i >= mTileset->terrainCount() ? 0 : mTileset->terrain(i);
}

void TerrainModel::setTileset(MapDocument *mapDocument, Tileset *tileset)
{
    if (mMapDocument == mapDocument && mTileset == tileset)
        return;

    beginResetModel();
    mMapDocument = mapDocument;
    mTileset = tileset;
    endResetModel();
}

Terrain *TerrainModel::addTerrain(const QString &name, int imageTile)
{
    const int rowCount = mTileset->terrainCount() + 1;

    beginInsertRows(QModelIndex(), rowCount, rowCount);
    Terrain *terrain = mTileset->addTerrain(name, imageTile);
    endInsertRows();

    return terrain;
}

void TerrainModel::removeTerrain(const QModelIndex &index)
{
    const int row = index.row();

    beginRemoveRows(QModelIndex(), row, row);
    mTileset->takeTerrainAt(row - 1);
    endRemoveRows();
}

void TerrainModel::tilesetChanged()
{
    beginResetModel();
    endResetModel();
}

void TerrainModel::terrainAdded(Tileset *tileset, int terrainId)
{
    if (mTileset != tileset)
        return;

    // Wrong... I need to be the one adding terrains
}

void TerrainModel::terrainRemoved(Tileset *tileset, int terrainId)
{
    if (mTileset != tileset)
        return;

    // Wrong... I need to be the one removing terrains
}

void TerrainModel::terrainChanged(Tileset *tileset, int terrainId)
{
    if (mTileset != tileset)
        return;

    const QModelIndex topLeft = index(terrainId + 1, 0);
    const QModelIndex bottomRight = index(terrainId + 1, 1);
    emit dataChanged(topLeft, bottomRight);
}
