// Jolt Physics Library
// Barrage Extension

#include "Experimental/CollisionGroupUnaware_FleshBroadPhase.h"
#include <concepts>


PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
JPH_NAMESPACE_BEGIN
	//shadow copy ops are a tick apart. as a result, you need to be not one, but two ticks behind to actually experience repercussions.
	//this means we _directly reference without locking_
	void CollisionGroupUnaware_FleshBroadPhase::CastRay(const RayCast& inRay, RayCastBodyCollector& ioCollector,
	                                                    const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                    const ObjectLayerFilter& inObjectLayerFilter,
	                                                    uint32_t AllowedHits) const
	{
		// Load ray
		Vec3 origin(inRay.mOrigin);
		RayInvDirection inv_direction(inRay.mDirection);
		thread_local std::vector<BodyID> Results;
		Results.clear();
		//highway to the dangerzonnnneeeeee
		//auto RayQueryResult = RTree->rayQuery(origin.mF32, origin.mF32, std::back_inserter(Results));
		// For all bodies
		float early_out_fraction = ioCollector.GetEarlyOutFraction();
		uint32_t hits = 0;
		ApproxCastRay(inRay, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter, AllowedHits);
	}

	bool CollisionGroupUnaware_FleshBroadPhase::ScoopUpHitsRay(RayCastBodyCollector& ioCollector,
	                                                           const ObjectLayerFilter& inObjectLayerFilter,
	                                                           uint32_t AllowedHits, Vec3 origin,
	                                                           RayInvDirection inv_direction,
	                                                           std::vector<uint32_t> tset, float early_out_fraction,
	                                                           uint32_t hits) const
	{
		size_t prevID = SIZE_T_MAX;
		//we actually get a ton of info from WHICH points in a BB's set are in our knn.
		//using it is a bit complicated, and I just want to see if this works at all first.
		//but if you're reading this, just be aware that there's a ton of optimizations available to us using a truthtable
		//right now, tset isn't even necessarily sorted from my grasp on the problem, not even really by distance due to the approximating
		//behavior. one interesting thing, though, is that this is a COVERING LSH so because we're pulling so many points, we will always get
		//at least some of the actual nearest neighbor as far as I can deduce.
		for (auto b : tset)
		{
			auto idx = b;
			if (idx == prevID)
			{
				//this will glide us past the other points from a bb's set that we hit
				//BEFORE we dig up the body id and grab the body.
				continue;
			}
			prevID = idx;
			//turn the idx into the body id by key_ref
			auto id = BodyID((*mBodies)[idx].meta);
			//TODO: check the api... I think this throws on a missed find...

			//get the original bb. doing it this way may mean that we can release the ds_ref? idk yet.
			auto& bodyshadow = (*mBodies)[idx]; //body shadows are both smaller and safer.

			// Test layer
			if (
				inObjectLayerFilter.ShouldCollide(bodyshadow.GetObjectLayer())
				&& hits < AllowedHits)
			{
				// Test intersection with ray
				const AABox& bounds = bodyshadow.GetWorldSpaceBounds();
				float fraction = RayAABox(origin, inv_direction, bounds.mMin, bounds.mMax);
				if (fraction < early_out_fraction)
				{
					// Store hit
					BroadPhaseCastResult result{id, fraction};
					ioCollector.AddHit(result);
					if (ioCollector.ShouldEarlyOut())
					{
						return true;
					} //we're don!
					early_out_fraction = ioCollector.GetEarlyOutFraction();
				}
			}
		}
		return false;
	}

	void CollisionGroupUnaware_FleshBroadPhase::ApproxCastRay(const RayCast& inRay, RayCastBodyCollector& ioCollector,
	                                                          const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                          const ObjectLayerFilter& inObjectLayerFilter,
	                                                          uint32_t AllowedHits) const
	{
		// Load ray
		RayInvDirection inv_direction(inRay.mDirection);

		//as you read this, bear in mind that we give even points a minimum volume. hbp only deals with positive volume convex solids represented as AABBs.
		thread_local std::vector<BodyID> Results;
		Results.clear();
		//highway to the dangerzonnnneeeeee
		//isn't this fun? it was easiest to fully decompose the damn thing instead of screwing with pack pragmas
		// and four different vec types.
		FLESHPoint Query = FLESHPoint::FromLine(
			inRay.mOrigin.GetX(), inRay.mOrigin.GetY(), inRay.mOrigin.GetZ(),
			inRay.mDirection.GetX(), inRay.mDirection.GetY(), inRay.mDirection.GetZ());

		// For all bodies
		float early_out_fraction = ioCollector.GetEarlyOutFraction();
		uint32_t hits = 0;
		EMBED::HashBlob& hashes = EMBED::getHash(Query, 1);
		std::vector<uint32_t> results = LSH->query(hashes, PointsPerBB * 15);

		ScoopUpHitsRay(ioCollector, inObjectLayerFilter, AllowedHits, inRay.mOrigin, inv_direction, results,
		               early_out_fraction,
		               hits);
	}

	void CollisionGroupUnaware_FleshBroadPhase::CollideAABox(const AABox& inBox, CollideShapeBodyCollector& ioCollector,
	                                                         const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                         const ObjectLayerFilter& inObjectLayerFilter) const
	{
		struct AABBCollideBehavior
		{
			CollideShapeBodyCollector& ioCollector;

			AABBCollideBehavior(CollideShapeBodyCollector& ioCollector): ioCollector(ioCollector)
			{
			}

			bool Acc(const AABox& inBox,
			         const ObjectLayerFilter& inObjectLayerFilter, float lenny,
			         [[maybe_unused]] UnorderedSet<uint32_t>& Setti,
			         CollisionGroupUnaware_FleshBroadPhase::BodyBoxSafeShadow& body,
			         [[maybe_unused]] uint32_t idx)
			{
				if (inBox.Intersect(body.GetWorldSpaceBounds()).IsValid())
				{
					if (inObjectLayerFilter.ShouldCollide(body.layer))
					{
						ioCollector.AddHit(body.meta);
						if (ioCollector.ShouldEarlyOut())
						{
							return true;
						}
					}
				}
				return false;
			}
		};
		auto Pin = mBodies;
		thread_local UnorderedSet<uint32_t> Setti;
		Setti.reserve(TOPK * 3);
		FLESHPoint Batch[PointsPerBB];
		//we do this SO much less than I expected.
		auto a = inBox.GetCenter();
		//center
		Batch[0] = FLESHPoint::FromPoint(a.GetX(), a.GetY(), a.GetZ(), MaxExtent);
		//min\max line
		Batch[1] = FLESHPoint::embedLineL1((inBox.mMin.GetX()), (inBox.mMin.GetY()), (inBox.mMin.GetZ()),
		                                inBox.mMax.GetX(), inBox.mMax.GetY(), inBox.mMax.GetZ());
		//offset
		auto QHashes = LSH->get2Hashes(Batch, 1);
		auto results = LSH->query(QHashes.data(), PointsPerBB, TOPK);
		Accumulate<AABBCollideBehavior, AABox>(inBox, ioCollector, inObjectLayerFilter, 0, Setti, results);
		Setti.ClearAndKeepMemory();
	}

	void CollisionGroupUnaware_FleshBroadPhase::CollideSphere(Vec3Arg inCenter, float inRadius,
	                                                          CollideShapeBodyCollector& ioCollector,
	                                                          const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                          const ObjectLayerFilter& inObjectLayerFilter) const
	{
		struct SphereCollideBehavior
		{
			CollideShapeBodyCollector& ioCollector;

			SphereCollideBehavior(CollideShapeBodyCollector& ioCollector): ioCollector(ioCollector)
			{
			}

			bool Acc(const Vec3& InCent,
			         const ObjectLayerFilter& inObjectLayerFilter, float lenny,
			         [[maybe_unused]] UnorderedSet<uint32_t>& Setti,
			         CollisionGroupUnaware_FleshBroadPhase::BodyBoxSafeShadow& body,
			         [[maybe_unused]] uint32_t idx)
			{
				if (body.GetWorldSpaceBounds().GetSqDistanceTo(InCent) <= lenny * lenny)
				{
					if (inObjectLayerFilter.ShouldCollide(body.layer))
					{
						ioCollector.AddHit(body.meta);
						if (ioCollector.ShouldEarlyOut())
						{
							return true;
						}
					}
				}
				return false;
			}
		};
		auto Pin = mBodies;
		thread_local UnorderedSet<uint32_t> Setti;
		Setti.reserve(3 * TOPK);
		FLESHPoint A = FLESHPoint::FromPoint(inCenter.GetX(), inCenter.GetY(), inCenter.GetZ(), MaxExtent);
		auto result = LSH->query(EMBED::getHash(A, 1), 100);
		Accumulate<SphereCollideBehavior, Vec3>(inCenter, ioCollector, inObjectLayerFilter, inRadius, Setti, result);
		Setti.ClearAndKeepMemory();
	}

	void CollisionGroupUnaware_FleshBroadPhase::CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector& ioCollector,
	                                                         const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                         const ObjectLayerFilter& inObjectLayerFilter) const
	{
		//points are just very small spheres. we don't do anything smaller than this, because we already voxelize.
		return CollideSphere(inPoint, 1, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter);
	}

	//unused and unsupported. beepboop
	void CollisionGroupUnaware_FleshBroadPhase::CollideOrientedBox(const OrientedBox& inBox,
	                                                               CollideShapeBodyCollector& ioCollector,
	                                                               const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                               const ObjectLayerFilter& inObjectLayerFilter) const
	{
		throw;
	}


	template <typename T>
	concept Accumulator = requires(T a) { a.Acc(); };

	//accumulator behaviors effectively determine when to stop seeking results or running. The accumulate function
	//is basically just a higher order function that applies the behavior in a specific way. I just happen to be most comfy with templates in this context.
	template <typename behavior, typename T>
	bool CollisionGroupUnaware_FleshBroadPhase::Accumulate(const T& inBox, behavior collector,
	                                                       const ObjectLayerFilter& inObjectLayerFilter, float lenny,
	                                                       UnorderedSet<uint32_t>& Setti,
	                                                       std::vector<uint32_t>& result) const
	{
		if (!result.empty() && mBodies && !mBodies->empty())
		{
			for (auto idx : result)
			{
				if (Setti.insert(idx).second)
				{
					auto& body = (*mBodies)[idx];

					if ((collector.Acc(inBox, inObjectLayerFilter, lenny, Setti, body, idx)))
					{
						return true;
					}
				}
			}
		}
		return false;
	}


	void CollisionGroupUnaware_FleshBroadPhase::CastAABoxNoLock(const AABoxCast& inBox,
	                                                            CastShapeBodyCollector& ioCollector,
	                                                            const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                            const ObjectLayerFilter& inObjectLayerFilter) const
	{
		// Load box
		struct AABBCastBehavior
		{
			CastShapeBodyCollector& ioCollector;

			AABBCastBehavior(CastShapeBodyCollector& ioCollector): ioCollector(ioCollector)
			{
			}

			bool Acc(const AABoxCast& inBox,
			         const ObjectLayerFilter& inObjectLayerFilter, float lenny,
			         UnorderedSet<uint32_t>& Setti, CollisionGroupUnaware_FleshBroadPhase::BodyBoxSafeShadow& body,
			         [[maybe_unused]] uint32_t idx)
			{
				auto closest = inBox.mBox.GetClosestPoint(body.GetCenter());
				auto k = RayAABox(closest, RayInvDirection(inBox.mDirection), {__LOCO_FFABV(body)},
				                  {__LOCO_FFABVM(body)});
				bool hit = lenny / k < 1.0;
				if (hit)
				{
					if (inObjectLayerFilter.ShouldCollide(body.layer))
					{
						ioCollector.AddHit(BroadPhaseCastResult(body.meta, hit));
						if (ioCollector.ShouldEarlyOut())
						{
							Setti.ClearAndKeepMemory();
							return true;
						}
					}
				}
				return false;
			}
		};

		Vec3 origin(inBox.mBox.GetCenter());
		Vec3 mini(inBox.mBox.mMin);
		Vec3 maxi(inBox.mBox.mMax);
		Vec3 extent(inBox.mBox.GetExtent());
		float lenny = extent.Length();
		float early_out_fraction = ioCollector.GetPositiveEarlyOutFraction();
		auto Pin = mBodies;
		thread_local UnorderedSet<uint32_t> Setti;
		Setti.ClearAndKeepMemory();
		Setti.reserve(3 * TOPK);
		thread_local std::vector<uint32_t> result(3 * TOPK);
		result.clear();
		result.reserve(3 * TOPK);
		if (Pin)
		{
			std::vector<FLESHPoint> Aleph(3);
			Aleph[0] = FLESHPoint::FromLine(origin.GetX(), origin.GetY(), origin.GetZ(), extent.GetX(), extent.GetY(),
			                                extent.GetZ());
			Aleph[1] = FLESHPoint::FromLine(mini.GetX(), mini.GetY(), mini.GetZ(), extent.GetX(), extent.GetY(),
			                                extent.GetZ());
			Aleph[2] = FLESHPoint::FromLine(maxi.GetX(), maxi.GetY(), maxi.GetZ(), extent.GetX(), extent.GetY(),
			                                extent.GetZ());
			auto hashstack = LSH->getHashes(Aleph, 1);
			uint32_t unused = 0;
			LSH->query_byoa(hashstack.data(), 3, TOPK, result);
			Accumulate<AABBCastBehavior>(inBox, ioCollector, inObjectLayerFilter, lenny, Setti, result);
		}
	}

	void CollisionGroupUnaware_FleshBroadPhase::FindCollidingPairs(BodyID* ioActiveBodies, int inNumActiveBodies,
	                                                               float inSpeculativeContactDistance,
	                                                               const ObjectVsBroadPhaseLayerFilter&
	                                                               inObjectVsBroadPhaseLayerFilter,
	                                                               const ObjectLayerPairFilter& inObjectLayerPairFilter,
	                                                               BodyPairCollector& ioPairCollector) const
	{
		struct CollidingPairsBehavior
		{
			BodyPairCollector& Collector;
			BodyBoxSafeShadow Current;
			BodyManager* mBM;
			EMBED::HashBlob& hash;
			std::shared_ptr<NodeBlob> mCachedBodiesRef;
			float SpeculativeContactDistance;
			const ObjectLayerPairFilter& mObjectLayerPairFilter;

			CollidingPairsBehavior(BodyPairCollector& ioCollector, BodyManager* BM, std::shared_ptr<NodeBlob> Shadowed,
			                       const ObjectLayerPairFilter& inOF, EMBED::HashBlob& Target,
			                       float inSpeculativeContactDistance)
				: Collector(ioCollector), mBM(BM), hash(Target),
				  mCachedBodiesRef(Shadowed), SpeculativeContactDistance(inSpeculativeContactDistance), mObjectLayerPairFilter(inOF)
			{
			}

			bool AttemptAdd(uint32_t IdxToAdd)
			{
				//std::cerr << IdxToAdd << std::endl;
				BodyBoxSafeShadow hit = (*mCachedBodiesRef)[IdxToAdd]; //COPY NOW.
				//std::cerr << hit.GetIndexAndSequenceNumber() << ":" << (*mCachedBodiesRef)[IdxToAdd].GetCenter() << " & "<< Current.meta.GetIndexAndSequenceNumber() << ":" << Current.meta.IsInvalid() << std::endl;


				//std::cerr << body2.IsDynamic() << std::endl;
				if (!Current.sFindCollidingPairsCanCollide(hit))
					return false;

				// Check if layers can collide
				const ObjectLayer layer1 = Current.GetObjectLayer();
				if (!mObjectLayerPairFilter.ShouldCollide(layer1, hit.GetObjectLayer()))
					return false;

				// Check if bounds overlap
				AABox bounds1 = Current.GetWorldSpaceBounds(); // we expanded earlier!
				const AABox& bounds2 = hit.GetWorldSpaceBounds();
				if (!bounds1.Intersect(bounds2).IsValid()) //they use overlaps. should it be intersects for us?
					return false;

				// Store overlapping pair
				BodyPair Input(hit.meta, Current.meta);
				Collector.AddHit(Input);
				return true;
			}
		};

		auto Pin = mBodies;

		if (Pin && Pin.get() && !Pin.get()->empty())
		{
			for (int b1 = 0; b1 < inNumActiveBodies; ++b1)
			{
				//this is VERY slow now. and badly needs redone.
				BodyID b1_id = ioActiveBodies[b1];
				const Body& body1 = mBodyManager->GetBody(b1_id);
				auto bounds1 = body1.GetWorldSpaceBounds();
				auto Cent = bounds1.GetCenter();
				auto Extt = bounds1.GetExtent();
				bounds1.ExpandBy(Vec3::sReplicate(SpeculativeContactDistance));
				BodyBoxSafeShadow BodyShadowStruct(Cent.GetX(), Cent.GetY(), Cent.GetZ(),
				                                   Extt.GetX(), Extt.GetY(), Extt.GetZ(),
				                                   body1.GetObjectLayer(),
				                                   body1.IsKinematic(),
				                                   body1.IsDynamic(),
				                                   body1.IsSensor(),
				                                   body1.IsRigidBody(),
				                                   body1.IsStatic(),
				                                   body1.IsActive(),
				                                   body1.GetCollideKinematicVsNonDynamic(),
				                                   b1_id);

				FLESHPoint Query = FLESHPoint::FromPoint(
					BodyShadowStruct.x, BodyShadowStruct.y, BodyShadowStruct.z, MaxExtent);
				auto q = EMBED::getHash(Query, 1);

				CollidingPairsBehavior HandleIOCollection(ioPairCollector, mBodyManager, Pin, inObjectLayerPairFilter,
				                                          q, inSpeculativeContactDistance);
				HandleIOCollection.Current = BodyShadowStruct;
				LSH->query_byob(&q, 1, TOPK, HandleIOCollection);
			}
		}
	}

	void CollisionGroupUnaware_FleshBroadPhase::TestInvokeBuild()
	{
		//NO-OP for the time being!!!
		//UpdateFinalize(UpdateState());
	}

	//FIt-SNE may be of eventual interest but I'm _really_ hoping not. That said, they do something positively
	//fascinating with the fast fourier that may actually point the way to a fast option for lightweight
	//dim reduction
	void CollisionGroupUnaware_FleshBroadPhase::UpdateFinalize(const UpdateState& inUpdateState)
	{
		mBodiesShadow->clear();
		if (mBodyManager)
		{
			//this line blows up, which probably points a little towards what's causing our various lock problems.
			//it's some lock in body manager.
			//mBodiesShadow->reserve(mBodyManager->GetNumBodies());
			for (auto body : mBodyManager->GetBodies())
			{
				if (mBodyManager->sIsValidBodyPointer(body))
				{
					if (JPH::BodyID M(body->GetID()); !M.IsInvalid())
					{
						//build mbodies here
						// we expand the bounding box by the speculative contact distance HERE, not during cast or collide.
						AABox bounds1 = body->GetWorldSpaceBounds();
						bounds1.ExpandBy(Vec3::sReplicate(SpeculativeContactDistance));
						auto Cent = bounds1.GetCenter();
						auto Extt = bounds1.GetExtent();
						BodyBoxSafeShadow J(Cent.GetX(), Cent.GetY(), Cent.GetZ(),
						                    Extt.GetX() + 1, Extt.GetY() + 1, Extt.GetZ() + 1,
						                    body->GetObjectLayer(),
						                    body->IsKinematic(),
						                    body->IsDynamic(),
						                    body->IsSensor(),
						                    body->IsRigidBody(),
						                    body->IsStatic(),
						                    body->IsActive(),
						                    body->GetCollideKinematicVsNonDynamic(),
						                    M);
						//std::cerr << M.GetIndexAndSequenceNumber() << ":" << M.IsInvalid() << " & "<< J.meta.GetIndexAndSequenceNumber() << ":" << J.meta.IsInvalid() << std::endl;

						mBodiesShadow->push_back(J);
					}
				}
			}
		}
		mBodies.swap(mBodiesShadow);
		//build and swap rtree here
		RebuildLSH();
	}

	//without the RTree, bounds will need to be accumulated for maximum speed.
	//There's not a great way to recover bounds info from the LSH, and we need it anyway
	//for the T-constant used in the point embedding
	AABox CollisionGroupUnaware_FleshBroadPhase::GetBounds() const
	{
		AABox bounds({0, 0, 0}, MaxExtent);
		return bounds;
	}

	void CollisionGroupUnaware_FleshBroadPhase::GenerateBBHashes(float oldM, FLESHPoint* Batch,
	                                                             std::vector<
		                                                             CollisionGroupUnaware_FleshBroadPhase::BodyBoxSafeShadow>::value_type
	                                                             & body)
	{
		//this code appears twice and we really need to converge it but that turns out to be slightly more annoying than you could reasonably expect.
		//center
		//do not under any circumstance even if you think you are very clever insert lines using the FromLine function.
		//Lines are INTENDED to be highly collisive. This will do EXACTLY what you might expect.
		MaxExtent = std::max(abs(MaxExtent), 1.0);
		Batch[0] = FLESHPoint::FromPoint(__LOCO_FFABVC(body), MaxExtent);
		Batch[1] = FLESHPoint::embedLineL1(__LOCO_FFABVM(body),__LOCO_FFABV(body));
		//offset
	}

	void CollisionGroupUnaware_FleshBroadPhase::RebuildLSH()
	{
		//////////////////////////
		///UNDER CONSTRUCTION
		////////
		///it really looks like we can use the index mappings behavior of FLINNG to avoid ever
		/// actually storing our embeddings. That'd be EXTREMELY dope, bluntly.

		float oldM = MaxExtent;
		MaxExtent = 0;
		LSHShadow->RegenerateTheFLESH(mBodies->size() * PointsPerBB);
		//if an ordering is applied to mBodies, you'll get a deterministic build of the LSH.
		FLESHPoint Batch[PointsPerBB];
		for (auto& body : *mBodies)
		{
			auto id = body.meta;
			//you might notice that we push the center in first! that means it's always the lowest idx of a bb's point set.
			//that's semantically important. please don't "fix" this.

			//////////////////////////////////////////////////////////////
			///WARNING: SUBTLE AND EVIL
			//PointsPerBB must be adjusted as this code changes. failure to do so will cause funny but devastating bugs.
			//I believe we can reduce the number of datastructures here significantly as we transisition away from CLSH
			//////////////////////////////////////////////////////////////
			//auto ctr
			auto c1Extent = abs(FVector::Distance(FVector::ZeroVector, {__LOCO_FFABVM(body)}));
			auto c2Extent = abs(FVector::Distance(FVector::ZeroVector, {__LOCO_FFABVM(body)}));
			MaxExtent = ceil(FMath::Max3(MaxExtent, c1Extent, c2Extent));
			GenerateBBHashes(oldM, Batch, body);
			auto A = LSHShadow->get2Hashes(Batch, 0x1bead).data();
			LSHShadow->addFoldedHashedPoints(A, PointsPerBB, 0x1bead);
		}
		//LSHShadow->BuildTableAndIndex();
		ArenaBack.swap(ArenaFront);
		LSH.swap(LSHShadow);
	}

	void CollisionGroupUnaware_FleshBroadPhase::UnlockModifications()
	{
		//less sure than I was.
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// Currently Uninteresting Functions below this note
	//
	// I suspect we'll end up making add and remove operational using the per-thread spsc queue model
	// that we use elsewhere in barrage. At the moment, I'm going to try to defer that, because I'd _really_ like to use the BGI
	// bulk construct for the RTree and LSH update algorithms aren't well considered for cLSH. this means we gain no benefit from incremental updates
	// but that was already broadly true due to the shadow copy strategy. I think there's _actually_ a way past that, using fast mmcpy "off update"
	//
	// now, we could queue everything, mash it down every time we do the stuff, but I've checked and update finalize has
	// the body manager locked in all cases (assuming we add some functionality in lock\unlock
	// and we only need a NARROW window to generate our shadow copies.
	////////////////////////////////////////////////////////////////////////////////////////////////
	///

#define BORING_FUNCTION_REGION_OF_HBPH true
#ifdef BORING_FUNCTION_REGION_OF_HBPH
#pragma region BORON_ZONE


	void CollisionGroupUnaware_FleshBroadPhase::Init(JPH::BodyManager* inBodyManager,
	                                                 const JPH::BroadPhaseLayerInterface& inLayerInterface)
	{
		BroadPhase::Init(inBodyManager, inLayerInterface);
	}


	CollisionGroupUnaware_FleshBroadPhase::~CollisionGroupUnaware_FleshBroadPhase()
	{
	}

	void CollisionGroupUnaware_FleshBroadPhase::Optimize()
	{
		// this maybe should generate an out-of-band update for the shadowtree
	}

	void CollisionGroupUnaware_FleshBroadPhase::FrameSync()
	{
		BroadPhase::FrameSync();
	}

	void CollisionGroupUnaware_FleshBroadPhase::LockModifications()
	{
		//BroadPhase::LockModifications();
		//this is a no-op for us, oddly.
	}

	BroadPhase::UpdateState CollisionGroupUnaware_FleshBroadPhase::UpdatePrepare()
	{
		return BroadPhase::UpdatePrepare();
	}


	CollisionGroupUnaware_FleshBroadPhase::AddState CollisionGroupUnaware_FleshBroadPhase::AddBodiesPrepare(
		JPH::BodyID* ioBodies, int inNumber)
	{
		return BroadPhase::AddBodiesPrepare(ioBodies, inNumber);
	}

	void CollisionGroupUnaware_FleshBroadPhase::AddBodiesFinalize(BodyID* ioBodies, int inNumber,
	                                                              CollisionGroupUnaware_FleshBroadPhase::AddState
	                                                              inAddState)
	{
		// Add bodies
		BodyVector& bodies = mBodyManager->GetBodies();
		for (const BodyID *b = ioBodies, *b_end = ioBodies + inNumber; b < b_end; ++b)
		{
			Body& body = *bodies[b->GetIndex()];

			// Validate that body ID is consistent with array index
			JPH_ASSERT(body.GetID() == *b);
			JPH_ASSERT(!body.IsInBroadPhase());

			// Indicate body is in the broadphase
			body.SetInBroadPhaseInternal(true);
		}
	}

	void CollisionGroupUnaware_FleshBroadPhase::AddBodiesAbort(JPH::BodyID* ioBodies, int inNumber, AddState inAddState)
	{
		//no-op. until finalize happens, nothing happens with this model.
	}

	void CollisionGroupUnaware_FleshBroadPhase::RemoveBodies(BodyID* ioBodies, int inNumber)
	{
		BodyVector& bodies = mBodyManager->GetBodies();

		JPH_ASSERT(bodies.size() >= inNumber);

		// Remove bodies
		for (const BodyID *b = ioBodies, *b_end = ioBodies + inNumber; b < b_end; ++b)
		{
			Body& body = *bodies[b->GetIndex()];

			// Indicate body is no longer in the broadphase
			body.SetInBroadPhaseInternal(false);
		}
	}

	void CollisionGroupUnaware_FleshBroadPhase::NotifyBodiesAABBChanged(BodyID* ioBodies, int inNumber, bool inTakeLock)
	{
		// Do nothing, we directly reference the body
	}

	void CollisionGroupUnaware_FleshBroadPhase::NotifyBodiesLayerChanged(BodyID* ioBodies, int inNumber)
	{
		// Do nothing, we directly reference the body
	}

	void CollisionGroupUnaware_FleshBroadPhase::CastRay(const RayCast& inRay, RayCastBodyCollector& ioCollector,
	                                                    const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                    const ObjectLayerFilter& inObjectLayerFilter) const
	{
		CastRay(inRay, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter, DefaultMaxAllowedHitsPerCast);
	}

	void CollisionGroupUnaware_FleshBroadPhase::CastAABox(const AABoxCast& inBox, CastShapeBodyCollector& ioCollector,
	                                                      const BroadPhaseLayerFilter& inBroadPhaseLayerFilter,
	                                                      const ObjectLayerFilter& inObjectLayerFilter) const
	{
		CastAABoxNoLock(inBox, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter);
	}
#endif
#pragma endregion
#undef BORING_FUNCTION_REGION_OF_HBPH
JPH_NAMESPACE_END

PRAGMA_POP_PLATFORM_DEFAULT_PACKING
