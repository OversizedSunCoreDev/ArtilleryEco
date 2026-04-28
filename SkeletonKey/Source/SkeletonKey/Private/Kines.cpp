#include "Kines.h"

void SkeletalMeshKine::SetBonePose(const TArray<FTransform>& ComponentSpaceTransforms, const TArray<FTransform>& BoneSpaceTransforms) {
	auto SkeletalMeshCompPtr = SkeletalMeshComp.Get();
	
	if (!ensure(SkeletalMeshCompPtr)) {
		return;
	}
	
}

void SkeletalMeshKine::SetTransformlike(FTransform Input) {
}

void SkeletalMeshKine::SetLocation(FVector3d Location) {
}

void SkeletalMeshKine::SetRotation(FQuat4d Rotation) {
}

void SkeletalMeshKine::SetLocationAndRotation(FVector3d Loc, FQuat4d Rot) {
}

void SkeletalMeshKine::SetLocationAndRotationWithScope(FVector3d Loc, FQuat4d Rot) {
}

TOptional<FTransform> SkeletalMeshKine::CopyOfTransformlike_Impl() {
	return {};
}
