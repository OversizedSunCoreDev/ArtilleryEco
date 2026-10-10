#pragma once

#include "IsolatedJoltIncludes.h"
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
JPH_NAMESPACE_BEGIN

//factored this out just in bloody time. ffs. :/
struct BodyBoxFlatCopy
		{
#define ___LOCO_CHAOSDUNK Vec3Arg{x  +  xBound, y  +  yBound, z + zBound }
			float x; //switching this to center would improve our ability to express large volumes
			float y;
			float z; //could go to a half float here? get it to 24 bytes if you dropped the padding. tbh, could get it to 20 bytes if you tried but
			HalfFloat xBound;
			HalfFloat yBound;
			HalfFloat zBound;
			unsigned char layer;
			unsigned char Unused; // this
			unsigned short material;
			unsigned char Padding; //and this would be unused space at our alignments.
			//good chance we'll need everything from the JPH enum class EFlags
			//also this bitpacking style isn't ACTUALLY platform agnostic, sadly, so when we go to prod, we'll need to actually pack manually.
			//TODO: before prod, pack manually.
			bool IsKinematic : 1;
			bool IsDynamic : 1;
			bool IsSensor : 1;
			bool IsRigid : 1;
			bool HumsInstrumentsOfSurrender : 1;
			bool IsStatic : 1;
			bool IsActive : 1;
			bool GetCollideKinematicVsNonDynamic : 1;
			BodyID meta;

			Vec3 GetCenter() const
			{
				return 0.5f * (___LOCO_CHAOSDUNK + Vec3Arg{x, y, z});
			}

			BodyBoxFlatCopy()
				: x(0),
				  y(0),
				  z(0),
				  xBound(0),
				  yBound(0),
				  zBound(0),
				  layer(0),
				  IsKinematic(false),
				  IsDynamic(false),
				  IsSensor(false),
				  IsRigid(false),
				  HumsInstrumentsOfSurrender(true),
				  //1..2..3.. pause and wait and hhmmm m'hm hm MMmmm hmhmmm mmm may uhm HMmmmm
				  IsStatic(false),
				  IsActive(false),
				  GetCollideKinematicVsNonDynamic(true),
				  meta(0)
			{
			}

			/// Get size of bounding box
			Vec3 GetSize() const
			{
				return ___LOCO_CHAOSDUNK - Vec3Arg{x, y, z};
			}

			AABox GetWorldSpaceBounds()
			{
				//in general, compute & temp alloc is pretty cheap compared to possible cache misses. however, I'd like to ditch this.
				return AABox(Vec3Arg{x, y, z},
				             ___LOCO_CHAOSDUNK
				);
			}

			inline ObjectLayer GetObjectLayer()
			{
				return layer;
			}

			//makes a "point"
			inline BodyBoxFlatCopy(float x, float y, float z): x(x), y(y), z(z), xBound(1), yBound(1), zBound(1),
			                                                     layer(0), IsKinematic(false), IsDynamic(false),
			                                                     IsSensor(false),
			                                                     IsRigid(false),
			                                                     HumsInstrumentsOfSurrender(false),
			                                                     IsStatic(false), IsActive(false),
			                                                     GetCollideKinematicVsNonDynamic(false),
			                                                     meta(0)
			{
			}
			
			explicit BodyBoxFlatCopy(Body& body): HumsInstrumentsOfSurrender(true) //baaaa da daaaaaaaaaa ba da da daaaaaaaaa daa daa badaaaaaaaaa
			{
				SubShapeID emptyDefaultID;
				auto shp = body.GetShape()->GetMaterial(emptyDefaultID);
				auto& a = body.GetWorldSpaceBounds();
				auto Cent = a.GetCenter();
				auto Extt = a.GetExtent();
				x = Cent.GetX();
				y = Cent.GetY();
				z = Cent.GetZ();
				xBound = Extt.GetX() + 1;
				yBound = Extt.GetY() + 1;
				zBound = Extt.GetZ() + 1;
				layer = body.GetObjectLayer();
				material = MashFunctions::lowerbias3216(shp->GetDebugColor().GetUInt32());
				IsKinematic =
					body.IsKinematic();
				IsDynamic =
					body.IsDynamic();
				IsSensor =
					body.IsSensor();
				IsRigid = body.IsRigidBody();
				IsStatic =
					body.IsStatic();
				IsActive =
					body.IsActive();
				GetCollideKinematicVsNonDynamic =
					body.GetCollideKinematicVsNonDynamic();
				meta =
					body.GetID();
			}

			BodyBoxFlatCopy(float X, float Y, float Z, HalfFloat XBound, HalfFloat YBound, HalfFloat ZBound,
			                unsigned char Layer, unsigned short Material, bool bIsKinematic, bool bIsDynamic, bool bIsSensor, bool bIsRigid,
			                bool bIsStatic, bool bIsActive, bool bGetCollideKinematicVsNonDynamic,
			                const BodyID& Meta)
				: x(X),
				  y(Y),
				  z(Z),
				  xBound(XBound),
				  yBound(YBound),
				  zBound(ZBound),
				  layer(Layer),
	              material(Material),
				  IsKinematic(bIsKinematic),
				  IsDynamic(bIsDynamic),
				  IsSensor(bIsSensor),
				  IsRigid(bIsRigid),
				  HumsInstrumentsOfSurrender(true), //nice try. cuno doesn't care
				  IsStatic(bIsStatic),
				  IsActive(bIsActive),
				  GetCollideKinematicVsNonDynamic(bGetCollideKinematicVsNonDynamic),
				  meta(Meta)
			{
			}

			inline BodyBoxFlatCopy(float x, float y, float z, HalfFloat xExt, HalfFloat yExt, HalfFloat zExt,
			                         unsigned char LayerUpTo255, JPH::BodyID ID): x(x), y(y), z(z), xBound(xExt),
				yBound(yExt),
				zBound(zExt), layer(LayerUpTo255), IsKinematic(false), IsDynamic(false), IsSensor(false),
				IsRigid(false),
				HumsInstrumentsOfSurrender(false),
				IsStatic(false),
				IsActive(false),
				GetCollideKinematicVsNonDynamic(false), meta(ID)
			{
			}

			inline bool sFindCollidingPairsCanCollide(const BodyBoxFlatCopy& inBody2)
			{
				//all further behavior assumes body1 (this) is rigid, not soft.
				if (IsRigid)
					return false;

				// One of these conditions must be true
				// - We always allow detecting collisions between kinematic and non-dynamic bodies
				// - One of the bodies must be dynamic to collide
				// - A kinematic object can collide with a sensor
				if (!GetCollideKinematicVsNonDynamic
					&& !inBody2.GetCollideKinematicVsNonDynamic
					&& (!IsDynamic && !inBody2.IsDynamic)
					&& !(IsKinematic && inBody2.IsSensor)
					&& !(inBody2.IsKinematic && IsSensor))
					return false;


				// If the pair A, B collides we need to ensure that the pair B, A does not collide or else we will handle the collision twice.
				// If A is the same body as B we don't want to collide (1)
				// If A is dynamic / kinematic and B is static we should collide (2)
				// If A is dynamic / kinematic and B is dynamic / kinematic we should only collide if
				//	- A is active and B is not active (3)
				//	- A is active and B will become active during this simulation step (4)
				//	- A is active and B is active, we require a condition that makes A, B collide and B, A not (5)
				//
				// the original code deduces activeness. we just record it, because we do not WANT TO TOUCH EXTERNAL STATE.
				//
				// (1) if A.id = B.id then A = B, so to collide A != B
				// (2) A.Index != 0xffffffff, B.Index = 0xffffffff (because it's static and cannot be in the active list), so to collide A.Index != B.Index
				// (3) A.Index != 0xffffffff, B.Index = 0xffffffff (because it's not yet active), so to collide A.Index != B.Index
				// (4) A.Index != 0xffffffff, B.Index = 0xffffffff currently. But it can activate during the Broad/NarrowPhase step at which point it
				//     will be added to the end of the active list which will make B.Index > A.Index (this holds only true when we don't deactivate
				//     bodies during the Broad/NarrowPhase step), so to collide A.Index < B.Index.
				// (5) As tie breaker we can use the same condition A.Index < B.Index to collide, this means that if A, B collides then B, A won't
				static_assert(Body::cInactiveIndex == 0xffffffff, "The algorithm below uses this value");
				if (meta.GetIndexAndSequenceNumber() == inBody2.meta.GetIndexAndSequenceNumber())
					return false;
				if (inBody2.IsRigid && meta.GetIndex() >= inBody2.meta.GetIndex())
					return false;

				return true;
			}
		};

JPH_NAMESPACE_END
PRAGMA_POP_PLATFORM_DEFAULT_PACKING