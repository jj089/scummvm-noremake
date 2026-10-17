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

#ifndef ULTIMA8_GUMPS_CRUOPTIONSGUMP_H
#define ULTIMA8_GUMPS_CRUOPTIONSGUMP_H

#include "ultima/ultima8/gumps/modal_gump.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Crusader styled options screen: movement, display and sound settings,
 * with the key bindings still available through the ScummVM dialog.
 */
class CruOptionsGump : public ModalGump {
public:
	ENABLE_RUNTIME_CLASSTYPE()

	CruOptionsGump();
	~CruOptionsGump() override;

	void InitGump(Gump *newparent, bool take_focus = true) override;
	void Close(bool no_del = false) override;

	void PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) override;

	Gump *onMouseMotion(int32 mx, int32 my) override;
	void onMouseClick(int button, int32 mx, int32 my) override;
	void onMouseDouble(int button, int32 mx, int32 my) override;
	bool OnKeyDown(int key, int mod) override;
	bool OnTextInput(int unicode) override;

	static void show();

	bool loadData(Common::ReadStream *rs, uint32 version);
	void saveData(Common::WriteStream *ws) override;

private:
	int _selected;
	bool _restartNeeded;

	int hitTest(int32 mx, int32 my) const;
	void change(int option, int delta);
	void activate(int option);
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
