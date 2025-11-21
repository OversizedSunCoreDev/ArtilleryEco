// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "FBarrageKey.h"
#include "SkeletonKey.h"
#include "HAL/Platform.h"
THIRD_PARTY_INCLUDES_START

PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include <Jolt/Jolt.h>
#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Body/BodyPair.h>
#include <Jolt/Physics/Collision/AABoxCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>
#include "Jolt/ConfigurationString.h"
#include "Jolt/Jolt.h"
#include "Jolt/Core/QuickSort.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Geometry/AABox.h"
#include "Jolt/Math/Quat.h"
#include "Jolt/Math/Vec3.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Character/CharacterBase.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhase.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Constraints/FixedConstraint.h"
#include "LocomoCore/Public/Distances/AtypicalDistances.h"
#include "LocomoCore/Public/Distances/ZOrderDistances.h"
#include "PhysicsEngine/BodySetup.h"
#include "libcuckoo/cuckoohash_map.hh"
#include "Jolt/Geometry/RayAABox.h"
#include "Jolt/Math/HalfFloat.h"

#include <Memory/IntraTickThreadblindAlloc.h>
typedef libcuckoo::cuckoohash_map<FSkeletonKey, FBarrageKey> KeyToKey;

typedef libcuckoo::cuckoohash_map<uint64_t, JPH::Ref<JPH::Shape>> BoundsToShape;
typedef libcuckoo::cuckoohash_map<FBarrageKey, JPH::BodyID> KeyToBody;
// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNINGS

PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

class IsolatedJoltIncludes
{
};

namespace JOLT
{
	using namespace JPH;
	using namespace JPH::literals;

	namespace BroadPhaseLayers
	{
		static constexpr BroadPhaseLayer NON_MOVING(0);
		static constexpr BroadPhaseLayer MOVING(1);
		static constexpr BroadPhaseLayer ENEMYHITBOX(2);
		static constexpr BroadPhaseLayer DEBRIS(3);
		static constexpr uint NUM_LAYERS(4);
	};	
}
