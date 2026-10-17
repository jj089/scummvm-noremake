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

#ifndef ULTIMA8_GFX_POSTPROCESS_H
#define ULTIMA8_GFX_POSTPROCESS_H

#include "graphics/surface.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Whole-screen effects applied to the finished frame just before it is
 * shown: a light sharpen with a little extra colour and contrast.
 * Only 32-bit surfaces are processed.
 */
class PostProcess {
public:
	PostProcess();
	~PostProcess();

	//! Process the frame into a separate buffer and show that on screen;
	//! the frame itself is left unchanged. Returns false if nothing was done.
	bool present(const Graphics::Surface &frame);

private:
	Graphics::Surface _out;
	uint8 _contrast[256];

};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
