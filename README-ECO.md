# The Artillery plugin set (a.k.a. "Eco")

This `Plugins/` tree is the shared technology layer for games built on
Artillery — physics, spatial indexing, dispatch, input, rendering helpers,
and the supporting cast. The same tree is published publicly as
[ArtilleryEco](https://github.com/OversizedSunCoreDev/ArtilleryEco).

**Upstream truth lives in the game.** Work happens here, in Bristle54, and
the public repo is a mirror we publish to — not the other way around. The
boundary is managed by `tools/eco-sync` (see its README): a manifest at
`Plugins/ECO-MANIFEST.json` records which public commit this tree currently
mirrors, and every crossing is a 3-way comparison so nothing gets eaten in
either direction.

## For game developers (this repo)

- Add and edit things here as normal. Nothing about your workflow changes.
- `python tools\eco-sync\eco_sync.py status` shows what has drifted where.
- Publishing out is a deliberate act: `eco_sync.py outbound --apply`, then
  review/commit in the public checkout.
- Community commits come in with `eco_sync.py inbound <ref> --apply` and
  land as ordinary game commits for review.

## For modders and other Artillery games (public mirror)

The public mirror is where the plugin set's documentation and shared
modding conventions live. If you are building on Artillery, this tree is
the SDK: take the plugins you need, keep the layout, and your game is an
artillery game.

### How you add an asset to an artillery game

_(The seed of the shared modding guide — conventions land here as they are
established. Watch this file.)_

- **Code** (new systems, physics types, abilities): add source to the
  appropriate plugin under `Source/`, following the existing module
  structure (public API in `Public/`, implementation in `Private/`).
  Ship it back as a pull request on ArtilleryEco if it is generally useful.
- **Content** (assets, data tables, configs): lives under each plugin's
  `Content/` and `Config/`. (Detailed conventions to be written — this is
  the "small set of shared core modding tools" we intend to grow.)

## Layout

Eleven plugins: `Artillery` (guns, dispatch, core runtime), `Barrage` and
`BarrageTests` (physics, Jolt integration), `Bristlecone` (input),
`Cabling`, `ImGui`, `LocomoCore` (movement + spatial indexing — home of
the tesseral tree), `MegafunkUtils`, `NiagaraUIRenderer`, `SkeletonKey`,
`sunflower`, `Thistle`. Each is a normal Unreal plugin; the game is just
their first consumer.
