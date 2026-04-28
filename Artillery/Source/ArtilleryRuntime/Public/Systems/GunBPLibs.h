#pragma once

#include "ArtilleryDispatch.h"
#include "ArtilleryBPLibs.h"
#include "SkeletonTypes.h"
#include "GunBPLibs.generated.h"
UCLASS(meta=(ScriptName="GunUtilLibrary"))
class UGunUtilLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GenerateRandomSpreadQuat", DisplayName = "Generate Random Spread Quaternion", Category="Guns|Math Util"))
	static FQuat K2_GenerateRandomSpreadQuat(float MaxSpreadAngle)
	{
		auto HalfMax = MaxSpreadAngle / 2.f;
		return FRotator(FMath::RandRange(-HalfMax, HalfMax), FMath::RandRange(-HalfMax, HalfMax), 0.f).Quaternion();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyRandomSpreadToQuat", DisplayName = "Apply Random Spread To Quaternion", Category="Guns|Math Util"))
	static FQuat K2_ApplyRandomSpreadToQuat(FQuat const &RotationIn, float const MaxSpreadAngle)
	{
		return RotationIn * K2_GenerateRandomSpreadQuat(MaxSpreadAngle);
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyRandomSpreadToVector", DisplayName = "Apply Random Spread To Vector", Category="Guns|Math Util"))
	static FVector K2_ApplyRandomSpreadToVector(FVector const &RotationIn, float const MaxSpreadAngle)
	{
		return K2_ApplyRandomSpreadToQuat(RotationIn.ToOrientationQuat(), MaxSpreadAngle).Vector();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyRandomSpreadToRotator", DisplayName = "Apply Random Spread To Rotator", Category="Guns|Math Util"))
	static FRotator K2_ApplyRandomSpreadToRotator(FRotator const &RotationIn, float const MaxSpreadAngle)
	{
		return K2_ApplyRandomSpreadToQuat(RotationIn.Quaternion(), MaxSpreadAngle).Rotator();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyDamageToValidEntity", DisplayName = "Apply Damage To Valid Entity", Category="Guns|Gameplay Util"))
	static void K2_ApplyDamage(FSkeletonKey ObjectKey, float Damage, UArtilleryDispatch* Dispatch, bool isEnemyTarget = true)
	{
		UArtilleryLibrary::ApplyDamage(ObjectKey, Damage);
	}
};
