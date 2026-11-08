# 06 · Engine layout: the OG equipped-items lock

## Symptom

The OG build hung the game. Two F4SE tasks take the same engine lock with `RE::BSAutoLock` and never return:

- `ReadEquipped` (`src/Live.cpp`, equipped weapon and ammo read)
- `ReadGear` (`src/Live.cpp`, worn armour read)

Both lock `middleHigh->equippedItemsLock`. A live hang dump of the shipped OG 1.10.163 DLL showed the spin inside `BSSpinLock::Lock`, on the high dword of a heap pointer.

## Mechanism

The OG build compiles against the VR-configured CommonLibF4 headers. CI sets `COMMONLIBF4_PATH` to `commonlib/VR` (G-A-R-D-E-N/commonlibf4-nomad at 696f5f5), and that tree defines `ENABLE_FALLOUT_VR`. Under that define, `MiddleHighProcessData` gets an 8-byte block before `subGraphIdleManagerRoots` (1.10.163 position +0x268). Every later member moves by +0x8, so the compiled `equippedItemsLock` lands at +0x288 and `equippedItems` at +0x290.

The real 1.10.163 object keeps the lock at +0x280 and `equippedItems` at +0x288. So the code locked +0x288, which is the start of the `equippedItems` array header (data pointer, capacity, size). `BSSpinLock::Lock` spun on that data pointer forever.

The same define also moves `PlayerCharacter` members from +0xB70 onward by +0x470 (the VR header pads 0x470 bytes before `firstPersonBipedAnim`). That affected three members OG reads (see the list below).

## Evidence

- Hang dump, shipped OG 1.10.163 DLL: the lock address is `middleHigh + 0x288`, and it holds `{heap pointer, capacity 4, size 1}`. That is an array header, not a lock.
- Flat headers in the same checkout (the 1.10.163 layout): `MiddleHighProcessData` has `equippedItemsLock` at +0x280, `equippedItems` at +0x288, and `sizeof == 0x4C0`. `PlayerCharacter` has `questTargetLock` at +0x630, `questTargets` at +0x7F0, `autoAimActor` at +0xC50, `perkCount` at +0xCF1, `playerMapMarker` at +0xBC4.
- VR header comments in `VR/CommonLibF4/include/RE/Bethesda/Actor.h` say the VR lock is at +0x288 and the array at +0x290. VR `sizeof(MiddleHighProcessData)` is 0x4C8.
- Building against the flat root headers was tried in CI (`og-build`, run 42428873727) and fails to compile: about 105 errors, all in `src/Live.cpp`. The flat headers lack these names the OG code uses: `RE::LIFE_STATE`, `RE::DIFFICULTY_LEVEL`, `BGSBipedObjectForm::GetFilledSlots`, the `Scaleform::GFx::Value` / `GValue` / `Movie::GetVariable` API, and one `.get` call on a type at `Live.cpp:481`. Porting those is a separate task, so the OG build stays on `commonlib/VR`.

## Fix

Instead of trusting the VR header offsets for these fields, the code reads the flat 1.10.163 positions explicitly:

- `OG/src/EngineLayout.h` adds accessors: `EquippedItemsLock` (+0x280), `EquippedItems` (+0x288), `AutoAimActor` (+0xC50), `PerkCount` (+0xCF1), `PlayerMapMarker` (+0xBC4). They reinterpret the object bytes at those positions. Under `PHOSPHOR_AE` they return the header members, so call sites are the same in both builds.
- Call sites changed to use the accessors:
  - `src/Live.cpp`: `ReadEquipped` and `ReadGear` (lock and iteration), `perks` read, auto-aim actor read. The file now includes `EngineLayout.h`.
  - `src/EngineProbe.cpp` and `src/EngineQuests.cpp`: `playerMapMarker` reads.
- `AE/src` is a separate copy of these sources and keeps direct header access. Its vendored headers are the correct ones for that build.

## Checked for other VR-vs-flat differences

Used by OG sources, with the result:

- `MiddleHighProcessData`: only `equippedItemsLock` and `equippedItems` are read. Both fixed above. The VR-only padding is at +0x268, and nothing past it is read.
- `PlayerCharacter`, positions at or above +0xB70 (shifted in the VR build, now fixed): `autoAimActor` (+0xC50), `perkCount` (+0xCF1), `playerMapMarker` (+0xBC4).
- `PlayerCharacter`, positions below +0xB70 (not shifted, no change): `questTargetLock` (+0x630), `questTargets` (+0x7F0), `objectives` (+0x7D8), `currentMapMarkers` (+0x738), `cachedWorldspace` (+0x8B0), `exteriorPosition` (+0x8B8).
- `Actor::currentProcess` (+0x300), `AIProcess::middleHigh` (+0x08): the VR headers do not shift these.
- `Actor` `gunState` read: lives in the `ActorState` base, which has no VR conditional and is identical in both trees.
- `EquippedItem`: `sizeof` 0x28 in both trees, no VR conditional.
- `UI`: VR `sizeof` 0x250 against flat 0x260, because the flat tree carries an AE-only tail. OG uses only `UI` methods (`GetMenuOpen`, `GetMenu`, `RegisterSink`), so no member is read.
- `TESDataHandler`: VR size conditionals differ, but OG uses only its methods.
- `BSFadeNode`, `BSGeometry`, `NiNode`, `BSTriShape`: VR conditionals exist, but OG sources do not reference these types.

How it was checked: grep of `ENABLE_FALLOUT_VR` in the VR tree (files: `Actor.h`, `BSFadeNode.h`, `BSGeometry.h`, `NiNode.h`, `PlayerCharacter.h`, `TESDataHandler.h`), grep of the `// flat:` and `vr:` annotations (only `Actor.h` and `PlayerCharacter.h` carry them), and `static_assert(sizeof(...))` values compared between the two trees for every `RE::` name the OG sources use. The member list was taken from the OG source text.

## Known risk, not fixed here

Other VR-only members or conditionals in classes the plugin does not read are outside this change. The `UI` and `TESDataHandler` sizes differ between the trees, which matters only if OG ever reads their members directly. VR-only API names (listed above) remain in `Live.cpp`, so the OG build stays tied to the VR headers until they are ported.

## Status

Unverified in game. Nothing was compiled locally. The CI `og-build` job is the first compile of this change. Before calling it fixed, the rebuilt DLL needs a run on OG 1.10.163 with no hang in `ReadEquipped` or `ReadGear`.
