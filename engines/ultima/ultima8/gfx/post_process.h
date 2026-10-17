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

#include "common/array.h"
#include "graphics/surface.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Whole-screen effects applied to the finished frame just before it is
 * shown: a light sharpen with a little extra colour and contrast, and a soft
 * glow around bright, strongly coloured pixels (lights, fire, screens).
 * Only 32-bit surfaces are processed.
 */
class PostProcess {
public:
	PostProcess();
	~PostProcess();

	//! Process the frame into a separate buffer and show that on screen;
	//! the frame itself is left unchanged. Returns false if nothing was done.
	//! glowMask marks which cells (GLOW_SCALE pixels square) give off light.
	bool present(const Graphics::Surface &frame, bool sharpen, bool glow,
				 const Common::Array<uint8> &glowMask, int maskW, int maskH);

private:
	Graphics::Surface _out;
	uint8 _contrast[256];

	// Glow, worked out at a quarter of the screen size.
	int _qw, _qh;
	Common::Array<uint8> _glow[3], _tmp; //!< r, g, b planes
	Common::Array<uint32> _glowRows;     //!< glow rows stretched to full width
	Common::Array<bool> _glowRowUsed;

	void sharpenInto(const Graphics::Surface &frame);
	bool buildGlow(const Graphics::Surface &frame, const Common::Array<uint8> &mask, int maskW, int maskH);
	void blurPlane(uint8 *plane);
	void addGlow();
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
