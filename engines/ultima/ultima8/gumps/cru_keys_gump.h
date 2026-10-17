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

#ifndef ULTIMA8_GUMPS_CRUKEYSGUMP_H
#define ULTIMA8_GUMPS_CRUKEYSGUMP_H

#include "ultima/ultima8/gumps/modal_gump.h"
#include "backends/keymapper/hardware-input.h"

namespace Common {
struct Action;
class InputWatcher;
class Keymap;
}

namespace Ultima {
namespace Ultima8 {

/**
 * Crusader styled key binding screen. Each game action can have two
 * inputs (keys, mouse or controller buttons).
 */
class CruKeysGump : public ModalGump {
public:
	ENABLE_RUNTIME_CLASSTYPE()

	CruKeysGump();
	~CruKeysGump() override;

	void InitGump(Gump *newparent, bool take_focus = true) override;
	void Close(bool no_del = false) override;
	void run() override;

	void PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) override;

	Gump *onMouseMotion(int32 mx, int32 my) override;
	void onMouseClick(int button, int32 mx, int32 my) override;
	void onMouseDouble(int button, int32 mx, int32 my) override;
	bool OnKeyDown(int key, int mod) override;
	bool OnTextInput(int unicode) override;

	static void show();

	bool loadData(Common::ReadStream *rs, uint32 version);
	void saveData(Common::WriteStream *ws) override;

	static const int ROWS_PER_PAGE = 10;

private:
	Common::Keymap *_keymap;
	Common::Array<Common::Action *> _actions;
	Common::InputWatcher *_watcher;
	int _page;
	int _selected;    //!< row on the page
	int _column;      //!< 0 or 1: which binding of the row
	int _capturing;   //!< action index being bound, -1 when not
	uint32 _captureStart;
	bool _watchStarted;
	Common::String _message;

	enum HitArea { HIT_NONE = -1, HIT_PREV = 1000, HIT_NEXT, HIT_RESET, HIT_BACK };

	int numPages() const;
	int actionFor(int row) const { return _page * ROWS_PER_PAGE + row; }
	Common::Array<Common::HardwareInput> inputsFor(int action) const;
	int hitTest(int32 mx, int32 my, int *column) const;
	void changePage(int delta);
	void startCapture();
	void stopCapture();
	void bind(int action, int column, const Common::HardwareInput &input);
	void clearBinding(int action);
	void resetAll();
	void save();
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
