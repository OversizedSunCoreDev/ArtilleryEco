#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BarrageJoltVisualDebuggerSettings.generated.h"

UCLASS(Config = Editor, DefaultConfig, meta=(DisplayName = "Barrage Visual Debugger"), MinimalAPI)
class UBarrageJoltVisualDebuggerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

private:
	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	FColor PointColor;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	float PointSize;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	FColor BoxColliderColor;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	float BoxColliderLineThickness;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	FColor SphereColliderColor;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	float SphereColliderLineThickness;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	FColor CapsuleColliderColor;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	float CapsuleColliderLineThickness;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	FColor ConvexColliderColor;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	float ConvexColliderLineThickness;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	FColor TriangleMeshColliderColor;

	UPROPERTY(Config, EditAnywhere, Category = "Visuals")
	float TriangleMeshColliderLineThickness;

public:
	BARRAGEEDITOR_API UBarrageJoltVisualDebuggerSettings();

	FORCEINLINE FColor GetPointColor() const { return PointColor; }
	FORCEINLINE float GetPointSize() const { return PointSize; }
	FORCEINLINE FColor GetBoxColliderColor() const { return BoxColliderColor; }
	FORCEINLINE float GetBoxColliderLineThickness() const { return BoxColliderLineThickness; }
	FORCEINLINE FColor GetSphereColliderColor() const { return SphereColliderColor; }
	FORCEINLINE float GetSphereColliderLineThickness() const { return SphereColliderLineThickness; }
	FORCEINLINE FColor GetCapsuleColliderColor() const { return CapsuleColliderColor; }
	FORCEINLINE float GetCapsuleColliderLineThickness() const { return CapsuleColliderLineThickness; }
	FORCEINLINE FColor GetConvexColliderColor() const { return ConvexColliderColor; }
	FORCEINLINE float GetConvexColliderLineThickness() const { return ConvexColliderLineThickness; }
	FORCEINLINE FColor GetTriangleMeshColliderColor() const { return TriangleMeshColliderColor; }
	FORCEINLINE float GetTriangleMeshColliderLineThickness() const { return TriangleMeshColliderLineThickness; }

	/**
	* Singleton-like access to this settings object default, verifies it is valid.
	**/
	FORCEINLINE static const UBarrageJoltVisualDebuggerSettings& Get()
	{
		auto Ptr = GetDefault<UBarrageJoltVisualDebuggerSettings>();
		check(Ptr);
		return *Ptr;
	}
};
