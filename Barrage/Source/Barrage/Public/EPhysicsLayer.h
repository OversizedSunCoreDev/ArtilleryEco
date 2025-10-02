// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "EPhysicsLayer.generated.h"

namespace Layers
{
	// Layer that objects can be in, determines which other objects it can collide with
	// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
	// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
	// but only if you do collision testing).

	enum EJoltPhysicsLayer : uint8
	{
		NON_MOVING,
		MOVING,
		HITBOX,
		PROJECTILE,
		ENEMYPROJECTILE,
		ENEMYHITBOX, //this is used for things that must 
		ENEMY,
		CAST_QUERY,
		CAST_QUERY_LEVEL_GEOMETRY_ONLY,
		DEBRIS,
		NUM_LAYERS
	};
}

// TODO: Convert to autowiring somehow.
UENUM(BlueprintType)
enum class EPhysicsLayer : uint8
{
	NON_MOVING = Layers::NON_MOVING,
	MOVING = Layers::MOVING,
	HITBOX = Layers::HITBOX,
	PROJECTILE = Layers::PROJECTILE,
	ENEMYPROJECTILE = Layers::ENEMYPROJECTILE,
	ENEMYHITBOX = Layers::ENEMYHITBOX,
	ENEMY = Layers::ENEMY,
	CAST_QUERY = Layers::CAST_QUERY,
	CAST_QUERY_LEVEL_GEOMETRY_ONLY = Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY,
	DEBRIS = Layers::DEBRIS,
	NUM_LAYERS = Layers::NUM_LAYERS
};

static constexpr Layers::EJoltPhysicsLayer ENEMYBODYLAYERS[2] = {Layers::ENEMY, Layers::ENEMYHITBOX};