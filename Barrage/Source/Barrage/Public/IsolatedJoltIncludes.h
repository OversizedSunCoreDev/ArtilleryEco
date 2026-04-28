// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once
// ReSharper disable CppUnusedIncludeDirective

#include "FBarrageKey.h"
#include "SkeletonKey.h"
#include "HAL/Platform.h"
#include "seq/concurrent_map.hpp"
THIRD_PARTY_INCLUDES_START


#include <Jolt/Jolt.h>
// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNING_PUSH
JPH_SUPPRESS_WARNINGS 
#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
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
#include "Jolt/Geometry/RayAABox.h"
#include "Jolt/Math/HalfFloat.h"

#include <Memory/IntraTickThreadblindAlloc.h>
typedef seq::concurrent_map<FSkeletonKey, FBarrageKey> KeyToKey;
typedef seq::concurrent_map<FBarrageKey, JPH::BodyID> KeyToBody;


constexpr inline static JPH::EAllowedDOFs StandardBoxAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ | JPH::EAllowedDOFs::RotationY | JPH::EAllowedDOFs::RotationX;
constexpr inline static JPH::EAllowedDOFs RelaxedBoxDOFs = JPH::EAllowedDOFs::All;
constexpr inline static JPH::EAllowedDOFs StandardCapAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ | JPH::EAllowedDOFs::RotationY;

JPH_SUPPRESS_WARNING_POP

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
