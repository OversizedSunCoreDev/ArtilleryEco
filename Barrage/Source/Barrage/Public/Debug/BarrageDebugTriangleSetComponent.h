// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once
#include "Components/MeshComponent.h"
#include "BarrageDebugTriangleSetComponent.generated.h"

/** 
 * line set component uses FVector3f for line positions. Uses vertex colors which are passed in as another vector
 * @todo upon trying it out VoxelCore's debug drawing is exponentially faster than this, we are probably going to switch to that long-term
 */ 

UCLASS()
class BARRAGE_API U3DTriangleSetComponent final : public UMeshComponent
{
	GENERATED_BODY()

public:
	U3DTriangleSetComponent();
	
	/** Clears the component state and marks component as dirty. */
	void ClearComponent();

	/** Specify material that handles lines. */
	void SetTriangleMaterial(UMaterialInterface* InTriangleMaterial);
	
	//~ UMeshComponent Interface (we only have one material, shocker)
	virtual int32 GetNumMaterials() const override { return 1; }


	UPROPERTY()
	TObjectPtr<const UMaterialInterface> TriangleMaterial;

	UPROPERTY()
	mutable FBoxSphereBounds Bounds;

	UPROPERTY()
	mutable bool bBoundsDirty = true;

public:
	/** Clear all triangles and component state. */
	void Clear();

	virtual FPrimitiveSceneProxy * CreateSceneProxy() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;


	friend class FBarrageDebugTriangleSetSceneProxy;


	// Number of triangles + color
	int32 NumElements() const
	{
		return Num;
	}

	// This was adapted from TBasicElementSet to be just for this, I tried to simplify things to make it easier to understand
	static constexpr int32 ElementDataSize = 4;

	
	// Reserves enough memory for a triangle + one color vector
	void ReserveElements(const int32 MaxNum)
	{
		TriangleAndColorElements.SetNumUninitialized(MaxNum * ElementDataSize);
	}
	
	// Reserve enough memory for an additional number of triangles + their color vector
	void ReserveAdditionalElements(const int32 AdditionalNum)
	{
		TriangleAndColorElements.SetNumUninitialized(TriangleAndColorElements.Num() + AdditionalNum * ElementDataSize);
	}

	// Add a point to be rendered using the component.
	void AddElement(const FVector3f& PointOne, const FVector3f& PointTwo, const FVector3f& PointThree, const FVector3f& Color);

	void ClearElements()
	{
		TriangleAndColorElements.Reset();
		Num = 0;
		bElementsDirty = true;
	}

protected:
	TArray<FVector3f> TriangleAndColorElements;
	int32 Num = 0;

	bool bElementsDirty = true;
};
