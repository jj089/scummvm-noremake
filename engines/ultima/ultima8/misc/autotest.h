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

#ifndef ULTIMA8_MISC_AUTOTEST_H
#define ULTIMA8_MISC_AUTOTEST_H

#include "common/array.h"
#include "common/str.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Scripted test driver, enabled with the "cru_autotest" config key pointing
 * at a text script.  One command per line:
 *   wait <ms>                 - pause script
 *   action <NAME> start|end   - send an engine keymap action (e.g. MOVE_UP)
 *   key <NAME>                - tap a raw key (ESCAPE, RETURN, SPACE, Y, N)
 *   click <x> <y>             - left click at game screen coordinates
 *   mousemove <x> <y>         - move the mouse to game screen coordinates
 *   mousedown | mouseup       - press or release the left button where the mouse is
 *   rstick <x> <y>            - set the right analog stick position
 *   log [label]               - write avatar state to the debug log
 *   logevery <ms>             - also log avatar state periodically (0 = off)
 *   shot <path>               - save the current screen as PNG
 *   god                       - make the avatar invincible
 *   newgame | load <slot> | save <slot> | quit
 */
class AutoTest {
public:
	AutoTest();
	bool loadScript(const Common::String &path);
	//! Called once per main-loop iteration, after painting.
	void update();
	bool isActive() const { return !_lines.empty(); }

private:
	void execute(const Common::String &line);
	void logAvatar(const Common::String &label);

	Common::Array<Common::String> _lines;
	uint _pc;
	uint32 _waitUntil;
	uint32 _logEvery;
	uint32 _nextLog;
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
