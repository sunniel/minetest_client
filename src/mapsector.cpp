#include "mapsector.h"
#include <jmutexautolock.h>
namespace jthread {
} // JThread 1.2 support
using namespace jthread;
// JThread 1.3 support
#include "client.h"
#include "exceptions.h"
#include "main.h"
#include "map.h"

MapBlock * BlockGenerator::makeBlock(MapSector *sector, s16 block_y) {
    MapBlock *block = sector->createBlankBlockNoInsert(block_y);
    v2s16 sectorPos = sector->getPos();
    //=====Set boundary block and normal block differently=====
    if (((sectorPos.X == -1) || (sectorPos.X == MAP_LENGTH))
            && (sectorPos.Y > -1) && (sectorPos.Y < MAP_WIDTH)) {
        //=====Boundary alone X axis=====
        s16 x0;
        if (sectorPos.X == -1) {
            x0 = MAP_BLOCKSIZE - 1;
        } else {
            x0 = 0;
        }
        for (s16 z0 = 0; z0 < MAP_BLOCKSIZE; z0++) {
            for (s16 y0 = 0; y0 < MAP_BLOCKSIZE; y0++) {
                MapNode n;
                n.d = MATERIAL_IGNORE;
                block->setNode(v3s16(x0, y0, z0), n);
            }
        }

    } else if (((sectorPos.Y == -1) || (sectorPos.Y == MAP_WIDTH))
            && (sectorPos.X > -1) && (sectorPos.X < MAP_LENGTH)) {
        //=====Boundary alone Z axis=====
        s16 z0;
        if (sectorPos.Y == -1) {
            z0 = MAP_BLOCKSIZE - 1;
        } else {
            z0 = 0;
        }
        for (s16 x0 = 0; x0 < MAP_BLOCKSIZE; x0++) {
            for (s16 y0 = 0; y0 < MAP_BLOCKSIZE; y0++) {
                MapNode n;
                n.d = MATERIAL_IGNORE;
                block->setNode(v3s16(x0, y0, z0), n);
            }
        }

    } else if ((sectorPos.X > -1) && (sectorPos.X < MAP_LENGTH)
            && (sectorPos.Y > -1) && (sectorPos.Y < MAP_WIDTH)) {
        //=====Normal block=====
        //=====Just generate a base level of grass=====
        for (s16 z0 = 0; z0 < MAP_BLOCKSIZE; z0++) {
            for (s16 x0 = 0; x0 < MAP_BLOCKSIZE; x0++) {
                for (s16 y0 = 0; y0 < MAP_BLOCKSIZE; y0++) {
                    MapNode n;
                    if (y0 == 0 && block_y == MAP_BOTTOM) {
                        n.d = MATERIAL_GRASS;
                    } else {
                        n.d = MATERIAL_AIR;
                    }
                    block->setNode(v3s16(x0, y0, z0), n);
                }
            }
        }
        bool probably_dark = false;
        block->setProbablyDark(probably_dark);
    }
    return block;
}

MapBlock * ClientBlockGenerator::makeBlock(MapSector *sector, s16 block_y) {
    MapBlock *block = sector->createBlankBlockNoInsert(block_y);
    v2s16 sectorPos = sector->getPos();
    //=====Set boundary block and normal block differently=====
    if (((sectorPos.X == -1) || (sectorPos.X == MAP_LENGTH))
            && (sectorPos.Y > -1) && (sectorPos.Y < MAP_WIDTH)) {
        //=====Boundary alone X axis=====
        s16 x0;
        if (sectorPos.X == -1) {
            x0 = MAP_BLOCKSIZE - 1;
        } else {
            x0 = 0;
        }
        for (s16 z0 = 0; z0 < MAP_BLOCKSIZE; z0++) {
            for (s16 y0 = 0; y0 < MAP_BLOCKSIZE; y0++) {
                MapNode n;
                n.d = MATERIAL_IGNORE;
                block->setNode(v3s16(x0, y0, z0), n);
            }
        }

    } else if (((sectorPos.Y == -1) || (sectorPos.Y == MAP_WIDTH))
            && (sectorPos.X > -1) && (sectorPos.X < MAP_LENGTH)) {
        //=====Boundary alone Z axis=====
        s16 z0;
        if (sectorPos.Y == -1) {
            z0 = MAP_BLOCKSIZE - 1;
        } else {
            z0 = 0;
        }
        for (s16 x0 = 0; x0 < MAP_BLOCKSIZE; x0++) {
            for (s16 y0 = 0; y0 < MAP_BLOCKSIZE; y0++) {
                MapNode n;
                n.d = MATERIAL_IGNORE;
                block->setNode(v3s16(x0, y0, z0), n);
            }
        }

    } else if ((sectorPos.X > -1) && (sectorPos.X < MAP_LENGTH)
            && (sectorPos.Y > -1) && (sectorPos.Y < MAP_WIDTH)) {
        //=====Normal block=====
        //=====Just generate a base level of grass=====
        for (s16 z0 = 0; z0 < MAP_BLOCKSIZE; z0++) {
            for (s16 x0 = 0; x0 < MAP_BLOCKSIZE; x0++) {
                for (s16 y0 = 0; y0 < MAP_BLOCKSIZE; y0++) {
                    MapNode n;
                    if (y0 == 0 && block_y == MAP_BOTTOM) {
                        n.d = MATERIAL_GRASS;
                    } else {
                        n.d = MATERIAL_AIR;
                    }
                    block->setNode(v3s16(x0, y0, z0), n);
                }
            }
        }
        bool probably_dark = false;
        block->setProbablyDark(probably_dark);
    }
    //#####fetchBlock() should not send TOSERVER_GETBLOCK command, need to change it#####
    v3s16 blockpos_map(sector->getPos().X, block_y, sector->getPos().Y);
    m_client->fetchBlock(blockpos_map);
    return block;
}

MapBlock * MapSector::getBlockBuffered(s16 y) {
    MapBlock *block;
    if (m_block_cache != NULL && y == m_block_cache_y) {
        return m_block_cache;
    }
    // If block doesn't exist, return NULL
    core::map<s16, MapBlock*>::Node *n = m_blocks.find(y);
    if (n == NULL) {
        /*
         TODO: Check if block is stored on disk
         and load dynamically
         */
        block = NULL;
    }
    // If block exists, return it
    else {
        block = n->getValue();
    }
    // Cache the last result
    m_block_cache_y = y;
    m_block_cache = block;
    return block;
}

MapBlock * MapSector::getBlockNoCreate(s16 y) {
    /*std::cout<<"MapSector("<<m_pos.X<<","<<m_pos.Y<<")::getBlock("<<y
     <<")"<<std::endl;*/

    JMutexAutoLock lock(m_mutex);

    MapBlock *block = getBlockBuffered(y);

    if (block == NULL)
        throw InvalidPositionException();

    return block;
}

MapBlock * MapSector::createBlankBlockNoInsert(s16 y) {
    // There should not be a block at this position
    if (getBlockBuffered(y) != NULL)
        throw AlreadyExistsException("Block already exists");

    v3s16 blockpos_map(m_pos.X, y, m_pos.Y);

    MapBlock *block = new MapBlock(m_parent, blockpos_map);

    return block;
}

MapBlock * MapSector::createBlankBlock(s16 y) {
    JMutexAutoLock lock(m_mutex);

    /*std::cout<<"MapSector("<<m_pos.X<<","<<m_pos.Y<<")::createBlankBlock("<<y
     <<")"<<std::endl;*/

    MapBlock *block = createBlankBlockNoInsert(y);

    m_blocks.insert(y, block);

    return block;
}

/*
 This will generate a new block as needed.

 A valid heightmap is assumed to exist already.
 */
MapBlock * MapSector::getBlock(s16 block_y) {
    {
        JMutexAutoLock lock(m_mutex);
        MapBlock *block = getBlockBuffered(block_y);
        if (block != NULL)
            return block;
    }

    /*std::cout<<"MapSector("<<m_pos.X<<","<<m_pos.Y<<
     ")::getBlock("<<block_y<<")"<<std::endl;*/

    MapBlock *block = m_block_gen->makeBlock(this, block_y);

    //block->propagateSunlight();

    // Insert to container
    {
        JMutexAutoLock lock(m_mutex);
        m_blocks.insert(block_y, block);
    }

    return block;
}

void MapSector::insertBlock(MapBlock *block) {
    s16 block_y = block->getPos().Y;

    {
        JMutexAutoLock lock(m_mutex);
        MapBlock *block = getBlockBuffered(block_y);
        if (block != NULL) {
            throw AlreadyExistsException("Block already exists");
        }
    }

    v2s16 p2d(block->getPos().X, block->getPos().Z);
    assert(p2d == m_pos);

    block->propagateSunlight();

    // Insert to container
    {
        JMutexAutoLock lock(m_mutex);
        m_blocks.insert(block_y, block);
    }
}

core::list<MapBlock*> MapSector::getBlocks() {
    JMutexAutoLock lock(m_mutex);

    core::list<MapBlock*> ref_list;

    core::map<s16, MapBlock*>::Iterator bi;

    bi = m_blocks.getIterator();
    for (; bi.atEnd() == false; bi++) {
        MapBlock *b = bi.getNode()->getValue();
        ref_list.push_back(b);
    }

    return ref_list;
}

//END
