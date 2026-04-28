// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Jolt.h>


#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Core/Mutex.h>


class ABarrageDebugDrawActor;

namespace JPH {
	/// Implementation specific batch object
	class BatchImpl : public RefTargetVirtual {
	public:
		JPH_OVERRIDE_NEW_DELETE BatchImpl(uint32 inID) : mID(inID) {
		}

		virtual void AddRef() override { ++mRefCount; }
		virtual void Release() override { if (--mRefCount == 0) delete this; }

		atomic<uint32> mRefCount = 0;
		uint32 mID;
	};

	/**
	 * Jolt debug render override
	 */
	class BarrageJoltDebugRender final : public DebugRenderer {
	public:
		BarrageJoltDebugRender();
		virtual ~BarrageJoltDebugRender() override;

		virtual void DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override;
		virtual void DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow) override;
		virtual Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;
		virtual Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32* inIndices, int inIndexCount) override;
		virtual void DrawGeometry(RMat44Arg inModelMatrix,
		                          const AABox& inWorldSpaceBounds,
		                          float inLODScaleSq,
		                          ColorArg inModelColor,
		                          const GeometryRef& inGeometry,
		                          ECullMode inCullMode,
		                          ECastShadow inCastShadow,
		                          EDrawMode inDrawMode) override;
		virtual void DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor, float inHeight) override;

		Mutex mMutex;
		uint32 mIDCounter;

		TMap<uint32, TArray<Triangle>> mTriangleBatches;

		TMap<uint32, TArray<Vertex>> mVertexBatches;
		TMap<uint32, TArray<uint32>> mIndicesForVertexBatches;

		template <typename ElementType, bool bInAllowDuplicateKeys /*= false*/>
		struct JoltRefKeyFuncs : BaseKeyFuncs<ElementType, ElementType, bInAllowDuplicateKeys> {
			typedef typename TTypeTraits<ElementType>::ConstPointerType KeyInitType;
			typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

			/**
			 * @return The key used to index the given element.
			 */
			[[nodiscard]] static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element) {
				return Element;
			}

			/**
			 * @return True if the keys match.
			 */
			[[nodiscard]] static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B) {
				return A == B;
			}

			/**
			 * @return True if the keys match.
			 */
			template <typename ComparableKey>
			[[nodiscard]] static FORCEINLINE bool Matches(KeyInitType A, ComparableKey B) {
				return A == B;
			}

			/** Calculates a hash index for a key. */
			[[nodiscard]] static FORCEINLINE uint32 GetKeyHash(KeyInitType Key) {
				return Key.GetHash();
			}

			/** Calculates a hash index for a key. */
			template <typename ComparableKey>
			[[nodiscard]] static FORCEINLINE uint32 GetKeyHash(ComparableKey Key) {
				return Key.GetHash();
			}
		};

		TSet<GeometryRef, JoltRefKeyFuncs<GeometryRef, false>> Geometries;

		// Currently this is expected to be set and managed by users. The intent is you have one of these early on 
		ABarrageDebugDrawActor* DrawActor = nullptr;
	};
}
#endif
