#include "BarrageJoltVisualDebuggerSettings.h"

UBarrageJoltVisualDebuggerSettings::UBarrageJoltVisualDebuggerSettings()
	: Super()
	, PointColor(FColor::Emerald)
	, PointSize(5.0f)
	, BoxColliderColor(FColor::Red)
	, BoxColliderLineThickness(2.0f)
	, SphereColliderColor(FColor::Green)
	, SphereColliderLineThickness(2.0f)
	, CapsuleColliderColor(FColor::Blue)
	, CapsuleColliderLineThickness(2.0f)
	, ConvexColliderColor(FColor::Cyan)
	, ConvexColliderLineThickness(2.0f)
	, TriangleMeshColliderColor(FColor::Magenta)
	, TriangleMeshColliderLineThickness(2.0f)
{
}