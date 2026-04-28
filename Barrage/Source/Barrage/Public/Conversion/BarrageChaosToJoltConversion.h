// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once


namespace JPH
{
	class MeshShapeSettings;
	class ShapeSettings;
	class BodyCreationSettings;
	class StaticCompoundShapeSettings;
}

/**
 * 
 */
namespace Barrage::Conversion
{
	BARRAGE_API JPH::MeshShapeSettings* TriMeshToJoltMeshShape(const Chaos::FTriangleMeshImplicitObject& TriangleMesh);
	
	/** 
	 * Convert an unreal FImplicitObject from Chaos runtime data to a new Jolt body setting
	 * The most notable inclusions here are things like unreal heightfield terrain
	 * This also reveals
	 */ 
	BARRAGE_API JPH::ShapeSettings* ConvertChaosGeoToJoltBody(const Chaos::FImplicitObject& ChaosGeo);
	
	/**
	 * @param bComplexCollision includes simple aggregate geometry (boxes, spheres etc)
	 * @param bComplexCollision includes complex mesh geo
	 * @param bIgnoreTransform if we want to ignore transform offsets to not make shapes offset from the center (if you have your own handling for that)
	 * @return the completed shapesettings (which can be potentially compound shapes) 
	 * You are responsible for keeping track of this pointer, but compound shape's sub shapes are refcounted by jolt
	 * (It is recommended to use jolt's JPH::Ref<JPH::ShapeSettings> as a simple shared ptr)
	 * Currently this does not support ALL shapes but the ones people actually use commonly are all here
	 * Jolt material settings are not set eitherwhich migth be useful later on
	 */
	BARRAGE_API JPH::ShapeSettings* UnrealBodySetupToJolt(const UBodySetup* UnrealBodySetup, const bool bSimpleCollision = true, const bool bComplexCollision = false, const bool bIgnoreTransform = false);
	
	// A more opinionated conversion that considers complex geo
	BARRAGE_API JPH::BodyCreationSettings* BodyInstanceToJoltBodyCreation(const FPhysicsActorHandle& ActorHandle);

};
