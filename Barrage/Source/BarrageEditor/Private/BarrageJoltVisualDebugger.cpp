#pragma once

#include "BarrageVisualDebugger.h"
#include "HAL/Platform.h"
THIRD_PARTY_INCLUDES_START

#ifdef JPH_DEBUG_RENDERER

PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include <Jolt/Renderer/DebugRenderer.h>
JPH_SUPPRESS_WARNINGS

PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END


namespace impl
{
	class JoltVisualDebuggerImpl : public JPH::DebugRenderer
	{
	public:
		JoltVisualDebuggerImpl();
		virtual					~JoltVisualDebuggerImpl();
		/// Draw line
		virtual void			DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override;
		/// Draw triangle
		virtual void			DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor) override;
		/// Create a batch of triangles that can be drawn efficiently
		virtual Batch			CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;
		virtual Batch			CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32* inIndices, int inIndexCount) override;
		/// Draw some geometry
		virtual void			DrawGeometry(RMat44Arg inModelMatrix, const AABox& inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override;
		/// Draw text
		virtual void			DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor = Color::sWhite, float inHeight = 0.5f) override;
	private:
		struct ImplData;
		ImplData* mImplData;
	};
}

#endif // JPH_DEBUG_RENDERER