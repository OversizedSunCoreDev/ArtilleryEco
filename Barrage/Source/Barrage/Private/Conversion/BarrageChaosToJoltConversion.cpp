// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#include "Conversion/BarrageChaosToJoltConversion.h"

#include "IsolatedJoltIncludes.h"
#include "CoordinateUtils.h"
#include "Chaos/CastingUtilities.h"
#include "Chaos/HeightField.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"
#include "Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Jolt/Physics/Collision/Shape/ScaledShape.h"
#include "Jolt/Physics/Collision/Shape/StaticCompoundShape.h"
#include "Physics/PhysicsFiltering.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"


using namespace Chaos;
using namespace JPH;

using ::CoordinateUtils;

static bool GJoltTriMeshUseChaosTriIndex = true;
FAutoConsoleVariableRef CVarJoltTriMeshUseChaosTriIndex(TEXT("barrage.JoltTriMeshUseChaosTriIndex"), GJoltTriMeshUseChaosTriIndex, TEXT(""));

static int32 GJoltTriMeshMaxTrisPerLeaf = 4;
FAutoConsoleVariableRef CVarJoltTriMeshMaxTrisPerLeaf(
	TEXT("barrage.JoltTriMeshMaxTrisPerLeaf"),
	GJoltTriMeshMaxTrisPerLeaf,
	TEXT("Jolt-specific internal setting for their mesh shape acceleration structure. From their code:\n") TEXT(
		"Maximum number of triangles in each leaf of the axis aligned box tree. This is a balance between memory and performance. Can be in the range [1, MeshShape::MaxTrianglesPerLeaf].\n")
	TEXT("Sensible values are between 4 (for better performance) and 8 (for less memory usage)."));


MeshShapeSettings* Barrage::Conversion::TriMeshToJoltMeshShape(const FTriangleMeshImplicitObject& ChaosTriangleMesh) {
	JPH::MeshShapeSettings* MeshShapeS = nullptr;

	// If we want we can use the chaos-provided set of indexes, or we can have Jolt indexify the mesh
	if (GJoltTriMeshUseChaosTriIndex) {
		JPH::VertexList JoltVerts;
		JPH::IndexedTriangleList JoltIndexedTriangles;

		const Chaos::FTrimeshIndexBuffer& VertToTriBuffers = ChaosTriangleMesh.Elements();
		// indexed triangles are made by collecting the vertexes, then generating triples describing the triangles.
		// this allows the heavier vertices to be stored only once, rather than each time they are used. for large models
		// like terrain, this can be extremely significant. though, it's not truly clear to me if it's worth it.
		if (VertToTriBuffers.RequiresLargeIndices()) {
			JoltIndexedTriangles.reserve(VertToTriBuffers.GetLargeIndexBuffer().Num());
			for (auto& aTri : VertToTriBuffers.GetLargeIndexBuffer()) {
				JoltIndexedTriangles.push_back(IndexedTriangle(aTri[2], aTri[1], aTri[0]));
			}
		}
		else {
			auto& SmallIndexBuffer = VertToTriBuffers.GetSmallIndexBuffer();
			JoltIndexedTriangles.reserve(SmallIndexBuffer.Num());
			for (auto& aTri : SmallIndexBuffer) {
				JoltIndexedTriangles.push_back(IndexedTriangle(aTri[2], aTri[1], aTri[0]));
			}
		}

		// Gather all verts
		auto& ChaosVerts = ChaosTriangleMesh.Particles().X();
		JoltVerts.reserve(ChaosVerts.Num());
		for (auto& vtx : ChaosVerts) {
			JoltVerts.push_back(CoordinateUtils::ToJoltCoordinatesFloat3(vtx));
		}

		MeshShapeS = new JPH::MeshShapeSettings(MoveTemp(JoltVerts), MoveTemp(JoltIndexedTriangles));
	}
	else {
		// In this style we let Jolt indexify the triangle, 
		// this is a much dumber way of visiting triangles in a giant box but it does work well enough
		TriangleList JoltTris;
		JoltTris.reserve(ChaosTriangleMesh.Elements().GetNumTriangles());
		ChaosTriangleMesh.VisitTriangles(FAABB3(FVector(-DBL_MAX), FVector(DBL_MAX)),
		                                 FRigidTransform3(),
		                                 [&](const FTriangle& Tri,
		                                     const int32 TriangleIndex,
		                                     const int32 VertexIndex0,
		                                     const int32 VertexIndex1,
		                                     const int32 VertexIndex2) {
			                                 /// from Jolt docs:
			                                 /// Triangles must be provided in counter clockwise order.
			                                 /// 
			                                 //... so I insert these in backwards order which works likely due to the right/left hand swap...
			                                 Vec3 V0 = CoordinateUtils::ToJoltCoordinates(Tri.GetVertex(2));
			                                 Vec3 V1 = CoordinateUtils::ToJoltCoordinates(Tri.GetVertex(1));
			                                 Vec3 V2 = CoordinateUtils::ToJoltCoordinates(Tri.GetVertex(0));
			                                 JoltTris.push_back(Triangle(V0, V1, V2));
		                                 });

		if (ensure(JoltTris.size() > 0)) {
			MeshShapeS = new MeshShapeSettings(MoveTemp(JoltTris));
		}
	}

	if (MeshShapeS) {
		MeshShapeS->mMaxTrianglesPerLeaf = GJoltTriMeshMaxTrisPerLeaf;
	}

	return MeshShapeS;
};


ShapeSettings* Barrage::Conversion::ConvertChaosGeoToJoltBody(const FImplicitObject& ChaosGeo) {
	
	// convert the unreal shape to a Jolt shape
	// @todo handle instancing
	EImplicitObjectType ObjectType = ChaosGeo.GetCollisionType();
	
	if (IsScaled(ObjectType)) {
		
		
		const FImplicitObjectScaled* ScaledUnrealObject = ChaosGeo.AsA<FImplicitObjectScaled>();
		if (ensure(ScaledUnrealObject)) {
			
			JPH::ShapeSettings* PreScaleJoltSettings = Barrage::Conversion::ConvertChaosGeoToJoltBody(*ScaledUnrealObject->GetInnerObject());
			const Vec3 JoltScale = CoordinateUtils::ToJoltScale(ScaledUnrealObject->GetScale());
			return new ScaledShapeSettings(PreScaleJoltSettings, JoltScale);
		}
		
	}
	
	// There isn't really an approxomation to "instancing" inside of jolt AFAIK, but we still want the inner object!
	if (IsInstanced(ObjectType)) {
		const FImplicitObjectInstanced* InstancedUnrealObject = ChaosGeo.AsA<FImplicitObjectInstanced>();
		if (ensure(InstancedUnrealObject)) {
			
			return Barrage::Conversion::ConvertChaosGeoToJoltBody(*InstancedUnrealObject->GetInnerObject());
		}
	}

	// The inner type masks out scale/instanced (but still incldues Transformed, for some reason)
	const EImplicitObjectType InnerType = GetInnerType(ObjectType);
	switch (InnerType) {
	case ImplicitObjectType::Transformed: {
		if (const auto TransformedUnrealObject = ChaosGeo.GetObject<TImplicitObjectTransformed<FReal, 3>>()) 
		{
			const auto& UnrealTrasnform = TransformedUnrealObject->GetTransform();
			
			JPH::ShapeSettings* PreTransformJoltSettings = Barrage::Conversion::ConvertChaosGeoToJoltBody(*TransformedUnrealObject->GetTransformedObject());

			if (ensure(PreTransformJoltSettings)) {
			
				//@todo does this need to scale as well? Or does the previous IsScaled enum do that already?
				return new RotatedTranslatedShapeSettings(CoordinateUtils::ToJoltCoordinates(UnrealTrasnform.GetLocation()),
															CoordinateUtils::ToJoltRotation(UnrealTrasnform.GetRotation()),
															PreTransformJoltSettings);			}

		};
		break;
	}
		
	case ImplicitObjectType::Sphere: {

		if (const auto* Sphere = ChaosGeo.GetObject<TSphere<FReal, 3>>()) {
			const float Radius = CoordinateUtils::RadiusToJolt(Sphere->GetRadiusf());
			return new SphereShapeSettings(Radius);
		};
		break;
	}
		

	case ImplicitObjectType::Box: {
		if (const auto* Box = ChaosGeo.GetObject<TBox<FReal, 3>>()) {
			const FVector& HalfExtent = Box->Extents() * .5f;
			return new BoxShapeSettings(CoordinateUtils::ToJoltScale(HalfExtent));
		};
		break;
	}
		
	case ImplicitObjectType::Plane: {
		ensureMsgf(false, TEXT("Barrage: Chaos Plane geometry is not supported for Chaos -> Jolt body conversion"));
		// This is a weirder one... not sure exactly how these two relaayed 
		// if (const auto* Plane = ChaosGeo.GetObject<TPlane<FReal, 3>>()) {
		//
		// 	const auto& ConcretePlane = Plane->PlaneConcrete();
		// 	const JPH::RVec3 JoltNormal = CoordinateUtils::ToJoltNormal(ConcretePlane.Normal());
		//	const float Distance = ?
		// 	JPH::Plane JoltPlane = JPH::Plane(JoltNormal, Distance)
		// 	return new PlaneShapeSettings(JoltPlane);
		// };
		break;
	}
	case ImplicitObjectType::Capsule: {
		
		if (const auto* Capsule = ChaosGeo.GetObject<FCapsule>()) {
			const float JoltRadius = CoordinateUtils::RadiusToJolt(Capsule->GetRadiusf());
			const float JoltHalfHeight = CoordinateUtils::RadiusToJolt(Capsule->GetHeightf() * .5f);
			return new CapsuleShapeSettings(JoltHalfHeight, JoltRadius);
		};

		break;
	}
	
	case ImplicitObjectType::LevelSet: {
		ensureMsgf(false, TEXT("Barrage: Chaos LevelSet geometry is not supported for Chaos -> Jolt body conversion"));
		break;
	}
	case ImplicitObjectType::Convex: {
		if (const auto* Convex = ChaosGeo.GetObject<FConvex>()) {
			Array<Vec3> JoltVerts;
			for (TVec3<float> Vertex : Convex->GetVertices()) {
				JoltVerts.push_back(CoordinateUtils::ToJoltCoordinates(Vertex));
			}
			if (!JoltVerts.empty()) {
				return new ConvexHullShapeSettings(JoltVerts);
			}
		};
	}
	case ImplicitObjectType::TaperedCylinder: {
		ensureMsgf(false, TEXT("Barrage: Chaos TaperedCylinder geometry is not yet supported for Chaos -> Jolt body conversion"));
		break;
	}
	case ImplicitObjectType::Cylinder: {
		// The fact that basically... nothing supports this is kind of terrifying
		if (const auto* ChaosCylinder = ChaosGeo.AsA<FCylinder>())
		{
			const float JoltRadius = CoordinateUtils::RadiusToJolt(ChaosCylinder->GetRadiusf());
			const float JoltHalfHeight = CoordinateUtils::RadiusToJolt(ChaosCylinder->GetHeight() * .5f);
			return new CylinderShapeSettings(JoltHalfHeight, JoltRadius);
		}
		break;
		
		break;
	}
	case ImplicitObjectType::TriangleMesh: {
		// Mesh collision (often used as complex collision) is the most complicated case by far, so it gets its own function
		if (const auto* TriMesh = ChaosGeo.AsA<FTriangleMeshImplicitObject>())
		{
			return TriMeshToJoltMeshShape(*TriMesh);
		}
		break;
	}


	case ImplicitObjectType::Union: {
		TArray<JPH::ShapeSettings*, TInlineAllocator<8>> ShapeSettingsArray;

		// For unions we recursively convert the sub-shapes
		const FImplicitObjectUnion* Union = ChaosGeo.GetObject<FImplicitObjectUnion>();
		for (const FImplicitObjectPtr& ImplicitObject : Union->GetObjects()) {
			if (!ensure(ImplicitObject)) {
				continue;
			}
			if (JPH::ShapeSettings* NewShapeSettings = ConvertChaosGeoToJoltBody(*ImplicitObject.GetReference())) {
				ShapeSettingsArray.Add(NewShapeSettings);
			}
		}

		if (ensure(ShapeSettingsArray.Num() > 0)) {
			// We use StaticCompoundShapeSettings. There might be a situation where one would prefer to use MutableCompoundShapeSettings if you plan on modiying the object
			auto NewCompoundMeshSettings = new StaticCompoundShapeSettings();
			NewCompoundMeshSettings->mSubShapes.reserve(ShapeSettingsArray.Num());
			for (ShapeSettings* Settings : ShapeSettingsArray) {
				// @todo it feels like it would be considerably better to consider transformed objects for this rather than just decorating all of them to reduce indirection
				NewCompoundMeshSettings->AddShape(Vec3::sZero(), Quat::sIdentity(), Settings);
			}

			return NewCompoundMeshSettings;
		}
		break;
	}
		
		
	case ImplicitObjectType::HeightField: {
		if (const auto* HeightField = ChaosGeo.GetObject<FHeightField>()) {
			const FHeightField::FDataType& HeightFieldData = HeightField->GeomData;
			
			// Our goal here is to convert unreal's offset positions into a buffer of floats for jolt
			TArray<float> Samples;
			Samples.Reserve(HeightFieldData.NumCols * HeightFieldData.NumRows);
#if 0
				//iterate over the columns and rows but rotated 90 degrees
				for (int32 y = 0; y < HeightFieldData.NumCols; ++y) {
					// // inverted to flip to jolt coordinate space
					// for (int32 x = HeightFieldData.NumRows - 1; x >= 0; --x) {
					// 	// swap the col/rows to rotate it
					// 	auto BaseHeight = HeightFieldData.Heights[y + (x * HeightFieldData.NumRows)];
					// 	const double Height = BaseHeight * HeightFieldData.HeightPerUnit;
					// 	// A loss in precision here between double/float... I am not sure if it matters though
					// 	Samples.Add(Height);
					// }

					for (int32 x = 0; x < HeightFieldData.NumRows; ++x) {
						// swap the col/rows to rotate it
						auto BaseHeight = HeightFieldData.Heights[y + (x * HeightFieldData.NumRows)];
						const double Height = BaseHeight * HeightFieldData.HeightPerUnit;
						// A loss in precision here between double/float... I am not sure if it matters though
						Samples.Add(Height);
					}
				}

				// assumes square heightmap
				uint16 SampleCount = HeightFieldData.NumRows;
				// we need to push this sideways too
				FReal Zoffset = HeightFieldData.MinValue * HeightFieldData.Scale.Z;
				FVector corneroffset = FVector(0.0f, -HeightFieldData.Scale.X * (SampleCount - 1), -Zoffset);

			if (Samples.Num() > 0) {
				auto HeightFieldSettings = new HeightFieldShapeSettings(Samples.GetData(),
																		CoordinateUtils::ToJoltCoordinates(-corneroffset),
																		CoordinateUtils::ToJoltScale(HeightFieldData.Scale) * 0.01,
																		SampleCount);

				// Use a more aggressive block size (we like going fast)
				HeightFieldSettings->mBlockSize = 2;

				return HeightFieldSettings;
			}

#else
			
			ensureMsgf(HeightFieldData.NumCols == HeightFieldData.NumRows, TEXT("Jolt heightmaps expect square shapes only (equal columns and rows)"));
			
			for (int32 y = 0; y < HeightFieldData.NumCols; ++y) {
				for (int32 x = 0; x < HeightFieldData.NumRows; ++x) {
					uint16 BaseHeightUnits = HeightFieldData.Heights[x + (y * HeightFieldData.NumCols)];
					const float Height = BaseHeightUnits * HeightFieldData.HeightPerUnit;
					Samples.Add(Height);
				}
			}
			
			uint16 SampleCount = HeightFieldData.NumRows;
			FReal Zoffset = HeightFieldData.MinValue * HeightFieldData.Scale.Z;
			FVector corneroffset = FVector(0,0, Zoffset);
			
			if (Samples.Num() > 0) {
				auto HeightFieldSettings = new HeightFieldShapeSettings(Samples.GetData(),
																		CoordinateUtils::ToJoltCoordinates(corneroffset),
																		CoordinateUtils::ToJoltScale(HeightFieldData.Scale) * 0.01,
																		SampleCount);

				// Use a more aggressive block size (we like going fast)
				HeightFieldSettings->mBlockSize = 2;

				return HeightFieldSettings;
			}
			
#endif
			

		};
		break;
	}

	default: ensureMsgf(false,
	                    TEXT("Barrage: Chaos body type %s not supported for Chaos -> Jolt body conversion. (It doesn't even have a case yet!)"),
	                    *ChaosGeo.GetTypeName().ToString());
		break;
	}

	return nullptr;
}

JPH::ShapeSettings* Barrage::Conversion::UnrealBodySetupToJolt(const UBodySetup* UnrealBodySetup,
                                                               const bool bSimpleCollision,
                                                               const bool bComplexCollision,
                                                               const bool bIgnoreTransform) {
	if (!ensure(IsValid(UnrealBodySetup))) {
		return nullptr;
	}
	
	// https://jrouwe.github.io/JoltPhysics/#shapes:
	// Next to this there are a number of decorator shapes that change the behavior of their children:
	// 	ScaledShape - This shape can scale a child shape. Note that if a shape is rotated first and then scaled, you can introduce shearing which is not supported by the library.
	// 	RotatedTranslatedShape - This shape can rotate and translate a child shape, it can e.g. be used to offset a sphere from the origin.
	// 	OffsetCenterOfMassShape - This shape does not change its child shape but it does shift the calculated center of mass for that shape. It allows you to e.g. shift the center of mass of a vehicle down to improve its handling.

	// Decorated shape settings are how you express scaled and offset transform shapes in Jolt 
	
	// In this function we try to determine early on if we need to decorate the shape
	
	JPH::ShapeSettings* FinalShapeSettings = nullptr;

	const FKAggregateGeom& AggGeom = UnrealBodySetup->AggGeom;
	
	
	// Current we want a compound shape if there are multiple boxes/capsules etc. This might change if it proves to be too much of an assumption for users
	StaticCompoundShapeSettings* CompoundShapeSettings = nullptr;
	
	{
		// Figure out if we want to be a compound shape ahead of time as it affects how we scale/offset the final shape
		int32 NumShapesNeeded = 0;

		if (bSimpleCollision) {
			// @todo this should count the ones we... actually use
			NumShapesNeeded += AggGeom.GetElementCount();
		}

		if (bComplexCollision) {
			NumShapesNeeded += UnrealBodySetup->TriMeshGeometries.Num();
		}

		// Nothing to do
		if (NumShapesNeeded <= 0) {
			return nullptr;
		}

		if (NumShapesNeeded > 1) {
			// "StaticCompoundShapeSettings" means it cannot be messed with later, as opposed to MutableCompoundShapeSettings
			CompoundShapeSettings = new StaticCompoundShapeSettings();
		}
	}

	auto UpdateFinalShapeSettings = [&](ShapeSettings* InShapeSettings, const FTransform& Transform) {
		check(InShapeSettings);

		// might deserve to be a separate function at some point
		ShapeSettings* DecoratedShape = InShapeSettings;

		// First we consider compound shapes, which do care about decorating scale but otherwise can just be relative to their owner compound shape
		if (CompoundShapeSettings) {
			if (Transform.GetScale3D() != FVector::OneVector) {
				DecoratedShape = new ScaledShapeSettings(InShapeSettings, CoordinateUtils::ToJoltScale(Transform.GetScale3D()));
			}

			CompoundShapeSettings->AddShape(CoordinateUtils::ToJoltCoordinates(Transform.GetLocation()),
			                                CoordinateUtils::ToJoltRotation(Transform.GetRotation()),
			                                DecoratedShape);
			// We want to return the compound shape later
			FinalShapeSettings = CompoundShapeSettings;
			return;
		}


		// Regular non-compound shapes can decorate scale or be offset translaction/rotation from the origin
		if (!bIgnoreTransform) {
			// It might be 
			if (Transform.GetScale3D() != FVector::OneVector) {
				DecoratedShape = new ScaledShapeSettings(InShapeSettings, CoordinateUtils::ToJoltScale(Transform.GetScale3D()));
			}

			if (Transform.GetTranslation() != FVector::ZeroVector || Transform.GetRotation() != FQuat::Identity) {
				DecoratedShape = new RotatedTranslatedShapeSettings(CoordinateUtils::ToJoltCoordinates(Transform.GetLocation()),
				                                                    CoordinateUtils::ToJoltRotation(Transform.GetRotation()),
				                                                    InShapeSettings);
			}

			//@todo is OffsetCenterOfMassShape useful here? I'm not sure if body setups can express this, I will need to check
		}

		FinalShapeSettings = DecoratedShape;
	};

	if (bSimpleCollision) {
		for (const FKBoxElem& BoxElem : AggGeom.BoxElems) {
			FVector3f HalfExtents;
			HalfExtents.X = BoxElem.X * .5f;
			HalfExtents.Y = BoxElem.Y * .5f;
			HalfExtents.Z = BoxElem.Z * .5f;

			// HalfExtents.X = FMath::Max(HalfExtents.X, UE_KINDA_SMALL_NUMBER);
			// HalfExtents.Y = FMath::Max(HalfExtents.Y, UE_KINDA_SMALL_NUMBER);
			// HalfExtents.Z = FMath::Max(HalfExtents.Z, UE_KINDA_SMALL_NUMBER);

			// It's better to treat these like scale but they must also be converted down as they are absolute distance, this is just nicer than ABSing a coordinate converstion
			Vec3 JoltHalfExtent = CoordinateUtils::ToJoltScale(HalfExtents) * 0.01f;
			
			// Don't use a collision margin larger than the smallest box extent
			float CollisionMargin = cDefaultConvexRadius;
			CollisionMargin = FMath::Min(JoltHalfExtent.ReduceMin(), CollisionMargin);

			auto NewBoxSettings = new BoxShapeSettings(JoltHalfExtent, CollisionMargin);
			UpdateFinalShapeSettings(NewBoxSettings, BoxElem.GetTransform());
		}

		for (const FKSphereElem& SphereElem : AggGeom.SphereElems) {
			auto NewSphereSettings = new SphereShapeSettings(SphereElem.Radius * 0.01f);
			UpdateFinalShapeSettings(NewSphereSettings, SphereElem.GetTransform());
		}
		for (const FKSphylElem& SphylElem : AggGeom.SphylElems) {
			const float HalfHeight = (SphylElem.Length / 2) * 0.01;
			const float Radius = SphylElem.Radius * 0.01;
			auto NewCapsuleShapeSettings = new CapsuleShapeSettings(HalfHeight, Radius);
			UpdateFinalShapeSettings(NewCapsuleShapeSettings, SphylElem.GetTransform());
		}

		// Convex elements can come in multiple pieces generally
		for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems) {
			constexpr float DefaultCollisionMarginFractionFromJolt = cDefaultConvexRadius;
			const FVector ScaledSize = (ConvexElem.ElemBox.GetSize()); // Note: unreal convex scale can be negative?
			const float CollisionMargin = FMath::Min(ScaledSize.GetMin() * DefaultCollisionMarginFractionFromJolt,
			                                         DefaultCollisionMarginFractionFromJolt);

			Array<Vec3> JoltVerts;
			JoltVerts.reserve(ConvexElem.VertexData.Num());
			for (const FVector& Vertex : ConvexElem.VertexData) {
				JoltVerts.push_back(CoordinateUtils::ToJoltCoordinates(Vertex));
			}
			
			if (ensure(JoltVerts.size() > 0)) {
				auto NewConvexSettings = new ConvexHullShapeSettings(MoveTemp(JoltVerts), CollisionMargin);
				UpdateFinalShapeSettings(NewConvexSettings, ConvexElem.GetTransform());
			}
		}
	}

	if (bComplexCollision) {
		for (const Chaos::FTriangleMeshImplicitObjectPtr& ChaosTriMeshGeo : UnrealBodySetup->TriMeshGeometries) {
			if (!ensure(ChaosTriMeshGeo.IsValid())) {
				continue;
			}
			
			if (Chaos::FTriangleMeshImplicitObject* ChaosTriMeshGeoRef = ChaosTriMeshGeo.GetReference()) {
				MeshShapeSettings* NewShapeSettings = TriMeshToJoltMeshShape(*ChaosTriMeshGeoRef);
				
				// Triangle meshes don't have an offset transform afaik
				UpdateFinalShapeSettings(NewShapeSettings, FTransform::Identity);
			}
		}
	}

	//@todo there are some leftover pieces but... I do not see these being used a lot? Please let us know if they show up
	ensureMsgf(AggGeom.TaperedCapsuleElems.Num() <= 0, TEXT("TaperedCapsuleElems not supported by UnrealBodySetupToJolt"));
	ensureMsgf(AggGeom.LevelSetElems.Num() <= 0, TEXT("LevelSetElems not supported by UnrealBodySetupToJolt"));
	ensureMsgf(AggGeom.SkinnedLevelSetElems.Num() <= 0, TEXT("SkinnedLevelSetElems not supported by UnrealBodySetupToJolt"));
	ensureMsgf(AggGeom.MLLevelSetElems.Num() <= 0, TEXT("MLLevelSetElems not supported by UnrealBodySetupToJolt"));
	ensureMsgf(AggGeom.SkinnedTriangleMeshElems.Num() <= 0, TEXT("SkinnedTriangleMeshElems not supported by UnrealBodySetupToJolt"));
	
	
	
	// With an offsset vcenter of center of mass we wrap the entire shape in OffsetCenterOfMassShapeSettings
	if (UnrealBodySetup->DefaultInstance.COMNudge != FVector::ZeroVector) {
		const JPH::Vec3 JoltCOMOffset = CoordinateUtils::ToJoltCoordinates(UnrealBodySetup->DefaultInstance.COMNudge);
		return new OffsetCenterOfMassShapeSettings(JoltCOMOffset, FinalShapeSettings);
	}

	return FinalShapeSettings;
}

BodyCreationSettings* Barrage::Conversion::BodyInstanceToJoltBodyCreation(const FPhysicsActorHandle& ActorHandle) {
	
	FRigidBodyHandle_External& GameThreadAPI = ActorHandle->GetGameThreadAPI();
	
	PhysicsInterfaceTypes::FInlineShapeArray Shapes;
	const int32 NumShapes = FillInlineShapeArray_AssumesLocked(Shapes, ActorHandle);

	// Iterate over each shape (we return the first good one for now, as we generally have a complex/simple pair? Dumb but This is getting overcomplicated)
	for (int32 ShapeIdx = 0; ShapeIdx < NumShapes; ShapeIdx++) {
		FPhysicsShapeReference_Chaos& ShapeRef = Shapes[ShapeIdx];
		Chaos::FPerShapeData* Shape = ShapeRef.Shape;
		check(Shape);
		
		// Filter so we trace against the right kind of collision
		FCollisionFilterData ShapeFilter = Shape->GetQueryData();
		const bool bShapeIsComplex = (ShapeFilter.Word3 & EPDF_ComplexCollision) != 0;
		const bool bShapeIsSimple = (ShapeFilter.Word3 & EPDF_SimpleCollision) != 0;

		if (FImplicitObject* Geo = Shape->GetGeometry()) {
			if (ShapeSettings* NewShapeSettings = ConvertChaosGeoToJoltBody(*Geo)) {
				ShapeSettings::ShapeResult ShapeResult = NewShapeSettings->Create();
				if (ShapeResult.HasError()) {
					UE_LOGFMT(LogTemp, Error, "Barrage: Failed to create shape {Error}", FString(ShapeResult.GetError().c_str()));
					continue;
				}

				ShapeRefC FinishedShape = ShapeResult.Get();


				// make an rvec3 from the particle position
				RVec3 position = CoordinateUtils::ToJoltCoordinates(GameThreadAPI.X());

				//position /= 100; // convert to jolt coordinate space
				Quat rot = CoordinateUtils::ToJoltRotation(GameThreadAPI.R());

				EMotionType MotionType = EMotionType::Dynamic;

				ObjectLayer ObjectLayer = {};

				if (GameThreadAPI.ObjectState() == EObjectStateType::Static) {
					MotionType = EMotionType::Static;
					// ObjectLayer = Layers::NON_MOVING;
				}
				else if (GameThreadAPI.ObjectState() == EObjectStateType::Kinematic) {
					// I don't think unreal and jolt have share the idea of what kinematic means? In Unreal things can ALSO be kinematic?
					//MotionType = EMotionType::Kinematic;
				}

				// we don't support moving complex meshes, can we?
				if (bShapeIsComplex && MotionType != EMotionType::Static) {
					return nullptr;
				}

				// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
				return new BodyCreationSettings(FinishedShape, position, rot, MotionType, ObjectLayer);
			}
		}
	}
	return nullptr;
}


