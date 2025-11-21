// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "EPhysicsLayer.h"
#include "IsolatedJoltIncludes.h"

struct BarrageContactEntity
{
	BarrageContactEntity()
	{
		ContactKey = FBarrageKey();
		MyLayer = Layers::NUM_LAYERS;
	}
	
	BarrageContactEntity(const FBarrageKey ContactKeyIn)
	{
		ContactKey = ContactKeyIn;
		MyLayer = Layers::NUM_LAYERS;
	}
	
	BarrageContactEntity(const FBarrageKey ContactKeyIn, const JPH::Body& BodyIn)
	{
		ContactKey = ContactKeyIn;
		bIsProjectile = BodyIn.GetObjectLayer() == Layers::PROJECTILE || BodyIn.GetObjectLayer() == Layers::ENEMYPROJECTILE;
		bIsStaticGeometry = BodyIn.GetObjectLayer() == Layers::NON_MOVING;
		bIsNormalCastQuery = BodyIn.GetObjectLayer() == Layers::CAST_QUERY;
		bIsPureHitbox = BodyIn.GetObjectLayer() == Layers::ENEMYHITBOX || BodyIn.GetObjectLayer() == Layers::HITBOX;
		MyLayer = static_cast<Layers::EJoltPhysicsLayer>(BodyIn.GetObjectLayer());
	}

	FBarrageKey ContactKey;
	bool bIsProjectile = false;
	bool bIsStaticGeometry = false;
	bool bIsNormalCastQuery = false;
	bool bIsPureHitbox = false;
	Layers::EJoltPhysicsLayer MyLayer;
};

enum class EBarrageContactEventType
{
	ADDED,
	PERSISTED,
	REMOVED, // REMOVE EVENTS REQUIRE ADDITIONAL SPECIAL HANDLING AS THEY DO NOT HAVE ALL DATA SET
	GHOST
};

struct BarrageContactEvent
{
	BarrageContactEvent()
	{
		ContactEventType = EBarrageContactEventType::GHOST;
	}
	
	BarrageContactEvent(EBarrageContactEventType EventType, const BarrageContactEntity& BarrageContactEntity,
	                    const ::BarrageContactEntity& BarrageContactEntity1);
	EBarrageContactEventType ContactEventType;
	BarrageContactEntity ContactEntity1;
	BarrageContactEntity ContactEntity2;
	FVector PointIfAny = {0,0,0};

	BarrageContactEvent(EBarrageContactEventType EventType, const BarrageContactEntity& BarrageContactEntity,
						const ::BarrageContactEntity& BarrageContactEntity1, FVector CollisionPoint);
	
	bool IsEitherEntityAProjectile() const
	{
		return ContactEntity1.bIsProjectile || ContactEntity2.bIsProjectile;
	}
};

inline BarrageContactEvent::BarrageContactEvent(EBarrageContactEventType EventType,
                                                const BarrageContactEntity& BarrageContactA,
                                                const BarrageContactEntity& BarrageContactB)
{
	ContactEventType = EventType;
	ContactEntity1 = BarrageContactA;
	ContactEntity2 = BarrageContactB;
}

inline BarrageContactEvent::BarrageContactEvent(EBarrageContactEventType EventType,
	const BarrageContactEntity& BarrageContactA, const BarrageContactEntity& BarrageContactB,
	FVector CollisionPoint): ContactEventType(EventType)
{
	ContactEntity1 = BarrageContactA;
	ContactEntity2 = BarrageContactB;
	PointIfAny = CollisionPoint;
}
