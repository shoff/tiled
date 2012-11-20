/*
 * terrainmodel.h
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

#ifndef TERRAINMODEL_H
#define TERRAINMODEL_H

#include <QAbstractTableModel>
#include <tileset.h>

namespace Tiled {

class Tileset;
class Terrain;

namespace Internal {

class MapDocument;

/**
 * A model wrapping the terrain within a tileset. Used to display the terrain
 * types.
 */
class TerrainModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * Constructor.
     *
     * @param tileset the initial tileset to display
     */
    TerrainModel(MapDocument *mapDocument,
                 Tileset *tileset,
                 QObject *parent = 0);

    /**
     * Returns the number of rows. This is equal to the number of tiles.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    /**
     * Returns the number of columns.
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /**
     * Returns the data stored under the given <i>role</i> for the item
     * referred to by the <i>index</i>.
     */
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    /**
     * Allows for changing the name of a terrain.
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    /**
     * Makes terrain names are editable.
     */
    Qt::ItemFlags flags(const QModelIndex &index) const;

    /**
     * Returns a small size hint, to prevent the headers from affecting the
     * minimum width and height of the sections.
     */
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    /**
     * Returns the tile at the given index.
     */
    Terrain *terrainAt(const QModelIndex &index) const;

    /**
     * Returns the tileset associated with this model.
     */
    Tileset *tileset() const { return mTileset; }

    /**
     * Sets the tileset associated with this model.
     */
    void setTileset(MapDocument *mapDocument, Tileset *tileset);

    /**
     * Adds a new terrain type.
     *
     * @param name      the name of the terrain
     * @param imageTile the id of the tile that represents the terrain visually
     * @return the created Terrain instance
     */
    Terrain *addTerrain(const QString &name, int imageTile);

    /**
     * Removes the terrain type at the given index.
     */
    void removeTerrain(const QModelIndex &index);

    /**
     * Performs a reset on the model.
     */
    void tilesetChanged();

private slots:
    void terrainChanged(Tileset *tileset, int terrainId);

private:
    MapDocument *mMapDocument;
    Tileset *mTileset;
};

} // namespace Internal
} // namespace Tiled

#endif // TERRAINMODEL_H
