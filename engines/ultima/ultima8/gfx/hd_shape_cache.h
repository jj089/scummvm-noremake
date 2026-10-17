/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ULTIMA8_GFX_HD_SHAPE_CACHE_H
#define ULTIMA8_GFX_HD_SHAPE_CACHE_H

#include "common/hashmap.h"
#include "common/scummsys.h"

namespace Graphics {
struct Surface;
}

namespace Ultima {
namespace Ultima8 {

/**
 * Replacement high resolution artwork for shapes.
 *
 * Frames are PNG files (RGBA, HD_SCALE times the original size, same anchor
 * point scaled) in the game data directory:
 *   hdshapes/<shape>_<frame>.png   (4-digit, zero padded; main shapes only)
 *
 * The directory is indexed once, frames are decoded on first use and kept in
 * a size-limited cache.
 */
class HDShapeCache {
public:
	static const int HD_SCALE = 2;

	HDShapeCache();
	~HDShapeCache();

	static HDShapeCache *get_instance();
	static void destroyInstance();

	//! Returns the HD frame as an ARGB8888 surface, or nullptr if none exists.
	const Graphics::Surface *getFrame(uint16 flexId, uint32 shapenum, uint32 framenum);

	bool hasAny() const { return !_available.empty(); }

private:
	void clear();

	static HDShapeCache *_instance;

	typedef Common::HashMap<uint32, bool> AvailableMap;
	typedef Common::HashMap<uint32, Graphics::Surface *> SurfaceMap;
	AvailableMap _available;
	SurfaceMap _loaded;
	uint32 _loadedBytes;
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
