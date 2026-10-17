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

#ifndef ULTIMA8_GUMPS_CRUPANELSTYLE_H
#define ULTIMA8_GUMPS_CRUPANELSTYLE_H

#include "ultima/ultima8/gfx/fonts/font.h"
#include "ultima/ultima8/gfx/fonts/font_manager.h"
#include "ultima/ultima8/gfx/fonts/rendered_text.h"
#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/gfx/texture.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Shared look of the Crusader styled menu screens (save/load, options, keys):
 * a dark translucent panel with a red frame and green terminal text.
 */
namespace CruPanel {

// Green and yellow terminal fonts from the HUD, large orange titles.
static const int FONT_TEXT = 6;
static const int FONT_HILITE = 15;
static const int FONT_TITLE = 14;

static const int PANEL_W = 600;
static const int PANEL_H = 420;
static const int MARGIN = 24;
static const int LIST_Y = 64;

static const uint32 COL_PANEL = TEX32_PACK_RGBA(6, 10, 22, 232);
static const uint32 COL_FRAME = TEX32_PACK_RGB(190, 24, 24);
static const uint32 COL_FRAME_DIM = TEX32_PACK_RGB(90, 14, 14);
static const uint32 COL_ROW = TEX32_PACK_RGBA(40, 160, 40, 70);
static const uint32 COL_ROW_LINE = TEX32_PACK_RGB(20, 70, 20);
static const uint32 COL_DIM = TEX32_PACK_RGBA(6, 10, 22, 150);
static const uint32 COL_TITLE = TEX32_PACK_RGBA(235, 45, 20, 190);

inline void drawText(RenderSurface *surf, int fontnum, const Common::String &text, int x, int y,
					 int width = 0, Font::TextAlign align = Font::TEXT_LEFT, uint32 blend = 0) {
	Font *font = FontManager::get_instance()->getGameFont(fontnum, true);
	if (!font || text.empty())
		return;
	unsigned int remaining;
	RenderedText *rt = font->renderText(text, remaining, width, 0, align);
	if (blend)
		rt->drawBlended(surf, x, y, blend);
	else
		rt->draw(surf, x, y);
	delete rt;
}

//! Panel background, frame, title and the rule under it.
inline void drawPanel(RenderSurface *surf, const Common::String &title) {
	const Common::Rect32 panel(0, 0, PANEL_W, PANEL_H);
	surf->fillBlended(COL_PANEL, panel);
	surf->frameRect32(COL_FRAME, panel);
	surf->frameRect32(COL_FRAME_DIM, Common::Rect32(3, 3, PANEL_W - 3, PANEL_H - 3));
	// Tinted: the title font's yellow edge pixels look like a green fringe
	// once upscaled.
	drawText(surf, FONT_TITLE, title, MARGIN, 18, 0, Font::TEXT_LEFT, COL_TITLE);
	surf->drawLine32(COL_FRAME_DIM, MARGIN, LIST_Y - 8, PANEL_W - MARGIN, LIST_Y - 8);
}

} // End of namespace CruPanel

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
