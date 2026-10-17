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

#ifndef ULTIMA8_GUMPS_CRUSAVEGUMP_H
#define ULTIMA8_GUMPS_CRUSAVEGUMP_H

#include "ultima/ultima8/gumps/modal_gump.h"
#include "graphics/managed_surface.h"

namespace Ultima {
namespace Ultima8 {

class EditWidget;

/**
 * Crusader styled load / save screen, used in place of the ScummVM dialogs.
 * Ten slots per page, with a thumbnail and details of the selected save.
 */
class CruSaveGump : public ModalGump {
public:
	ENABLE_RUNTIME_CLASSTYPE()

	CruSaveGump(bool saveMode);
	~CruSaveGump() override;

	void InitGump(Gump *newparent, bool take_focus = true) override;
	void Close(bool no_del = false) override;

	void PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) override;

	Gump *onMouseMotion(int32 mx, int32 my) override;
	void onMouseClick(int button, int32 mx, int32 my) override;
	void onMouseDouble(int button, int32 mx, int32 my) override;
	bool OnKeyDown(int key, int mod) override;
	bool OnTextInput(int unicode) override;
	void ChildNotify(Gump *child, uint32 message) override;

	//! Open the screen over everything else.
	static void show(bool saveMode);

	bool loadData(Common::ReadStream *rs, uint32 version);
	void saveData(Common::WriteStream *ws) override;

	static const int SLOTS_PER_PAGE = 10;
	static const int NUM_PAGES = 10;

private:
	struct SlotInfo {
		bool used;
		Common::String description;
		Common::String when;
		uint32 playtime;
	};

	bool _save;
	int _page;
	int _selected; //!< row on the page, -1 for none
	SlotInfo _slots[SLOTS_PER_PAGE];
	Graphics::ManagedSurface _thumb;
	int _thumbSlot;
	EditWidget *_editor;

	enum HitArea { HIT_NONE = -1, HIT_PREV = 100, HIT_NEXT, HIT_BACK };

	void loadPage(int page);
	void loadThumbnail();
	int slotFor(int row) const { return _page * SLOTS_PER_PAGE + row; }
	int hitTest(int32 mx, int32 my) const;
	void select(int row);
	void activate(int row);
	void startEdit(int row);
	void stopEdit();
	void changePage(int delta);
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
