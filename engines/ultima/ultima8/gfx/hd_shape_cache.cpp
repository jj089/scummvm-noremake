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
#include "ultima/ultima8/gfx/palette_manager.h"
#include "common/algorithm.h"
#include "common/archive.h"
#include "common/file.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "image/png.h"

namespace Ultima {
namespace Ultima8 {

HDShapeCache *HDShapeCache::_instance = nullptr;

// Background preloading stops once this much is decoded (frames needed on
// screen are still loaded). The full Crusader set is about 1.3 GB.
static const uint64 HD_PRELOAD_LIMIT = 3ULL * 1024 * 1024 * 1024;

// Replacement art directories by shape archive and palette (the same shape
// drawn with a different palette needs its own HD version).
static const struct {
	uint16 flexId;
	uint16 palette;
	const char *dir;
} HD_DIRS[] = {
	{ GameData::MAINSHAPES, PaletteManager::Pal_Game, "hdshapes" },
	{ GameData::GUMPS, PaletteManager::Pal_Game, "hdgumps" },
	{ GameData::GUMPS, PaletteManager::Pal_Misc, "hdgumps_misc" },
	{ GameData::GUMPS, PaletteManager::Pal_Diff, "hdgumps_diff" },
	{ GameData::OTHER, PaletteManager::Pal_Game, "hdmouse" }, // mouse cursor (shape 0)
	{ HDShapeCache::FONTS_FLEX, PaletteManager::Pal_Game, "hdfonts" },
};

// Key: archive (2 bits) | palette (3 bits) | shape (11 bits) | frame (16 bits)
static inline uint32 hdKey(uint32 flexId, uint32 palette, uint32 shapenum, uint32 framenum) {
	return ((flexId & 3) << 30) | ((palette & 7) << 27) | ((shapenum & 0x7FF) << 16) | (framenum & 0xFFFF);
}

static const char *hdDir(uint32 flexId, uint32 palette) {
	for (const auto &d : HD_DIRS) {
		if (d.flexId == flexId && d.palette == palette)
			return d.dir;
	}
	return nullptr;
}

HDShapeCache::HDShapeCache() : _loadedBytes(0), _preloadPos(0) {
	for (const auto &d : HD_DIRS) {
		Common::ArchiveMemberList files;
		SearchMan.listMatchingMembers(files, Common::Path(Common::String::format("%s/*.png", d.dir)), true);
		for (const auto &f : files) {
			const Common::String name = f->getFileName();
			uint shapenum, framenum;
			if (sscanf(name.c_str(), "%u_%u.png", &shapenum, &framenum) == 2 && shapenum < 0x800) {
				const uint32 key = hdKey(d.flexId, d.palette, shapenum, framenum);
				_available[key] = true;
				_preloadOrder.push_back(key);
				_framesByShape[key >> 16].push_back(key);
			}
		}
	}
	Common::sort(_preloadOrder.begin(), _preloadOrder.end());
	debug(1, "HDShapeCache: %u high resolution frames available", _available.size());
}

Graphics::Surface *HDShapeCache::decode(uint32 key) {
	const uint32 flexId = key >> 30, palette = (key >> 27) & 7, shapenum = (key >> 16) & 0x7FF, framenum = key & 0xFFFF;
	const uint32 startMs = g_system->getMillis();
	Graphics::Surface *result = nullptr;
	Common::File f;
	Image::PNGDecoder png;
	const Common::String filename = Common::String::format("%s/%04u_%04u.png", hdDir(flexId, palette), shapenum, framenum);
	if (f.open(Common::Path(filename)) && png.loadStream(f) && png.getSurface()) {
		// Native 32-bit 0xAARRGGBB values, as RenderSurface reads them.
		result = png.getSurface()->convertTo(Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24));
		_loadedBytes += (uint64)result->w * result->h * 4;
	} else {
		warning("HDShapeCache: could not load %s", filename.c_str());
		_available.erase(key);
	}
	_loaded[key] = result;
	const uint32 took = g_system->getMillis() - startMs;
	if (took >= 8)
		debug(1, "HDShapeCache: slow load %s took %u ms", filename.c_str(), took);
	return result;
}

void HDShapeCache::preload(uint32 budgetMs) {
	if (_loadedBytes > HD_PRELOAD_LIMIT)
		return;
	const uint32 start = g_system->getMillis();
	while (g_system->getMillis() - start < budgetMs) {
		uint32 key;
		if (!_priority.empty()) {
			key = _priority.front();
			_priority.pop_front();
		} else if (_preloadPos < _preloadOrder.size()) {
			key = _preloadOrder[_preloadPos++];
			if (_preloadPos == _preloadOrder.size())
				debug(1, "HDShapeCache: preload complete after %u ms, %u MB decoded",
					  g_system->getMillis(), (uint32)(_loadedBytes / (1024 * 1024)));
		} else {
			return;
		}
		if (_available.contains(key) && !_loaded.contains(key))
			decode(key);
	}
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

const Graphics::Surface *HDShapeCache::getFrame(uint16 flexId, uint32 shapenum, uint32 framenum, uint32 palette) {
	if (!hdDir(flexId, palette) || shapenum >= 0x800)
		return nullptr;

	const uint32 key = hdKey(flexId, palette, shapenum, framenum);
	if (!_available.contains(key))
		return nullptr;

	SurfaceMap::iterator it = _loaded.find(key);
	if (it != _loaded.end())
		return it->_value;

	// First use of this shape: preload its other frames next, since an
	// animation (firing, explosion...) will need them in the next moments.
	ShapeFramesMap::iterator sf = _framesByShape.find(key >> 16);
	if (sf != _framesByShape.end()) {
		for (const uint32 k : sf->_value)
			_priority.push_back(k);
		_framesByShape.erase(sf);
	}

	return decode(key);
}

} // End of namespace Ultima8
} // End of namespace Ultima
