// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#ifdef JPH_DEBUG_RENDERER
#include "Debug/BarrageDebugDraw.h"
#include "Debug/BarrageDebugDrawActor.h"

#include "CoordinateUtils.h"


using namespace JPH;


static bool GBarrageJoltDebugDraw = true;
static FAutoConsoleVariableRef CVarUnrealJoltDebugDraw(TEXT("barrage.DrawJoltDebug"),
	GBarrageJoltDebugDraw,
	TEXT("if JoltUnrealDebugRender is on"));


FORCEINLINE FColor ToFColorFromJoltColor(JPH::ColorArg Color)
{
	return FColor(Color.r, Color.g, Color.b, Color.a);
};
FORCEINLINE JPH::Color ToJoltColor(const FColor Color)
{
	return JPH::Color(Color.R, Color.G, Color.B, Color.A);
};


BarrageJoltDebugRender::BarrageJoltDebugRender()
{
	Initialize();
}

BarrageJoltDebugRender::~BarrageJoltDebugRender()
{
	DrawActor = nullptr;
}

void BarrageJoltDebugRender::DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::DrawLine);
	if (!GBarrageJoltDebugDraw)
	{
		return;
	}
	
	if (IsValid(DrawActor))
	{
		// @todo add dedicated line drawing. for now we just draw very thin triangle...
		DrawTriangle(inFrom, inTo, inTo + RVec3::sReplicate(0.001f), inColor, ECastShadow::Off); 
	}
}

void BarrageJoltDebugRender::DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::DrawTriangle);
	if (!GBarrageJoltDebugDraw)
	{
		return;
	}
	
	using namespace UE::Geometry;
	FTriangle3d Tri;
	
	Tri.V[0] = FVector(CoordinateUtils::FromJoltCoordinates(inV1));
	Tri.V[1] = FVector(CoordinateUtils::FromJoltCoordinates(inV2));
	Tri.V[2] = FVector(CoordinateUtils::FromJoltCoordinates(inV3));
	
	if (IsValid(DrawActor))
	{
		FColor UnrealColor = ToFColorFromJoltColor(inColor);
		DrawActor->TrisToDrawQueue.Enqueue<FTriangleWithColor>({Tri, UnrealColor });
	}
}

DebugRenderer::Batch BarrageJoltDebugRender::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::CreateTriangleBatch);

	if (inTriangles == nullptr || inTriangleCount == 0) return new BatchImpl(0);

	lock_guard lock(mMutex);

	uint32 id = mIDCounter++;
	mTriangleBatches.Add(id).Append(inTriangles, inTriangleCount);

	return new BatchImpl(id);
}

DebugRenderer::Batch BarrageJoltDebugRender::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32* inIndices,
                                                                int inIndexCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::CreateTriangleBatch);


	if (inVertices == nullptr || inVertexCount == 0 || inIndices == nullptr || inIndexCount == 0) return new BatchImpl(0);

	lock_guard lock(mMutex);
	uint32 id = mIDCounter++;
	mVertexBatches.Add(id).Append(inVertices, inVertexCount);

	mIndicesForVertexBatches.FindOrAdd(id).Append(inIndices, inIndexCount);
	return new BatchImpl(id);
}

void BarrageJoltDebugRender::DrawGeometry(RMat44Arg inModelMatrix, const AABox& inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor,
                                         const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::DrawGeometry);

	// inGeometry->AddRef();
	// Geometries.Add(inGeometry);
	//inGeometry->mLODs.back()
	if (!GBarrageJoltDebugDraw)
	{
		return;
	}

	// @todo find a sensible LOD for this mesh?

	//@todo might need a mutex here
	float Distance = 10000.0f;
	if (IsValid(DrawActor)) {
		if (APlayerController* LocalPC = DrawActor->GetWorld()->GetFirstPlayerController()) {

			FVector Location;
			FRotator Rot;
			LocalPC->GetPlayerViewPoint(Location, Rot);
			JPH::Vec3 CameraLoc = CoordinateUtils::ToJoltCoordinates(Location);
			Distance = inWorldSpaceBounds.GetSqDistanceTo(CameraLoc);
		}
	}
	
	// @todo This is very very slow and should really store off geometry drawn for later. I will do that if need be but this was not intended to draw an entire scene

	int32 LODIndex = -1;
	for (const LOD& MLoD : inGeometry->mLODs)
	{
		LODIndex++;
		if (Distance < MLoD.mDistance)
		{
			break;
		}
	}

	if (LODIndex == -1) {
		return;
	}
	
	if (BatchImpl* Batch = static_cast<BatchImpl*>(inGeometry->mLODs[LODIndex].mTriangleBatch.GetPtr()))
	{
		if(TArray<Triangle>* Triangles = mTriangleBatches.Find(Batch->mID))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::DrawGeometry);
			// A qeustionable parallel for... this is definitely a bad way to do this


			EParallelForFlags Flags = {};
			// Just draw singlethreaded when we have a small count
			if (Triangles->Num() < 100) {
				Flags = EParallelForFlags::ForceSingleThread;
			}
			
			ParallelFor(Triangles->Num(), [&](int32 i)
			{
				const Triangle& Tri = (*Triangles)[i];
				RVec3 V1 =  static_cast<RVec3>(Tri.mV[0].mPosition);
				RVec3 V2 =  static_cast<RVec3>(Tri.mV[1].mPosition);
				RVec3 V3 =  static_cast<RVec3>(Tri.mV[2].mPosition);
				//transform the verts to world space
				V1 = inModelMatrix * V1;
				V2 = inModelMatrix * V2;
				V3 = inModelMatrix * V3;
				
				DrawTriangle(V1, V2, V3, inModelColor, ECastShadow::Off);
			}, Flags);
		}
		else
		{
			//UE_LOG(LogTemp, Error, TEXT("Indices no batch found? do we create one?"));
		}

		TArray<uint32>* Indices = mIndicesForVertexBatches.Find(Batch->mID);
		TArray<Vertex>* Vertices = mVertexBatches.Find(Batch->mID);
		
		if(Indices && Vertices)
		{
			if(Indices->Num() % 3 != 0)
			{
				ensure(false);
				UE_LOG(LogTemp, Error, TEXT("Indices not divisible by 3"));
				return;
			}

			EParallelForFlags Flags = {};
			// Just draw singlethreaded when we have a small count
			if (Indices->Num() < 100) {
				Flags = EParallelForFlags::ForceSingleThread;
			}

			ParallelFor(Indices->Num() / 3, [&](int32 i)
			{
				i *= 3;
				
				RVec3 V1 = static_cast<Vec3>((*Vertices)[(*Indices)[i]].mPosition);
				RVec3 V2 = static_cast<Vec3>((*Vertices)[(*Indices)[i + 1]].mPosition);
				RVec3 V3 = static_cast<Vec3>((*Vertices)[(*Indices)[i + 2]].mPosition);
				//transform the verts to world space
				V1 = inModelMatrix * V1;
				V2 = inModelMatrix * V2;
				V3 = inModelMatrix * V3;
				DrawTriangle(V1, V2, V3, inModelColor, ECastShadow::Off);
			}, Flags);
		}
	}
}

void BarrageJoltDebugRender::DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor, float inHeight)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(JoltUnrealDebugRender::DrawText3D);
	if (GBarrageJoltDebugDraw && IsValid(DrawActor))
	{
		FColor UnrealColor = ToFColorFromJoltColor(inColor);
		FVector Location = FVector(CoordinateUtils::FromJoltCoordinates(inPosition));
		FString String = FString(inString.data());
		DrawActor->StringsToDraw.Enqueue<FBarrageDebugStringDrawQueueElement>({FString(inString.data()), Location, UnrealColor});
	}
}
#endif
