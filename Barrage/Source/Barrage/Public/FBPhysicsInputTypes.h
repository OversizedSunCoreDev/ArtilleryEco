// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

enum PhysicsInputType
{
	SelfMovement,
	Velocity,
	OtherForce,
	Rotation,
	SetPosition,
	SetGravityFactor,
	SetCharacterGravity,
	AIMovement,
	
	// Throttle only applies to characters at the moment, but may come to be used for vehicles.
	// Throttle offers four variables. Generally, these scale:
	//x: carry over velocity from the prior tick
	//y: gravity
	//z: locomotion (self-directed thrust generated as part of the core movement model)
	//w: forces (thrust applied by abilities, including jump or dash)
	Throttle,
	ADD
};

enum FBShape
{
	Uninitialized,
	Capsule,
	Box,
	Sphere,
	Static,
	Complex,
	Character,
	Projectile
};
