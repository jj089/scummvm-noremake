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

#include "ultima/ultima8/gumps/cru_keys_gump.h"
#include "ultima/ultima8/gumps/cru_options_gump.h"
#include "ultima/ultima8/gumps/cru_panel_style.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/input-watcher.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/keymapper.h"
#include "backends/keymapper/standard-actions.h"
#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/ultima8.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(CruKeysGump)

using namespace CruPanel;

static const int ROW_H = 26;
static const int ACTION_W = 250;
static const int KEY1_X = MARGIN + 270;
static const int KEY_W = 140;
static const int KEY2_X = KEY1_X + KEY_W + 4;
static const int NAV_Y = PANEL_H - 40;

// Wait before listening, so the release of the key that started the
// capture isn't taken as the new binding.
static const uint32 CAPTURE_DELAY = 250;
static const uint32 CAPTURE_TIMEOUT = 6000;

static bool inputLess(const Common::HardwareInput &a, const Common::HardwareInput &b) {
	if (a.type != b.type)
		return a.type < b.type;
	return a.id.compareTo(b.id) < 0;
}

static Common::String inputName(const Common::HardwareInput &input) {
	Common::String name = input.description.encode();
	name.toUppercase();
	if (input.type == Common::kHardwareInputTypeJoystickButton ||
		input.type == Common::kHardwareInputTypeJoystickHalfAxis)
		name = "PAD " + name;
	return name;
}

CruKeysGump::CruKeysGump()
	: ModalGump(0, 0, PANEL_W, PANEL_H, 0, FLAG_DONT_SAVE), _keymap(nullptr), _watcher(nullptr),
	  _page(0), _selected(0), _column(0), _capturing(-1), _captureStart(0), _watchStarted(false) {
	Mouse::get_instance()->pushMouseCursor(Mouse::MOUSE_HAND);
}

CruKeysGump::~CruKeysGump() {
	delete _watcher;
}

void CruKeysGump::InitGump(Gump *newparent, bool take_focus) {
	ModalGump::InitGump(newparent, take_focus);

	Common::EventManager *eventMan = g_system->getEventManager();
	Common::Keymapper *keymapper = eventMan->getKeymapper();
	// The main game keymap; the cheat and debug ones stay hidden.
	for (Common::Keymap *km : keymapper->getKeymaps()) {
		if (km->getType() == Common::Keymap::kKeymapTypeGame && !km->getId().hasPrefix("ultima8_")) {
			_keymap = km;
			break;
		}
	}
	if (_keymap) {
		for (Common::Action *act : _keymap->getActions()) {
			const Common::String id = act->id;
			// Leave the mouse buttons alone, or the menus become unusable.
			if (id == Common::kStandardActionLeftClick || id == Common::kStandardActionRightClick)
				continue;
			// Actions unused in this control scheme (no default and no key).
			if (act->getDefaultInputMapping().empty() && _keymap->getActionMapping(act).empty())
				continue;
			_actions.push_back(act);
		}
	}
	_watcher = new Common::InputWatcher(eventMan->getEventDispatcher(), keymapper);
}

void CruKeysGump::Close(bool no_del) {
	stopCapture();
	Gump *options = Ultima8Engine::get_instance()->getDesktopGump()->FindGump<CruOptionsGump>();
	if (options)
		options->UnhideGump();
	Mouse::get_instance()->popMouseCursor();
	ModalGump::Close(no_del);
}

int CruKeysGump::numPages() const {
	return MAX<int>(1, (_actions.size() + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
}

Common::Array<Common::HardwareInput> CruKeysGump::inputsFor(int action) const {
	Common::Array<Common::HardwareInput> inputs = _keymap->getActionMapping(_actions[action]);
	Common::sort(inputs.begin(), inputs.end(), inputLess);
	return inputs;
}

void CruKeysGump::run() {
	ModalGump::run();
	if (_capturing < 0)
		return;

	const uint32 now = g_system->getMillis();
	if (now - _captureStart < CAPTURE_DELAY)
		return;
	if (!_watchStarted) {
		_watchStarted = true;
		_watcher->startWatching();
		return;
	}

	// The watcher stops by itself once it has an input.
	const Common::HardwareInput input = _watcher->checkForCapturedInput();
	if (input.type != Common::kHardwareInputTypeInvalid) {
		const int action = _capturing;
		_capturing = -1;
		// Escape cancels rather than binding.
		if (input.type == Common::kHardwareInputTypeKeyboard && input.key.keycode == Common::KEYCODE_ESCAPE)
			_message = "CANCELLED";
		else
			bind(action, _column, input);
	} else if (now - _captureStart > CAPTURE_TIMEOUT) {
		stopCapture();
		_message = "NO KEY PRESSED";
	}
}

void CruKeysGump::PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) {
	drawPanel(surf, "CONTROLS");
	drawText(surf, FONT_TEXT, Common::String::format("PAGE %d OF %d", _page + 1, numPages()),
			 PANEL_W - MARGIN - 200, 26, 200, Font::TEXT_RIGHT);

	if (!_keymap) {
		drawText(surf, FONT_TEXT, "NO CONTROLS FOUND", MARGIN, LIST_Y);
		return;
	}

	drawText(surf, FONT_TEXT, "ACTION", MARGIN, LIST_Y, 0, Font::TEXT_LEFT, COL_DIM);
	drawText(surf, FONT_TEXT, "KEY", KEY1_X, LIST_Y, 0, Font::TEXT_LEFT, COL_DIM);
	drawText(surf, FONT_TEXT, "ALTERNATE", KEY2_X, LIST_Y, 0, Font::TEXT_LEFT, COL_DIM);

	const int top = LIST_Y + 22;
	for (int row = 0; row < ROWS_PER_PAGE; row++) {
		const int action = actionFor(row);
		if (action >= (int)_actions.size())
			break;
		const int y = top + row * ROW_H;
		const bool sel = row == _selected;
		if (sel) {
			surf->fillBlended(COL_ROW, Common::Rect32(MARGIN - 4, y - 5, PANEL_W - MARGIN, y + ROW_H - 7));
			const int cx = _column ? KEY2_X : KEY1_X;
			surf->frameRect32(COL_ROW_LINE, Common::Rect32(cx - 4, y - 4, cx + KEY_W - 4, y + ROW_H - 8));
		}
		surf->drawLine32(COL_ROW_LINE, MARGIN, y + ROW_H - 6, PANEL_W - MARGIN - 4, y + ROW_H - 6);

		const int font = sel ? FONT_HILITE : FONT_TEXT;
		Common::String desc = _actions[action]->description.encode();
		desc.toUppercase();
		drawText(surf, font, desc, MARGIN, y, ACTION_W);

		const Common::Array<Common::HardwareInput> inputs = inputsFor(action);
		for (int col = 0; col < 2; col++) {
			const int x = col ? KEY2_X : KEY1_X;
			if (_capturing == action && _column == col)
				drawText(surf, FONT_HILITE, "PRESS A KEY", x, y, KEY_W - 8);
			else if (col < (int)inputs.size())
				drawText(surf, font, inputName(inputs[col]), x, y, KEY_W - 8);
			else
				drawText(surf, font, "--", x, y, 0, Font::TEXT_LEFT, COL_DIM);
		}
	}

	const char *hint = _capturing >= 0 ? "PRESS THE NEW KEY OR BUTTON  -  ESC TO CANCEL"
		: Ultima8Engine::get_instance()->isControllerActive() ? "A CHANGE  -  X CLEAR  -  B BACK"
		: "ENTER TO CHANGE  -  DELETE TO CLEAR";
	drawText(surf, FONT_TEXT, _message.empty() ? Common::String(hint) : _message,
			 MARGIN, NAV_Y - 30, 0, Font::TEXT_LEFT, _message.empty() ? COL_DIM : 0);

	surf->drawLine32(COL_FRAME_DIM, MARGIN, NAV_Y - 10, PANEL_W - MARGIN, NAV_Y - 10);
	drawText(surf, FONT_TEXT, "< PREV", MARGIN, NAV_Y, 0, Font::TEXT_LEFT, _page > 0 ? 0 : COL_DIM);
	drawText(surf, FONT_TEXT, "NEXT >", MARGIN + 120, NAV_Y, 0, Font::TEXT_LEFT,
			 _page < numPages() - 1 ? 0 : COL_DIM);
	drawText(surf, FONT_TEXT, "RESET ALL", MARGIN + 270, NAV_Y);
	drawText(surf, FONT_HILITE, "BACK", PANEL_W - MARGIN - 60, NAV_Y, 60, Font::TEXT_RIGHT);
}

int CruKeysGump::hitTest(int32 mx, int32 my, int *column) const {
	const int top = LIST_Y + 22;
	if (mx >= MARGIN - 4 && mx < PANEL_W - MARGIN && my >= top - 5 && my < top - 5 + ROWS_PER_PAGE * ROW_H) {
		const int row = (my - (top - 5)) / ROW_H;
		if (actionFor(row) < (int)_actions.size()) {
			if (column)
				*column = mx >= KEY2_X - 4 ? 1 : (mx >= KEY1_X - 4 ? 0 : -1);
			return row;
		}
	}
	if (my >= NAV_Y - 6 && my < NAV_Y + 20) {
		if (mx >= MARGIN - 4 && mx < MARGIN + 80)
			return HIT_PREV;
		if (mx >= MARGIN + 110 && mx < MARGIN + 200)
			return HIT_NEXT;
		if (mx >= MARGIN + 260 && mx < MARGIN + 370)
			return HIT_RESET;
		if (mx >= PANEL_W - 110 && mx < PANEL_W - 10)
			return HIT_BACK;
	}
	return HIT_NONE;
}

Gump *CruKeysGump::onMouseMotion(int32 mx, int32 my) {
	Gump *handled = ModalGump::onMouseMotion(mx, my);
	if (_capturing < 0) {
		ParentToGump(mx, my);
		int column = -1;
		const int hit = hitTest(mx, my, &column);
		if (hit >= 0 && hit < ROWS_PER_PAGE) {
			_selected = hit;
			if (column >= 0)
				_column = column;
		}
	}
	return handled;
}

void CruKeysGump::onMouseClick(int button, int32 mx, int32 my) {
	if (button != Mouse::BUTTON_LEFT || _capturing >= 0)
		return;
	ParentToGump(mx, my);
	int column = -1;
	const int hit = hitTest(mx, my, &column);
	_message.clear();
	if (hit == HIT_PREV) {
		changePage(-1);
	} else if (hit == HIT_NEXT) {
		changePage(1);
	} else if (hit == HIT_RESET) {
		resetAll();
	} else if (hit == HIT_BACK) {
		Close();
	} else if (hit >= 0) {
		_selected = hit;
		_column = MAX(column, 0);
		startCapture();
	}
}

void CruKeysGump::onMouseDouble(int button, int32 mx, int32 my) {
	onMouseClick(button, mx, my);
}

void CruKeysGump::changePage(int delta) {
	const int page = _page + delta;
	if (page < 0 || page >= numPages())
		return;
	_page = page;
	const int rows = MIN<int>(ROWS_PER_PAGE, _actions.size() - _page * ROWS_PER_PAGE);
	_selected = MIN(_selected, rows - 1);
}

void CruKeysGump::startCapture() {
	if (actionFor(_selected) >= (int)_actions.size())
		return;
	_capturing = actionFor(_selected);
	_captureStart = g_system->getMillis();
	_watchStarted = false;
	_message.clear();
}

void CruKeysGump::stopCapture() {
	_capturing = -1;
	if (_watcher && _watcher->isWatching())
		_watcher->stopWatching();
}

void CruKeysGump::bind(int action, int column, const Common::HardwareInput &input) {
	// An input can only do one thing: take it away from any other action.
	for (uint i = 0; i < _actions.size(); i++) {
		if ((int)i == action)
			continue;
		Common::Array<Common::HardwareInput> others = inputsFor(i);
		bool found = false;
		for (uint j = 0; j < others.size(); j++) {
			if (others[j].id == input.id) {
				others.remove_at(j);
				found = true;
				break;
			}
		}
		if (found) {
			_keymap->unregisterMapping(_actions[i]);
			for (const auto &hw : others)
				_keymap->registerMapping(_actions[i], hw);
			Common::String desc = _actions[i]->description.encode();
			desc.toUppercase();
			_message = Common::String::format("%s WAS UNBOUND FROM %s", inputName(input).c_str(), desc.c_str());
		}
	}

	Common::Array<Common::HardwareInput> inputs = inputsFor(action);
	for (uint j = 0; j < inputs.size(); j++) {
		if (inputs[j].id == input.id) {
			inputs.remove_at(j);
			break;
		}
	}
	if (column < (int)inputs.size())
		inputs[column] = input;
	else
		inputs.push_back(input);

	_keymap->unregisterMapping(_actions[action]);
	for (const auto &hw : inputs)
		_keymap->registerMapping(_actions[action], hw);
	save();
}

void CruKeysGump::clearBinding(int action) {
	if (action >= (int)_actions.size())
		return;
	Common::Array<Common::HardwareInput> inputs = inputsFor(action);
	if (_column < (int)inputs.size())
		inputs.remove_at(_column);
	_keymap->unregisterMapping(_actions[action]);
	for (const auto &hw : inputs)
		_keymap->registerMapping(_actions[action], hw);
	save();
}

void CruKeysGump::resetAll() {
	for (Common::Action *act : _keymap->getActions())
		_keymap->resetMapping(act);
	save();
	_message = "ALL CONTROLS RESET TO DEFAULTS";
}

void CruKeysGump::save() {
	_keymap->saveMappings();
	ConfMan.flushToDisk();
}

bool CruKeysGump::OnKeyDown(int key, int mod) {
	if (_capturing >= 0 || !_keymap)
		return true;
	_message.clear();
	const int rows = MIN<int>(ROWS_PER_PAGE, _actions.size() - _page * ROWS_PER_PAGE);

	switch (key) {
	case Common::KEYCODE_ESCAPE:
	case Common::KEYCODE_BACKSPACE:
		Close();
		break;
	case Common::KEYCODE_UP:
	case Common::KEYCODE_KP8:
		if (_selected > 0) {
			_selected--;
		} else if (_page > 0) {
			changePage(-1);
			_selected = ROWS_PER_PAGE - 1;
		}
		break;
	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_KP2:
		if (_selected < rows - 1) {
			_selected++;
		} else if (_page < numPages() - 1) {
			changePage(1);
			_selected = 0;
		}
		break;
	case Common::KEYCODE_LEFT:
	case Common::KEYCODE_KP4:
		_column = 0;
		break;
	case Common::KEYCODE_RIGHT:
	case Common::KEYCODE_KP6:
		_column = 1;
		break;
	case Common::KEYCODE_PAGEUP:
		changePage(-1);
		break;
	case Common::KEYCODE_PAGEDOWN:
		changePage(1);
		break;
	case Common::KEYCODE_DELETE:
		clearBinding(actionFor(_selected));
		break;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
	case Common::KEYCODE_SPACE:
		startCapture();
		break;
	default:
		break;
	}
	return true;
}

bool CruKeysGump::OnTextInput(int unicode) {
	return true;
}

//static
void CruKeysGump::show() {
	// Only one panel at a time, they're see-through.
	Gump *options = Ultima8Engine::get_instance()->getDesktopGump()->FindGump<CruOptionsGump>();
	if (options)
		options->HideGump();
	CruKeysGump *gump = new CruKeysGump();
	gump->InitGump(nullptr);
	gump->setRelativePosition(CENTER);
}

bool CruKeysGump::loadData(Common::ReadStream *rs, uint32 version) {
	warning("Trying to load ModalGump");
	return false;
}

void CruKeysGump::saveData(Common::WriteStream *ws) {
	warning("Trying to save ModalGump");
}

} // End of namespace Ultima8
} // End of namespace Ultima
