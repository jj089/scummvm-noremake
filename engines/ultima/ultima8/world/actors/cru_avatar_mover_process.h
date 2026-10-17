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

#ifndef WORLD_ACTORS_CRUAVATARMOVERPROCESS_H
#define WORLD_ACTORS_CRUAVATARMOVERPROCESS_H

#include "ultima/ultima8/world/actors/avatar_mover_process.h"
#include "ultima/ultima8/world/actors/animation.h"

namespace Ultima {
namespace Ultima8 {

class Actor;

/**
 * Mover process that replicates the feel of Crusader - moving, combat, jumps, etc.
 * Tries sliding left and right if movement is blocked.  Walking cancels combat.
 * TODO: Support combat rolls and side-steps.
 */
class CruAvatarMoverProcess : public AvatarMoverProcess {
public:
	CruAvatarMoverProcess();
	~CruAvatarMoverProcess() override;

	ENABLE_RUNTIME_CLASSTYPE()

	void run() override;

	bool loadData(Common::ReadStream *rs, uint32 version);
	void saveData(Common::WriteStream *ws) override;

	double getAvatarAngleDegrees() const {
		return static_cast<double>(_avatarAngle) / 100.0;
	}

	void clearMovementFlag(uint32 mask) override;

	//! Analog stick input, used to aim with the right stick in fluid movement.
	void onJoystickAxis(int axis, int position);

	//! Is the left mouse button held on the game map (for diagnostics)
	bool isMapButtonDown() const {
		return _mouseButton[0].isState(MBS_DOWN);
	}

	//! Is the player aiming with the mouse (fluid movement)
	bool isMouseAimActive() const {
		return _mouseAimActive;
	}

private:
	/** Try readying or firing weapon. */
	void tryAttack();

	/** Check if we need to alert NPCs after firing. */
	void checkForAlertingNPCs();

	/**
	* Angle of avatar in centidegrees (1/100deg).  The original game runs the keyboard
	* process 45 times per second and rotates the crosshair by 2 (regular) or
	* 5 ('run') degrees each time. This process runs 60 times per second, so we choose a
	* multiplier that can use integers - rotating 3.75 or 1.5 degrees each time.
	*/
	int32 _avatarAngle;

	/**
	 * Whether we've reloaded the SGA1 yet (it needs to happen every shot)
	 */
	bool _SGA1Loaded;

	/**
	 * Next tick the avatar can fire a weapon again.
	 */
	uint32 _nextFireTick;

	/**
	 * Last time we alerted NPCs on a shot.
	 */
	uint32 _lastNPCAlertTick;

	void handleHangingMode() override;
	void handleCombatMode() override;
	void handleNormalMode() override;

	void step(Animation::Sequence action, Direction direction, bool adjusted = false);

	bool checkOneShotMove(Direction direction);

	/**
	 * Fluid movement: move the avatar continuously in the direction of the
	 * movement keys/stick, playing locomotion frames by distance travelled,
	 * instead of committing to whole animation steps.
	 * Returns true if it handled the avatar this tick.
	 */
	bool handleFluidMovement();
	void stopFluidMovement(Actor *avatar);
	Animation::Sequence fluidMoveAnim(const Actor *avatar) const;
	//! Move by (dx, dy), sliding along walls if needed. Reports the move made.
	bool fluidTryMove(Actor *avatar, int32 dx, int32 dy, Direction dir, int32 &movedx, int32 &movedy);
	//! World direction from the avatar to the mouse cursor. False if unavailable.
	bool getMouseAimDir(const Actor *avatar, Direction &dir, int32 *angle = nullptr) const;
	//! Run-and-gun: fire the active weapon without stopping for the attack animation.
	void fireWhileMoving(Actor *avatar, Direction dir);

	bool _fluidMoving;
	//! Mouse aiming is active once the mouse moves; keyboard turning disables it.
	bool _mouseAimActive;
	int32 _lastMouseX, _lastMouseY;
	//! Right stick position (-32768..32767)
	int32 _aimStickX, _aimStickY;
	Animation::Sequence _fluidAnim;
	unsigned int _fluidFrame;
	//! Distance travelled since the last locomotion frame change (world units * 256)
	int32 _fluidDist;
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
