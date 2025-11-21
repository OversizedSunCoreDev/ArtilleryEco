// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoordinateUtils.h"
#include "MassByCategory.h"

//The immediate question is why not use type polymorphism? This looks like THE standard example!
//Well, meshes can't be primitives but they are shape parameters. Okay, we'll just specialize them.
//Well, we need to keep things packed to both the 8 and 4 byte boundary (practically, the 8)
//That's not... too bad. It's enough to make most C++ programmers a little uneasy, thinking about polymorphic types
//and fixed memory allocations. It's doable, actually, not even hard, but it is a weird thing.

//Okay, so killer question:
//Are they ever usable interchangeably? And here, actually, the answer is no. All functionality is unshared except the
//centroid, and when we add offset shapes, even that will be unshared.

//This is normally where I would use templates, but I spent some time thinking about it, and honestly, it doesn't really
//save any code or remove any code paths. I worked it out, it saves about 30 lines of code. Here's where it gets interesting.
//As this develops and bug fixes get added, and the requirements for what params you need begin to accumulate,
//these are going to diverge more and more until there's really very little in common at all.

//For example, we will almost certainly need to support 2d shapes. Those are similar, sure, but should they actually share an ancestor?
//They can't be used in the same parts of the code base or in the same ways at all. In fact, the resulting shapes have totally different purposes

//What if we start to need splines? Welp. Is a spline a shape? Not really! But it needs params, and it has an origin.
//So you might lump it in or factor out a class. If we had good trait support and most programmers in C++ understood them well,
//this entire problem goes away. Likewise, we could use template composition, however, public opinion is really strongly
//divided on that.

//finally, what about density clouds? right now, jolt doesn't have a good concept of them, but we may need to add them
//to improve the iterative numerical stability of collisions. clouds are a shape, but they don't work the damn same.

//so we'd have two different polymorphic tag classes for a total of 6 or 7 types, in a language without true pattern matching.
//it's just not worth it.

//So what does that leave us? It leaves us with... nonpolymorphic types. These types APPEAR to share everything.
//But actually? They share next to nothing.

//Important additional note: these must pack exactly the same on pack 4 or pack 8
//I'm reasonably sure I've done everything right and ensured that the boundaries are correctly annotated
//and data is marshalled correctly across them, but for the sake of my sanity, I'm also going to make sure this struct is
//identically packed in both domains.

//don't change this unless you're sure it's safe. member size AND order alter packing.
//remember to convert https://youtu.be/jhCupKFly_M?si=aoBCNbbAA9DzDPyy&t=438

//REMINDER: these use UE type conventions and so are in UE space.
//I bounced back and forth on this a bunch.

class FBBoxParams
{
public:
	FVector3d Point;
	double JoltX;
	double JoltY;
	double JoltZ;
	FVector3f Offset;
	FMassByCategory::BMassCategories MassClass;
};

class FBSphereParams
{
public:
	FVector3d point;
	double JoltRadius;
};

class FBCapParams
{
public:
	FVector3d point;
	double JoltHalfHeightOfCylinder;
	double JoltRadius;
	double taper;
	FVector3f Offset;
	FMassByCategory::BMassCategories MassClass;
};

struct FBTransform
{
	FVector4d Location;
	FVector4d Scale;
	FQuat4f Rotation;
	
	FBTransform()
	{
		Location = FVector3d::Zero();
		Scale = FVector3d::Zero();
		Rotation = FQuat4f::Identity;
	}
	
	FBTransform(const FTransform& NewTransform)
	{
		SetTransform(NewTransform);
	}

	void SetTransform(const FTransform& NewTransform)
	{
		Location = NewTransform.GetLocation();
		Scale = NewTransform.GetScale3D();
		Rotation = FQuat4f(NewTransform.GetRotation());
	}

	FVector3d GetUnrealLocation() const { return Location; }
	FVector3d GetUnrealScale() const { return Scale; }
	JPH::Vec3Arg GetJoltScale() const { return CoordinateUtils::ToJoltScale(Scale.X, Scale.Y, Scale.Z); }
	FQuat4f GetRotationQuat() const { return Rotation; }
};

class FBCharParams
{
public:
	FVector3d point;
	double JoltHalfHeightOfCylinder;
	double JoltRadius;
	double speed;
};
//this should evaluate in most IDEs, allowing you to see the size if you need to make changes. try not to need to.
//constexpr const static int size = sizeof(FBShapeParams);
