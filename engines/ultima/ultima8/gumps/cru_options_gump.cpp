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

#include "ultima/ultima8/gumps/cru_options_gump.h"
#include "ultima/ultima8/gumps/cru_keys_gump.h"
#include "ultima/ultima8/gumps/cru_panel_style.h"

#include "common/config-manager.h"
#include "ultima/ultima8/gfx/fonts/font.h"
#include "ultima/ultima8/gfx/fonts/font_manager.h"
#include "ultima/ultima8/gfx/fonts/rendered_text.h"
#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/gfx/texture.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/ultima8.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(CruOptionsGump)

using namespace CruPanel;


static const int LIST_X = 24;
static const int LIST_W = PANEL_W - 48;
static const int VALUE_X = 300;
static const int ROW_H = 30;
static const int BAR_W = 200;

static const uint32 COL_BAR = TEX32_PACK_RGB(40, 200, 40);
static const uint32 COL_BAR_BACK = TEX32_PACK_RGB(10, 40, 10);

enum OptionKind { OPT_TOGGLE, OPT_SCALE, OPT_VOLUME, OPT_ACTION };

static const struct Option {
	const char *label;
	const char *key;
	OptionKind kind;
	bool defaultOn;
	bool needsRestart;
	const char *onText;
	const char *offText;
} OPTIONS[] = {
	{ "MOVEMENT", "fluid_movement", OPT_TOGGLE, true, true, "MODERN", "CLASSIC" },
	{ "SMOOTH MOTION", "interpolate", OPT_TOGGLE, false, false, "ON", "OFF" },
	{ "SUBTITLES", "subtitles", OPT_TOGGLE, true, false, "ON", "OFF" },
	{ "HD ARTWORK", "hd_scale", OPT_SCALE, true, true, "ON", "OFF" },
	{ "WIDESCREEN", "widescreen", OPT_TOGGLE, true, true, "ON", "OFF" },
	{ "MUSIC VOLUME", "music_volume", OPT_VOLUME, true, false, nullptr, nullptr },
	{ "EFFECTS VOLUME", "sfx_volume", OPT_VOLUME, true, false, nullptr, nullptr },
	{ "SPEECH VOLUME", "speech_volume", OPT_VOLUME, true, false, nullptr, nullptr },
	{ "CONTROLS AND KEYS", nullptr, OPT_ACTION, false, false, nullptr, nullptr },
	{ "BACK", nullptr, OPT_ACTION, false, false, nullptr, nullptr },
};
static const int NUM_OPTIONS = ARRAYSIZE(OPTIONS);
static const int OPTION_CONTROLS = NUM_OPTIONS - 2;
static const int OPTION_BACK = NUM_OPTIONS - 1;

static bool getOn(const Option &opt) {
	if (opt.kind == OPT_SCALE)
		return !ConfMan.hasKey(opt.key) || ConfMan.getInt(opt.key) > 1;
	return ConfMan.hasKey(opt.key) ? ConfMan.getBool(opt.key) : opt.defaultOn;
}

static int getVolume(const Option &opt) {
	return CLIP(ConfMan.hasKey(opt.key) ? ConfMan.getInt(opt.key) : 192, 0, 256);
}

CruOptionsGump::CruOptionsGump()
	: ModalGump(0, 0, PANEL_W, PANEL_H, 0, FLAG_DONT_SAVE), _selected(0), _restartNeeded(false) {
	Mouse::get_instance()->pushMouseCursor(Mouse::MOUSE_HAND);
}

CruOptionsGump::~CruOptionsGump() {
}

void CruOptionsGump::InitGump(Gump *newparent, bool take_focus) {
	ModalGump::InitGump(newparent, take_focus);
}

void CruOptionsGump::Close(bool no_del) {
	ConfMan.flushToDisk();
	Mouse::get_instance()->popMouseCursor();
	ModalGump::Close(no_del);
}

void CruOptionsGump::PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) {
	drawPanel(surf, "OPTIONS");

	for (int i = 0; i < NUM_OPTIONS; i++) {
		const Option &opt = OPTIONS[i];
		int y = LIST_Y + i * ROW_H;
		if (opt.kind == OPT_ACTION)
			y += 8; // a little gap before the buttons
		const bool sel = i == _selected;
		const int font = sel ? FONT_HILITE : FONT_TEXT;
		if (sel)
			surf->fillBlended(COL_ROW, Common::Rect32(LIST_X - 4, y - 5, LIST_X + LIST_W, y + ROW_H - 7));
		if (opt.kind != OPT_ACTION)
			surf->drawLine32(COL_ROW_LINE, LIST_X, y + ROW_H - 6, LIST_X + LIST_W - 4, y + ROW_H - 6);

		drawText(surf, font, opt.label, LIST_X, y);

		switch (opt.kind) {
		case OPT_TOGGLE:
		case OPT_SCALE: {
			const bool on = getOn(opt);
			// Both choices shown, the current one lit.
			drawText(surf, on ? font : FONT_TEXT, opt.onText, VALUE_X, y, 0, Font::TEXT_LEFT, on ? 0 : COL_DIM);
			drawText(surf, !on ? font : FONT_TEXT, opt.offText, VALUE_X + 100, y, 0, Font::TEXT_LEFT, !on ? 0 : COL_DIM);
			if (opt.needsRestart)
				drawText(surf, FONT_TEXT, "*", VALUE_X + 200, y, 0, Font::TEXT_LEFT, COL_DIM);
			break;
		}
		case OPT_VOLUME: {
			const int vol = getVolume(opt);
			const Common::Rect32 bar(VALUE_X, y + 1, VALUE_X + BAR_W, y + 11);
			surf->fill32(COL_BAR_BACK, bar);
			surf->fill32(COL_BAR, Common::Rect32(bar.left, bar.top, bar.left + BAR_W * vol / 256, bar.bottom));
			surf->frameRect32(COL_ROW_LINE, bar);
			drawText(surf, font, Common::String::format("%d%%", (vol * 100 + 128) / 256), VALUE_X + BAR_W + 14, y);
			break;
		}
		default:
			break;
		}
	}

	const int footY = PANEL_H - 34;
	surf->drawLine32(COL_FRAME_DIM, LIST_X, footY - 10, PANEL_W - 24, footY - 10);
	drawText(surf, FONT_TEXT, _restartNeeded ? "* CHANGES APPLY NEXT TIME THE GAME STARTS"
						: "* APPLIES NEXT TIME THE GAME STARTS", LIST_X, footY, 0, Font::TEXT_LEFT,
			 _restartNeeded ? 0 : COL_DIM);
	drawText(surf, FONT_TEXT, "ARROWS OR CLICK TO CHANGE", PANEL_W - 24 - 260, footY, 260,
			 Font::TEXT_RIGHT, COL_DIM);
}

int CruOptionsGump::hitTest(int32 mx, int32 my) const {
	if (mx < LIST_X - 4 || mx >= LIST_X + LIST_W)
		return -1;
	for (int i = 0; i < NUM_OPTIONS; i++) {
		int y = LIST_Y + i * ROW_H;
		if (OPTIONS[i].kind == OPT_ACTION)
			y += 8;
		if (my >= y - 5 && my < y + ROW_H - 5)
			return i;
	}
	return -1;
}

Gump *CruOptionsGump::onMouseMotion(int32 mx, int32 my) {
	Gump *handled = ModalGump::onMouseMotion(mx, my);
	ParentToGump(mx, my);
	const int hit = hitTest(mx, my);
	if (hit >= 0)
		_selected = hit;
	return handled;
}

void CruOptionsGump::onMouseClick(int button, int32 mx, int32 my) {
	if (button != Mouse::BUTTON_LEFT)
		return;
	ParentToGump(mx, my);
	const int hit = hitTest(mx, my);
	if (hit < 0)
		return;
	_selected = hit;
	const Option &opt = OPTIONS[hit];
	switch (opt.kind) {
	case OPT_TOGGLE:
	case OPT_SCALE:
		// Clicking a choice picks it; clicking the label flips it.
		if (mx >= VALUE_X && mx < VALUE_X + 90)
			change(hit, getOn(opt) ? 0 : 1);
		else if (mx >= VALUE_X + 100 && mx < VALUE_X + 190)
			change(hit, getOn(opt) ? -1 : 0);
		else
			change(hit, 1);
		break;
	case OPT_VOLUME:
		if (mx >= VALUE_X - 8 && mx <= VALUE_X + BAR_W + 8) {
			const int vol = CLIP((mx - VALUE_X) * 256 / BAR_W, 0, 256);
			ConfMan.setInt(opt.key, vol);
			Ultima8Engine::get_instance()->syncSoundSettings();
		}
		break;
	default:
		activate(hit); // may delete this
		break;
	}
}

void CruOptionsGump::onMouseDouble(int button, int32 mx, int32 my) {
	onMouseClick(button, mx, my);
}

void CruOptionsGump::change(int option, int delta) {
	if (!delta)
		return;
	const Option &opt = OPTIONS[option];
	Ultima8Engine *engine = Ultima8Engine::get_instance();
	switch (opt.kind) {
	case OPT_TOGGLE:
		ConfMan.setBool(opt.key, !getOn(opt));
		break;
	case OPT_SCALE:
		ConfMan.setInt(opt.key, getOn(opt) ? 1 : 2);
		break;
	case OPT_VOLUME:
		ConfMan.setInt(opt.key, CLIP(getVolume(opt) + delta * 16, 0, 256));
		engine->syncSoundSettings();
		return;
	default:
		return;
	}
	if (opt.needsRestart)
		_restartNeeded = true;
	else
		engine->applyGameSettings();
}

void CruOptionsGump::activate(int option) {
	if (option == OPTION_BACK) {
		Close();
	} else if (option == OPTION_CONTROLS) {
		CruKeysGump::show();
	} else {
		change(option, 1);
	}
}

bool CruOptionsGump::OnKeyDown(int key, int mod) {
	switch (key) {
	case Common::KEYCODE_ESCAPE:
	case Common::KEYCODE_BACKSPACE:
		Close();
		break;
	case Common::KEYCODE_UP:
	case Common::KEYCODE_KP8:
	case Common::KEYCODE_w:
		_selected = (_selected + NUM_OPTIONS - 1) % NUM_OPTIONS;
		break;
	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_KP2:
	case Common::KEYCODE_s:
		_selected = (_selected + 1) % NUM_OPTIONS;
		break;
	case Common::KEYCODE_LEFT:
	case Common::KEYCODE_KP4:
	case Common::KEYCODE_a:
		change(_selected, -1);
		break;
	case Common::KEYCODE_RIGHT:
	case Common::KEYCODE_KP6:
	case Common::KEYCODE_d:
		change(_selected, 1);
		break;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
	case Common::KEYCODE_SPACE:
		activate(_selected); // may delete this
		break;
	default:
		break;
	}
	return true;
}

bool CruOptionsGump::OnTextInput(int unicode) {
	return true;
}

//static
void CruOptionsGump::show() {
	CruOptionsGump *gump = new CruOptionsGump();
	gump->InitGump(nullptr);
	gump->setRelativePosition(CENTER);
}

bool CruOptionsGump::loadData(Common::ReadStream *rs, uint32 version) {
	warning("Trying to load ModalGump");
	return false;
}

void CruOptionsGump::saveData(Common::WriteStream *ws) {
	warning("Trying to save ModalGump");
}

} // End of namespace Ultima8
} // End of namespace Ultima
