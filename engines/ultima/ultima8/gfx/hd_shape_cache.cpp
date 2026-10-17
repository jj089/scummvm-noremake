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

#include "ultima/ultima8/gfx/hd_shape_cache.h"
#include "ultima/ultima8/games/game_data.h"
#include "common/archive.h"
#include "common/file.h"
#include "graphics/surface.h"
#include "image/png.h"

namespace Ultima {
namespace Ultima8 {

HDShapeCache *HDShapeCache::_instance = nullptr;

// Decoded frames kept in memory before the cache is flushed.
static const uint32 HD_CACHE_LIMIT = 512 * 1024 * 1024;

static inline uint32 hdKey(uint32 shapenum, uint32 framenum) {
	return (shapenum << 16) | (framenum & 0xFFFF);
}

HDShapeCache::HDShapeCache() : _loadedBytes(0) {
	Common::ArchiveMemberList files;
	SearchMan.listMatchingMembers(files, "hdshapes/*.png", true);
	for (const auto &f : files) {
		const Common::String name = f->getFileName();
		uint shapenum, framenum;
		if (sscanf(name.c_str(), "%u_%u.png", &shapenum, &framenum) == 2)
			_available[hdKey(shapenum, framenum)] = true;
	}
	debug(1, "HDShapeCache: %u high resolution frames available", _available.size());
}

HDShapeCache::~HDShapeCache() {
	clear();
}

HDShapeCache *HDShapeCache::get_instance() {
	if (!_instance)
		_instance = new HDShapeCache();
	return _instance;
}

void HDShapeCache::destroyInstance() {
	delete _instance;
	_instance = nullptr;
}

void HDShapeCache::clear() {
	for (auto &entry : _loaded) {
		if (entry._value) {
			entry._value->free();
			delete entry._value;
		}
	}
	_loaded.clear();
	_loadedBytes = 0;
}

const Graphics::Surface *HDShapeCache::getFrame(uint16 flexId, uint32 shapenum, uint32 framenum) {
	if (flexId != GameData::MAINSHAPES)
		return nullptr;

	const uint32 key = hdKey(shapenum, framenum);
	if (!_available.contains(key))
		return nullptr;

	SurfaceMap::iterator it = _loaded.find(key);
	if (it != _loaded.end())
		return it->_value;

	if (_loadedBytes > HD_CACHE_LIMIT)
		clear();

	Graphics::Surface *result = nullptr;
	Common::File f;
	Image::PNGDecoder png;
	const Common::String filename = Common::String::format("hdshapes/%04u_%04u.png", shapenum, framenum);
	if (f.open(Common::Path(filename)) && png.loadStream(f) && png.getSurface()) {
		// Native 32-bit 0xAARRGGBB values, as RenderSurface reads them.
		result = png.getSurface()->convertTo(Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24));
		_loadedBytes += result->w * result->h * 4;
	} else {
		warning("HDShapeCache: could not load %s", filename.c_str());
		_available.erase(key);
	}
	_loaded[key] = result;
	return result;
}

} // End of namespace Ultima8
} // End of namespace Ultima
