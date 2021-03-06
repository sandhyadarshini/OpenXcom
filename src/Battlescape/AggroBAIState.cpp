/*
 * Copyright 2010-2012 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _USE_MATH_DEFINES
#include <cmath>
#include "AggroBAIState.h"
#include "ProjectileFlyBState.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/TileEngine.h"
#include "../Savegame/Tile.h"
#include "../Battlescape/Pathfinding.h"
#include "../Engine/RNG.h"
#include "../Ruleset/Armor.h"
#include "../Resource/ResourcePack.h"

namespace OpenXcom
{

/**
 * Sets up a BattleAIState.
 * @param game pointer to the game.
 * @param unit pointer to the unit.
 */
AggroBAIState::AggroBAIState(SavedBattleGame *game, BattleUnit *unit) : BattleAIState(game, unit), _aggroTarget(0), _lastKnownTarget(0), _timesNotSeen(0)
{

}

/**
 * Deletes the BattleAIState.
 */
AggroBAIState::~AggroBAIState()
{

}

/**
 * Loads the AI state from a YAML file.
 * @param node YAML node.
 */
void AggroBAIState::load(const YAML::Node &node)
{
	int targetID;
	node["aggrotarget"] >> targetID;
	if (targetID != -1)
	{
		for (std::vector<BattleUnit*>::iterator j = _game->getUnits()->begin(); j != _game->getUnits()->end(); ++j)
		{
			if ((*j)->getId() == targetID)
				_aggroTarget = (*j);
		}
	}
	node["lastKnownTarget"] >> targetID;
	if (targetID != -1)
	{
		for (std::vector<BattleUnit*>::iterator j = _game->getUnits()->begin(); j != _game->getUnits()->end(); ++j)
		{
			if ((*j)->getId() == targetID)
				_lastKnownTarget = (*j);
		}
	}
	node["lastKnownPosition"][0] >> _lastKnownPosition.x;
	node["lastKnownPosition"][1] >> _lastKnownPosition.y;
	node["lastKnownPosition"][2] >> _lastKnownPosition.z;
	node["timesNotSeen"] >> _timesNotSeen;
}

/**
 * Saves the AI state to a YAML file.
 * @param out YAML emitter.
 */
void AggroBAIState::save(YAML::Emitter &out) const
{
	out << YAML::BeginMap;
	out << YAML::Key << "state" << YAML::Value << "AGGRO";
	if (_aggroTarget)
	{
		out << YAML::Key << "aggrotarget" << YAML::Value << _aggroTarget->getId();
	}
	else
	{
		out << YAML::Key << "aggrotarget" << YAML::Value << -1;
	}
	if (_lastKnownTarget)
	{
		out << YAML::Key << "lastKnownTarget" << YAML::Value << _lastKnownTarget->getId();
	}
	else
	{
		out << YAML::Key << "lastKnownTarget" << YAML::Value << -1;
	}
	out << YAML::Key << "lastKnownPosition" << YAML::Value << YAML::Flow;
	out << YAML::BeginSeq << _lastKnownPosition.x << _lastKnownPosition.y << _lastKnownPosition.z << YAML::EndSeq;
	out << YAML::Key << "timesNotSeen" << YAML::Value << _timesNotSeen;
	out << YAML::EndMap;
}

/**
 * Enters the current AI state.
 */
void AggroBAIState::enter()
{
	// ROOOAARR !

}


/**
 * Exits the current AI state.
 */
void AggroBAIState::exit()
{

}

/**
 * Runs any code the state needs to keep updating every
 * AI cycle.
 * @param action (possible) AI action to execute after thinking is done.
 */
void AggroBAIState::think(BattleAction *action)
{
	action->type = BA_RETHINK;
	action->actor = _unit;
	/* Aggro is mainly either shooting a target or running towards it (melee).
	   If we do no action here - we assume we lost aggro and will go back to patrol state.
	*/
	int aggression = _unit->getAggression();
	_aggroTarget = 0;
	int unitsSpottingMe = _game->getSpottingUnits(_unit);


	/*	psionic targetting: pick from any of the "exposed" units.
		exposed means they have been previously spotted, and are therefore "known" to the AI,
		regardless of whether we can see them or not, because we're psychic.
	*/
	if (_unit->getStats()->psiSkill && _unit->getType() != "SOLDIER" && _game->getExposedUnits()->size() > 0 && RNG::generate(0, 100) > 66)
	{
		int psiAttackStrength = _unit->getStats()->psiSkill * _unit->getStats()->psiStrength / 50;
		int chanceToAttack = 0;
		int tries = 0;
		for (std::vector<BattleUnit*>::const_iterator i = _game->getExposedUnits()->begin(); i != _game->getExposedUnits()->end() && tries < 80; ++i)
		{
			// don't target tanks
			if ((*i)->getArmor()->getSize() != 2)
			{
				int chanceToAttackMe = psiAttackStrength
					+ ((*i)->getStats()->psiSkill * -0.4)
					- (_game->getTileEngine()->distance(_unit->getPosition(), (*i)->getPosition()) / 2)
					- ((*i)->getStats()->psiStrength)
					+ (RNG::generate(0, 50))
					+ 55;

				if (chanceToAttackMe > chanceToAttack)
				{
					chanceToAttack = chanceToAttackMe;
					_aggroTarget = *i;
				}
			}
			++tries;
		}

		if (!_aggroTarget)
			chanceToAttack = 0;

		if (chanceToAttack)
		{
			if (_unit->getMainHandWeapon()->getAmmoItem())
			{
				if (_unit->getMainHandWeapon()->getAmmoItem()->getRules()->getPower() >= chanceToAttack)
				{
					chanceToAttack = 0;
					_aggroTarget = 0;
				}
			}
			else
			{
				if (RNG::generate(35, 155) >= chanceToAttack)
				{
					chanceToAttack = 0;
					_aggroTarget = 0;
				}
			}
			if (chanceToAttack >= 30)
			{
				int controlOrPanic = 60;
				int morale = _aggroTarget->getMorale();
				int bravery = _aggroTarget->getStats()->bravery;
				if (bravery > 60)
					controlOrPanic += 15;
				if ( bravery < 40)
					controlOrPanic -= 15;
				if (morale >= 40)
				{
					if (morale - 10 * bravery < 50)
						controlOrPanic += 15;
				}
				else
				{
					controlOrPanic -= 15;
				}
				if (!morale)
				{
					controlOrPanic = 0;
				}
				if (RNG::generate(0, 100) >= controlOrPanic)
				{
					action->type = BA_MINDCONTROL;
					action->target = _aggroTarget->getPosition();
				}
				else
				{
					action->type = BA_PANIC;
					action->target = _aggroTarget->getPosition();
				}
			}
			else if (chanceToAttack)
			{
					action->type = BA_PANIC;
					action->target = _aggroTarget->getPosition();
			}
		}
	}

	
	/*	
	*	waypoint targetting: pick from any units currently spotted by our allies.
	*/
	if (_unit->getMainHandWeapon() && _unit->getMainHandWeapon()->getAmmoItem() && _unit->getMainHandWeapon()->getRules()->isWaypoint() && _unit->getType() != "SOLDIER")
	{
		for (std::vector<BattleUnit*>::const_iterator i = _game->getUnits()->begin(); i != _game->getUnits()->end() && _aggroTarget == 0; ++i)
		{
			if ((*i)->getFaction() == _unit->getFaction())
			{
				for (std::vector<BattleUnit*>::const_iterator j = (*i)->getVisibleUnits()->begin(); j != (*i)->getVisibleUnits()->end() && _aggroTarget == 0; ++j)
				{
					_game->getPathfinding()->calculate(_unit, (*j)->getPosition(), *j);
					if (_game->getPathfinding()->getStartDirection() != -1 && explosiveEfficacy((*j)->getPosition(), _unit, (_unit->getMainHandWeapon()->getAmmoItem()->getRules()->getPower()/20)+1, action->diff))
					{
						_aggroTarget = *j;
					}
					_game->getPathfinding()->abortPath();
				}
			}
		}

		if (_aggroTarget != 0)
		{
			action->weapon = _unit->getMainHandWeapon();
			action->type = BA_LAUNCH;
			action->TU = action->actor->getActionTUs(action->type, action->weapon);
			action->waypoints.clear();

			int PathDirection;
			int CollidesWith;
			Position LastWayPoint = _unit->getPosition();
			Position LastPosition = _unit->getPosition();
			Position CurrentPosition = _unit->getPosition();
			Position DirectionVector;

			_game->getPathfinding()->calculate(_unit, _aggroTarget->getPosition(), _aggroTarget);
			PathDirection = _game->getPathfinding()->dequeuePath();
			while (PathDirection != -1)
			{
				LastPosition = CurrentPosition;
				_game->getPathfinding()->directionToVector(PathDirection, &DirectionVector);
				CurrentPosition = CurrentPosition + DirectionVector;
				Position voxelPosA ((CurrentPosition.x * 16)+8, (CurrentPosition.y * 16)+8, (CurrentPosition.z * 24)+12);
				Position voxelPosb ((LastWayPoint.x * 16)+8, (LastWayPoint.y * 16)+8, (LastWayPoint.z * 24)+12);
				CollidesWith = _game->getTileEngine()->calculateLine(voxelPosA, voxelPosb, false, 0, _unit, true, false);
				if (CollidesWith > -1 && CollidesWith < 4)
				{
					action->waypoints.push_back(LastPosition);
					LastWayPoint = LastPosition;
				}
				else if (CollidesWith == 4)
				{
					BattleUnit* target = _game->getTile(CurrentPosition)->getUnit();
					if (target == _aggroTarget)
					{
						action->waypoints.push_back(CurrentPosition);
						LastWayPoint = CurrentPosition;
					}
				}

				PathDirection = _game->getPathfinding()->dequeuePath();
			}
			action->target = action->waypoints.front();
			if( action->waypoints.size() > 6 + (action->diff * 2) || LastWayPoint != _aggroTarget->getPosition())
			{
				action->type = BA_RETHINK;
			}
		}
	}


	/*
	 *	Regular targetting: we can see an enemy, or an enemy can see us (in case we need to take cover)
	 */
	if (_unit->getVisibleUnits()->size() > 0 || unitsSpottingMe)
	{
		for (std::vector<BattleUnit*>::iterator j = _unit->getVisibleUnits()->begin(); j != _unit->getVisibleUnits()->end(); ++j)
		{
			//pick closest living unit
			if (!_aggroTarget || _game->getTileEngine()->distance(_unit->getPosition(), (*j)->getPosition()) < _game->getTileEngine()->distance(_unit->getPosition(), _aggroTarget->getPosition()))
			{
				if(!(*j)->isOut())
					_aggroTarget = (*j);
			}
		}
	
		// if we currently see no target, we either can move to it's last seen position or lose aggro
		if (_aggroTarget == 0 && _lastKnownTarget != 0)
		{
			_timesNotSeen++;
			if (_timesNotSeen > _unit->getIntelligence() || aggression == 0)
			{
				// we lost aggro - going back to patrol state
				return;
			}
			// lets go looking where we've last seen him
			action->type = BA_WALK;
			action->target = _lastKnownPosition;
		}
		else if (_aggroTarget != 0)
		{
			// if we see the target, we either can shoot him, or take cover.
			bool takeCover = true;
			bool charge = false;
			_unit->setCharging(0);
			int number = RNG::generate(0,100);

			// extra 5% chance per unit that sees us
			number += unitsSpottingMe * 5;

			// lost health, chances to take cover get bigger
			if (_unit->getHealth() < _unit->getStats()->health)
				number += 10;

			// aggrotarget has no weapon - chances of take cover get smaller
			if (!_aggroTarget->getMainHandWeapon())
				number -= 50;
			if (aggression == 0 && number < 10)
				takeCover = false;
			if (aggression == 1 && number < 50)
				takeCover = false;
			if (aggression == 2 && number < 90)
				takeCover = false;
		
			// we're using melee, so CHAAAAAAAARGE!!!!!
			if (_unit->getMainHandWeapon() && _unit->getMainHandWeapon()->getRules()->getBattleType() == BT_MELEE)
			{
				_unit->lookAt(_aggroTarget->getPosition() + Position(_unit->getArmor()->getSize()-1, _unit->getArmor()->getSize()-1, 0), false);
				while (_unit->getStatus() == STATUS_TURNING)
					_unit->turn();
				if (_game->getTileEngine()->validMeleeRange(_unit, _aggroTarget))
				{
					action->target = _aggroTarget->getPosition();
					action->weapon = action->actor->getMainHandWeapon();
					action->type = BA_HIT;
					charge = true;
				}
				else
				{
					takeCover = true;
					bool targetFound = false;
					int distance = 200;
					for (int x = 0 - action->actor->getArmor()->getSize()-1; x <= _aggroTarget->getArmor()->getSize()-1; ++x)
					{
						for (int y = 0 - action->actor->getArmor()->getSize()-1; y <= _aggroTarget->getArmor()->getSize()-1; ++y)
						{
							if (!(x == 0 && y == 0))
							{
								Position p (x, y, 0);
								Position checkPath = _aggroTarget->getPosition() + p;
								_game->getPathfinding()->calculate(action->actor, checkPath, 0);
								int newDistance = _game->getTileEngine()->distance(action->actor->getPosition(), checkPath);
								bool valid = _game->getTileEngine()->validMeleeRange(checkPath, -1, action->actor->getArmor()->getSize(), action->actor->getHeight(), _aggroTarget);
								if (_game->getPathfinding()->getStartDirection() != -1 && valid &&
									newDistance < distance)
								{
									// CHAAAAAAARGE!
									action->target = checkPath;
									action->type = BA_WALK;
									charge = true;
									_unit->setCharging(_aggroTarget);
									distance = newDistance;
								}
								_game->getPathfinding()->abortPath();
							}
						}
					}
				}
			}

			if (!takeCover && !charge)
			{
				_timesNotSeen = 0;
				_lastKnownPosition = _aggroTarget->getPosition();
				action->target = _aggroTarget->getPosition();
				action->type = BA_NONE;

				// lets' evaluate if we could throw a grenade
				int tu = 4; // 4TUs for picking up the grenade

				// do we have a grenade on our belt?
				BattleItem *grenade = _unit->getGrenadeFromBelt();
				// distance must be more than X tiles, otherwise it's too dangerous to play with explosives
				if (grenade && explosiveEfficacy(_aggroTarget->getPosition(), _unit, (grenade->getRules()->getPower()/10)+1, action->diff))
				{
					if((_unit->getFaction() == FACTION_NEUTRAL && _aggroTarget->getFaction() == FACTION_HOSTILE) || _unit->getFaction() == FACTION_HOSTILE)
					{
						action->weapon = grenade;
						tu += _unit->getActionTUs(BA_PRIME, grenade);
						tu += _unit->getActionTUs(BA_THROW, grenade);
						// do we have enough TUs to prime and throw the grenade?
						if (tu <= _unit->getStats()->tu)
						{
							// are we within range?
							if (ProjectileFlyBState::validThrowRange(action))
							{
								grenade->setExplodeTurn(_game->getTurn());
								_unit->spendTimeUnits(_unit->getActionTUs(BA_PRIME, grenade));
								action->type = BA_THROW;
							}
						}
					}
				}

				if (action->type == BA_NONE)
				{
					action->weapon = action->actor->getMainHandWeapon();
					// out of ammo or no weapon or ammo at all, we have to take cover
					if (!action->weapon)
					{
						takeCover = true;
					}
					else
					{
						if(((_unit->getFaction() == FACTION_NEUTRAL && _aggroTarget->getFaction() == FACTION_HOSTILE) || _unit->getFaction() == FACTION_HOSTILE))
						{
							if (action->weapon->getAmmoItem()->getRules()->getDamageType() != DT_HE || explosiveEfficacy(_aggroTarget->getPosition(), _unit, (action->weapon->getAmmoItem()->getRules()->getPower() / 10) +1, action->diff))
							if (RNG::generate(1,10) < 5 && action->weapon->getAmmoQuantity() > 2)
								action->type = BA_AUTOSHOT;
							else
								action->type = BA_SNAPSHOT;
							tu = action->actor->getActionTUs(action->type, action->weapon);
							// enough time units to shoot?
							if (tu > _unit->getTimeUnits())
							{
								takeCover = true;
							}
						}
						else
							takeCover = true;
					}
				}
			}


			if (takeCover && !charge)
			{
				// the idea is to check within a 5 tile radius for a tile which is not seen by our aggroTarget
				// if there is no such tile, we run away from the target.
				action->type = BA_WALK;
				int tries = 0;
				bool coverFound = false;
				while (tries < 30 && !coverFound)
				{
					tries++;
					action->target = _unit->getPosition();
					action->target.x += RNG::generate(-5,5);
					action->target.y += RNG::generate(-5,5);
					if (tries < 20)

						coverFound = !_game->getTileEngine()->visible(_aggroTarget, _game->getTile(action->target));
					else
						coverFound = true;

					if (coverFound)
					{
						// check if we can reach this tile
						_game->getPathfinding()->calculate(_unit, action->target);
						if (_game->getPathfinding()->getStartDirection() == -1)
						{
							coverFound = false;
						}
						_game->getPathfinding()->abortPath();
					}
				}
			}
		}
		if (action->type != BA_RETHINK)
			action->TU = action->actor->getActionTUs(action->type, action->weapon);
	}
	if (_aggroTarget != 0)
		setAggroTarget(_aggroTarget);
}

/**
 * Sets the aggro target to be used by the AI.
 * Note that this does not mean the AI will chase the unit, it will just walk towards this position.
 * Until it forgets about it and goes back to patrolling.
 * @param unit Pointer to the unit.
 */
void AggroBAIState::setAggroTarget(BattleUnit *unit)
{
	_timesNotSeen = 0;
	_lastKnownTarget = unit;
	_lastKnownPosition = unit->getPosition();
}

bool AggroBAIState::explosiveEfficacy(Position targetPos, BattleUnit *attackingUnit, int radius, int diff)
{
	// i hate the player and i want him dead, but i don't want to piss him off.
	if (_game->getTurn() < 3)
		return false;
	int distance = _game->getTileEngine()->distance(attackingUnit->getPosition(), targetPos);
	int injurylevel = attackingUnit->getStats()->health - attackingUnit->getHealth();
	int desperation = (100 - attackingUnit->getMorale()) / 10;
	// assume there's one viable target, the one at ground zero.
	int enemiesAffected = 1;
	// if we're below 1/3 health, let's assume things are dire, and increase desperation.
	if (injurylevel > (attackingUnit->getStats()->health / 3) * 2)
		desperation += 3;

	int efficacy = desperation + enemiesAffected;
	if (distance <= radius)
		efficacy -= 3;

	// we don't want to ruin our own base, but we do want to ruin XCom's day.
	if (_game->getMissionType() == "STR_ALIEN_BASE_ASSAULT") efficacy -= 3;
	else if (_game->getMissionType() == "STR_BASE_DEFENSE" || _game->getMissionType() == "STR_TERROR_MISSION") efficacy += 3;


	BattleUnit *target = _game->getTile(targetPos)->getUnit();
	for (std::vector<BattleUnit*>::iterator i = _game->getUnits()->begin(); i != _game->getUnits()->end(); ++i)
	{
		if (!(*i)->isOut() && (*i) != attackingUnit && (*i)->getPosition().z == targetPos.z && _game->getTileEngine()->distance((*i)->getPosition(), targetPos) <= radius)
		{
			Position voxelPosA = Position ((targetPos.x * 16)+8, (targetPos.y * 16)+8, (targetPos.z * 24)+12);
			Position voxelPosB = Position (((*i)->getPosition().x * 16)+8, ((*i)->getPosition().y * 16)+8, ((*i)->getPosition().z * 24)+12);
			int collidesWith = _game->getTileEngine()->calculateLine(voxelPosA, voxelPosB, false, 0, target, true, false);
			if (collidesWith == 4)
			{
				if ((*i)->getFaction() != attackingUnit->getFaction())
				{
					++enemiesAffected;
					++efficacy;
				}
				else
					efficacy -= 2; // friendlies count double
			}
		}
	}
	// spice things up a bit by adding a random number based on difficulty level
	efficacy += RNG::generate(0, diff+1) - RNG::generate(0,2);
	if (efficacy > 0 || enemiesAffected >= 10)
		return true;
	return false;
}
}
