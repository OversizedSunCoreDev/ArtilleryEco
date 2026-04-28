// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "IsolatedJoltIncludes.h"


/** Helper functions to convert between Barrage coordinates and Unreal coordinates
 * 
 * We intentionally use * 0.01 instead of dividing to try to get a bit closer to full cross-platform determinism: 
 * https://github.com/jrouwe/JoltPhysics/discussions/1413#discussioncomment-11652454
 */
class CoordinateUtils
{
public:
#define USE_NOLEFTRIGHTSWAP_JOLT_CONVERSION 0
#if !USE_NOLEFTRIGHTSWAP_JOLT_CONVERSION
	static JPH::Vec3 ToJoltCoordinates(FVector3d In)
	{
		return JPH::Vec3(In.X * 0.01, In.Z * 0.01, In.Y * 0.01); //reverse is 0,2,1
	}
	
	static JPH::Vec3 ToJoltCoordinates(FVector3f In)
	{
		return JPH::Vec3(In.X * 0.01, In.Z * 0.01, In.Y * 0.01); //reverse is 0,2,1
	}
	
	static JPH::Vec3 ToJoltCoordinates(double InX, double InY, double InZ)
	{
		return JPH::Vec3(InX * 0.01, InZ * 0.01, InY * 0.01); //reverse is 0,2,1
	}
	
	static JPH::Float3 ToJoltCoordinatesFloat3(const Chaos::TVector<Chaos::FReal, 3> In)
	{
		return JPH::Float3(In.X * 0.01, In.Z * 0.01, In.Y * 0.01); //reverse is 0,2,1
	}
	
	static JPH::Vec3 ToJoltScale(FVector3d In)
	{
		return JPH::Vec3(In.X, In.Z, In.Y); //reverse is 0,2,1
	}
	
	static JPH::Vec3 ToJoltScale(FVector3f In)
	{
		return JPH::Vec3(In.X, In.Z, In.Y); //reverse is 0,2,1
	}
	
	static JPH::Vec3 ToJoltScale(double InX, double InY, double InZ)
	{
		return JPH::Vec3(InX, InZ, InY); //reverse is 0,2,1
	}
	
	//we store forces and rotations both in 4d vecs to allow better memory locality.
	static JPH::Quat ToBarrageForce(FVector3d In)
	{
		return JPH::Quat(In.X, In.Z, In.Y, 1); //reverse is 0,2,1
	}
	
	//we store forces and rotations both in 4d vecs to allow better memory locality.
	static JPH::Quat ToBarrageRotation(FQuat4d In)
	{
		return ToJoltRotation(In);
	}
	
	//we store forces and rotations both in 4d vecs to allow better memory locality.
	static JPH::Quat ToBarrageRotation(FQuat4f In)
	{
		return ToJoltRotation(In);
	}
	
	//we store velocity and rotations both in 4d vecs to allow better memory locality.
	// This one requires a unit conversion unlike force since force is newtons
	static JPH::Quat ToBarrageVelocity(FVector3d In)
	{
		return JPH::Quat(In.X * 0.01, In.Z * 0.01, In.Y * 0.01, 1); //reverse is 0,2,1
	}
	
	static double RadiusToJolt(double In)
	{
		return In * 0.01; 
	}

	static double JoltToRadius(double In)
	{
		return In * 100.0;
	}

	static double DiamToJoltHalfExtent(double In)
	{
		return In / 200.0; 
	}
	
	static FVector3f FromJoltCoordinates(JPH::Vec3 In)
	{
		return FVector3f(In[0] * 100.0, In[2] * 100.0, In[1] * 100.0); // this looks _wrong_.
	}
#define RISKY_FromJoltCoordinates(x) FVector3f(x[0] * 100.0, x[2] * 100.0, x[1] * 100.0)
	
	static FVector3f FromJoltUnitVector(JPH::Vec3 In)
	{
		return FVector3f(In[0], In[2], In[1]); // this looks _wrong_.
	}
	
	static JPH::Quat ToJoltRotation(FQuat4d In)
	{
		return JPH::Quat(-In.X, -In.Z, -In.Y, In.W);
	}

	static JPH::Quat ToJoltRotation(FQuat4f In)
	{
		return JPH::Quat(-In.X, -In.Z, -In.Y, In.W);
	}
	
	static FQuat4f FromJoltRotation(JPH::Quat In)
	{
		return FQuat4f(-In.GetX(), -In.GetZ(), -In.GetY(), In.GetW());
	}
	static FQuat FQFromJoltRotation(JPH::Quat In)
	{
		return FQuat(-In.GetX(), -In.GetZ(), -In.GetY(), In.GetW());
	}
#define RISKY_FromJoltRotation(x) FQuat4f(-x.GetX(), -x.GetZ(), -x.GetY(), x.GetW())
	
	
	
	FORCEINLINE static JPH::Mat44 ToJoltMat44NoScale(const FTransform& InMat)
	{
		JPH::Mat44 Result = JPH::Mat44::sRotationTranslation(
			ToJoltRotation(InMat.GetRotation()),
			ToJoltCoordinates(InMat.GetLocation())
		);
		
		return Result;
	}
#else
	
	
	#define UnrealToJoltCoord(UnrealVector) UnrealVector.Y * -0.01f, UnrealVector.Z * 0.01f, UnrealVector.X * 0.01f
	#define JoltToUnrealCoord(JoltVector) JoltVector[2] * 100.f, JoltVector[0] * -100.f, JoltVector[1] * 100.f

	#define UnrealToJoltNormal(UnrealVector) -UnrealVector.Y, UnrealVector.Z, UnrealVector.X
	#define JoltToUnrealNormal(JoltVector) JoltVector[2], -JoltVector[0], JoltVector[1]
	
	#define UnrealToJoltScale(UnrealVector) UnrealVector.Y, UnrealVector.Z, UnrealVector.X
	#define JoltToUnrealScale(JoltVector) JoltVector[2], JoltVector[0], JoltVector[1]

	#define UnrealToJoltQuat(UnreaQuat) UnreaQuat.Y, -UnreaQuat.Z, -UnreaQuat.X, UnreaQuat.W
	#define JoltToUnrealQuat(JoltQuat) -JoltQuat.GetZ(), JoltQuat.GetX(), -JoltQuat.GetY(), JoltQuat.GetW()

	// #define UnrealToJoltQuat(UnreaQuat) UnreaQuat.X, -UnreaQuat.Z, UnreaQuat.Y, -UnreaQuat.W
	// #define JoltToUnrealQuat(JoltQuat) JoltQuat.GetX(), -JoltQuat.GetZ(), JoltQuat.GetY(), -JoltQuat.GetW()

	#define RISKY_FromJoltCoordinates(In) FVector3f(JoltToUnrealCoord(##In))
	#define RISKY_FromJoltRotation(In) FQuat4f(JoltToUnrealQuat(##In))
	
	static JPH::Vec3 ToJoltCoordinates(FVector3d In)
	{
		return JPH::Vec3(UnrealToJoltCoord(In));
	}
	
	static JPH::Vec3 ToJoltCoordinates(FVector3f In)
	{
		return JPH::Vec3(UnrealToJoltCoord(In));
	}
	
	static JPH::Vec3 ToJoltCoordinates(double InX, double InY, double InZ)
	{
		return JPH::Vec3(InY * -0.01f, InZ * 0.01f, InX * 0.01f);
	}
	
	static JPH::Float3 ToJoltCoordinatesFloat3(const Chaos::TVector<Chaos::FReal, 3> In)
	{
		return JPH::Float3(UnrealToJoltCoord(In));
	}
	
	static JPH::Vec3 ToJoltScale(FVector3d In)
	{
		return JPH::Vec3(UnrealToJoltScale(In));
	}
	
	static JPH::Vec3 ToJoltScale(FVector3f In)
	{
		return JPH::Vec3(UnrealToJoltScale(In));
	}
	
	static JPH::Vec3 ToJoltScale(double InX, double InY, double InZ)
	{
		return JPH::Vec3(InY, InZ, InX);
	}
	
	//we store forces and rotations both in 4d vecs to allow better memory locality.
	static JPH::Quat ToBarrageRotation(FQuat4d In)
	{
		return ToJoltRotation(In);
	}
	
	//we store forces and rotations both in 4d vecs to allow better memory locality.
	static JPH::Quat ToBarrageRotation(FQuat4f In)
	{
		return ToJoltRotation(In);
	}
	
	// we store velocity and rotations both in 4d vecs to allow better memory locality.
	// This one requires a unit conversion unlike force since force is newtons (so we use coordinate conversion)
	static JPH::Quat ToBarrageVelocity(FVector3d In)
	{
		return JPH::Quat(UnrealToJoltCoord(In), 1); //reverse is 0,2,1
	}
	
	
	//we store forces and rotations both in 4d vecs to allow better memory locality.
	static JPH::Quat ToBarrageForce(FVector3d In)
	{
		return JPH::Quat(UnrealToJoltNormal(In), 1);
	}
	
	static double RadiusToJolt(double In)
	{
		return In * 0.01; 
	}

	static float RadiusToJolt(float In)
	{
		return In * 0.01; 
	}
	
	static double JoltToRadius(double In)
	{
		return In * 100.0;
	}

	static double DiamToJoltHalfExtent(double In)
	{
		return In / 200.0; 
	}
	
	static FVector3f FromJoltCoordinates(JPH::Vec3 In)
	{
		return FVector3f(JoltToUnrealCoord(In));
	}

	static FVector3f FromJoltUnitVector(JPH::Vec3 In)
	{
		return FVector3f(JoltToUnrealNormal(In));
	}
	
	static JPH::Quat ToJoltRotation(FQuat4d In)
	{
		return JPH::Quat(UnrealToJoltQuat(In));
	}

	static JPH::Quat ToJoltRotation(FQuat4f In)
	{
		return JPH::Quat(UnrealToJoltQuat(In));
	}
	
	static FQuat4f FromJoltRotation(JPH::Quat In)
	{
		return FQuat4f(JoltToUnrealQuat(In));
	}

	static FQuat FQFromJoltRotation(JPH::Quat In)
	{
		return FQuat(JoltToUnrealQuat(In));
	}
	
	FORCEINLINE static JPH::Mat44 ToJoltMat44NoScale(const FTransform& InMat)
	{
		JPH::Mat44 Result = JPH::Mat44::sRotationTranslation(
			ToJoltRotation(InMat.GetRotation()),
			ToJoltCoordinates(InMat.GetLocation())
		);
		
		return Result;
	}
#endif
};
