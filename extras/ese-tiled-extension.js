// ese.js - compatible with Tiled 1.11.2
// Loads/saves a Entity Sprite Engine atlas format.

function getBasePath(fileName) {
    const parts = fileName.split('/');
    parts.pop();
    return parts.join('/');
}

function getFileName(fileName) {
    const parts = fileName.split('/');
    return parts.pop();
}

function getTileId(tileset, spriteName) {
    if (!tileset) return null;
    for (const key of Object.keys(tileset)) {
        const list = tileset[key];
        if (!Array.isArray(list)) continue;
        for (const item of list) {
            if (item && item.sprite === spriteName) return String(key);
        }
    }
    return null;
}

function nextTileId(tileset) {
    if (!tileset) return "0";
    const keys = Object.keys(tileset)
        .map(k => Number(k))
        .filter(n => Number.isFinite(n));
    const max = keys.length ? Math.max(...keys) : -1;
    return String(max + 1);
}

function getLayerCount(data) {
    if (!Array.isArray(data)) return 0;
    return data.reduce((max, entry) => {
        const len = Array.isArray(entry && entry.layers) ? entry.layers.length : 0;
        return len > max ? len : max;
    }, 0);
}

function isIntegerString(str) {
    return typeof str === "string" && /^\s*[+-]?\d+\s*$/.test(str);
}

let ESE_Atlas = {
    name: "ESE-ATLAS",
    extension: "jsonc",

    read: function (fileName) {
        const basePath = getBasePath(fileName);

        const file = new TextFile(fileName, TextFile.ReadOnly);
        const text = file.readAll();
        file.close();

        let data = null;
        try {
            data = JSON.parse(text);
        } catch (e) {
            console.error('Error parsing JSON: ' + e);
            return null;
        }

        if (data.version !== 'ESE-ATLAS-1') {
            tiled.error('Unsupported version: ' + data.version);
            return null;
        }

        // Is this a uniform atlas where all frames are the same size and on a grid?
        const sameSize = new Set(data.frames.map(f => `${f.width}x${f.height}`)).size === 1;
        let onGrid = sameSize
        if (sameSize) {
            const width = data.frames[0].width;
            const height = data.frames[0].height;

            onGrid = data.frames.every(f =>
                f.x % width === 0 && f.y % height === 0
            );
        }

        if (onGrid) {
            const tileset = new Tileset();
            tileset.tileRenderSize = Tileset.GridSize;
            tileset.fillMode = Tileset.PreserveAspectFit
            tileset.setTileSize(data.frames[0].width, data.frames[0].height);
            tiled.log(basePath + '/' + data.image);
            tileset.imageFileName = basePath + '/' + data.image;

            tileset.tiles.forEach(tile => {
                let frame = data.frames.find(f => f.x === tile.imageRect.x &&
                    f.y === tile.imageRect.y &&
                    f.width === tile.imageRect.width &&
                    f.height === tile.imageRect.height);

                if (frame && frame.name !== tile.id.toString()) {
                    tile.className = frame.name;
                }
            });

            return tileset;
        } else {
            const tileset = new Tileset();
            return tileset;
        }
    },

    write: function (tileset, fileName) {
        const basePath = getBasePath(fileName);
        const imageName = getFileName(tileset.imageFileName);

        // Save the image
        const img = new Image(tileset.imageFileName);
        const fullImage = basePath + '/' + imageName;
        img.save(fullImage);

        const out = {
            version: 'ESE-ATLAS-1',
            image: imageName,
            sprites: [],
            frames: [],
        };

        if (!tileset.isCollection) {
            const tiles = tileset.tiles;
            tiles.forEach(tile => {
                out.frames.push({
                    name: tile.className || tile.id.toString(),
                    x: tile.imageRect.x,
                    y: tile.imageRect.y,
                    width: tile.width,
                    height: tile.height,
                });
            });

            tiles.forEach(tile => {
                const sprite = {
                    name: tile.className || tile.id.toString(),
                    speed: 0,
                    frames: [],
                };

                if (tile.animation) {
                    // Not supported yet
                } else {
                    sprite.frames.push(tile.className || tile.id.toString());
                }

                out.sprites.push(sprite);
            });

            const file = new TextFile(fileName, TextFile.WriteOnly);
            file.write(JSON.stringify(out, null, 2));
            file.commit();
        } else {
            // Not supported yet
        }
    },
};

const ESE_Map = {
    name: "ESE-MAP",
    extension: "jsonc",

    read: function (fileName) {
        const basePath = getBasePath(fileName);

        const file = new TextFile(fileName, TextFile.ReadOnly);
        const text = file.readAll();
        file.close();

        let data = null;
        try {
            data = JSON.parse(text);
        } catch (e) {
            tiled.error('Error parsing JSON: ' + e);
            return null;
        }

        if (data.version !== 'ESE-MAP-1') {
            tiled.error('Unsupported version: ' + data.version);
            return null;
        }

        // Find open tilesets
        const tilesets = [];
        tiled.openAssets.forEach(asset => {
            if (asset.isTileset) {
                tilesets.push(asset);
            }
        });

        if (tilesets.length === 0) {
            tiled.error('Please open required tilesets first');
            return null;
        }

        const layerCount = getLayerCount(data.cells);

        const map = new TileMap();
        map.setProperty('title', data.title || 'Unknown');
        map.setProperty('author', data.author || 'Unknown');
        map.setSize(data.width, data.height);
        map.tileHeight = 64;     // - display only
        map.tileWidth = 64;      // - display only

        if (data.type === 'grid') {
            map.orientation = TileMap.Orthogonal;
        }

        for (let i = 0; i < layerCount; i++) {
            const layer = new TileLayer();
            layer.name = 'Tile Layer ' + (i + 1);
            layer.resize({ width: map.width, height: map.height }, { x: 0, y: 0 });
            map.addLayer(layer);
        }

        for (let y = 0; y < map.height; y++) {
            for (let x = 0; x < map.width; x++) {
                const cellIndex = y * map.width + x;
                const cell = data.cells[cellIndex];

                if (!cell) {
                    tiled.error('Invalid cell array');
                    return null;
                }

                for (let i = 0; i < cell.layers.length; i++) {
                    const tileId = cell.layers[i];
                    if (tileId === -1) {
                        continue;
                    }

                    const spriteId = data.tileset[tileId.toString()][0].sprite;

                    let tile = null;
                    tilesets.forEach(tileset => {
                        tileset.tiles.forEach(t => {
                            if (t.className === spriteId) {
                                tile = t;
                            }
                            if (
                                tile === null &&
                                isIntegerString(t.className) &&
                                t.id.toString() === spriteId
                            ) {
                                tile = t;
                            }
                        });
                    });

                    if (tile !== null) {
                        const layer = map.layerAt(i);
                        const layerEdit = layer.edit();
                        layerEdit.setTile(x, y, tile);
                        layerEdit.apply();
                    } else {
                        tiled.error('Tile not found: ' + spriteId);
                        return null;
                    }
                }
            }
        }

        return map;
    },

    write: function (map, fileName) {
        const basePath = getBasePath(fileName);

        const out = {
            version: 'ESE-MAP-1',
            title: map.property('title') || map.property('Title') || 'Unknown',
            author: map.property('author') || map.property('Author') || 'Unknown',
            width: map.width,
            height: map.height,
            type: 'grid',
            tileset: {},
            cells: [],
        };

        map.layers.forEach(layer => {
            if (layer.isTileLayer) {
                for (let y = 0; y < layer.size.height; y++) {
                    for (let x = 0; x < layer.size.width; x++) {
                        const cellIndex = y * layer.size.width + x;
                        const tile = layer.tileAt(x, y);

                        let tileId = -1;
                        if (tile) {
                            const spriteId = tile.className || tile.id.toString();
                            tileId = getTileId(out.tileset, spriteId);
                            if (!tileId) {
                                tileId = nextTileId(out.tileset);
                                out.tileset[tileId] = [
                                    { 'sprite': spriteId, weight: 1 },
                                ];
                            }
                        }

                        if (!out.cells[cellIndex]) {
                            out.cells[cellIndex] = {
                                layers: []
                            };
                        }

                        out.cells[cellIndex].layers.push(parseInt(tileId));
                    }
                }
            }
        });

        const file = new TextFile(fileName, TextFile.WriteOnly);
        file.write(JSON.stringify(out, null, 2));
        file.commit();
    },
};

// Register the format; try direct, then fallback signature.
tiled.registerTilesetFormat(ESE_Atlas.name, ESE_Atlas);
tiled.registerMapFormat(ESE_Map.name, ESE_Map);