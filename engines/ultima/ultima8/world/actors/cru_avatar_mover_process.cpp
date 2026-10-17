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

#include "ultima/ultima8/world/actors/cru_avatar_mover_process.h"

#include "ultima/ultima8/world/actors/main_actor.h"
#include "ultima/ultima8/kernel/kernel.h"
#include "ultima/ultima8/world/actors/actor_anim_process.h"
#include "ultima/ultima8/world/get_object.h"
#include "ultima/ultima8/world/current_map.h"
#include "ultima/ultima8/world/world.h"
#include "ultima/ultima8/misc/direction_util.h"
#include "ultima/ultima8/audio/audio_process.h"
#include "ultima/ultima8/games/game_data.h"
#include "ultima/ultima8/gfx/anim_dat.h"
#include "ultima/ultima8/gfx/main_shape_archive.h"
#include "ultima/ultima8/world/actors/anim_action.h"
#include "ultima/ultima8/world/camera_process.h"
#include "ultima/ultima8/world/actors/auto_firer_process.h"
#include "ultima/ultima8/gumps/game_map_gump.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/ultima8.h"
#include "common/config-manager.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(CruAvatarMoverProcess)

static const int REBEL_BASE_MAP = 40;

CruAvatarMoverProcess::CruAvatarMoverProcess() : AvatarMoverProcess(),
_avatarAngle(-1), _SGA1Loaded(false), _nextFireTick(0), _lastNPCAlertTick(0),
_fluidMoving(false), _fluidAnim(Animation::stand), _fluidFrame(0), _fluidDist(0),
_mouseAimActive(false), _lastMouseX(-1), _lastMouseY(-1), _aimStickX(0), _aimStickY(0) {
}


CruAvatarMoverProcess::~CruAvatarMoverProcess() {
}

static bool _isAnimRunningWalking(Animation::Sequence anim) {
	return (anim == Animation::run || anim == Animation::combatRunSmallWeapon ||
			anim == Animation::walk);
}

void CruAvatarMoverProcess::run() {

	// Even when we are not doing anything (because we're waiting for an anim)
	// we check if the combat angle needs updating - this keeps it smooth.

	const Actor *avatar = getControlledActor();

	// Controlled actor may have gone
	if (!avatar)
		return;

	if (handleFluidMovement())
		return;

	// When in combat and not running, update the angle.
	// Otherwise, angle is kept as -1 and direction is just actor dir.
	if (avatar->isInCombat() && (avatar->getLastAnim() != Animation::run)) {
		if (_avatarAngle < 0) {
			_avatarAngle = Direction_ToCentidegrees(avatar->getDir());
		}
		if (!hasMovementFlags(MOVE_FORWARD | MOVE_JUMP | MOVE_STEP)) {
			// See comment on _avatarAngle in header about these constants
			if (hasMovementFlags(MOVE_TURN_LEFT)) {
				if (hasMovementFlags(MOVE_RUN))
					_avatarAngle -= 375;
				else
					_avatarAngle -= 150;

				if (_avatarAngle < 0)
					_avatarAngle += 36000;
			}
			if (hasMovementFlags(MOVE_TURN_RIGHT)) {
				if (hasMovementFlags(MOVE_RUN))
					_avatarAngle += 375;
				else
					_avatarAngle += 150;

				_avatarAngle = _avatarAngle % 36000;
			}
		}
	} else {
		_avatarAngle = -1;
		// Check for a turn request while running or walking.  This only happens
		// once per arrow keydown, so clear the flag.
		if (_isAnimRunningWalking(avatar->getLastAnim())
			&& hasMovementFlags(MOVE_FORWARD)
			&& (hasMovementFlags(MOVE_TURN_LEFT) || hasMovementFlags(MOVE_TURN_RIGHT) ||
				hasMovementFlags(MOVE_PENDING_TURN_LEFT) || hasMovementFlags(MOVE_PENDING_TURN_RIGHT))) {
			Kernel *kernel = Kernel::get_instance();
			// Stop the current animation and turn now.
			kernel->killProcesses(avatar->getObjId(), ActorAnimProcess::ACTOR_ANIM_PROC_TYPE, true);

			Direction curdir = avatar->getDir();
			Animation::Sequence anim = hasMovementFlags(MOVE_RUN) ? Animation::run : Animation::walk;
			DirectionMode dirmode = avatar->animDirMode(anim);
			Direction dir = getTurnDirForTurnFlags(curdir, dirmode);
			clearMovementFlag(MOVE_TURN_LEFT | MOVE_TURN_RIGHT |
							  MOVE_PENDING_TURN_LEFT | MOVE_PENDING_TURN_RIGHT);
			step(anim, dir);
			return;
		}
	}

	// Pending turns shouldn't stick around.
	clearMovementFlag(MOVE_PENDING_TURN_LEFT | MOVE_PENDING_TURN_RIGHT);

	// Now do the regular process
	AvatarMoverProcess::run();
}

void CruAvatarMoverProcess::clearMovementFlag(uint32 mask) {
	// Set a pending turn if we haven't already cleared the turn
	if ((mask & MOVE_TURN_LEFT) && hasMovementFlags(MOVE_TURN_LEFT))
		setMovementFlag(MOVE_PENDING_TURN_LEFT);
	else if ((mask & MOVE_TURN_RIGHT) && hasMovementFlags(MOVE_TURN_RIGHT))
		setMovementFlag(MOVE_PENDING_TURN_RIGHT);

	AvatarMoverProcess::clearMovementFlag(mask);
}

void CruAvatarMoverProcess::handleHangingMode() {
	// No hanging in crusader, this shouldn't happen?
	assert(false);
}

static bool _isAnimRunningJumping(Animation::Sequence anim) {
	return (anim == Animation::run || anim == Animation::combatRunSmallWeapon ||
			anim == Animation::combatRunLargeWeapon || anim == Animation::jumpForward);
}

static bool _isAnimStartRunning(Animation::Sequence anim) {
	return (anim == Animation::startRun || anim == Animation::startRunSmallWeapon /*||
			// don't test this as it overlaps with kneel :(
			anim == Animation::startRunLargeWeapon*/);
}

bool CruAvatarMoverProcess::checkOneShotMove(Direction direction) {
	Actor *avatar = getControlledActor();
	MainActor *mainactor = dynamic_cast<MainActor *>(avatar);

	static const MovementFlags oneShotFlags[] = {
		MOVE_ROLL_LEFT, MOVE_ROLL_RIGHT,
		MOVE_STEP_LEFT, MOVE_STEP_RIGHT,
		MOVE_STEP_FORWARD, MOVE_STEP_BACK,
		MOVE_SHORT_JUMP, MOVE_TOGGLE_CROUCH
	};

	static const Animation::Sequence oneShotAnims[] = {
		Animation::combatRollLeft, Animation::combatRollRight,
		Animation::slideLeft, Animation::slideRight,
		Animation::advance, Animation::retreat,
		Animation::jumpForward, Animation::kneelStartCru
	};

	static const Animation::Sequence oneShotKneelingAnims[] = {
		Animation::kneelCombatRollLeft, Animation::kneelCombatRollRight,
		Animation::slideLeft, Animation::slideRight,
		Animation::kneelingAdvance, Animation::kneelingRetreat,
		Animation::jumpForward, Animation::kneelEndCru
	};

	for (int i = 0; i < ARRAYSIZE(oneShotFlags); i++) {
		if (hasMovementFlags(oneShotFlags[i])) {
			Animation::Sequence anim = (avatar->isKneeling() ?
							oneShotKneelingAnims[i] : oneShotAnims[i]);

			// All the animations should finish with gun drawn, *except*
			// jump which should finish with gun stowed.  For other cases we should
			// toggle.
			bool incombat = avatar->isInCombat();
			bool isjump = (anim == Animation::jumpForward);
			if (mainactor && (incombat == isjump)) {
				mainactor->toggleInCombat();
			}

			clearMovementFlag(oneShotFlags[i]);

			if (anim == Animation::advance || anim == Animation::retreat ||
				anim == Animation::kneelingAdvance || anim == Animation::kneelingRetreat) {
				step(anim, direction);
			} else {
				avatar->doAnim(anim, direction);
			}
			return true;
		}
	}

	return false;
}

void CruAvatarMoverProcess::handleCombatMode() {
	Actor *avatar = getControlledActor();
	MainActor *mainactor = dynamic_cast<MainActor *>(avatar);
	const Animation::Sequence lastanim = avatar->getLastAnim();
	Direction direction = (_avatarAngle >= 0 ? Direction_FromCentidegrees(_avatarAngle) : avatar->getDir());
	const Direction curdir = avatar->getDir();
	const bool stasis = Ultima8Engine::get_instance()->isAvatarInStasis();

	if (avatar->getMapNum() == REBEL_BASE_MAP) {
		avatar->clearInCombat();
		return;
	}

	// never idle when in combat
	_idleTime = 0;

	if (stasis)
		return;

	if (checkOneShotMove(direction))
		return;

	if (hasMovementFlags(MOVE_FORWARD)) {
		Animation::Sequence nextanim;
		if (hasMovementFlags(MOVE_STEP)) {
			nextanim = avatar->isKneeling() ?
							Animation::kneelingAdvance : Animation::advance;
		} else if (hasMovementFlags(MOVE_RUN) && avatar->hasAnim(Animation::combatRunSmallWeapon)) {
			// Take a step before running
			if (lastanim == Animation::walk || _isAnimRunningJumping(lastanim) || _isAnimStartRunning(lastanim))
				nextanim = Animation::combatRunSmallWeapon;
			else
				nextanim = Animation::startRunSmallWeapon;
		} else if (hasMovementFlags(MOVE_JUMP) && avatar->hasAnim(Animation::jumpForward)) {
			if (lastanim == Animation::walk || lastanim == Animation::run || lastanim == Animation::combatRunSmallWeapon)
				nextanim = Animation::jumpForward;
			else
				nextanim = Animation::jump;
			// Jump always ends out of combat
			avatar->clearInCombat();
		} else if (avatar->isKneeling()) {
			avatar->doAnim(Animation::kneelEndCru, direction);
			avatar->clearActorFlag(Actor::ACT_KNEELING);
			return;
		} else {
			// moving forward from combat stows weapon
			nextanim = Animation::walk;
			if (mainactor)
				mainactor->toggleInCombat();
		}

		// Ensure the dir we are about to use is valid
		if (avatar->animDirMode(nextanim) == dirmode_8dirs)
			direction = static_cast<Direction>(direction - (static_cast<uint32>(direction) % 2));

		// don't check weapon here, Avatar can go straight from drawn-weapon to
		// walking forward.
		step(nextanim, direction);
		return;
	} else if (hasMovementFlags(MOVE_BACK)) {
		Animation::Sequence nextanim;
		if (hasMovementFlags(MOVE_JUMP)) {
			if (!avatar->isKneeling() && avatar->hasAnim(Animation::kneelStartCru)) {
				nextanim = Animation::kneelStartCru;
				avatar->setActorFlag(Actor::ACT_KNEELING);
			} else {
				// Do nothing if already kneeling
				return;
			}
		} else {
			nextanim = Animation::retreat;
		}
		step(nextanim, direction);
		return;
	} else if (hasMovementFlags(MOVE_STEP)) {
		if (avatar->isKneeling()) {
			avatar->doAnim(Animation::kneelEndCru, direction);
			return;
		} else {
			if (hasMovementFlags(MOVE_TURN_LEFT)) {
				avatar->doAnim(Animation::slideLeft, direction);
				return;
			} else if (hasMovementFlags(MOVE_TURN_RIGHT)) {
				avatar->doAnim(Animation::slideRight, direction);
				return;
			}
		}
	} else if (hasMovementFlags(MOVE_JUMP)) {
		if (hasMovementFlags(MOVE_TURN_LEFT)) {
			if (avatar->isKneeling())
				avatar->doAnim(Animation::kneelCombatRollLeft, direction);
			else
				avatar->doAnim(Animation::combatRollLeft, direction);
			return;
		} else if (hasMovementFlags(MOVE_TURN_RIGHT)) {
			if (avatar->isKneeling())
				avatar->doAnim(Animation::kneelCombatRollRight, direction);
			else
				avatar->doAnim(Animation::combatRollRight, direction);
			return;
		}
	}

	int x, y;
	getMovementFlagAxes(x, y);
	if (x != 0 || y != 0) {
		Direction nextdir = (_avatarAngle >= 0 ? Direction_FromCentidegrees(_avatarAngle) : avatar->getDir());

		if (checkTurn(nextdir, true))
			return;

		Animation::Sequence nextanim = Animation::combatStand;
		if ((lastanim == Animation::run || lastanim == Animation::combatRunSmallWeapon) && !hasMovementFlags(MOVE_RUN)) {
			// want to go back to combat mode  from run
			nextanim = Animation::stopRunningAndDrawSmallWeapon;
		} else if (hasMovementFlags(MOVE_BACK)) {
			nextanim = Animation::retreat;
			nextdir = Direction_Invert(direction);
		}

		if (hasMovementFlags(MOVE_RUN)) {
			nextanim = Animation::combatRunSmallWeapon;
		}

		Animation::Sequence wpnanim = Animation::checkWeapon(nextanim, lastanim);
		step(wpnanim, nextdir);
		return;
	}

	Animation::Sequence idleanim = avatar->isKneeling() ?
						Animation::kneel : Animation::combatStand;

	if (curdir != direction) {
		// Slight hack: don't "wait" for this - we want to keep turning smooth,
		// and the process will not do anything else if an anim is active, so
		// it's safe.
		avatar->doAnim(idleanim, direction);
		return;
	}

	if (hasMovementFlags(MOVE_ATTACKING) && !hasMovementFlags(MOVE_FORWARD | MOVE_BACK)) {
		tryAttack();
		return;
	}

	if (_isAnimRunningJumping(lastanim) || _isAnimStartRunning(idleanim)) {
		idleanim = Animation::stopRunningAndDrawSmallWeapon;
	}

	// Not doing anything in particular? stand.
	if (lastanim != idleanim) {
		Animation::Sequence nextanim = Animation::checkWeapon(idleanim, lastanim);
		waitFor(avatar->doAnim(nextanim, direction));
	}
}

void CruAvatarMoverProcess::handleNormalMode() {
	Actor *avatar = getControlledActor();
	MainActor *mainactor = dynamic_cast<MainActor *>(avatar);
	const Animation::Sequence lastanim = avatar->getLastAnim();
	Direction direction = avatar->getDir();
	const bool stasis = Ultima8Engine::get_instance()->isAvatarInStasis();
	const bool rebelBase = (avatar->getMapNum() == REBEL_BASE_MAP);

	if (!rebelBase && hasMovementFlags(MOVE_STEP | MOVE_JUMP) && hasMovementFlags(MOVE_ANY_DIRECTION | MOVE_TURN_LEFT | MOVE_TURN_RIGHT)) {
		// All jump and step movements in crusader are handled identically
		// whether starting from combat mode or not.
		avatar->setInCombat(0);
		handleCombatMode();
		return;
	}

	// Store current idle time. (Also see end of function.)
	uint32 currentIdleTime = _idleTime;
	_idleTime = 0;

	// User toggled combat while in combatRun
	if (avatar->isInCombat()) {
		if (mainactor)
			mainactor->toggleInCombat();
	}

	if (!hasMovementFlags(MOVE_ANY_DIRECTION) && lastanim == Animation::run) {
		// if we were running, slow to a walk before stopping
		// (even in stasis)
		Animation::Sequence nextanim;
		if (rebelBase) {
			nextanim = Animation::stand;
		} else {
			nextanim = Animation::stopRunningAndDrawSmallWeapon;
			// Robots don't slow down from  running
			if (!avatar->hasAnim(nextanim))
				nextanim = Animation::stand;
		}
		waitFor(avatar->doAnim(nextanim, direction));
		avatar->setInCombat(0);
		return;
	}

	// can't do any new actions if in stasis
	if (stasis)
		return;

	if (checkOneShotMove(direction))
		return;

	bool moving = (lastanim == Animation::step || lastanim == Animation::run || lastanim == Animation::walk);

	DirectionMode dirmode = avatar->animDirMode(Animation::step);

	//  if we are trying to move, allow change direction only after move occurs to avoid spinning
	if (moving || !hasMovementFlags(MOVE_FORWARD | MOVE_BACK)) {
		direction = getTurnDirForTurnFlags(direction, dirmode);
	}

	Animation::Sequence nextanim = Animation::walk;

	if (hasMovementFlags(MOVE_RUN)) {
		if (lastanim == Animation::run
			|| lastanim == Animation::startRun
			|| lastanim == Animation::startRunSmallWeapon
			|| lastanim == Animation::combatRunSmallWeapon
			|| lastanim == Animation::walk) {
			// keep running
			nextanim = Animation::run;
		} else {
			// start running
			nextanim = Animation::startRun;
		}
	}

	if (hasMovementFlags(MOVE_FORWARD)) {
		step(nextanim, direction);
		return;
	}

	if (!rebelBase && hasMovementFlags(MOVE_BACK)) {
		if (mainactor)
			mainactor->toggleInCombat();
		step(Animation::retreat, direction);
		return;
	}

	int x, y;
	getMovementFlagAxes(x, y);

	if (x != 0 || y != 0) {
		direction = Direction_Get(y, x, dirmode_8dirs);
		step(nextanim, direction);
		return;
	}

	if (checkTurn(direction, moving))
		return;

	// doing another animation?
	if (avatar->isBusy())
		return;

	if (hasMovementFlags(MOVE_ATTACKING) && !hasMovementFlags(MOVE_FORWARD | MOVE_BACK)) {
		tryAttack();
		return;
	}

	// not doing anything in particular? stand
	if (lastanim != Animation::stand && currentIdleTime == 0) {
		waitFor(avatar->doAnim(Animation::stand, direction));
		return;
	}

	// idle
	_idleTime = currentIdleTime + 1;
}

void CruAvatarMoverProcess::step(Animation::Sequence action, Direction direction,
							  bool adjusted) {
	Actor *avatar = getControlledActor();

	// For "start run" animations, don't call it a success unless we can actually run
	Animation::Sequence testaction = _isAnimStartRunning(action) ? Animation::run : action;

	Animation::Result res = avatar->tryAnim(testaction, direction);
	Animation::Result initialres = res;

	if (res != Animation::SUCCESS) {
		World *world = World::get_instance();
		const CurrentMap *currentmap = world->getCurrentMap();

		// Search right/left gradually increasing distance to see if we can make the move work.

		Direction dir_right = Direction_TurnByDelta(direction, 4, dirmode_16dirs);
		Direction dir_left = Direction_TurnByDelta(direction, -4, dirmode_16dirs);
		Point3 origpt = avatar->getLocation();

		int32 dims[3];
		avatar->getFootpadWorld(dims[0], dims[1], dims[2]);

		// Double the values in original to match our coordinate space
		static const int ADJUSTMENTS[] = {0x20, 0x20, 0x40, 0x40, 0x60, 0x60,
			0x80, 0x80, 0xA0, 0xA0};

		for (int i = 0; i < ARRAYSIZE(ADJUSTMENTS); i++) {
			Direction testdir = (i % 2 ? dir_left : dir_right);
			int32 x = origpt.x + Direction_XFactor(testdir) * ADJUSTMENTS[i];
			int32 y = origpt.y + Direction_YFactor(testdir) * ADJUSTMENTS[i];
			int32 z = origpt.z;

			//
			// Check if we can slide from the original point to a different
			// start point (otherwise we might pop through walls, lasers, etc).
			// This is like Item::collideMove, but we want to stop on any blockers
			// and not trigger any events
			//
			bool startvalid = true;
			Common::List<CurrentMap::SweepItem> collisions;
			Point3 end(x, y, z);
			avatar->setLocation(origpt);
			currentmap->sweepTest(origpt, end, dims, avatar->getShapeInfo()->_flags,
								  avatar->getObjId(), true, &collisions);
			for (const auto &collision : collisions) {
				if (!collision._touching && collision._blocking) {
					startvalid = false;
					break;
				}
			}

			if (startvalid) {
				avatar->setLocation(x, y, z);
				res = avatar->tryAnim(testaction, direction);
				if (res == Animation::SUCCESS) {
					// move to starting point for real (trigger fast area updates etc)
					avatar->setLocation(origpt);
					avatar->move(x, y, z);
					break;
				}
			}
		}

		if (res != Animation::SUCCESS) {
			// reset location and result (in case it's END_OFF_LAND now)
			// couldn't find a better move.
			avatar->setLocation(origpt);
			res = initialres;
		}
	}

	if ((action == Animation::step || action == Animation::run ||
		 action == Animation::startRun || action == Animation::walk)
		&& res == Animation::FAILURE) {
		action = Animation::stand;
	}
	else if ((action == Animation::advance || action == Animation::retreat ||
		 action == Animation::combatRunSmallWeapon ||
		 action == Animation::startRunSmallWeapon)
		&& res == Animation::FAILURE) {
		action = Animation::combatStand;
	}

	bool moving = _isAnimRunningWalking(action);

	if (checkTurn(direction, moving))
		return;

	//debug(6, "Cru avatar step: picked action %d dir %d (test result %d)", action, direction, res);
	avatar->doAnim(action, direction);
}

void CruAvatarMoverProcess::tryAttack() {
	// Don't do it while this process is waiting
	if (is_suspended())
		return;

	Actor *avatar = getControlledActor();
	if (!avatar || avatar->getMapNum() == REBEL_BASE_MAP || avatar->isBusy())
		return;

	Item *wpn = getItem(avatar->getActiveWeapon());
	if (!wpn || !wpn->getShapeInfo() || !wpn->getShapeInfo()->_weaponInfo)
		return;

	Kernel *kernel = Kernel::get_instance();
	if (kernel->getTickNum() < _nextFireTick)
		return;

	if (!avatar->isInCombat()) {
		avatar->setInCombat(0);
	}

	AudioProcess *audio = AudioProcess::get_instance();
	const WeaponInfo *wpninfo = wpn->getShapeInfo()->_weaponInfo;

	if (avatar->getObjId() != kMainActorId) {
		// Non-avatar NPCs never need to reload or run out of energy.
		Animation::Sequence fireanim = (avatar->isKneeling() ?
										Animation::kneelAndFire : Animation::attack);
		waitFor(avatar->doAnim(fireanim, avatar->getDir()));
		return;
	}

	int shotsleft;
	if (wpninfo->_ammoShape) {
		shotsleft = wpn->getQuality();
	} else if (wpninfo->_energyUse) {
		shotsleft = avatar->getMana() / wpninfo->_energyUse;
	} else {
		shotsleft = 1;
	}

	if (!shotsleft) {
		Item *ammo = avatar->getFirstItemWithShape(wpninfo->_ammoShape, true);
		if (ammo) {
			// reload now
			// SGA1 is special, it reloads every shot.
			if (wpn->getShape() == 0x332)
				_SGA1Loaded = true;

			wpn->setQuality(wpninfo->_clipSize);
			ammo->setQuality(ammo->getQuality() - 1);
			if (ammo->getQuality() == 0)
				ammo->destroy();

			if (wpninfo->_reloadSound) {
				audio->playSFX(0x2a, 0x80, avatar->getObjId(), 1);
			}
			if (avatar->getObjId() == kMainActorId && !avatar->isKneeling()) {
				avatar->doAnim(Animation::reloadSmallWeapon, dir_current);
			}

			_nextFireTick = kernel->getTickNum() + 15;
		} else {
			// no shots left
			audio->playSFX(0x2a, 0x80, avatar->getObjId(), 1);
			_nextFireTick = kernel->getTickNum() + 20;
		}
	} else {
		// Check for SGA1 reload anim (which happens every shot)
		if (wpn->getShape() == 0x332 && !avatar->isKneeling() && !_SGA1Loaded) {
			if (wpninfo->_reloadSound) {
				audio->playSFX(0x2a, 0x80, avatar->getObjId(), 1);
			}
			if (avatar->getObjId() == kMainActorId) {
				avatar->doAnim(Animation::reloadSmallWeapon, dir_current);
			}
			_SGA1Loaded = true;
		} else {
			Direction dir = avatar->getDir();
			// Fire event happens from animation
			Animation::Sequence fireanim = (avatar->isKneeling() ?
											Animation::kneelAndFire : Animation::attack);
			uint16 fireanimpid = avatar->doAnim(fireanim, dir);

			if (wpn->getShape() == 0x332)
				_SGA1Loaded = false;

			// Use a shot up
			if (wpninfo->_ammoShape) {
				wpn->setQuality(shotsleft - 1);
			} else if (wpninfo->_energyUse) {
				avatar->setMana(avatar->getMana() - wpninfo->_energyUse);
			}

			// Check if we should alert nearby NPCs
			checkForAlertingNPCs();

			if (wpninfo->_shotDelay) {
				_nextFireTick = kernel->getTickNum() + wpninfo->_shotDelay;
			} else {
				waitFor(fireanimpid);
			}
		}
	}
}

void CruAvatarMoverProcess::checkForAlertingNPCs() {
	uint32 nowtick = Kernel::get_instance()->getTickNum();
	if (nowtick - _lastNPCAlertTick < 240)
		return;

	_lastNPCAlertTick = nowtick;
	uint16 controllednpc = World::get_instance()->getControlledNPCNum();
	for (int i = 2; i < 256; i++) {
		if (i == controllednpc)
			continue;

		Actor *a = getActor(i);
		if (!a || a->isDead() || !a->isOnScreen())
			continue;

		if (!a->isInCombat()) {
			uint16 currentactivity = a->getCurrentActivityNo();
			uint16 activity2 = a->getDefaultActivity(2);
			if (currentactivity == activity2) {
				// note: original game also seems to check surrendering flag here?
				if (currentactivity == 8) {
					// Was guarding, attack!
					a->setActivity(5);
				}
			} else {
				uint16 range = 0;
				uint32 npcshape = a->getShape();
				if (npcshape == 0x2f5 || npcshape == 0x2f6 || npcshape == 0x2f7 ||
					(GAME_IS_REMORSE && (npcshape == 0x595 || npcshape == 0x597)) ||
					(GAME_IS_REGRET && (npcshape == 0x344 || npcshape == 0x384))) {
					Actor *c = getActor(controllednpc);
					if (c)
						range = a->getRangeIfVisible(*c);
				} else {
					range = 1;
				}
				if (range) {
					a->setActivity(a->getDefaultActivity(2));
				}
			}
		} else {
			// Was guarding, attack!
			a->setActivity(5);
		}
	}
}

void CruAvatarMoverProcess::saveData(Common::WriteStream *ws) {
	AvatarMoverProcess::saveData(ws);
	ws->writeSint32LE(_avatarAngle);
	ws->writeByte(_SGA1Loaded ? 1 : 0);

	// We don't bother saving _lastNPCAlertTick or _nextFireTick, they both
	// will get reset to 0 which will behave almost identically in practice.
}

bool CruAvatarMoverProcess::loadData(Common::ReadStream *rs, uint32 version) {
	if (!AvatarMoverProcess::loadData(rs, version)) return false;
	_avatarAngle = rs->readSint32LE();
	_SGA1Loaded = (rs->readByte() != 0);
	return true;
}

// Animations that fluid movement may cut short: locomotion and standing poses.
static bool _isFluidInterruptibleAnim(Animation::Sequence anim) {
	switch (anim) {
	case Animation::stand:
	case Animation::step:
	case Animation::walk:
	case Animation::run:
	case Animation::startRun:
	case Animation::startRunSmallWeapon:
	case Animation::startRunLargeWeapon:
	case Animation::combatRunSmallWeapon:
	case Animation::combatRunLargeWeapon:
	case Animation::stopRunningAndDrawSmallWeapon:
	case Animation::combatStand:
	case Animation::advance:
	case Animation::retreat:
		return true;
	default:
		return false;
	}
}

// True if the avatar's queued/running animation may be cut short.  Checks the
// actual animation process rather than lastAnim, which only updates once a new
// animation starts running (so a just-queued attack still reports the old anim).
static bool _canInterruptAvatarAnim(const Actor *avatar) {
	Kernel *kernel = Kernel::get_instance();
	const uint32 count = kernel->getNumProcesses(avatar->getObjId(), ActorAnimProcess::ACTOR_ANIM_PROC_TYPE);
	if (count == 0)
		return true;
	if (count > 1)
		return false;
	const ActorAnimProcess *proc = dynamic_cast<const ActorAnimProcess *>(
		kernel->findProcess(avatar->getObjId(), ActorAnimProcess::ACTOR_ANIM_PROC_TYPE));
	return proc && _isFluidInterruptibleAnim(proc->getAction());
}

Animation::Sequence CruAvatarMoverProcess::fluidMoveAnim(const Actor *avatar) const {
	// Modern default: run. Holding the run key walks instead.
	const bool walk = hasMovementFlags(MOVE_RUN);
	Actor *a = const_cast<Actor *>(avatar);
	if (avatar->isInCombat()) {
		const Animation::Sequence anim = walk ? Animation::advance : Animation::combatRunSmallWeapon;
		if (a->hasAnim(anim))
			return anim;
	}
	if (!walk && a->hasAnim(Animation::run))
		return Animation::run;
	return Animation::walk;
}

bool CruAvatarMoverProcess::fluidTryMove(Actor *avatar, int32 dx, int32 dy, Direction dir, int32 &movedx, int32 &movedy) {
	movedx = movedy = 0;
	if (!dx && !dy)
		return true;

	CurrentMap *cm = World::get_instance()->getCurrentMap();
	const Point3 pt = avatar->getLocation();
	int32 xd, yd, zd;
	avatar->getFootpadWorld(xd, yd, zd);
	const uint32 shapeflags = avatar->getShapeInfo()->_flags;
	const Box start(pt.x, pt.y, pt.z, xd, yd, zd);

	// Try the full move, then sliding along each axis.  For each, allow small
	// steps up or down so stairs and ramps are followed.
	const int32 moves[3][2] = { { dx, dy }, { dx, 0 }, { 0, dy } };
	static const int32 zsteps[] = { 0, 8, -8, 16, -16 };
	for (const auto &mv : moves) {
		if (!mv[0] && !mv[1])
			continue;
		for (const int32 dz : zsteps) {
			const Box target(pt.x + mv[0], pt.y + mv[1], pt.z + dz, xd, yd, zd);
			const PositionInfo info = cm->getPositionInfo(target, start, shapeflags, avatar->getObjId());
			if (info.valid && info.supported) {
				// Forced collideMove, as the animation process does, so eggs,
				// triggers and touch events fire normally.
				avatar->collideMove(pt.x + mv[0], pt.y + mv[1], pt.z + dz, false, true);
				movedx = mv[0];
				movedy = mv[1];
				return true;
			}
		}
	}

	// Fall back to the search the original animations use, which handles
	// floor pieces at any height (e.g. raised floor markings). Only accept
	// small adjustments so we never pop through walls.
	int32 tx, ty, tz;
	if (cm->scanForValidPosition(pt.x + dx, pt.y + dy, pt.z, avatar, dir, true, tx, ty, tz) &&
		ABS(tx - (pt.x + dx)) <= 16 && ABS(ty - (pt.y + dy)) <= 16 && ABS(tz - pt.z) <= 24) {
		avatar->collideMove(tx, ty, tz, false, true);
		movedx = tx - pt.x;
		movedy = ty - pt.y;
		return true;
	}

	if (gDebugLevel >= 1) {
		const Box target(pt.x + dx, pt.y + dy, pt.z, xd, yd, zd);
		const PositionInfo info = cm->getPositionInfo(target, start, shapeflags, avatar->getObjId());
		debug(1, "fluid move blocked at %d,%d,%d by %s (shape %d) valid=%d supported=%d land=%d",
			  pt.x, pt.y, pt.z, info.blocker ? "item" : "nothing",
			  info.blocker ? (int)info.blocker->getShape() : -1, info.valid, info.supported,
			  info.land ? (int)info.land->getShape() : -1);
	}
	return false;
}

void CruAvatarMoverProcess::onJoystickAxis(int axis, int position) {
	if (axis == Common::JOYSTICK_AXIS_RIGHT_STICK_X)
		_aimStickX = position;
	else if (axis == Common::JOYSTICK_AXIS_RIGHT_STICK_Y)
		_aimStickY = position;
}

// Exact world angle (centidegrees, 0 = north, clockwise; as _avatarAngle) for a world vector.
static int32 _worldVectorToCentidegrees(int32 wx, int32 wy) {
	int32 cd = (int32)round(Math::rad2deg(atan2((double)wx, (double)-wy)) * 100.0);
	if (cd < 0)
		cd += 36000;
	return cd % 36000;
}

bool CruAvatarMoverProcess::getMouseAimDir(const Actor *avatar, Direction &dir, int32 *angle) const {
	GameMapGump *gmg = Ultima8Engine::get_instance()->getGameMapGump();
	const Mouse *mouse = Mouse::get_instance();
	if (!gmg || !mouse)
		return false;

	// Avatar's screen position, computed like GameMapGump::GetLocationOfItem
	// but without disturbing the avatar's interpolation state.
	const Point3 a = avatar->getLocation();
	const Point3 c = CameraProcess::GetCameraLocation();
	int32 sx = (a.x - a.y) / 4 - (c.x - c.y) / 4;
	int32 sy = (a.x + a.y) / 8 - a.z - ((c.x + c.y) / 8 - c.z);
	gmg->GumpToScreenSpace(sx, sy);
	sy -= 24; // aim from the chest, not the feet

	int32 mx, my;
	mouse->getMouseCoords(mx, my);
	const int32 dsx = mx - sx;
	const int32 dsy = my - sy;
	if (ABS(dsx) < 4 && ABS(dsy) < 4)
		return false;

	// Invert the isometric projection: screen x ~ (x - y), screen y ~ (x + y) / 2.
	const int32 wx = dsx + 2 * dsy;
	const int32 wy = 2 * dsy - dsx;
	dir = Direction_GetWorldDir(wy, wx, dirmode_16dirs);
	if (angle)
		*angle = _worldVectorToCentidegrees(wx, wy);
	return true;
}

void CruAvatarMoverProcess::fireWhileMoving(Actor *avatar, Direction dir) {
	if (avatar->getObjId() != kMainActorId || avatar->getMapNum() == REBEL_BASE_MAP)
		return;
	Item *wpn = getItem(avatar->getActiveWeapon());
	if (!wpn || !wpn->getShapeInfo() || !wpn->getShapeInfo()->_weaponInfo)
		return;
	const WeaponInfo *wpninfo = wpn->getShapeInfo()->_weaponInfo;

	Kernel *kernel = Kernel::get_instance();
	if (kernel->getTickNum() < _nextFireTick)
		return;

	int shotsleft;
	if (wpninfo->_ammoShape)
		shotsleft = wpn->getQuality();
	else if (wpninfo->_energyUse)
		shotsleft = avatar->getMana() / wpninfo->_energyUse;
	else
		shotsleft = 1;

	// Reloading, and the SGA1's per-shot reload, use the original handling.
	if (!shotsleft || wpn->getShape() == 0x332) {
		tryAttack();
		return;
	}

	// Muzzle position comes from the attack animation's firing frame.
	const uint32 actionnum = AnimDat::getActionNumberForSequence(Animation::attack, avatar);
	const AnimAction *action = GameData::get_instance()->getMainShapes()->getAnim(avatar->getShape(), actionnum);
	if (!action)
		return;
	const AnimFrame *fireframe = nullptr;
	for (unsigned int i = 0; i < action->getSize(); i++) {
		const AnimFrame &f = action->getFrame(dir, i);
		if (f.is_cruattack()) {
			fireframe = &f;
			break;
		}
	}
	if (!fireframe) {
		tryAttack();
		return;
	}

	if (wpninfo->_damageType == 6)
		kernel->addProcess(new AutoFirerProcess());
	avatar->fireWeapon(fireframe->cru_attackx(), fireframe->cru_attacky(), fireframe->cru_attackz(),
					   dir, wpninfo->_damageType, true);
	AudioProcess *audio = AudioProcess::get_instance();
	if (audio)
		audio->playSFX(wpninfo->_sound, 0x80, avatar->getObjId(), 0, false);

	if (wpninfo->_ammoShape)
		wpn->setQuality(shotsleft - 1);
	else if (wpninfo->_energyUse)
		avatar->setMana(avatar->getMana() - wpninfo->_energyUse);
	checkForAlertingNPCs();

	// Same rate of fire as the original: the weapon's shot delay, or the
	// length of the attack animation for weapons that wait for it.
	uint32 delay = wpninfo->_shotDelay;
	if (!delay)
		delay = action->getSize() * (action->getFrameRepeat() + 1);
	_nextFireTick = kernel->getTickNum() + delay;
}

void CruAvatarMoverProcess::stopFluidMovement(Actor *avatar) {
	_fluidMoving = false;
	const Animation::Sequence standanim = avatar->isInCombat() ? Animation::combatStand : Animation::stand;
	avatar->setToStartOfAnim(standanim);
	avatar->setLastAnim(standanim);
	avatar->setAnimFrame(0);
}

bool CruAvatarMoverProcess::handleFluidMovement() {
	if (!ConfMan.hasKey("fluid_movement") || !ConfMan.getBool("fluid_movement"))
		return false;

	Actor *avatar = getControlledActor();
	if (!avatar || avatar->isDead() || avatar->isKneeling() ||
		!avatar->hasFlags(Item::FLG_FASTAREA) || avatar->getGravityPID() != 0 ||
		Ultima8Engine::get_instance()->isAvatarInStasis()) {
		_fluidMoving = false;
		return false;
	}

	// Rolls, side-steps, crouch, mouse movement and tank-style keys keep the
	// original behaviour.
	if (hasMovementFlags(MOVE_MOUSE_DIRECTION | MOVE_STEP | MOVE_FORWARD | MOVE_BACK |
						 MOVE_TURN_LEFT | MOVE_TURN_RIGHT | MOVE_SHORT_JUMP |
						 MOVE_ROLL_LEFT | MOVE_ROLL_RIGHT | MOVE_STEP_LEFT | MOVE_STEP_RIGHT |
						 MOVE_STEP_FORWARD | MOVE_STEP_BACK | MOVE_TOGGLE_CROUCH)) {
		if (hasMovementFlags(MOVE_TURN_LEFT | MOVE_TURN_RIGHT))
			_mouseAimActive = false;
		_fluidMoving = false;
		return false;
	}

	// Mouse aiming becomes active as soon as the mouse is moved.
	int32 mx, my;
	Mouse::get_instance()->getMouseCoords(mx, my);
	if (mx != _lastMouseX || my != _lastMouseY) {
		if (_lastMouseX >= 0)
			_mouseAimActive = true;
		_lastMouseX = mx;
		_lastMouseY = my;
	}
	Direction aimdir = avatar->getDir();
	int32 aimAngle = Direction_ToCentidegrees(aimdir);
	const bool canAim = avatar->getMapNum() != REBEL_BASE_MAP;

	// Right stick aims (and readies the weapon) once pushed past the dead zone.
	const double stickLen = sqrt((double)_aimStickX * _aimStickX + (double)_aimStickY * _aimStickY);
	const bool stickAim = canAim && stickLen > 12000;
	if (stickAim) {
		_mouseAimActive = false;
		if (!avatar->isInCombat())
			avatar->setInCombat(0);
		const int32 swx = _aimStickX + 2 * _aimStickY, swy = 2 * _aimStickY - _aimStickX;
		aimdir = Direction_GetWorldDir(swy, swx, dirmode_16dirs);
		aimAngle = _worldVectorToCentidegrees(swx, swy);
	}

	// Left button on the game world fires: draw the weapon if needed.
	// (The mover only receives button presses made on the game map.)
	if (canAim && !stickAim && _mouseButton[0].isState(MBS_DOWN)) {
		_mouseAimActive = true;
		if (!avatar->isInCombat())
			avatar->setInCombat(0);
	}

	const bool haveAim = stickAim || (canAim && _mouseAimActive && avatar->isInCombat() &&
		getMouseAimDir(avatar, aimdir, &aimAngle));
	const bool mouseFire = haveAim && !stickAim && _mouseButton[0].isState(MBS_DOWN);

	int x, y;
	getMovementFlagAxes(x, y);
	if (x == 0 && y == 0) {
		if (haveAim) {
			// Standing with weapon drawn: face the cursor, fire on left button.
			const bool wantFire = mouseFire || hasMovementFlags(MOVE_ATTACKING);
			if (avatar->isBusy()) {
				if (!_canInterruptAvatarAnim(avatar) ||
					(aimdir == avatar->getDir() && !wantFire))
					return false;
				Kernel::get_instance()->killProcesses(avatar->getObjId(), ActorAnimProcess::ACTOR_ANIM_PROC_TYPE, true);
			}
			_fluidMoving = false;
			if (avatar->getDir() != aimdir || avatar->getLastAnim() != Animation::combatStand) {
				avatar->setDir(aimdir);
				stopFluidMovement(avatar);
			}
			// Exact angle, so the crosshair sits on the aim rather than
			// snapping between the 16 facing directions.
			_avatarAngle = aimAngle;
			if (wantFire)
				tryAttack();
			_idleTime = 0;
			return true;
		}
		if (_fluidMoving && !avatar->isBusy())
			stopFluidMovement(avatar);
		_fluidMoving = false;
		return false;
	}

	if (avatar->isBusy()) {
		if (!_canInterruptAvatarAnim(avatar))
			return false;
		Kernel::get_instance()->killProcesses(avatar->getObjId(), ActorAnimProcess::ACTOR_ANIM_PROC_TYPE, true);
	}

	// Screen up is world north-west and screen right is world north-east.
	const int32 wx = x - y;
	const int32 wy = -x - y;
	const Direction dir = Direction_GetWorldDir(wy, wx, dirmode_16dirs);

	// Jump while moving: leap in the movement direction.
	if (hasMovementFlags(MOVE_JUMP) && avatar->hasAnim(Animation::jumpForward)) {
		clearMovementFlag(MOVE_JUMP);
		_fluidMoving = false;
		avatar->setDir(dir);
		avatar->clearInCombat();
		step(Animation::jumpForward, dir);
		return true;
	}

	const bool firing = (hasMovementFlags(MOVE_ATTACKING) || mouseFire) &&
		avatar->getMapNum() != REBEL_BASE_MAP;
	if (firing && !avatar->isInCombat())
		avatar->setInCombat(0);
	// Face the direction of travel, except while firing with the mouse or
	// aiming with the right stick: then face the aim and strafe. (Always
	// facing the cursor while moving reads as sliding.)
	const bool strafing = mouseFire || stickAim;
	Direction facedir = strafing ? aimdir : dir;

	// The locomotion animation sets the speed of travel.
	const Animation::Sequence moveanim = fluidMoveAnim(avatar);
	const MainShapeArchive *shapes = GameData::get_instance()->getMainShapes();
	const AnimAction *moveaction = shapes->getAnim(avatar->getShape(),
		AnimDat::getActionNumberForSequence(moveanim, avatar));
	if (!moveaction || moveaction->getSize() == 0)
		return false;

	// Average distance per animation frame for an action in a direction.
	auto frameLength = [](const AnimAction *action, Direction d) {
		int32 deltasum = 0;
		for (unsigned int i = 0; i < action->getSize(); i++)
			deltasum += ABS(action->getFrame(d, i)._deltaDir);
		if (!deltasum)
			return 32.0;
		const double factorlen = sqrt((double)(Direction_XFactor(d) * Direction_XFactor(d) +
											   Direction_YFactor(d) * Direction_YFactor(d)));
		return MAX(1.0, 4.0 * deltasum * factorlen / action->getSize());
	};

	// Speed matches the original animation, spread over its frame repeat ticks.
	const double speed = frameLength(moveaction, dir) / (moveaction->getFrameRepeat() + 1);
	const double inputlen = sqrt((double)(wx * wx + wy * wy));
	const int32 mdx = (int32)round(wx / inputlen * speed);
	const int32 mdy = (int32)round(wy / inputlen * speed);

	int32 movedx = 0, movedy = 0;
	if (!fluidTryMove(avatar, mdx, mdy, dir, movedx, movedy)) {
		// Blocked: stand facing the requested direction.
		avatar->setDir(facedir);
		_avatarAngle = avatar->isInCombat() ? (strafing ? aimAngle : Direction_ToCentidegrees(facedir)) : -1;
		stopFluidMovement(avatar);
		_fluidMoving = true;
		if (firing)
			fireWhileMoving(avatar, facedir);
		_idleTime = 0;
		return true;
	}

	// Sliding along a wall: face the way we're actually going.
	Direction movedir = dir;
	if ((movedx || movedy) && (movedx != mdx || movedy != mdy)) {
		movedir = Direction_GetWorldDir(movedy, movedx, dirmode_16dirs);
		if (!strafing)
			facedir = movedir;
	}

	// When strafing, show a side-step or backpedal instead of running in the
	// facing direction, so the legs match the direction of travel.
	Animation::Sequence visanim = moveanim;
	if (facedir != movedir && avatar->isInCombat()) {
		const int rel = ((int)movedir - (int)facedir + 16) % 16;
		Animation::Sequence strafe = moveanim;
		if (rel >= 3 && rel <= 5)
			strafe = Animation::slideRight;
		else if (rel >= 11 && rel <= 13)
			strafe = Animation::slideLeft;
		else if (rel >= 6 && rel <= 10)
			strafe = Animation::retreat;
		if (strafe != moveanim && avatar->hasAnim(strafe))
			visanim = strafe;
	}
	const AnimAction *visaction = (visanim == moveanim) ? moveaction :
		shapes->getAnim(avatar->getShape(), AnimDat::getActionNumberForSequence(visanim, avatar));
	if (!visaction || visaction->getSize() == 0) {
		visanim = moveanim;
		visaction = moveaction;
	}
	const unsigned int nframes = visaction->getSize();

	if (!_fluidMoving || visanim != _fluidAnim) {
		_fluidAnim = visanim;
		_fluidFrame = 0;
		_fluidDist = 0;
	}

	const double frameLen = frameLength(visaction, facedir);

	avatar->setDir(facedir);
	_avatarAngle = avatar->isInCombat() ? (strafing ? aimAngle : Direction_ToCentidegrees(facedir)) : -1;

	{
		_fluidDist += (int32)(speed * 256);
		while (_fluidDist >= (int32)(frameLen * 256)) {
			_fluidDist -= (int32)(frameLen * 256);
			_fluidFrame = (_fluidFrame + 1) % nframes;
			const AnimFrame &sf = visaction->getFrame(facedir, _fluidFrame);
			if (sf._sfx) {
				AudioProcess *audio = AudioProcess::get_instance();
				if (audio)
					audio->playSFX(sf._sfx, 0x60, avatar->getObjId(), 0);
			}
		}
		const AnimFrame &f = visaction->getFrame(facedir, _fluidFrame);
		avatar->setFrame(f._frame);
		if (f.is_cruflipped())
			avatar->setFlag(Item::FLG_FLIPPED);
		else
			avatar->clearFlag(Item::FLG_FLIPPED);
		avatar->setAnimFrame(_fluidFrame);
		avatar->setLastAnim(visanim);
		_fluidMoving = true;
	}

	if (firing)
		fireWhileMoving(avatar, facedir);

	_idleTime = 0;
	return true;
}

} // End of namespace Ultima8
} // End of namespace Ultima
