# MegafunkUtils Plugin
A collection of useful code from my own projects. Currently mostly performance helpers intended for more experienced Unreal users.

- Fully parallel skeletal mesh component ticking (with caveats)
- Examples of small ways to make regular actor components faster in a similar manner

Currently for version 5.7 but it should work for previous versions without major changes. 

## Parallel Skeletal Mesh Component Ticking

A longstanding issue with this engine is how slow it is to have more than a handful of actively animated meshes in the scene.
Unreal has many features and tools to make things tick less or divert steps of the process to another thread but these never really eliminate the main bottleneck of needing mainthread work.
This plugin features an example of how to tick skeletal mesh components nearly entirely in parallel with the majority of useful features intact.
Note that this is not the same as vertex animation textures. This is actually using the real animation graph with almost all features intact and does not require some kind of baking step or special tooling.
Texture-driven animation is a great idea if it fits your project but I personally did not want to deal with the limitations and extra tooling it generally imposes.

As stated earlier this is for people who are familiar with CPU profiling tools and have an understanding of the actual work their project performs every frame on the gamethread side.

The intention here is to not to give a generic component that fits your project, but to give examples and functions to help use what you actually need and want to reason about.
Using the example alone without understanding what is being left out and how things work before could cause subtle issues that will be tedious to track down.

### Skeletal Mesh Component ticking the normal way

It must be stressed this is intended to make the gamethread CPU-side parts of this faster, so we are describing those alone.

Note: that this is a "generic" case and there are many many different codepaths that are not super out of the ordinary. 
For example sometimes the evaluation can happen on the gamethread when initializing or manually refreshing bone poses. (Which is expensive but sometimes helpful)
Cvars can also influence which steps happen on which thread, but I'm going to assume defaults here.

A rough general overview of a single skeletal mesh component's work done in a frame:

1. Gamethread Pre-evaulation (TickComponent)
   - Mainthread callbacks for user C++ and bp
   - TickPose -> TickAnimation
     - Recalculates required bones/curves if needed
     - main threaded anim instance update TickAnimInstances
       - call UpdateAnimation on all anim instances (including post process and linked anim instances)
         - Cache gamethread data and clear out the old info (PreUpdateAnimation)
         - Update montage state
         - Regular user NativeUpdateAnimation/BlueprintUpdateAnimation callbacks
         
   - DispatchParallelTickPose  
     - Worker/gamethread swap chains are flipped (SwapEvaluationContextBuffers)
     - Launches a secondary tick function on a task thread

2. Worker thread evaluation (FParallelAnimationEvaluationTask)
    - Update the anim instance in parallel (AnimInstance::ParallelUpdateAnimation -> FAnimInstanceProxy::UpdateAnimation)
      - FAnimInstanceProxy::UpdateAnimation_WithRoot
        - Thread-safe callbacks for user  C++ and bp (NativeThreadSafeUpdateAnimation)
        - Updates the root animation node
    - Evaluates the anim graph and post process anim graph (The majority of the work we care about) (UAnimInstance::ParallelEvaluateAnimation)
    - Do some math on the final evaluated pose to conver missing parts and normalize the rotations (FinalizePoseEvaluationResult)
    - Convert bone-space transforms to component-space transforms (FillComponentSpaceTransforms)
    - Copy and interpolate data conditionally (ParallelDuplicateAndInterpolate)
    - (Writes are generally performed to separate containers from the ones available on the gamethread)

3. Gamethread Post-evaluation callbacks (FParallelAnimationCompletionTask)
   - Worker/gamethread swap chains are flipped (SwapEvaluationContextBuffers)
     - The worker thread mostly touches AnimEvaluationContext, and the gamethread stores the results in their own member variables
       - See GetEditableComponentSpaceTransforms etc, this is why it's safe to access bone transforms on the main thread
   - Accumulate rootmotion and other values
   - Write chaos collision bodies with the new bone transforms (UpdateKinematicBonesToAnim)
     - This is very expensive and you can alternatively update this once a frame in bDeferKinematicBoneUpdate to reduce fighting over chaos locks
   - FinalizeAnimationUpdate
     - FinalizeBoneTransform
       - Set the gamethread's editable bone transform arrays to use the newly finished ones  (FinalizeBoneTransform , FlipEditableSpaceBases etc)
       - Callbacks for user code for things like anim notifies and montages delegates (ConditionallyDispatchQueuedAnimEvents)
     - Update child transforms for objects attached to sockets
     - Invalidate + Update component bounds
     - Mark render transform and dynamic data dirty (MarkRenderTransformDirty, MarkRenderDynamicDataDirty)

4. Post-tick Render Update (Game thread in a parallel for, depending on cvars and build)
    - Because MarkRenderTransformDirty was called earlier the component added itself to UWorld::ComponentsThatNeedEndOfFrameUpdate
    - After ticking is over, UWorld::SendAllEndOfFrameUpdates tries to update all primitives with 
      - Calls DoDeferredRenderUpdates_Concurrent:
      - Recreates or updates render transform/dynamic data/etc depending on dirtiness  (RecreateRenderState_Concurrent,SendRenderTransform_Concurrent)


To summarize:
1. Gather gamethread data, flip buffers, and launch the parallel task
2. Do all animation graph work on a task thread to self-contained anim eval data
3. Collect finished data on the gamethread, flip buffers, trigger callbacks, update bounds+dirty render
4. Post-tick bounds update + handing off to the render thread 

A lot is left out here. For example a decent portion of the work involved to show each skeletal mesh occurs in render graph tasks and on the gpu of course but we are mainly concerned with gamethread stuff here.

### Example parallell tick

This plugin provides an example skeletal mesh component that ticks, evaluates, and updates rendering in parallel in a parallel for from a ticking world subsystem. 

Typically the main challenge with modifying singlethreaded code to run in parallel is that most of it assumes it won't do that... For example:
- Writing external data that others could be also writing or reading
- Reading external data that can change and race
- Assertions and checks to try to prevent users from accidentally doing anything unintended off the gamethread
- External messages/delegates etc broadcasting that could do any of the above

This example does the "important" parts with a new set of functions that change the code to not require running on the gamethread: 
- All functions that call GetProxyOnGameThread are replaced entirely with a proxy obtained directly
- All functions with delegate broadcasts that external code can listen to are removed or enqueued for calls on the main thread
  - For example anim notifies, anim states, and montage delegates are queu'd up for a main thread callback
- Writes to external unsafe containers that would just enqueue work for later are performed inline instead
- Flipping buffers mostly is pointless as it's all happening in one place. (You could re-introduce this if it's useful for some reason)
  - We still do some flipping for the render steps but that's mostly just because that's what unreal expects. This will be looked at later

  
#### UMegafunkUtilsExampleAnimSubsystem

`UMegafunkUtilsExampleAnimSubsystem` has an array of skeletal mesh components with some state.
Every frame, the FMegafunkUtilsManagerTick::ExecuteTick tries to tick all registered components
1. Gamethread: 
   - Optional: register collision enabled components for deferred chaos update
   - Editor-only check for still-compiling components
2. Parallel For tasks:
   - Set a task tag to mark this is a EParallelGameThread to help with some internal checks
   - Floating point semantics settings and blueprint graph internals   
   - Update the skeletal mesh component entirely in one step
     - Do all normally gamethread-only updates and anim instance + montage ticking
     - Call USkeletalMeshComponent::PerformAnimationProcessing which is the actual work we care about from the anim graph (blending, sequence sampling etc)
     - Call AnimInstance_PostUpdateAnimationAnyThread to manage some miscellaneous state and call FAnimInstanceProxy::PostUpdate
     - Manually set the component's GetEditableComponentSpaceTransforms and BoneSpaceTransforms to the finished bone transforms 
     - Call SkeletalMeshComponent_ForceFinalizeBoneTransform directly to flip the bone swapchain and mark them as a valid transform
     - Call SkeletalMeshComponent_PushRenderUpdate which updates bounds and pushes render tasks to the render thread
       - This step is arguably wasteful if you don't have anything to update
       - This is why we don't need a main thread step for rendering. This does the same thing marking for render dirty does except right now 
       - This could be done later if you need to update the transform or other rendered state later in the frame
     - Finally, clear the evaluation context so it doesn't have stale pointers
   - Optional: During the parallel task we can also queue up main thread callbacks for anim notifies + montage delegates

3. Gamethread:
   - Optional: Dequeue anim notifies + montage delegates
     - Anin Notifies are not thread safe by default. 
       - This is optional and mostly an example of how you could keep some things done on the main thread sparodically

   
The creation/initialziation of the anim instance and skeletal mesh is done the normal way currently, but live ticking moves to be managed from 


### How should I use this and what's left out? (Caveats and potential future work)

The default skeletal mesh is intended to fulfill a wide variety of usecases that involves dozens of dozens of permutations of what to do or not to do in a given frame.
This code was for a very specific usecase and is not intended to replace the skeletal mesh component for everyone.
For example:`USkeletalMeshComponent::RefreshBoneTransforms` shows a decent overview of the sheer number of codepaths each tick must consider for what work to do.
It is not possible for me to make how you use this entirely thread-safe if you decide to try to run these however you want. 
The example will assume your anim graph or anim instance aren't randomly reading or writing outside state and racing against other threads.

I would generally recommend using this for more "simple" meshes that don't use the full set of animation features as they will be more likely to work well and not introduce confusing new behaviour from missing things.



The function `MegafunkUtils::Anim::ValidateAnimInstanceBeingSafeToUseInParallel`will try to catch anim bps/ anim instances that have unsupported features in them and log them.


- [ ] TODO: support VisibilityBasedAnimTickOption or something similar  
  - Currently the example just evaluates and ticks poses EVERY FRAME for all of the managed components
  - For my project this was a requirement so I just made it cheap enough to not need to care.
  - This is probably the first thing I want to make nicer even if it's just a simple example. 
  - What this means in practice is just respecting ShouldOnlyTickMontages and updating montage state, then calling ConditionallyDispatchQueuedAnimEvents

- [ ] TODO: Eliminate the initialization and deinit steps 
  - Initialization of both the anim instance and the owning skeletal mesh are still 100% main threaded.
  - For the actor component this will be difficult to omit entirely but the anim instance itself

- [ ] TODO: Curves and attributes are untested

- [ ] TODO: Remove the need for a skeletal mesh component at all and manually manage and init the anim instance
    - This would help if your project doesn't use the unreal gameplay framework or just the cost of that is prohibitive
    - Initializing the anim instance does not seem complicated but rendering a skeletal mesh without a component will take some work

- [ ] TODO: Support attached components (Even in a simple way)
  - In my own project I just run my own transform hierarchy logic but this seems like a simple thing to throw in
  - Without collisions to update it's possible to do this during the parallel step I suppose

- Physics collision (physics asset hitboxes etc)
  - Currently this is supported in the example but it should be mentioned not needing to update physics asset kinematic bodies saves a lot of time
  - It should be  

- Montage delegates and Anim Notifies/States are "good enough" and I doubt I caught every single edge case
  - Currently I tested starting states and montage delegates. I could do some more throughal testing involving trying to mimic results 1:1 with the existing timings  
  - I anticipate I have missed something there considering how many weird patterns these can execute in
  - Default anim notifies are soemwhat untrustworthy as they can choose to not fire for a variety of reasons so it would be interesting to replace them with something more consistent 
    that also supports the concept of running on another thread
  - I would argue some anim notifies are simple enough they could be easily ran entirely in parallel and some will be too inter-connected to get away with doing in parallel

- Linked anim instances and post process anim instances
  - Secondary instances are simple in some steps (Most are literally just a for loop that calls the same thing on them at the time of the owner) 
  - I do not currently use these in my project but if there is demand I could make a token attempt at bringing them in the example flow.

- OnBoneTransformsFinalizedMC delegates are skipped. This would be trivial to add back in but I don't personally see the point yet.
  - If needed it could be done as a queued mainthread step afterwards similar to how anim notifies are handled now

- Replicated CMC root motion resolution
  - ACharacter CMC skeletal meshes when ran in a client-predicted server-authoratative way are a special case and they are partially manually ticked by the engine.
  - This is so that they can extract root motion data to figure out where the mesh should be
  - Because of that if you use root motion with a typical ACharacter driven multiplayer project I doubt my example setup here will work.
  - I do not need this usecase but I assume it would be somewhat simple to implement if you follow how the typical flow choose when to tick and what steps to skip.

- Cloth sim is untested
- Morphtarget curves are not handled and I don't think I am sure when those are even used

### Misc notes and other tidbits
- Unreal has been working to replace the current anim graph with their new [Unreal Animation Framework](https://dev.epicgames.com/community/learning/knowledge-base/nWWx/unreal-engine-unreal-animation-framework-uaf-faq)
  - One of the main features is that it will work all on a worker thread similar to this example,
    so it will probably be a better option once it is ready and more battle-tested. 
- I am far from the first person to figure out to make animation ticking run in parallel but I am unaware of other examples that go into detail like this.
- This was originally done for my own internal personal project and was made open source to be a part of [Artillery](https://github.com/OversizedSunCoreDev/ArtilleryEco).
  This is mostly to explain why only certain things are supported in here. The reason is simply... We did not need them yet.
  - My project and artillery ignore many default unreal features and they both run at a fixed tickrate outside of normal ticking
  - In my own personal project I do interpolation in a secondary step in-between two complete fixed tick evaluations



## Parallel Component Movement