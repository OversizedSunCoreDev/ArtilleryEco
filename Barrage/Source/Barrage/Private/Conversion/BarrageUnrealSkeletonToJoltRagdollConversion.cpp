// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#include "Conversion/BarrageUnrealSkeletonToJoltRagdollConversion.h"

#include "CoordinateUtils.h"
#include "EPhysicsLayer.h"
#include "Conversion/BarrageChaosToJoltConversion.h"
#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/Shape/Shape.h"
#include "Jolt/Physics/Constraints/SwingTwistConstraint.h"
#include "Jolt/Physics/Ragdoll/Ragdoll.h"
#include "Jolt/Skeleton/Skeleton.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

JPH::RagdollSettings* Barrage::Conversion::CreateJoltRagdollSettingsFromUnrealSkeleton(TNotNull<const USkeleton*> InUnrealSkeleton, TNotNull<const UPhysicsAsset*> InUnrealPhysAsset) {
	

	// maps from jolt bone index to unreal bone setup index
	struct FBSkeletonConversionHitboxInfo {
		FTransform ToParentTransform = FTransform::Identity;
		FName BoneName = NAME_None;
		JPH::Shape* Shape = nullptr;
		JPH::Shape* ParentShape = nullptr; // Not always set
		FConstraintInstance* OriginalConstraint = nullptr; // Not always set
	};

	TArray<FBSkeletonConversionHitboxInfo, TInlineAllocator<64>> HitBoxInfos;


	int32 rootindex = 0;

	const FReferenceSkeleton& UnrealRefSkeleton = InUnrealSkeleton->GetReferenceSkeleton();

	//LocalToWorld.SetRotation(FRotator(0,-90,0).Quaternion());

	//@todo don't copy this, just walk the original. Are physics assets always connected in one chain?
	TArray<USkeletalBodySetup*> SortedBodySetups = InUnrealPhysAsset->SkeletalBodySetups;

	SortedBodySetups.Sort([&](const USkeletalBodySetup& A, const USkeletalBodySetup& B) {
		return UnrealRefSkeleton.FindBoneIndex(A.BoneName) < UnrealRefSkeleton.FindBoneIndex(B.BoneName);
	});
	
	
	auto JoltSkeleton = new JPH::Skeleton;

	for (USkeletalBodySetup* SkelBS : SortedBodySetups) {
		int32 BoneIndex = UnrealRefSkeleton.FindBoneIndex(SkelBS->BoneName);
		FTransform BoneRelativeToParentTransform = UnrealRefSkeleton.GetBoneAbsoluteTransform(BoneIndex);
		const int32 ParentBoneIndex = UnrealRefSkeleton.GetParentIndex(BoneIndex);
		const FName BoneName = UnrealRefSkeleton.GetBoneName(BoneIndex);
		

		
		FConstraintInstance* ConstraintInstance = nullptr;
		uint32 addedjoint = 0;
		if (ParentBoneIndex != INDEX_NONE) {
			
			// If we have a parent let's apply their transform to ours
			BoneRelativeToParentTransform = BoneRelativeToParentTransform.GetRelativeTransform(UnrealRefSkeleton.GetBoneAbsoluteTransform(ParentBoneIndex));
			//UE_LOG(LogTemp, Warning, TEXT("Parent bone %s"), *ParentBone.ToString());
			const int32 ConstraintIndex = InUnrealPhysAsset->FindConstraintIndex(BoneName);
			if (ConstraintIndex != INDEX_NONE) {
				//UE_LOG(LogTemp, Warning, TEXT("Found constraint %s"), *BoneName.ToString());

				ConstraintInstance = &InUnrealPhysAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;
				// todo: what parents matter here? This time we manually set the parent to the "actual" parent of the join

				addedjoint = JoltSkeleton->AddJoint(TCHAR_TO_ANSI(*BoneName.ToString()), TCHAR_TO_ANSI(*ConstraintInstance->GetParentBoneName().ToString()));
				//ConstraintInstance->CalculateDefaultParentTransform()
			}
			else {
				//UE_LOGFMT(LogTemp, Warning, "no constraint for parent of  {bone} ({ParentBone})", BoneName, ParentBone);
				addedjoint = JoltSkeleton->AddJoint(TCHAR_TO_ANSI(*BoneName.ToString()));
				rootindex = addedjoint;
			}
		}
		else {
			addedjoint = JoltSkeleton->AddJoint(TCHAR_TO_ANSI(*BoneName.ToString()));
		}


		// Create a body setup using simple collision
		// @todo let this be tweakable in some way?
		
		JPH::ShapeSettings* Settings = Barrage::Conversion::UnrealBodySetupToJolt(SkelBS, true, false,false);
		auto shaperesult = Settings->Create();
		if (shaperesult.HasError()) {
			ensure(false);
			return nullptr;
		}
	
		// we need to have stable order
		// todo: support physics assets with bodies connected with bones in between them 

		//shaperesult.Get()->Draw(DebugRenderer::sInstance, Vec3::sZero(), Quat::sIdentity(), BoneColor->ToFColor(false));

		FBSkeletonConversionHitboxInfo NewHitBoxInfo;
		NewHitBoxInfo.BoneName = BoneName;
		NewHitBoxInfo.Shape = shaperesult.Get();
		NewHitBoxInfo.ToParentTransform = BoneRelativeToParentTransform;
		
		if (ConstraintInstance) {
			NewHitBoxInfo.OriginalConstraint = ConstraintInstance;
		}
		else {
			NewHitBoxInfo.OriginalConstraint = nullptr;
		}



		HitBoxInfos.Add(NewHitBoxInfo);
	}

	// With no hitbox info we have no reason to make a ragdoll, do we?
	// @todo this might need to change if "empty" ragdolls are useful 
	if (HitBoxInfos.Num() == 0) {
		ensure(false);
		return nullptr;
	}

	HitBoxInfos.StableSort([&](const FBSkeletonConversionHitboxInfo& A, const FBSkeletonConversionHitboxInfo& B) {
		int32 BoneIndexA = UnrealRefSkeleton.FindBoneIndex(A.OriginalConstraint ? A.OriginalConstraint->ConstraintBone1 : NAME_None);
		int32 BoneIndexB = UnrealRefSkeleton.FindBoneIndex(B.OriginalConstraint ? B.OriginalConstraint->ConstraintBone1 : NAME_None);
		return BoneIndexA < BoneIndexB;
	});
	
	for (FBSkeletonConversionHitboxInfo& HitBoxInfo : HitBoxInfos) {
		
		//@todo absolutely nasty search up the tree
		if (HitBoxInfo.OriginalConstraint != nullptr) {
			
			auto ParentBoneName = HitBoxInfo.OriginalConstraint->ConstraintBone2;
			
			auto ParentHitboxInfo = 
			HitBoxInfos.FindByPredicate([&](const FBSkeletonConversionHitboxInfo& OtherInfo) {
				return OtherInfo.BoneName == ParentBoneName;
			});
			
			if (ensure(ParentHitboxInfo)) {
				if (ensure(ParentHitboxInfo->Shape)) {
					HitBoxInfo.ParentShape = ParentHitboxInfo->Shape;
				}
			}
		}
	}
	



	//@todo make sure only ONE of these has no parent! Kind of amazed it doesn't ensure
	JoltSkeleton->CalculateParentJointIndices(); //doing this after so that the constraints are in the right order while iterating.

	// Sort the hitboxes to follow the bone array (nasty..)
	for (int32 i = JoltSkeleton->GetJointCount() - 1; i >= 0; --i) {
		const JPH::Skeleton::Joint& Joint = JoltSkeleton->GetJoint(i);

		auto OtherIndex = HitBoxInfos.IndexOfByPredicate([&](const FBSkeletonConversionHitboxInfo& Info) {
			int32 BoneIndex = UnrealRefSkeleton.FindBoneIndex(ANSI_TO_TCHAR(Joint.mName.c_str()));
			return BoneIndex != INDEX_NONE && BoneIndex == UnrealRefSkeleton.FindBoneIndex(
				Info.OriginalConstraint ? Info.OriginalConstraint->ConstraintBone1 : NAME_None);
		});

		if (OtherIndex == INDEX_NONE || i == OtherIndex) {
			continue;
		}

		HitBoxInfos.Swap(i, OtherIndex);
	}


	JPH::RagdollSettings* settings = new JPH::RagdollSettings;
	
	settings->mSkeleton = JoltSkeleton;
	settings->mParts.resize(JoltSkeleton->GetJointCount());

	for (int32 p = 0; p < settings->mParts.size(); ++p) {
		JPH::RagdollSettings::Part& PartBodyCreationSettings = settings->mParts[p];
		PartBodyCreationSettings.SetShape(HitBoxInfos[p].Shape);
		const FTransform& ToParentTransform = HitBoxInfos[p].ToParentTransform;
		
		PartBodyCreationSettings.mPosition = CoordinateUtils::ToJoltCoordinates(ToParentTransform.GetLocation());
		PartBodyCreationSettings.mRotation = CoordinateUtils::ToJoltRotation(ToParentTransform.GetRotation().GetNormalized());

		const EPhysicsType UnrealPhysType = SortedBodySetups[p]->PhysicsType;

		switch (UnrealPhysType) {
		default:
		case PhysType_Default:
			// PartBodyCreationSettings.mMotionType = JPH::EMotionType::Dynamic;
			// break;
		case PhysType_Kinematic:
			PartBodyCreationSettings.mMotionType = JPH::EMotionType::Kinematic;
			break;
		case PhysType_Simulated:
			PartBodyCreationSettings.mMotionType = JPH::EMotionType::Dynamic;
			break;
		}
		
		// @todo make a hitbox only collision layer
		PartBodyCreationSettings.mObjectLayer = Layers::EJoltPhysicsLayer::DEBRIS;

		PartBodyCreationSettings.mUserData = p;
		// First part is the root, doesn't have a parent and doesn't have a constraint 
		if (p > 0) {
			JPH::SwingTwistConstraintSettings* JoltConstraintSettings = new JPH::SwingTwistConstraintSettings;
			
			// default is worldspace but since it always uses COM in both cases I have found this one easier to express
			JoltConstraintSettings->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
			JoltConstraintSettings->mDrawConstraintSize = .3f;
			// constraint->mUserData = p;
			const FConstraintInstance* UnrealConstraint = HitBoxInfos[p].OriginalConstraint;

			if (!UnrealConstraint) {
				continue;
			}
			
			check(UnrealConstraint->ConstraintBone1 != NAME_None)
			check(UnrealConstraint->ConstraintBone2 != NAME_None)
			
			auto UnrealConstraintToJolt = [&](bool bIsOne,
			                                  const FVector& InUnrealTwistAxis,
			                                  const FVector& InUnrealSecondaryAxis,
			                                  JPH::RVec3& OutPos,
			                                  JPH::Vec3& OutTwist,
			                                  JPH::Vec3& OutPlaneAxis) {
			
				FVector OtherOffset;
				JPH::Vec3 COM;
				if (!bIsOne) {
					COM = HitBoxInfos[p].Shape->GetCenterOfMass();
					OtherOffset = UnrealConstraint->Pos1;
				}
				else {
					COM = HitBoxInfos[p].ParentShape->GetCenterOfMass();
					OtherOffset = UnrealConstraint->Pos2;

				}
				
				// Unreal constraints are local to the bone, jolt constraints are local to the body COM
				OutPos = CoordinateUtils::ToJoltCoordinates(OtherOffset) - COM;
				
				//@todo make ToJoltNormal
				OutTwist = CoordinateUtils::ToJoltCoordinates(InUnrealTwistAxis) * 100;
				OutPlaneAxis = CoordinateUtils::ToJoltCoordinates(InUnrealSecondaryAxis) * 100;

			};

			UnrealConstraintToJolt(true,
			                       UnrealConstraint->PriAxis1,
			                       UnrealConstraint->SecAxis1,
			                       JoltConstraintSettings->mPosition1,
			                       JoltConstraintSettings->mTwistAxis2,
			                       JoltConstraintSettings->mPlaneAxis2);
			UnrealConstraintToJolt(false,
			                       UnrealConstraint->PriAxis2,
			                       UnrealConstraint->SecAxis2,
			                       JoltConstraintSettings->mPosition2,
			                       JoltConstraintSettings->mTwistAxis1,
			                       JoltConstraintSettings->mPlaneAxis1);

			switch (UnrealConstraint->GetAngularTwistMotion()) {
			case ACM_Free: 
				JoltConstraintSettings->mTwistMinAngle = -JPH::JPH_PI;
				JoltConstraintSettings->mTwistMaxAngle = JPH::JPH_PI;
				break;
			case ACM_Limited:
				JoltConstraintSettings->mTwistMinAngle = -(JPH::DegreesToRadians(UnrealConstraint->GetAngularTwistLimit()));
				JoltConstraintSettings->mTwistMaxAngle = (JPH::DegreesToRadians(UnrealConstraint->GetAngularTwistLimit()));
				break;
			case ACM_Locked: 
				JoltConstraintSettings->mTwistMinAngle = UE_KINDA_SMALL_NUMBER;
				JoltConstraintSettings->mTwistMaxAngle = UE_KINDA_SMALL_NUMBER;
				break;
			case ACM_MAX: default: ;
			}


			EAngularConstraintMotion PlaneMotion = UnrealConstraint->GetAngularSwing1Motion();
			float PlaneAngleDegrees = UnrealConstraint->GetAngularSwing1Limit();

			EAngularConstraintMotion NormalMotion = UnrealConstraint->GetAngularSwing2Motion();
			float NormalAngleDegrees = UnrealConstraint->GetAngularSwing2Limit();

			
			switch (PlaneMotion) {
			case EAngularConstraintMotion::ACM_Free:
				JoltConstraintSettings->mPlaneHalfConeAngle = JPH::JPH_PI;
				break;
			case EAngularConstraintMotion::ACM_Limited:
				JoltConstraintSettings->mPlaneHalfConeAngle = JPH::DegreesToRadians(PlaneAngleDegrees);
				// motor?
				break;
			case EAngularConstraintMotion::ACM_Locked:
				JoltConstraintSettings->mPlaneHalfConeAngle = UE_KINDA_SMALL_NUMBER;
				break;
			default: ;
			}

			switch (NormalMotion) {
			case EAngularConstraintMotion::ACM_Free:
				JoltConstraintSettings->mNormalHalfConeAngle = JPH::JPH_PI;
				break;
			case EAngularConstraintMotion::ACM_Limited:
				JoltConstraintSettings->mNormalHalfConeAngle = JPH::DegreesToRadians(NormalAngleDegrees);
				// motor?
				break;
			case EAngularConstraintMotion::ACM_Locked:
				JoltConstraintSettings->mNormalHalfConeAngle = UE_KINDA_SMALL_NUMBER;
				break;
			default: ;
			}
			
			
			// I have no clue what this should be. 
			// Unless I can find a clear unreal analogue to this that isn't just using some random forgotten float value this might force making a separate asset
			JoltConstraintSettings->mMaxFrictionTorque = 1.0f;
			PartBodyCreationSettings.mToParent = JoltConstraintSettings;
			
		}
	}
	
	if (ensure(JoltSkeleton)) {
		
		ensure(JoltSkeleton->AreJointsCorrectlyOrdered());
	}
	
	return settings;
}

JPH::Ragdoll* Barrage::Conversion::ExampleCreateJoltRagdollFromUnrealSkeleton(JPH::PhysicsSystem& PhysicsSystem,
	TNotNull<const USkeletalMeshComponent*> InUnrealSkeletalMeshComponent,
	TNotNull<const USkeleton*> InUnrealSkeleton,
	TNotNull<const UPhysicsAsset*> InUnrealPhysAsset) {
	
	JPH::RagdollSettings* RagdollSettings = CreateJoltRagdollSettingsFromUnrealSkeleton(InUnrealSkeleton, InUnrealPhysAsset);
	
	if (RagdollSettings) {
		
		using namespace JPH;
		// init pose
		const JPH::Skeleton* JoltSkeleton = RagdollSettings->GetSkeleton();
		
		JPH::SkeletonPose JoltPose;
		JoltPose.SetSkeleton(JoltSkeleton);
		
		JPH::Array<JPH::Mat44>& MatricesRef = JoltPose.GetJointMatrices();
		
	    	
		const TArray<FTransform>& ComponentSpaceBoneTransforms = InUnrealSkeletalMeshComponent->GetEditableComponentSpaceTransforms();


		const auto Unreal90Quat = FRotator(0, -90, 0).Quaternion(); // unreal 90 quat
		
		
		const auto Rotation = CoordinateUtils::ToJoltRotation(InUnrealSkeletalMeshComponent->GetComponentRotation().Quaternion());
		const FTransform& UnrealTransform = InUnrealSkeletalMeshComponent->GetComponentTransform();

		for (int32 MatIndex = 0; MatIndex < MatricesRef.size(); ++MatIndex) {
			const JPH::String& BoneName = JoltSkeleton->GetJoints()[MatIndex].mName;

			if (auto BoneIndex = InUnrealSkeletalMeshComponent->GetBoneIndex(BoneName.data())) {
				if (!ComponentSpaceBoneTransforms.IsValidIndex(BoneIndex)) {
					return nullptr;
				}
				// should we skip SetRootOffset??
				//@todo nasty -90 because unreal
				auto BoneWorldTransform = InUnrealSkeletalMeshComponent->GetBoneTransform(BoneIndex, UnrealTransform);
				FTransform BoneBoneTransform =  ComponentSpaceBoneTransforms[BoneIndex];
				// BoneBoneTransform *= Unreal90Quat;
				// auto Translation = JPH::ToVec3Coord(BoneBoneTransform.GetTranslation());
				// auto JoltRotation = JPH::ToQuat(BoneBoneTransform.GetRotation());
				//@todo this is definitely wrong?
				// auto JoltRotation90 =  Rotation * Quat::sRotation(Vec3(0,1,0), 90);
				MatricesRef[MatIndex] =  CoordinateUtils::ToJoltMat44NoScale(BoneWorldTransform);
			}
		}


		// const auto Location = CoordinateUtils::ToJoltCoordinates(InUnrealSkeletalMeshComponent->GetComponentLocation());
		// JoltPose.SetRootOffset(Location);

		// JoltPose.CalculateJointMatrices();


		// Optional: Stabilize the inertia of the limbs
		RagdollSettings->Stabilize();
	
		// Disable parent child collisions so that we don't get collisions between constrained bodies
		RagdollSettings->DisableParentChildCollisions();

		
		// Create and alloc ragdoll
		JPH::Ragdoll* Ragdoll = RagdollSettings->CreateRagdoll(0, 0, &PhysicsSystem);
		
		Ragdoll->AddToPhysicsSystem(JPH::EActivation::Activate);
		
		
		constexpr bool bUseKinematicBones = false;
		if (bUseKinematicBones) {
			Ragdoll->DriveToPoseUsingKinematics(JoltPose, 60/128, true);
		}
		else {
			Ragdoll->SetPose(JoltPose, true);
		}
		
		
		
		return Ragdoll;
	}
	
	
	return nullptr;

}

