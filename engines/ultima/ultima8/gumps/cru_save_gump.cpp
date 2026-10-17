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

#include "ultima/ultima8/gumps/cru_save_gump.h"
#include "ultima/ultima8/gumps/cru_panel_style.h"

#include "common/config-manager.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/metaengine.h"
#include "graphics/surface.h"
#include "gui/message.h"
#include "ultima/ultima8/filesys/savegame.h"
#include "ultima/ultima8/gfx/fonts/font.h"
#include "ultima/ultima8/gfx/fonts/font_manager.h"
#include "ultima/ultima8/gfx/fonts/rendered_text.h"
#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/gfx/texture.h"
#include "ultima/ultima8/gumps/cru_menu_gump.h"
#include "ultima/ultima8/gumps/widgets/edit_widget.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/ultima8.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(CruSaveGump)

using namespace CruPanel;

// Same as in savegame.cpp: Ultima 8 saves start with this, then a version.
static const uint32 SAVEGAME_IDENT = MKTAG('V', 'M', 'U', '8');


static const int LIST_X = 24;
static const int LIST_W = 340;
static const int ROW_H = 28;
static const int NAV_Y = PANEL_H - 40;
static const int INFO_X = 384;
static const int THUMB_W = 192;
static const int THUMB_H = 144;
static const int MAX_DESC = 28;


CruSaveGump::CruSaveGump(bool saveMode)
	: ModalGump(0, 0, PANEL_W, PANEL_H, 0, FLAG_DONT_SAVE), _save(saveMode), _page(0),
	  _selected(-1), _thumbSlot(-1), _editor(nullptr) {
	Mouse::get_instance()->pushMouseCursor(Mouse::MOUSE_HAND);
}

CruSaveGump::~CruSaveGump() {
}

void CruSaveGump::InitGump(Gump *newparent, bool take_focus) {
	ModalGump::InitGump(newparent, take_focus);

	// Open on the page with the most recent save.
	int lastSave = ConfMan.hasKey("lastSave") ? ConfMan.getInt("lastSave") : -1;
	int page = lastSave > 0 ? lastSave / SLOTS_PER_PAGE : 0;
	loadPage(CLIP(page, 0, NUM_PAGES - 1));
	if (lastSave > 0)
		select(lastSave % SLOTS_PER_PAGE);
}

void CruSaveGump::Close(bool no_del) {
	Mouse::get_instance()->popMouseCursor();
	ModalGump::Close(no_del);
}

void CruSaveGump::loadPage(int page) {
	stopEdit();
	_page = page;
	_selected = -1;
	_thumb.free();
	_thumbSlot = -1;

	Ultima8Engine *engine = Ultima8Engine::get_instance();
	for (int row = 0; row < SLOTS_PER_PAGE; row++) {
		SlotInfo &info = _slots[row];
		info.used = false;
		info.description.clear();
		info.when.clear();
		info.playtime = 0;

		Common::ScopedPtr<Common::InSaveFile> f(g_system->getSavefileManager()->openForLoading(
			engine->getSaveStateName(slotFor(row))));
		if (!f)
			continue;
		ExtendedSavegameHeader header;
		if (f->readUint32LE() != SAVEGAME_IDENT)
			continue;
		f->readUint32LE(); // version
		if (!::MetaEngine::readSavegameHeader(f.get(), &header, true))
			continue;
		info.used = true;
		info.description = header.description;
		info.description.toUppercase();
		info.when = Common::String::format("%02d/%02d/%04d %02d:%02d",
			(header.date >> 24) & 0xFF, (header.date >> 16) & 0xFF, header.date & 0xFFFF,
			(header.time >> 8) & 0xFF, header.time & 0xFF);
		info.playtime = header.playtime;
	}
}

void CruSaveGump::loadThumbnail() {
	_thumb.free();
	_thumbSlot = -1;
	if (_selected < 0 || !_slots[_selected].used)
		return;
	_thumbSlot = slotFor(_selected);

	Common::ScopedPtr<Common::InSaveFile> f(g_system->getSavefileManager()->openForLoading(
		Ultima8Engine::get_instance()->getSaveStateName(_thumbSlot)));
	if (!f || f->readUint32LE() != SAVEGAME_IDENT)
		return;
	f->readUint32LE();
	ExtendedSavegameHeader header;
	if (!::MetaEngine::readSavegameHeader(f.get(), &header, false) || !header.thumbnail)
		return;
	_thumb.copyFrom(*header.thumbnail);
	header.thumbnail->free();
	delete header.thumbnail;
}

void CruSaveGump::PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) {
	drawPanel(surf, _save ? "SAVE GAME" : "LOAD GAME");
	drawText(surf, FONT_TEXT, Common::String::format("PAGE %d OF %d", _page + 1, NUM_PAGES),
			 INFO_X, 26, PANEL_W - INFO_X - 24, Font::TEXT_RIGHT);

	// Slot list
	for (int row = 0; row < SLOTS_PER_PAGE; row++) {
		const int y = LIST_Y + row * ROW_H;
		const int slot = slotFor(row);
		const SlotInfo &info = _slots[row];
		const bool sel = row == _selected;
		if (sel)
			surf->fillBlended(COL_ROW, Common::Rect32(LIST_X - 4, y - 4, LIST_X + LIST_W, y + ROW_H - 6));
		surf->drawLine32(COL_ROW_LINE, LIST_X, y + ROW_H - 5, LIST_X + LIST_W - 4, y + ROW_H - 5);

		const int font = sel ? FONT_HILITE : FONT_TEXT;
		drawText(surf, font, Common::String::format("%02d", slot), LIST_X, y);
		if (_editor && sel)
			continue; // the edit widget draws the name
		if (info.used)
			drawText(surf, font, info.description, LIST_X + 36, y, LIST_W - 40);
		else if (slot == 0)
			drawText(surf, font, "AUTOSAVE", LIST_X + 36, y, 0, Font::TEXT_LEFT, COL_DIM);
		else
			drawText(surf, font, _save ? "EMPTY SLOT" : "--", LIST_X + 36, y, 0, Font::TEXT_LEFT, COL_DIM);
	}

	// Details of the selected save
	const Common::Rect32 thumbRect(INFO_X, LIST_Y - 2, INFO_X + THUMB_W, LIST_Y - 2 + THUMB_H);
	surf->fill32(TEX32_PACK_RGB(0, 0, 0), thumbRect);
	if (_selected >= 0 && _slots[_selected].used) {
		if (_thumbSlot != slotFor(_selected))
			loadThumbnail();
		if (_thumb.w > 0) {
			const Graphics::PixelFormat &fmt = surf->getRawSurface()->format;
			if (_thumb.format != fmt)
				_thumb.convertToInPlace(fmt);
			surf->BlitPhysical(_thumb, Common::Rect(_thumb.w, _thumb.h), thumbRect);
		}
		const SlotInfo &info = _slots[_selected];
		int y = thumbRect.bottom + 12;
		drawText(surf, FONT_HILITE, info.description, INFO_X, y, THUMB_W);
		int32 tw, th;
		unsigned int remaining;
		FontManager::get_instance()->getGameFont(FONT_HILITE, true)->getTextSize(info.description, tw, th, remaining, THUMB_W, 0);
		y += th + 10;
		drawText(surf, FONT_TEXT, "SAVED " + info.when, INFO_X, y, THUMB_W);
		y += 18;
		const uint32 mins = info.playtime / 60000;
		if (mins)
			drawText(surf, FONT_TEXT, Common::String::format("PLAYED %uH %02uM", mins / 60, mins % 60), INFO_X, y, THUMB_W);
	} else {
		drawText(surf, FONT_TEXT, "NO DATA", INFO_X, thumbRect.top + THUMB_H / 2 - 5, THUMB_W,
				 Font::TEXT_CENTER, COL_DIM);
	}
	surf->frameRect32(COL_FRAME_DIM, thumbRect);

	const char *hint = _editor ? "TYPE A NAME  -  ENTER TO SAVE  -  ESC TO CANCEL"
		: Ultima8Engine::get_instance()->isControllerActive()
			? (_save ? "A SAVE  -  B BACK  -  LB RB PAGE" : "A LOAD  -  B BACK  -  LB RB PAGE")
			: (_save ? "CLICK A SLOT TO SAVE" : "CLICK A SAVE TO LOAD");
	drawText(surf, FONT_TEXT, hint, INFO_X, NAV_Y - 54, THUMB_W, Font::TEXT_LEFT, COL_DIM);

	// Navigation
	surf->drawLine32(COL_FRAME_DIM, LIST_X, NAV_Y - 10, PANEL_W - 24, NAV_Y - 10);
	drawText(surf, _page > 0 ? FONT_TEXT : FONT_TEXT, "< PREV", LIST_X, NAV_Y, 0, Font::TEXT_LEFT,
			 _page > 0 ? 0 : COL_DIM);
	drawText(surf, FONT_TEXT, "NEXT >", LIST_X + 120, NAV_Y, 0, Font::TEXT_LEFT,
			 _page < NUM_PAGES - 1 ? 0 : COL_DIM);
	drawText(surf, FONT_HILITE, "BACK", PANEL_W - 24 - 60, NAV_Y, 60, Font::TEXT_RIGHT);
}

int CruSaveGump::hitTest(int32 mx, int32 my) const {
	if (mx >= LIST_X - 4 && mx < LIST_X + LIST_W && my >= LIST_Y - 4) {
		const int row = (my - (LIST_Y - 4)) / ROW_H;
		if (row < SLOTS_PER_PAGE)
			return row;
	}
	if (my >= NAV_Y - 6 && my < NAV_Y + 20) {
		if (mx >= LIST_X - 4 && mx < LIST_X + 80)
			return HIT_PREV;
		if (mx >= LIST_X + 110 && mx < LIST_X + 200)
			return HIT_NEXT;
		if (mx >= PANEL_W - 110 && mx < PANEL_W - 10)
			return HIT_BACK;
	}
	return HIT_NONE;
}

Gump *CruSaveGump::onMouseMotion(int32 mx, int32 my) {
	Gump *handled = ModalGump::onMouseMotion(mx, my);
	if (!_editor) {
		ParentToGump(mx, my);
		const int hit = hitTest(mx, my);
		if (hit >= 0 && hit < SLOTS_PER_PAGE && hit != _selected)
			select(hit);
	}
	return handled;
}

void CruSaveGump::onMouseClick(int button, int32 mx, int32 my) {
	if (button != Mouse::BUTTON_LEFT)
		return;
	ParentToGump(mx, my);
	const int hit = hitTest(mx, my);
	if (hit == HIT_PREV)
		changePage(-1);
	else if (hit == HIT_NEXT)
		changePage(1);
	else if (hit == HIT_BACK)
		Close();
	else if (hit >= 0 && !_editor)
		activate(hit); // may delete this
}

void CruSaveGump::onMouseDouble(int button, int32 mx, int32 my) {
	onMouseClick(button, mx, my);
}

void CruSaveGump::select(int row) {
	_selected = row;
}

void CruSaveGump::changePage(int delta) {
	const int page = _page + delta;
	if (page < 0 || page >= NUM_PAGES)
		return;
	const int row = _selected;
	loadPage(page);
	select(row < 0 ? 0 : row);
}

void CruSaveGump::activate(int row) {
	select(row);
	const int slot = slotFor(row);
	Ultima8Engine *engine = Ultima8Engine::get_instance();

	if (_save) {
		if (slot == 0)
			return; // reserved for autosaves
		if (engine->isControllerActive()) {
			// No keyboard to type a name: keep the old one, or number it.
			saveNow(_slots[row].used ? _slots[row].description : Common::String::format("SAVE %02d", slot));
			return;
		}
		startEdit(row);
		return;
	}

	if (!_slots[row].used)
		return;
	Common::Error err = engine->loadGameState(slot); // closes all gumps
	if (err.getCode() != Common::kNoError) {
		GUI::MessageDialog errorDialog(err.getDesc());
		errorDialog.runModal();
	}
}

void CruSaveGump::startEdit(int row) {
	stopEdit();
	const int y = LIST_Y + row * ROW_H;
	_editor = new EditWidget(LIST_X + 36, y, Common::String(), true, FONT_HILITE, LIST_W - 44, ROW_H - 6, MAX_DESC);
	_editor->InitGump(this, true);
	_editor->MakeFocus();
}

void CruSaveGump::stopEdit() {
	if (!_editor)
		return;
	if (_focusChild == _editor)
		_focusChild = nullptr;
	_editor->Close();
	_editor = nullptr;
}

void CruSaveGump::saveNow(const Common::String &name) {
	const int slot = slotFor(_selected);
	Ultima8Engine *engine = Ultima8Engine::get_instance();
	// Close first, and hide the menu, so the save thumbnail shows the game.
	Close(); // 'this' is gone after the next frame, don't use it below
	Gump *menu = engine->getDesktopGump()->FindGump<CruMenuGump>();
	if (menu)
		menu->HideGump();
	Common::Error err = engine->saveGameState(slot, name);
	if (menu)
		menu->UnhideGump();
	if (err.getCode() != Common::kNoError) {
		GUI::MessageDialog errorDialog(err.getDesc());
		errorDialog.runModal();
	}
}

void CruSaveGump::ChildNotify(Gump *child, uint32 message) {
	if (child != _editor)
		return;

	if (message == EditWidget::EDIT_ENTER) {
		Common::String name = _editor->getText();
		name.trim();
		if (name.empty())
			return;
		saveNow(name); // closes us
	} else if (message == EditWidget::EDIT_ESCAPE) {
		stopEdit();
	}
}

bool CruSaveGump::OnKeyDown(int key, int mod) {
	if (_editor)
		return ModalGump::OnKeyDown(key, mod);

	switch (key) {
	case Common::KEYCODE_ESCAPE:
	case Common::KEYCODE_BACKSPACE:
		Close();
		break;
	case Common::KEYCODE_UP:
	case Common::KEYCODE_KP8:
	case Common::KEYCODE_w:
		if (_selected > 0)
			select(_selected - 1);
		else if (_page > 0) {
			loadPage(_page - 1);
			select(SLOTS_PER_PAGE - 1);
		}
		break;
	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_KP2:
	case Common::KEYCODE_s:
		if (_selected < SLOTS_PER_PAGE - 1)
			select(_selected + 1);
		else if (_page < NUM_PAGES - 1) {
			loadPage(_page + 1);
			select(0);
		}
		break;
	case Common::KEYCODE_LEFT:
	case Common::KEYCODE_PAGEUP:
	case Common::KEYCODE_a:
		changePage(-1);
		break;
	case Common::KEYCODE_RIGHT:
	case Common::KEYCODE_PAGEDOWN:
	case Common::KEYCODE_d:
		changePage(1);
		break;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
	case Common::KEYCODE_SPACE:
		if (_selected >= 0)
			activate(_selected); // may delete this
		break;
	default:
		break;
	}
	return true;
}

bool CruSaveGump::OnTextInput(int unicode) {
	if (_editor)
		return ModalGump::OnTextInput(unicode);
	return true;
}

//static
void CruSaveGump::show(bool saveMode) {
	Ultima8Engine *engine = Ultima8Engine::get_instance();
	if (saveMode && !engine->canSaveGameStateCurrently()) {
		Mouse::get_instance()->flashCrossCursor();
		return;
	}
	CruSaveGump *gump = new CruSaveGump(saveMode);
	gump->InitGump(nullptr);
	gump->setRelativePosition(CENTER);
}

bool CruSaveGump::loadData(Common::ReadStream *rs, uint32 version) {
	warning("Trying to load ModalGump");
	return false;
}

void CruSaveGump::saveData(Common::WriteStream *ws) {
	warning("Trying to save ModalGump");
}

} // End of namespace Ultima8
} // End of namespace Ultima
