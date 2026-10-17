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

#include "ultima/ultima8/misc/autotest.h"
#include "ultima/ultima8/ultima8.h"
#include "ultima/ultima8/metaengine.h"
#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/kernel/kernel.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/gumps/game_map_gump.h"
#include "ultima/ultima8/world/camera_process.h"
#include "ultima/ultima8/gumps/movie_gump.h"
#include "ultima/ultima8/world/actors/main_actor.h"
#include "ultima/ultima8/world/get_object.h"
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "image/png.h"

namespace Ultima {
namespace Ultima8 {

static const struct {
	const char *name;
	KeybindingAction action;
} AUTOTEST_ACTIONS[] = {
	{ "MOVE_UP", ACTION_MOVE_UP }, { "MOVE_DOWN", ACTION_MOVE_DOWN },
	{ "MOVE_LEFT", ACTION_MOVE_LEFT }, { "MOVE_RIGHT", ACTION_MOVE_RIGHT },
	{ "MOVE_RUN", ACTION_MOVE_RUN }, { "MOVE_STEP", ACTION_MOVE_STEP },
	{ "MOVE_FORWARD", ACTION_MOVE_FORWARD }, { "MOVE_BACK", ACTION_MOVE_BACK },
	{ "TURN_LEFT", ACTION_TURN_LEFT }, { "TURN_RIGHT", ACTION_TURN_RIGHT },
	{ "ATTACK", ACTION_ATTACK }, { "COMBAT", ACTION_COMBAT },
	{ "JUMP", ACTION_JUMP }, { "MENU", ACTION_MENU },
	{ "ROLL_LEFT", ACTION_ROLL_LEFT }, { "ROLL_RIGHT", ACTION_ROLL_RIGHT },
	{ "TOGGLE_CROUCH", ACTION_TOGGLE_CROUCH }, { "SELECT_ITEMS", ACTION_SELECT_ITEMS },
	{ "USE_SELECTION", ACTION_USE_SELECTION }, { "NEXT_WEAPON", ACTION_NEXT_WEAPON },
};

static const struct {
	const char *name;
	Common::KeyCode code;
	uint16 ascii;
} AUTOTEST_KEYS[] = {
	{ "ESCAPE", Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE },
	{ "RETURN", Common::KEYCODE_RETURN, Common::ASCII_RETURN },
	{ "SPACE", Common::KEYCODE_SPACE, ' ' },
	{ "Y", Common::KEYCODE_y, 'y' },
	{ "N", Common::KEYCODE_n, 'n' },
};

AutoTest::AutoTest() : _pc(0), _waitUntil(0), _logEvery(0), _nextLog(0) {
}

bool AutoTest::loadScript(const Common::String &path) {
	Common::FSNode node(Common::Path(path, Common::Path::kNativeSeparator));
	Common::SeekableReadStream *rs = node.createReadStream();
	if (!rs) {
		warning("autotest: cannot open %s", path.c_str());
		return false;
	}
	while (!rs->eos()) {
		Common::String line = rs->readLine();
		line.trim();
		if (!line.empty() && line[0] != '#')
			_lines.push_back(line);
	}
	delete rs;
	debug("autotest: loaded %u commands from %s", _lines.size(), path.c_str());
	return true;
}

void AutoTest::update() {
	const uint32 now = g_system->getMillis();
	if (_logEvery && now >= _nextLog) {
		logAvatar("tick");
		_nextLog = now + _logEvery;
	}
	while (_pc < _lines.size() && now >= _waitUntil) {
		execute(_lines[_pc++]);
	}
}

void AutoTest::logAvatar(const Common::String &label) {
	const MainActor *av = getMainActor();
	if (!av) {
		debug("autotest[%s]: no avatar", label.c_str());
		return;
	}
	const Point3 pt = av->getLocation();
	// Avatar screen position (same formula as the mover's mouse aiming).
	int32 sx = 0, sy = 0;
	GameMapGump *gmg = Ultima8Engine::get_instance()->getGameMapGump();
	if (gmg) {
		const Point3 c = CameraProcess::GetCameraLocation();
		sx = (pt.x - pt.y) / 4 - (c.x - c.y) / 4;
		sy = (pt.x + pt.y) / 8 - pt.z - ((c.x + c.y) / 8 - c.z);
		gmg->GumpToScreenSpace(sx, sy);
	}
	int32 mx, my;
	Mouse::get_instance()->getMouseCoords(mx, my);
	const Item *wpn = getItem(av->getActiveWeapon());
	debug("autotest[%s]: t=%u frameNum=%u map=%d pos=%d,%d,%d dir=%d shapeFrame=%u lastAnim=%d busy=%d combat=%d flipped=%d screen=%d,%d mouse=%d,%d ammo=%d",
		label.c_str(), g_system->getMillis(), Kernel::get_instance()->getFrameNum(),
		av->getMapNum(), pt.x, pt.y, pt.z, (int)av->getDir(), av->getFrame(),
		(int)av->getLastAnim(), av->isBusy() ? 1 : 0, av->isInCombat() ? 1 : 0,
		av->hasFlags(Item::FLG_FLIPPED) ? 1 : 0, sx, sy, mx, my,
		wpn ? (int)wpn->getQuality() : -1);
}

void AutoTest::execute(const Common::String &line) {
	Common::StringTokenizer tok(line, " ");
	const Common::String cmd = tok.nextToken();
	Ultima8Engine *engine = Ultima8Engine::get_instance();
	debug("autotest> %s", line.c_str());

	if (cmd == "wait") {
		_waitUntil = g_system->getMillis() + atoi(tok.nextToken().c_str());
	} else if (cmd == "action") {
		const Common::String name = tok.nextToken();
		const bool start = (tok.nextToken() != "end");
		for (const auto &a : AUTOTEST_ACTIONS) {
			if (name == a.name) {
				Common::Event ev;
				ev.type = start ? Common::EVENT_CUSTOM_ENGINE_ACTION_START : Common::EVENT_CUSTOM_ENGINE_ACTION_END;
				ev.customType = a.action;
				g_system->getEventManager()->pushEvent(ev);
				return;
			}
		}
		warning("autotest: unknown action %s", name.c_str());
	} else if (cmd == "key") {
		const Common::String name = tok.nextToken();
		for (const auto &k : AUTOTEST_KEYS) {
			if (name == k.name) {
				Common::Event ev;
				ev.type = Common::EVENT_KEYDOWN;
				ev.kbd = Common::KeyState(k.code, k.ascii);
				g_system->getEventManager()->pushEvent(ev);
				ev.type = Common::EVENT_KEYUP;
				g_system->getEventManager()->pushEvent(ev);
				return;
			}
		}
		warning("autotest: unknown key %s", name.c_str());
	} else if (cmd == "rstick") {
		// Simulate the right analog stick (-32768..32767 per axis).
		Common::Event ev;
		ev.type = Common::EVENT_JOYAXIS_MOTION;
		ev.joystick.axis = Common::JOYSTICK_AXIS_RIGHT_STICK_X;
		ev.joystick.position = atoi(tok.nextToken().c_str());
		g_system->getEventManager()->pushEvent(ev);
		ev.joystick.axis = Common::JOYSTICK_AXIS_RIGHT_STICK_Y;
		ev.joystick.position = atoi(tok.nextToken().c_str());
		g_system->getEventManager()->pushEvent(ev);
	} else if (cmd == "mousedown" || cmd == "mouseup") {
		Common::Event ev;
		int32 mx, my;
		Mouse::get_instance()->getMouseCoords(mx, my);
		const int scale = engine->getRenderScale();
		ev.mouse.x = mx * scale;
		ev.mouse.y = my * scale;
		ev.type = (cmd == "mousedown") ? Common::EVENT_LBUTTONDOWN : Common::EVENT_LBUTTONUP;
		g_system->getEventManager()->pushEvent(ev);
	} else if (cmd == "click" || cmd == "mousemove") {
		// Coordinates are in game screen pixels (640x480 for Crusader).
		Common::Event ev;
		const int scale = engine->getRenderScale();
		ev.mouse.x = atoi(tok.nextToken().c_str()) * scale;
		ev.mouse.y = atoi(tok.nextToken().c_str()) * scale;
		ev.type = Common::EVENT_MOUSEMOVE;
		g_system->warpMouse(ev.mouse.x, ev.mouse.y);
		g_system->getEventManager()->pushEvent(ev);
		if (cmd == "click") {
			ev.type = Common::EVENT_LBUTTONDOWN;
			g_system->getEventManager()->pushEvent(ev);
			ev.type = Common::EVENT_LBUTTONUP;
			g_system->getEventManager()->pushEvent(ev);
		}
	} else if (cmd == "log") {
		logAvatar(tok.empty() ? "log" : tok.nextToken());
	} else if (cmd == "logevery") {
		_logEvery = atoi(tok.nextToken().c_str());
		_nextLog = 0;
	} else if (cmd == "shot") {
		const Common::String path = tok.nextToken();
		RenderSurface *screen = engine->getRenderScreen();
		Common::FSNode node(Common::Path(path, Common::Path::kNativeSeparator));
		Common::SeekableWriteStream *ws = node.createWriteStream(false);
		if (screen && ws) {
			Image::writePNG(*ws, *screen->getRawSurface()->surfacePtr());
			debug("autotest: wrote %s", path.c_str());
		}
		delete ws;
	} else if (cmd == "movie") {
		MovieGump::CruMovieViewer(tok.nextToken(), 640, 480, nullptr, nullptr, 0);
	} else if (cmd == "god") {
		MainActor *av = getMainActor();
		if (av)
			av->setActorFlag(Actor::ACT_INVINCIBLE);
	} else if (cmd == "newgame") {
		engine->newGame(-1);
	} else if (cmd == "load") {
		engine->loadGameState(atoi(tok.nextToken().c_str()));
	} else if (cmd == "save") {
		engine->saveGameState(atoi(tok.nextToken().c_str()), "autotest", false);
	} else if (cmd == "quit") {
		engine->quitGame();
	} else {
		warning("autotest: unknown command %s", cmd.c_str());
	}
}

} // End of namespace Ultima8
} // End of namespace Ultima
