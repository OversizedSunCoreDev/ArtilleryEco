// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once


namespace JPH {
	class SkeletonPose;
	class RagdollSettings;
	class PhysicsSystem;
	class Ragdoll;
	class Skeleton;
}

namespace Barrage::Conversion {
	
	/**
	 * 
	 * @return a RagdollSettings that can be used to create ragdoll bodies with constraints. Also stores a jolt skeleton
	 * @todo we might want to split off making a jolt skeleton from the ragdoll just in case
	 * You are responsible for storing this pointer safely (It is recommended to use jolt's JPH::Ref<JPH::RagdollSettings> as a simple shared ptr)
	 */
	BARRAGE_API JPH::RagdollSettings* CreateJoltRagdollSettingsFromUnrealSkeleton(TNotNull<const USkeleton*> InUnrealSkeleton,
	                                                                              TNotNull<const UPhysicsAsset*> InUnrealPhysAsset);

	BARRAGE_API JPH::Ragdoll* ExampleCreateJoltRagdollFromUnrealSkeleton(JPH::PhysicsSystem& PhysicsSystem,
																		 TNotNull<const USkeletalMeshComponent*> InUnrealSkeletalMeshComponent,
	                                                                     TNotNull<const USkeleton*> InUnrealSkeleton,
	                                                                     TNotNull<const UPhysicsAsset*> InUnrealPhysAsset);
}
