# CommonLibF4 findings

> Imported into this repo on 2026-08-07 from the workspace knowledge base
> (`Prisma/COMMONLIB_FINDINGS.md`). Confirmed struct layouts, field offsets and API availability for CommonLibF4. Shared across Prisma plugins, included here because this plugin's engine reads depend on it.

---

# COMMONLIB RESEARCH LOG

> [CONFIRMED 2026-07-20] **`RE::ExtraUniqueID` gives real, working per-INSTANCE item identity, not
> just per-form/stack.** `include/RE/E/ExtraUniqueID.h` is a fully laid-out, concrete class (not
> forward-declared): `{ std::uint16_t uniqueID; std::uint32_t baseID; }`, `sizeof == 0x20` including
> the `BSExtraData` base. Access path, confirmed working via a shipped plugin
> (`Projects/Plugins/WeaponCond/src/main.cpp`'s `MakeKey`), not just theoretically available:
> `TESObjectREFR::inventoryList` (`BGSInventoryList*`) -> `ForEachStack(filter, continuation)` walks
> per-STACK, not per-form; each `BGSInventoryItem::Stack` carries its own
> `BSTSmartPointer<ExtraDataList> extra`; `extra->GetByType<RE::ExtraUniqueID>()` (templated accessor,
> `ExtraDataList.h`) returns the per-instance ID when the engine has assigned one. Caveat found in
> WeaponCond's own code: NOT every stack has one — plain unenchanted stackable junk may lack it until
> something forces individuation, so callers need a fallback (WeaponCond falls back to actor+baseForm;
> PrismaAuctionHouse's `InventoryManager::VerifyItemStillPresent` falls back to bare quantity — see
> `docs/Code Notes/PrismaAuctionHouse.md`). Do NOT blanket-tag by bare formID when no `ExtraUniqueID`
> exists — that risks false-positiving every legitimately-acquired item of the same type.

> [⚠ WRONG IN COMMONLIBF4 2026-07-16] **`bhkPickData`'s default ctor leaves the collision filter at
> ZERO, so `TESObjectCELL::Pick` can NEVER hit anything.** The header (`RE/B/bhkPickData.h`) exposes
> only a no-arg `bhkPickData()`, bound to OG RVA `0x1f760`. Decompiled, that ctor does
> `*(undefined4*)(this + 0xc) = 0` — offset `0xC` is the collision filter
> (`castQuery.m_filterData.m_collisionFilterInfo`: `hknpRayCastQuery` @0x00 -> `hknpCollisionQuery`
> -> `m_filterData` @0x08 -> `m_collisionFilterInfo` @0x04 = 0xC, which matches the decompile
> exactly). Filter `0` is `COL_LAYER::kUnidentified` and matches **no layer**, so `HasHit()` returns
> false 100% of the time. **Every real vanilla ctor takes a `CFilter`** and writes that same field —
> e.g. `bhkPickData(CFilter, NiPoint3* start, NiPoint3* end)` @ RVA `0x105f20` does
> `*(uint*)(this + 0xc) = param_1.filter`. The no-arg one CommonLib binds is the odd one out, and
> nothing in the header hints at it. `customCollideLayers` (@0xC8) is **not** the mechanism — it is 0
> in both ctors. **Fix:** after `SetStartEnd`, set the layer yourself:
> ```cpp
> RE::bhkPickData pd;
> pd.SetStartEnd(start, end);
> RE::CFilter f{}; f.filter = 0;
> f.SetCollisionLayer(RE::COL_LAYER::kDroppingPick);   // 46; the engine's own "where would this land" pick layer
> pd.castQuery.m_filterData.m_collisionFilterInfo = f.filter;
> (void)cell->Pick(pd);
> ```
> Cost 3 in-game test cycles on WastelandHunter camp placement (1085 consecutive ticks, zero hits)
> before being decompile-proven. Other pick layers: `kItemPicker=40`, `kLOS=41`, `kPathingPick=42`.

> [⚠ FOOTGUN 2026-07-16] **`TESObjectREFR::SetStartingPosition` does NOT move a reference.** It is
> virtual slot 6B and reads like a position setter, but it writes the **`ExtraStartingPosition`
> extra-data** (a ref's original/editor placement, used for resets/respawns) — confirmed by
> `SetStartingPositionPtr(ExtraDataList*, TESObjectREFR*) -> ExtraStartingPosition*`. Calling it +
> `Update3DPosition(true)` silently does nothing: the 3D re-syncs to the unchanged real transform.
> **To actually move a ref**, write the live transform in `TESObjectREFR::data` (`OBJ_REFR`:
> `NiPoint3A angle` @0x00, `NiPoint3A location` @0x10) then `Update3DPosition(true)`:
> ```cpp
> ref->data.location.x = p.x; ref->data.location.y = p.y; ref->data.location.z = p.z;
> ref->data.angle.z = yawRadians;   // also zero .x/.y if you want it upright
> ref->Update3DPosition(true);      // warp
> ```
> **There is NO synchronous native move for a generic (non-Actor) reference.** Direct `SetPosition`
> exists only for `Actor` (RVA 0xd77690), `Projectile`, water objects, etc. The engine moves ordinary
> refs through a queued `GameScript::SetPositionFunctor` (`ConcreteDelayFunctorFactory<...,6>`),
> which is exactly why Papyrus `ObjectReference.SetPosition` is **latent**. Also: a ref you intend to
> drive per-frame must be `SetMotionType(2 /*Motion_Keyframed*/)` first or havok fights every write
> (visible as the object spinning/stuttering); pin it with `0 /*Motion_Fixed*/` when done.

> [⚠ WRONG IN COMMONLIBF4 2026-07-10] **`BGSTerminal::MenuItem::Flag` enum values are WRONG —
> the real `ucFlags` byte (`RE/B/BGSTerminal.h`, `MenuItem::flags`, offset +0x22, 1 byte —
> the OFFSET itself is correct, independently confirmed against the private symbol export
> (older runtime, private symbols)) is a **bitmask**, not the sequential ordinals the header
> declares (`kBackToTop=0,kRedraw=1,kSubmenu=2,kText=3,kImage=4,kHolotape=5`). Decompile-verified
> via the engine's own `BGSTerminal::MenuItem::GetResultType(bool)` @ OG 1.10.163 RVA `0x3070c0`
> (the same function `TerminalMenu::OnMenuItemPress` calls):
> `if(ucFlags&0x04) kSubmenu; if(ucFlags&0x08) kText; if(ucFlags&0x10) kImage;
> if(ucFlags&0x20) kHolotape; else -1 (invalid)`. **Do NOT** do
> `mi.flags.get()` (`REX::TEnumSet<Flag,int8_t>::get()` is a plain `static_cast<E>(_impl)`, no bit
> logic — confirmed by reading `REX/TEnumSet.h`) and switch/compare against the header's enum
> values — it compares a bitmask against sequential ordinals and essentially never matches.
> Instead read the raw byte via `mi.flags.underlying()` and bit-test `& 0x04/0x08/0x10/0x20`
> directly, replicating `GetResultType`. Whether bits `0x01`/`0x02` correspond to
> `kBackToTop`/`kRedraw` is **unconfirmed** — `GetResultType` never tests them, so don't assume it.
> This silently broke BOTH the visible "entry type" classification AND (independently, same wrong
> comparison) whether `MenuItem::selectionResult.subMenu`/`.displayText` got read at all — a real
> terminal's submenu/text entries were misclassified as `"unknown"`/`"image"` and their
> content never rendered. Found via PrismaTerminal_F4 Logs/Navigator showing titles with no text;
> see `Projects/Prisma/docs/findings/recent-worklog.md` pt7 (2026-07-10) for the full trace.

> [CONFIRMED 2026-07-10] **Item default material-swap / object-template access.** An item's default
> material swap (CROSS-style skins) is NOT in `TESObjectARMO::armorData.materialSwaps` /
> `TESObjectWEAP::weaponData.materialSwaps` — those are empty for default-template forms. Resolve it
> via the object template, all exposed in `RE/B/BGSMod.h` + `RE/T/TESForm.h`:
> `TESForm::GetObjectTemplate()` (virtual, → `BGSMod::Template::Items*` or nullptr) → iterate
> `Items::items` (`BSTArray<Template::Item*>`) → the `Item` with `isDefault` set. `Item` inherits
> `BGSMod::Container`; `Container::GetData(Data*)` fills `{Attachment::Instance* attachments;
> Property::Mod* propertyMods; u32 counts}`. Each `Attachment::Instance::mod` is the OMOD
> (`BGSMod::Attachment::Mod`, inherits `BGSModelMaterialSwap` so has `swapForm`, + `Container` so
> `GetData` gives its `propertyMods`). A material-swap property is a `Property::Mod` with
> `type == Property::TYPE::kForm` whose `data.form->As<BGSMaterialSwap>()` succeeds;
> `BGSMaterialSwap::swapMap` is `BSTHashMap<BSFixedString origMaterial, Entry{BSFixedString
> swapMaterial; float}>`. **GetData returns VIEWS into the form's internal storage** (same pointers
> the engine's `BGSMod::Container::PropertyMods`/`Attachments` accessors return — OG 1.10.163
> `0x140043A40` / `0x14004E2B0`, verified simple 88-byte accessors) — no allocation, nothing to free.
> Used to fix PrismaCrafting purple modded previews; see findings/prismacraft.md.


> CommonLibF4 research log: confirmed field layouts, offsets, API availability, and known gaps
> across all CommonLibF4 versions. **Source of truth** - the session-end workflow appends here
> (this replaced the old `COMMONLIB_FINDINGS.TXT`). Add new entries under the matching section,
> tagged `[CONFIRMED ...]` / `[REFERENCE ...]` / `[⚠ FORWARD-DECLARED ONLY]`, inside the section's
> fenced block; keep the Contents list and `Last updated` note current. See maintenance rules at the bottom.

## Contents

- [SECTION 0a — BUILD SYSTEM REALITY (read before building ANY Prisma cmake plugin)](#section-0a-build-system-reality-read-before-building-any-prisma-cmake-plugin)
- [SECTION 0 — F4SE PLUGIN DECLARATION (E:\F4SE OG\CommonLibF4, Dear-Modding fork)](#section-0-f4se-plugin-declaration-ef4se-ogcommonlibf4-dear-modding-fork)
- [QUICK INDEX — KEY TYPES BY PLUGIN USE CASE](#quick-index-key-types-by-plugin-use-case)
- [SECTION 1 — TESNPC / TESActorBase  (TESBoundAnimObjects.h)](#section-1-tesnpc-tesactorbase-tesboundanimobjectsh)
- [SECTION 2 — TESRace  (TESRace.h)](#section-2-tesrace-tesraceh)
- [SECTION 3 — BGSColorForm  (TESForms.h line 2913)](#section-3-bgscolorform-tesformsh-line-2913)
- [SECTION 4 — BGSCharacterTint  (BGSCharacterTint.h)](#section-4-bgscharactertint-bgscharactertinth)
- [SECTION 5 — BGSCharacterMorph  (BGSCharacterMorph.h)](#section-5-bgscharactermorph-bgscharactermorphh)
- [SECTION 6 — Actor Value / Survival AVs  (ActorValueInfo, RE::ActorValue)](#section-6-actor-value-survival-avs-actorvalueinfo-reactorvalue)
- [SECTION 7 — F4EE Interface Layout  (vendor/F4SEPlugins-original/f4ee/)](#section-7-f4ee-interface-layout-vendorf4seplugins-originalf4ee)
- [SECTION 8 — Utility APIs (confirmed usable from plugin code)](#section-8-utility-apis-confirmed-usable-from-plugin-code)
- [SECTION 9 — Known CommonLibF4 Gaps / Forward-Declared-Only Types](#section-9-known-commonlibf4-gaps-forward-declared-only-types)
- [SECTION 10 — BSScript / Papyrus VM Access Pattern](#section-10-bsscript-papyrus-vm-access-pattern)
- [SECTION 11 — PrismaUI_F4 API (IVPrismaUI* interface hierarchy)](#section-11-prismaui_f4-api-ivprismaui-interface-hierarchy)
- [MAINTENANCE RULES (Claude must follow these)](#maintenance-rules-claude-must-follow-these)
- [SECTION 12 — NiObject / BSGeometry / D3D scenegraph traversal](#section-12-niobject-bsgeometry-d3d-scenegraph-traversal)
- [SECTION 13 — Actor AI / ActorValue manipulation](#section-13-actor-ai-actorvalue-manipulation)
- [SECTION 14 — Build / Logger Gotchas](#section-14-build-logger-gotchas)

```text
  Confirmed field layouts, API availability, type definitions, and known gaps
  across all CommonLibF4 versions in this workspace.
  Last updated: 2026-07-10 -- BGSTerminal::MenuItem::Flag enum values wrong (bitmask vs sequential
  ordinals, see top-of-file callout). Prior: 2026-07-06 pt10 -- PHASE 3 COMPLETE, fully validated
  in-game. CEF renders inside
  Fallout 4 end to end: Sensor DLL (D3D11 swap-chain hook) + Host DLL (CEF, /MT) two-DLL split,
  shell browser + 3 dynamically-created sandboxed file:// iframes composited onto the game backbuffer
  via OnAcceleratedPaint shared texture. Three blockers found+fixed this pass: (1) DLL dependency
  search path -> SetDllDirectoryW; (2) ES module blocked over file:// -> allow-file-access-from-files
  + added OnConsoleMessage/OnLoadError diagnostics; (3) compositor now explicitly binds the backbuffer
  RTV+viewport. PROCESS-ISOLATION ANSWER: 3 sandboxed iframes consolidate into ~1 shared renderer
  (shell+3iframes = 6 procs / 3 renderers vs 3-separate-browsers = 7 / 4 renderers) -- the shared-
  shell process-count win is validated; per-iframe ping/pong IPC all passes. See SECTION 14.
  Prior 2026-07-06 pt9 -- First real launch attempt after the two-DLL split hung during
  F4SE plugin loading (never reached the game). Root-caused to xSE PluginPreloader flat-scanning
  Data\F4SE\Plugins\ and crashing on chrome_elf.dll -- CEF's runtime files had been deployed flat in
  that folder alongside the real plugin DLL. Fixed by moving everything except the Sensor DLL
  (PrismaUI_F4_CEF_GameSpike.dll) into a Plugins\PrismaUI_F4_CEF_GameSpike_Runtime\ subfolder.
  Prior 2026-07-06 pt8 -- Phase 3 game-spike's CRT blocker (pt7) RESOLVED via a two-DLL
  split: PrismaUI_F4_CEF_Host.dll (/MT, owns CEF entirely, zero commonlibf4/spdlog) +
  PrismaUI_F4_CEF_GameSpike.dll (/MD, owns RE::BSGraphics/D3D hook/compositor, the actual F4SE
  plugin), talking over a thin POD-only exported C ABI (GetProcAddress, same pattern as
  RequestPluginAPI). Both targets now build+link with zero errors.
  Prior 2026-07-06 pt7 -- Phase 3 game-spike written but BLOCKED at link time: commonlibf4/spdlog are
  built /MD and cannot link into the /MT CEF-consuming target (real LNK2005/LNK2019 errors, not a
  warning) -- a new, more fundamental CRT wall than the Phase 2 DirectXTK one.
  Prior 2026-07-06 pt6 -- 3-concurrent-browser process-isolation data captured (7 total
  processes: 1 gpu-process + 2 utility services shared, but 4 independent renderer processes for
  3 views -- one per view plus Chromium's spare-renderer pre-warm). IMPORTANT: this tested 3
  independent top-level browsers, NOT iframes-in-one-shared-shell -- the actual open design
  question about iframe process-isolation is still unanswered, flagged explicitly, see SECTION 14.
  Prior 2026-07-06 pt5 -- PHASE 2 RESOLVED. CEF Phase 2 harness now passes end to end
  (exit code 0) against CEF 147.0.14, the shipping version: CefInitialize -> OnContextInitialized
  -> real (never-shown) HWND passed to SetAsWindowless -> async CreateBrowserHost::CreateBrowser
  (not sync) -> OnAcceleratedPaint with a real shared D3D11 texture, screenshot-verified ->
  CefProcessMessage round trip -> clean CefShutdown. Root cause was two compounding bugs: (1)
  CreateBrowserSync called before CefBrowserProcessHandler::OnContextInitialized fired (a genuine
  race causing the original 15s-hang-then-crash), and (2) CreateBrowserSync itself (vs cefclient's
  async CreateBrowser) combined with a null parent HWND (vs a real one, which windowless/OSR
  rendering still expects despite the API doc implying it's optional) causing a fast silent null
  return once (1) was fixed. Both found by diffing against tests/cefclient/browser/osr_window_win.cc
  (a real OSR reference, unlike windowed cefsimple). Phase 3 can now proceed using this exact
  pattern. See SECTION 14 for the full trail. Prior 2026-07-06 pt4 (implemented
  PrismaCefApp::CefBrowserProcessHandler + OnContextInitialized gating). Prior 2026-07-06 pt3
  (version bisection against CEF 138.0.62 stable reproduced the IDENTICAL bug as 147.0.14, ruling
  out a version-specific regression). Prior 2026-07-06 pt2 (CONFIRMED CEF 147.0.14 requires /MT
  static CRT
  (STATUS_BREAKPOINT crashes in every child-process role otherwise, fixed via set_runtimes("MT"));
  CONFIRMED /DELAYLOAD not needed for libcef.dll in this architecture (isolated to separate exes,
  plain implicit linking works); OPEN[HIGH] Viz/GPU process hangs mid-init, CreateBrowserSync
  returns null, blocks Phase 3 — see SECTION 14 for full diagnostic trail. Prior 2026-07-06
  (CONFIRMED RE::BSInputEnableManager::EnableUserEvent as FO4's ToggleControls
  equivalent for per-category input masking [REL::VariantID{1432984,2268263}, RVA 0x1b21ea0], and
  RE::MenuControls::RegisterHandler firing regardless of native menu/focus state [gated only by
  BSInputDeviceManager::pollingEnabled] — both confirmed for the PrismaUI_F4 gamepad-support port,
  see SECTION 8. Prior 2026-06-22 (CONFIRMED BSInputDeviceManager::pollingEnabled @0x35 = master input-poll switch; false halts PollInputDevices and blocks game controls + ALL BSInputEventUser sinks, but also kills your own close hotkey -> softlock unless the escape path uses a bus-independent source [GetAsyncKeyState / XInputGetState / PrismaUI WndProc]. See new entry below + findings/falloutchat.md InputGuard. Prior 2026-06-18: PrismaUI_F4 framework facts from PrismaMCM hardening — Destroy() of a view focused with pauseGame=true LEAKS freezeFramePause [ClearOperations wipes the queued Unfocus, view erased before it runs -> "view not found" branch skips the freezeFramePause-- -> frozen black screen]; close paused views with Unfocus+Hide, not Destroy. View URL base = plugin folder NOT views/: absolute "/Interface/x" -> PrismaUI_F4/Interface/x [404]; use RELATIVE "../icons/x" from views/Interface/<Plugin>/. JS->C++ via BindUIEvent = per-event window.<name> [JS calls window.<name>(jsonStr) directly, e.g. button.html window.openMCM]; there is NO window.prismaCall unless a dispatcher is explicitly registered. Ultralight has NO HTML5 drag-and-drop and NO window.prompt [use pointer-based DnD + inline edit]. See FINDINGS Section 9. Prior: CommonLibF4 pulled a08c731e->57e2960c [ids only, safe]; CONFIRMED Pip-Boy possession via FillsBipedSlot(BIPED_OBJECT::kPipboy=30)+inventoryList->ForEachStack; Workshop API in CommonLibF4 > f4se [f4se only adds hCurrentWorkshop/UpdateSpline/etc.]; f4se source @ Reference/f4se as struct/offset cross-ref; 3D render = OWN your RT, Interface3D customRenderTarget=-1 is unreliable, use Inventory3DManager. Prior 2026-06-17: perk ownership = Actor::GetPerkRank(BGSPerk*) > 0 — there is NO Actor/PlayerCharacter HasPerk accessor; COBJ condition AND/OR parse via TESConditionItem.compareOr + functionData. Earlier: commonlibf4.plugin xmake rule emits NG F4SEPlugin_Version ONLY — OG F4SE needs F4SEPlugin_Query or it rejects the DLL; per-target plugin_template option + define Query in main.cpp — PrismaMCM)
```

### [CONFIRMED 2026-06-18] CommonLibF4 (E:\F4SE OG\CommonLibF4, Dear-Modding-FO4) updated a08c731e -> 57e2960c
```text
  ("upd ids" + commonlib-shared bump). Trivial/additive: removed "// Check" notes, corrected one NG
  address (Play 2276042 -> 2267042), added ID Size. No struct/API removals; rebuild confirmed clean.
```

### [CONFIRMED 2026-06-22] BSInputDeviceManager::pollingEnabled (bool @ offset 0x35) is the master
```text
  input-poll switch. Setting it false HALTS PollInputDevices: the engine stops generating/dispatching
  InputEvents, so it blocks game controls AND every plugin's BSInputEventUser/MenuControls sink
  (FocusMenu/ControlMap only blocks game controls, NOT those sinks — there is no per-event "consume").
  Verified in-game via FalloutChat: with it false, no mod hotkey fires.
  CRITICAL GOTCHA: it also kills your OWN BSInputEventUser handlers (e.g. a KeyHandler close hotkey),
  so disabling it without a bus-INDEPENDENT escape path SOFTLOCKS the player.
  Bus-independent input sources that still work while pollingEnabled=false (use these for the escape
  path): Win32 GetAsyncKeyState (keyboard), XInputGetState (gamepad, link Xinput9_1_0), and PrismaUI's
  WndProc subclass (mouse + WM_CHAR text). Restore pollingEnabled=true on UI close and defensively on
  load. See FINDINGS findings/falloutchat.md (InputGuard) for the full pattern.
```

### [CONFIRMED 2026-06-18] In-world / Pip-Boy possession check (HUD gating, FormID-free + replacer-proof):
```text
    auto* p = RE::PlayerCharacter::GetSingleton();
    if (!p || !p->GetCurrentLocation()) return false;          // TESObjectREFR::GetCurrentLocation()
    auto* inv = p->inventoryList;                              // TESObjectREFR::inventoryList (BGSInventoryList*)
    inv->ForEachStack(filter, cont);                           // BGSInventoryList::ForEachStack(filter,cont)
      filter(BGSInventoryItem& it): it.object (TESBoundObject*) -> GetFormType()==RE::ENUM_FORM_ID::kARMO,
        static_cast<RE::TESObjectARMO*>(obj)->FillsBipedSlot((int)RE::BIPED_OBJECT::kPipboy) // kPipboy=30
      cont(item, stack): set found, return false to stop.
  TESObjectARMO publicly inherits BGSBipedObjectForm. Must run on the game thread.
```

### [CONFIRMED 2026-06-18] Workshop API: CommonLibF4 RE/W/Workshop.h is MORE complete than f4se GameWorkshop.h.
```text
  Has (wrapped): ContextData, ExtraData(currentPowerGrid, powerGrid[]), FindNearestValidWorkshop,
  InitializePlacementReference, PlaceCurrentReference, ScrapReference, SetSelectedEditItem, ToggleEditMode,
  UpdateActiveEdit, the placement events; PowerUtils::PowerGrid (RE/P/PowerUtils.h) fully defined;
  SplineUtils::ConnectSpline (RE/S/SplineUtils.h). f4se ONLY adds (gaps): hCurrentWorkshop (cached global
  handle), UpdateSpline, EstablishTerminalLinks, GetObjectAtConnectPoint — only needed for build-budget
  display or in-UI wiring tools.
```

### [REFERENCE 2026-06-18] f4se source cloned to Reference/f4se (ianpatt, 227 files): authoritative Game*.h
```text
  struct/offset cross-reference (103 RelocAddr), 41 Papyrus*.cpp native bindings, GameRTTI.h. USE WHEN
  CommonLibF4 has a gap or [FORWARD-DECLARED ONLY] type. NOT a feature multiplier — CommonLibF4 is richer
  for well-covered systems (radiation, workshop). Radiation footprint in f4se = just RTTI_HUDRadiationMeter/
  Model.
```

### [CONFIRMED 2026-06-18] 3D render to texture: RE::Interface3D::Renderer offscreen + customRenderTarget is
```text
  FINICKY — it can stay -1 (engine never assigns a slot) and silently produce nothing (see FINDINGS
  [3D RENDER TO VIEW]). RELIABLE path = own your own ID3D11Texture2D RT+SRV (no MSAA), bind/draw/restore.
  RE::Inventory3DManager (RE/I/Inventory3DManager.h: Begin3D/ClearModel) is the proven vanilla 3D-item
  display. RE::BSGraphics::Renderer/RendererData::GetSingleton() for device/context.
```

### [CONFIRMED 2026-06-17] Writable plugin settings via SimpleINI (PrismaCrafting UI-scale persistence):
```text
  Vendor the single-header SimpleIni.h (copy from PrismaUI_Systems/FalloutChat/include) into src/,
  rely on the existing add_includedirs("src"); #include <SimpleIni.h>.
    CSimpleIniA ini; ini.SetUnicode();
    ini.LoadFile(path.c_str());                 // missing file is fine -> defaults
    long v = ini.GetLongValue("UI","CraftScale",100);
    ini.SetLongValue("UI","CraftScale",pct); ini.SaveFile(path.c_str());  // <0 = failure
  Write to Paths::F4SEUserDir() (Documents\My Games\Fallout4\F4SE\) — it is WRITABLE and NOT
  virtualized by MO2 (unlike Data\, whose writes land in overwrite). LoadFile before SaveFile to
  preserve other keys. JS<->INI: C++ pushes the saved value to the view on open (InteropCall
  "setUiScale"); the slider persists back through a debounced RegisterJSListener("saveUiScale").
```

### [CONFIRMED 2026-06-17] Perk ownership check (PrismaCrafting build helper):
```text
  auto* player = RE::PlayerCharacter::GetSingleton();   // derives Actor
  bool owned = player->GetPerkRank(static_cast<RE::BGSPerk*>(perk)) > 0;
  Actor::GetPerkRank(BGSPerk*) (RE/A/Actor.h:360, REL func) returns the rank, >0 = owned.
  There is NO HasPerk(BGSPerk*) on Actor or PlayerCharacter in this CommonLibF4 (only
  HasPerkEntries(uint8 entryPoint) and SelectPerk/SetPerkCount on the player). Resolve the perk
  form first: RE::TESForm::GetFormByID(id) then ->Is(RE::ENUM_FORM_ID::kPERK) before the cast.
```

### [CONFIRMED 2026-06-22] Schematic BOOK -> taught perk + value + remove (PrismaCrafting dedupe).
```text
  - BOOK taught perk: TESObjectBOOK::data (OBJ_BOOK @0x170). OBJ_BOOK::teaches is a UNION
    {ActorValueInfo* actorValueToAdvance; SpellItem* spell; BGSPerk* perk;} @0x08 selected by
    OBJ_BOOK::flags (int8 @0x00). All three union members are TESForm-derived with TESForm at
    offset 0, so read flag-agnostically and validate the type:
      auto* f = reinterpret_cast<RE::TESForm*>(book->data.teaches.perk);
      if (!f || !f->Is(RE::ENUM_FORM_ID::kPERK)) skip;   // not a BookPerk
      auto* perk = static_cast<RE::BGSPerk*>(f);
  - Book gold value: TESObjectBOOK is a TESValueForm -> book->GetFormValue() (int32). RE/T/TESValueForm.h:21.
  - Owned check: player->GetPerkRank(perk) > 0 (see 2026-06-17 entry; no HasPerk accessor).
  - TESContainerChangedEvent fields (RE/T/TESContainerChangedEvent.h): oldContainerFormID(0),
    newContainerFormID(4), baseObjectFormID(8), itemCount(0xC), referenceFormID(0x10), uniqueID(0x14).
    "entered player inventory" = newContainerFormID == player->GetFormID() (0x14).
  - Remove the just-added copy: TESObjectREFR::RemoveItemData (ctor (TESForm*, int32 count); set
    .reason = RE::ITEM_REMOVE_REASON::kRemove), then player->RemoveItem(data) (vfunc 0x6D,
    returns ObjectRefHandle). RE/T/TESObjectREFR.h:87-107,147.
```

### [CONFIRMED 2026-06-17] BGSConstructibleObject condition parse for tool/perk/schematic gates
```text
  (PrismaCrafting). Walk cobj->conditions.head -> item->next (RE/T/TESCondition.h). Per item:
    fn  = item->data.functionData.function.underlying()   // vs RE::SCRIPT_OUTPUT:: enum
    arg = static_cast<RE::TESForm*>(item->data.functionData.param[0])   // the perk/item form
    item->data.compareOr   // 1 = OR with the NEXT item (an OR-run); 0 ends the run (AND join)
  kScript_HasPerk (448) -> perk gate; kScript_GetItemCount (47) / kScript_GetEquipped /
  kScript_HasKeyword / kScript_WornHasKeyword -> tool gates. OR-runs are alternatives ("any one");
  separate runs are AND-ed ("need one from each"). requiredItems is
  BSTArray<BSTTuple<TESForm*, BGSTypedFormValuePair::SharedVal>>* (pair.second.i = count);
  filterKeywords is BGSTypedKeywordValueArray<kRecipeFilter> (resolve via
  RE::detail::BGSKeywordGetTypedKeywordByIndex). See FINDINGS SECTION 8.
```

### [CONFIRMED 2026-06-16] Reading a plugin-defined GlobalVariable from C++ (FW_PipboyHUD):
```text
    auto* dh = RE::TESDataHandler::GetSingleton();
    auto* g  = dh->LookupForm<RE::TESGlobal>(0x80B, "FW_PipboyPower.esp"); // localID + modname
    float v  = g ? g->value : 0.0f;
  Confirmed in this CommonLibF4: RE::TESDataHandler::LookupForm<T>(TESFormID, std::string_view)
  (RE/T/TESDataHandler.h) accepts a const char* literal; RE::TESGlobal::value is a public float
  member (RE/T/TESGlobal.h). Resolve by LOCAL id + plugin name (not the runtime FormID) so it
  works regardless of load order / ESL index.
```

### [CONFIRMED 2026-06-16] Game-thread dispatch from a worker/background std::thread:
```text
    if (auto* ti = F4SE::GetTaskInterface()) ti->AddTask([]{ /* runs on main thread */ });
  Read game forms / call PrismaUI ONLY inside the AddTask lambda. NEVER leave a joinable global
  std::thread in an F4SE plugin: the game has no clean plugin-unload, so the thread is still
  joinable at process exit and std::thread's destructor calls std::terminate() (CTD on quit).
  Detach the thread (and guard the loop with an atomic flag).
```

### [CONFIRMED 2026-06-16] PrismaUI_F4 C++ plugin pattern (FW_PipboyHUD, xmake, same as FalloutChat):
```text
  RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>() at kGameDataReady; CreateView(htmlPath, onDom)
  at kPostLoadGame/kNewGame then Hide; push state with InteropCall(view,"fnName",jsonCStr).
  Guard every call with api->IsValid(view). No link dependency — header uses GetProcAddress.
```

### [CONFIRMED 2026-06-18] PrismaUI_F4 framework gotchas (from PrismaMCM in-game hardening):
```text
  * Destroy() of a view focused with pauseGame=true LEAKS the pause -> frozen black screen.
    ViewManager::Destroy() runs ViewOperationQueue::ClearOperations() (wiping the queued Unfocus that
    would release the pause) then erases the view from `views` before its own queued Unfocus executes.
    That Unfocus then can't find the view and takes the "!viewData" branch (ViewManager.cpp ~275) which
    closes the FocusMenu but NEVER does `freezeFramePause--`. So the game stays frozen.
    -> To close a paused view: api->Unfocus(view); api->Hide(view);  (NOT Destroy). The view stays in the
    map so PerformUnfocusOperations decrements freezeFramePause + closes the cursor menu + restores input.
    Proper fix would be in the NG framework Destroy (release pause synchronously before erase).
  * View file URLs resolve against the PLUGIN folder, not views/. A view created with
    CreateView("Interface/PrismaMCM/mcm.html") loads as file:///views/Interface/PrismaMCM/mcm.html, so an
    ABSOLUTE resource "/Interface/icons/x.png" -> file:///Interface/icons/x.png -> PrismaUI_F4/Interface/..
    (no views/) -> 404. Use RELATIVE paths from the html ("../icons/x.png", "fonts/x.ttf"); that's why the
    bundled fonts work and the absolute-path images did not.
  * JS->C++ binding is PER-EVENT. BindUIEvent(view,"foo",cb) and RegisterJSListener create window.foo;
    JS calls it directly as window.foo(jsonStr) (see button.html: window.openMCM('{}')). There is NO global
    window.prismaCall dispatcher unless a plugin explicitly binds one. A JS bridge that funnels everything
    through window.prismaCall will SILENTLY drop every event against a per-event-binding C++ side.
  * Ultralight (this renderer) implements NEITHER HTML5 drag-and-drop NOR window.prompt(). Use pointer-based
    DnD (mousedown threshold -> ghost -> document.elementFromPoint hit-test) and inline-edit inputs.
  * Diagnose HANGS (not crashes) with AddictolCrashLogger bEnableThreadDumpHotkey=true -> Ctrl+Shift+F12
    writes threaddump-*.log listing EVERY thread's stack. Read THREAD 1 (the game main thread) bottom-up.
  * GOTCHA (2026-06-18, mis-diagnosed first): a "framework off = loads, on = freezes" load HANG is NOT
    necessarily the framework. Plugins that USE the framework only run their work when RRequestPluginAPI
    succeeds, so disabling the framework also silences THEM. Confirmed case: the real freeze was
    PrismaCrafting calling TESDescription::GetDescription() (a per-form DISK read) on every COBJ at
    kPostLoadGame on the MAIN THREAD (thread dump THREAD 1: BSFile::PageCache::ReadAt <- GetDescription <-
    PrismaCrafting.dll). An earlier dump catching the main thread in bink2w64.dll was just the load stalled
    at a different instant, NOT a present-hook/Bink conflict. A tried present-hook gate (skip unless
    HUDMenu open) was REVERTED: it would break full-screen Prisma menus (PrismaCrafting cooking window,
    PrismaTerminal, PrismaInventory) which open while HUDMenu is CLOSED. LESSON: get the THREAD 1 stack
    before changing the shared framework; don't gate the present hook on HUDMenu.
  * NEVER call TESDescription::GetDescription() (or other DESC-subrecord/file-backed reads) in a bulk
    main-thread loop at load — it seeks+reads the source plugin file per form, uncached. Thousands of calls
    = hard freeze. Load such text lazily for the small set actually shown, or not at all.
  * RESOLVED (2026-06-18): descriptions restored the SAFE way. CobjCache now keeps r.desc EMPTY during
    Build()'s full-array scan, and an EnsureDesc(i) helper reads the one DESC lazily at MENU-OPEN time
    (inside FilterJsonForBench / WorkshopBuildablesJson), only for the bounded set that menu shows, caching
    the result on the Recipe (descLoaded flag) so each form is read at most once per session. Pattern for
    any file-backed per-form text: never in the load-time bulk scan; lazily in the show-time builder, memoized.
  * VERIFIED CLEAN (2026-06-18): PrismaAmazon (VaultAzon) does NOT share the freeze. Its Catalog::Build()
    is lazy (first menu open, not kPostLoadGame) and touches only in-memory fields (GetFullName, weaponData/
    armorData.value, GetFormValue, IsFood, costOverride). No GetModel/GetDescription/file reads.
  * [FIXED 2026-06-19] THE REAL HARD-FREEZE = a CROSS-THREAD DEADLOCK in the framework's focus-query APIs.
    HasFocus / ViewHasInputFocus / HasAnyActiveFocus each did ultralightThread.submit(...).get() — a
    SYNCHRONOUS wait on the single UI thread, FROM THE GAME THREAD. When the UI thread was concurrently
    running a view's JS that called back into the game (JS->native listener / RegisterConsoleCallback),
    the two threads waited on each other = permanent freeze. Thread dump signature: THREAD 1 (game) parked
    in KERNELBASE WaitForSingleObject inside PrismaUI_F4 future.get() reached via a plugin's F4SE task
    callback (e.g. PrismaWelcome polling HasAnyActiveFocus every 250ms); THREAD 9 (UI worker) parked in
    ntdll wait DEEP inside a WebCore->f4se->PrismaUI callback (top frames ntdll/f4se, WebCore frames below).
    DIAGNOSTIC LESSON: the present-hook StallWatchdog (watches D3DPresent's get) did NOT fire — proving the
    blocking get() was a DIFFERENT, game-thread API path, not the present hook. Always check WHICH get().
    FIX: never block the game thread on the UI thread for focus state. Answer from the framework's own
    bookkeeping — g_currentlyFocusedViewId / g_isAnyInputCaptureActive / g_isFocusedTextInputActive in
    InputHandler (kept in lock-step with Focus()/Unfocus()) via IsInputCaptureActiveForView /
    IsAnyInputCaptureActive / new IsTextInputFocusedForView — all lock-free atomic reads. Same pattern as
    the upstream "ime-focus-freeze" PR (which fixed text-input focus; these 3 were the remaining instances).
    GENERAL RULE: a single-UI-thread renderer + synchronous game<->UI waits in BOTH directions deadlocks;
    keep game-thread framework APIs non-blocking (cached/atomic), let JS->native callbacks be fire-and-forget.
  * [OPEN][MED] PAUSED PrismaUI views are LAGGY / "only repaint on keypress" (welcome over the prewar/chargen
    scene). MECHANISM: the overlay is composited in DrawViews() inside the D3DPresent hook, drawn EVERY time
    the game calls Present — but it draws nothing between Present calls. During a paused prewar/chargen scene
    the game throttles Present (renders only on input), so the overlay only repaints on input. The pause flag
    is NOT the lever: tried swapping Focus(pauseGame) from RE::UI::freezeFramePause++ to the FocusMenu's
    UI_MENU_FLAGS::kPausesGame (1<<0) — did not fix it, made it WORSE, REVERTED (framework @ 9bfe378).
    DrawViews draws the cached texture unconditionally (not gated on dirty/newFrameReady), so a static view
    stays visible AS LONG AS Present is called; the gate is Present frequency, which the GAME controls.
    Likely real fixes are PrismaWelcome-side (don't pause during the intro, or show in normal gameplay), or a
    render-path regression vs the older smooth build. UI_MENU_FLAGS (RE/U/UI_MENU_FLAGS.h): kPausesGame=1<<0,
    kUsesCursor=1<<2, kModal=1<<4, kFreezeFrameBackground=1<<5, kFreezeFramePause=1<<24.
```

## SECTION 0a — BUILD SYSTEM REALITY (read before building ANY Prisma cmake plugin)

### [CONFIRMED 2026-06-17] commonlibf4.plugin xmake rule emits the NG F4SEPlugin_Version export ONLY —
```text
  NOT the classic F4SEPlugin_Query. The Fallen World load order runs OG Fallout 4 (1.10.163, F4SE
  0.6.23), whose loader looks for F4SEPlugin_Query; a DLL with only Version is REJECTED with
  "plugin ...\X.dll does not appear to be an F4SE plugin" (same msg as a non-plugin DLL) and never
  loads — no log file, no behavior. Verify exports with:
    dumpbin /exports X.dll | findstr F4SEPlugin   (must show Load + Query + Version)
  or python -c "import pefile;print([e.name.decode() for e in pefile.PE(r'X.dll').DIRECTORY_ENTRY_EXPORT.symbols if e.name])"

  ROOT of the rule behavior: CommonLibF4/xmake.lua on_config (lines ~59-66) configfile-generates
  commonlibf4-plugin.cpp from a template. Default template = CommonLibF4/res/commonlibf4-plugin.cpp.in,
  which contains ONLY the F4SE_PLUGIN_VERSION lambda (no Query). The rule accepts a PER-TARGET override:
    target:extraconf("rules","commonlibf4.plugin","plugin_template")
  i.e. in the plugin's xmake.lua:
    add_rules("commonlibf4.plugin", { name=..., author=..., version=...,
        plugin_template = path.join(os.scriptdir(), "scripts/commonlibf4-plugin.cpp.in") })

  TWO ways plugins have gotten Query in (only the FIRST two are robust):
    1. [BEST] Define F4SEPlugin_Query directly in src/main.cpp. Build-system-independent; always
       compiled. (PrismaMCM, PrismaJournal_F4, PrismaMap_F4, PrismaShowcase_F4, PrismaCharacterCreator do this.)
    2. [GOOD] Point plugin_template at a local template that defines Query (FW_PipboyHUD pattern).
       Per-target, no global side effects.
    3. [FRAGILE — caused the PrismaMCM "no MCM button" bug, 2026-06-17] A build .bat that copies the
       plugin's local template OVER the SHARED CommonLibF4/res/commonlibf4-plugin.cpp.in before build.
       A bare `xmake` (skipping the .bat) uses the Version-only shared template -> Query lost.
       WORSE: it mutates shared state — a Query template with name="ThisPlugin" then leaks that named
       export into any OTHER plugin later built from the shared template. NEVER overwrite the shared res.

  IMPORTANT: if you BOTH define Query in main.cpp AND use a template that also defines Query, the link
  fails with a duplicate symbol. Pick ONE source of Query. PrismaMCM keeps Query in main.cpp and its
  local template emits Version ONLY.
```

### [CRITICAL 2026-06-08] E:\F4SE OG\CommonLibF4 is now xmake-ONLY (has xmake.lua, NO CMakeLists.txt
```text
  anywhere). The old cmake-buildable nested copy (E:\F4SE OG\CommonLibF4\CommonLibF4) is GONE.
  Consequence: every cmake Prisma plugin whose cmake/commonlibf4.cmake does add_subdirectory(CommonLibF4)
  can NO LONGER fresh-configure — they only still build incrementally off cached external_builds/
  CommonLibF4 artifacts already in their build/ dirs. Delete that build dir and the plugin won't reconfigure.
  Fix options when a cmake Prisma plugin won't build: (a) restore a cmake CommonLibF4 at the expected
  path, or (b) convert the plugin to xmake (see PrismaHUD_F4 xmake.lua for the template).
```

### [CONFIRMED 2026-06-08] This CommonLibF4 fork exposes NO F4SE::log and NO F4SE::stl. Plugins that used
```text
  `namespace logger = F4SE::log;` / `F4SE::stl::report_and_fail` / `logger::log_directory()` must switch
  to spdlog directly (namespace logger = spdlog; set_default_logger in Query; resolve the log dir via
  SHGetKnownFolderPath(FOLDERID_Documents) + "\\My Games\\Fallout4\\F4SE"). The Non-Prisma plugins
  (Rad Mod etc.) already use this spdlog pattern.
```

### [CONFIRMED 2026-06-08] xmake plugin template against this CommonLibF4 (see Rad Mod + PrismaHUD_F4):
```text
  includes("../../../CommonLibF4"); add_deps("commonlibf4"); add_requires/add_packages spdlog+fmt+nlohmann_json;
  add_defines("COMMONLIB_RUNTIMECOUNT=3"); add_syslinks("bcrypt","version"); set_pcxxheader.
  OG (1.10.16x) needs F4SEPlugin_Query + F4SEPlugin_Load exports (manual); NG also wants F4SE_PLUGIN_VERSION.
```

## SECTION 0 — F4SE PLUGIN DECLARATION (E:\F4SE OG\CommonLibF4, Dear-Modding fork)

### [CONFIRMED] Version A (E:\F4SE OG\CommonLibF4) uses the MODERN plugin scheme.
```text
  File: include/F4SE/Interfaces.h
    line 477  struct PluginVersionData  (sizeof 0x45C, dataVersion kVersion=1)
    line 568  #define F4SE_EXPORT extern "C" [[maybe_unused]] __declspec(dllexport)
    line 571  #define F4SE_PLUGIN_LOAD(...)  F4SE_EXPORT bool F4SEPlugin_Load(...)
    line 572  #define F4SE_PLUGIN_VERSION    F4SE_EXPORT constinit F4SE::PluginVersionData F4SEPlugin_Version
  PluginVersionData setters (all constexpr — usable in constinit IIFE):
    PluginVersion(REL::Version)  PluginName(sv)  AuthorName(sv)
    UsesSigScanning(bool)
    UsesAddressLibrary(bool)   -> AE bit 1<<2 (1.11.137+)
    UsesAddressLibraryNG(bool) -> NG bit 1<<1 (1.10.980 / 1.10.984)
    HasNoStructUse(bool)
    IsLayoutDependent(bool)/IsLayoutDependentNG(bool)
    CompatibleVersions({...}) (empty ok when address-independent)
  REL::Version(major,minor,patch,build) — VersionBase ctor, Version.h:21.
  LoadInterface : public QueryInterface  (Interfaces.h:161) — so a_f4se->RuntimeVersion()
    IS available inside F4SEPlugin_Load (needed because Query is NOT called on NG).

  DUAL-TARGET RULE (OG + NG): define ALL of —
    F4SE_PLUGIN_VERSION = []{...}();                 // NG 1.10.980+ reads this
    extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(...) // OG reads this
    extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(...)  // both call this
  Verify exports with `dumpbin /exports <dll>` — all three must appear.
  NOTE: WeaponCond etc. use the OLDER fork at Non Prisma Projects\Plugins\CommonLibF4
        which is legacy Query/Load only (no PluginVersionData). Do not copy its main.cpp
        pattern for plugins linking E:\F4SE OG\CommonLibF4.

  PURPOSE: Avoid re-reading headers or re-researching struct layouts that have
  already been verified. Every entry here was confirmed by reading the actual
  CommonLibF4 header, not assumed.

  VERSIONS COVERED:
    A — E:\F4SE OG\CommonLibF4\           (cmake, OG only, RE/Bethesda/ layout)
    B — E:\F4SE OG\Prisma\CommonLibF4_OG\ (cmake, OG only, older Prisma fork)
    C — E:\F4SE OG\Prisma\NewCommonLib\   (xmake only, OG+NG, RE/A/ alphabetical layout)

  All entries reference version A unless otherwise noted.
```

## QUICK INDEX — KEY TYPES BY PLUGIN USE CASE
```text
  Face/Hair/Tint (PrismaCharacterCreator, PrismaMCM):
    TESRace::FaceRelatedData     → availableHairColors, tintingTemplate, morphGroups
    TESActorBase::HeadRelatedData → hairColor, facialHairColor, faceDetails
    BGSColorForm                 → color (uint32 ABGR), GetFormID(), TESFullName
    BGSCharacterTint::Template   → DEFINED (Groups, Group, Entry)
    BGSCharacterTint::Entries    → FORWARD-DECLARED ONLY — cannot read live values
    BGSCharacterMorph            → Group, FacialBoneRegion — DEFINED

  Body morphs (PrismaCharacterCreator + F4EEBridge):
    BodyMorphInterface (F4EE)    → vtable layout confirmed, 19 virtuals, no base class
    OverlayInterface (F4EE)      → vtable slots +3 due to BSTEventSink×2 base classes
    SkinInterface (F4EE)         → vtable layout confirmed, 10 own virtuals, no base class

  Survival AVs (PrismaHUD):
    ActorValue singleton         → has fatigue field but NOT Hunger/Dehydration
    Hunger / Dehydration         → EditorID-based lookup: TESForm::GetFormByEditorID<ActorValueInfo>

  Scripting / VM (PrismaMCM, general):
    IVirtualMachine::ForEachBoundObject → DO NOT USE (RE::msvc::function, no constructors)
    BSScript::Internal::VirtualMachine → use concreteVM->attachedScripts BSTHashMap directly

  Item weight / value (PrismaHUD quick loot):
    TESObjectWEAP::weaponData.weight       → float, base weapon weight (InstanceData+0x0EC)
    TESObjectWEAP::weaponData.value        → uint32_t, base gold value (InstanceData+0x10C)
    TESObjectWEAP::weaponData.attackDamage → uint16_t, base damage (InstanceData+0x132)
    TESObjectWEAP::weaponData.maxRange     → float, range (InstanceData+0x0D4)
    TESObjectARMO::armorData.weight        → float (InstanceData+0x40)
    TESObjectARMO::armorData.value         → uint32_t (InstanceData+0x48)
    TESObjectARMO::armorData.rating        → uint16_t, armor rating (InstanceData+0x54)
    TESWeightForm::weight                  → float; AlchemyItem/TESObjectMISC/TESAmmo inherit this
    TESValueForm::value                    → int32_t; TESObjectMISC/TESAmmo inherit this
    NOTE: TESObjectWEAP and TESObjectARMO do NOT inherit TESWeightForm — use .weaponData/.armorData

  [CONFIRMED 2026-07-07 in-game] ⚠ WEAPON_TYPE test: DO NOT use TESObjectWEAP::IsThrownWeapon() /
    IsGunWeapon() / IsMeleeWeapon(). They call weaponData.type.any(...) and REX::TEnumSet::any() is a
    BITWISE-AND OVERLAP test ((_impl & args)!=0, TEnumSet.h:96-100). WEAPON_TYPE is a SEQUENTIAL-value
    enum (kStaff=8, kGun=9, kGrenade=0xA, kMine=0xB), NOT bit flags — its values share bits, so
    .any(kGrenade,kMine) returns TRUE for a gun (9=0b1001 overlaps 0xA=0b1010 on bit 3). Proven in-game:
    the Kimber Custom pistol (formID 0x4822) logged gun=true AND thrown=true. Compare the EXACT value:
    weaponData.type.underlying() == static_cast<uint8_t>(WEAPON_TYPE::kGrenade) (or type == E via the
    TEnumSet operator==(E)). .any() is only valid on genuine bit-flag enums.
    weaponData.type = REX::TEnumSet<WEAPON_TYPE,uint8_t> @ InstanceData+0x137.

  [CONFIRMED 2026-07-07 via 1.10.163 decompile] ActorEquipManager::EquipObject flag semantics +
    aid-item USE-animation path (PrismaWheelMenu). EquipObject @ RVA 0xe1bcd0; UseObject @ 0xe1d100;
    ShouldQueueEquipObject @ 0xda4360. Signature: EquipObject(Actor*, const BGSObjectInstance&,
    uint32 stackID, uint32 number, const BGSEquipSlot*, bool queueEquip, bool forceEquip,
    bool playSounds, bool applyNow, bool locked).
    • forceEquip is THE flag for aid-item use animations. EquipObject reaches the use-anim path only
      via `if (forceEquip || !UseObject(...))` — UseObject (which plays the PlayAnimOnItemUse idle via
      AIProcess::FindSpecialIdletoPlay) runs ONLY when forceEquip==FALSE. forceEquip=true short-circuits
      the ||, goes straight to the data-only Equip branch, and NEVER animates (equip SOUND still fires).
      For an item WITHOUT the PlayAnimOnItemUse keyword (e.g. a weapon) UseObject returns false, so
      !UseObject is true and it falls through to a normal Equip — so forceEquip=FALSE is correct for
      ALL item types (aid items animate; weapons still equip). Use forceEquip=FALSE for "player uses/
      equips an item they own" (the vanilla Pipboy/Favorites path).
    • queueEquip only sets an ObjectEquipParams flag, and only if ShouldQueueEquipObject also returns
      true (which itself tests UI menuMode==0). applyNow/locked are stored ObjectEquipParams fields
      consumed inside the templated Equip<>. NEITHER queueEquip NOR applyNow touches the use animation
      or menu state — do not expect them to fix a stuck-after-use symptom.
    • Menu-mode gate: a first-person use/draw idle CANNOT advance while a modal menu holds the player
      behavior graph. Vanilla UseObject's menu branch closes the menu FIRST (PlayPipboyCloseAnim ->
      kHide) then plays the idle on return to gameplay. To use/equip from a custom modal UI: close the
      menu, then DEFER EquipObject(forceEquip=false) until back in gameplay mode.
    • "Back in gameplay mode" check: RE::UI::menuMode (uint32 @ UI+0x1E0, public in UI.h:128) == 0.
      Engine-authoritative — ShouldQueueEquipObject itself tests it. Pair with
      UI::GetMenuOpen("YourMenuName") to confirm your own menu torn down.
    • WEAPON DRAW is a SEPARATE call, NOT a side effect of EquipObject. EquipObject/UseObject never
      route a weapon into the draw/action-graph under any flag combo -- an equipped weapon stays
      HOLSTERED. To draw it (vanilla favorites/quick-use does this AFTER equip): call
      Actor::DrawWeaponMagicHands(true) -- plain virtual, Actor.h:135 (vtable 0xC9; PlayerCharacter
      override @ RVA 0xefb9d0 auto-dispatches, no REL::ID). Builds the default draw ActionInput +
      fires TESActionData::DoIt on the actor's anim graph. Guard with ActorState::GetWeaponMagicDrawn()
      (ActorState.h:27, = weaponState>=kDrawn) to avoid re-firing. Needs gameplay mode + loaded 3D/graph
      (same gate as equip); with applyNow=true the weapon is synchronously in-graph so calling it
      immediately after a successful EquipObject works -- defer one frame via AddTask only if it no-ops.

  [CONFIRMED 2026-07-10] AlchemyItem/MagicItem aid classification helpers (PrismaWheelMenu item
    categorization). MagicItem declares virtual IsFood() (vtable 71), IsPoison() (75), IsMedicine()
    (76) -- MagicItem.h:61-66; AlchemyItem overrides them from the ENIT flags. IsMedicine() is the
    reliable "this is an aid/med item" signal (true for stimpak/radaway/most mod meds) -- survives
    mod display-name renames AND missing keywords, unlike name-substring or keyword matching. CAVEAT:
    chems are NOT cleanly separable this way -- Buffout/Jet/Mentats are ENIT-flagged FoodItem (IsFood()
    true, IsMedicine() false) but ARE chems; they carry keyword ObjectTypeChem. RadX carries neither
    Medicine nor FoodItem flag (only ObjectTypeChem). So a robust "chem/aid" test is
    IsMedicine() || HasKeywordString("ObjectTypeChem") || HasKeywordString("ObjectTypeStimpak").
    Drink keywords (confirmed via get_record): ObjectTypeWater, ObjectTypeNukaCola, ObjectTypeAlcohol.
    There is NO single universal chem/food/drink keyword -- confirmed by direct record inspection.

  Quest alias refs (PrismaHUD compass questDir — path researched but blocked):
    BGSBaseAlias                   → DEFINED (BGSBaseAliases.h)
      aliasName    BSFixedString  @ 0x08
      owningQuest  TESQuest*      @ 0x10
      aliasID      uint32_t       @ 0x18
      flags        FLAGS enum     @ 0x1C
      fillType     uint16_t       @ 0x20   (sizeof = 0x28)
    BGSRefAlias (extends BGSBaseAlias) → DEFINED
      fillData     GenericFillData @ 0x28  (uint64_t data[3], sizeof=0x18)
      conditions   TESCondition*  @ 0x40   (sizeof = 0x48)
      NOTE: GenericFillData fields depend on fillType — no named members in CommonLibF4
      FILL_TYPE enum: kConditions, kForced, kFromAlias, kFromEvent, kCreated,
                      kFromExternal, kUniqueActor, kNearAlias
    TESQuest::aliasesTable         → BSTSmallIndexScatterTable at 0x170 — accessible
    PlayerCharacter::questTargets  → BSTHashMap<TESQuest*, QuestTargetArray*> at 0x7F0

  Quest markers (PrismaHUD compass questDir) — [RESOLVED 2026-06-08, prior "blocked" note was stale]:
    The current workspace CommonLibF4 (E:/F4SE OG/CommonLibF4, xmake) NOW defines the needed types:
      BGSQuestObjective            → DEFINED (RE/B/BGSQuestObjective.h), sizeof 0x28:
        displayText @00, ownerQuest(TESQuest*) @08, targets(TESQuestTarget**) @10,
        numTargets(uint32) @18, index(uint16) @1C, initialized @1E, state @1F, flags @20
      BGSInstancedQuestObjective   → DEFINED (RE/B/...), sizeof 0x10: objective(BGSQuestObjective*) @00,
        instanceID @08, enstanceState(TEnumSet<QUEST_OBJECTIVE_STATE,int32>) @0C   (note the typo "enstanceState")
      PlayerCharacter::objectives  → BSTArray<BGSInstancedQuestObjective> @0x7D9 (DIRECT member, iterable)
      TESQuest::GetAliasedRef(ObjectRefHandle*, uint32 aliasID) → relocation, resolves alias→ref
      QUEST_OBJECTIVE_STATE        → enum: kDormant/kDisplayed(1)/kCompleted/kCompletedDisplayed/kFailed/...
      TEnumSet accessor: .get() returns the enum; operator==(E) works directly.
    STILL forward-declared in CommonLibF4: TESQuestTarget (no header body). [CONFIRMED 2026-06-30 via
      named 1.10.163 Ghidra DB + CommonLibSSE (byte-identical) + FO4 TESCondition==0x8] — the alias is
      NOT at offset 0. Real layout (sizeof 0x60):
        flags(REX::EnumSet<Flag,uint8>) @0x00, pad @0x01, conditions(TESCondition,1 ptr) @0x08,
        alias(uint32) @0x10, pad @0x14, teleportPath @0x18
      Read alias via a local { uint8 flags; uint8 pad[7]; void* conditions; int32 alias; uint32 pad; }
      (static_assert offsetof(alias)==0x10). The earlier "alias @ offset 0" guess was WRONG: it read the
      flags byte (0/1) as the aliasID -> GetAliasedRef(0/1) -> wrong/missing compass blip.
    WORKING RECIPE (HUDDataProvider::BuildCompassJSON): iterate player->objectives, keep kDisplayed,
      obj->ownerQuest->GetAliasedRef(&h, target->alias /*@0x10*/), ref->GetPosition(), bearing=atan2(dx,dy)*rad2deg.
    ✅ [FIXED 2026-06-30] alias offset corrected 0x00->0x10 in QuestTargetLite; built+deployed. In-game
       blip-accuracy re-test still pending but the layout is now binary-verified.

  Utility:
    SimpleLock                   → 8 bytes (2× volatile SInt32, ICriticalSection NOT used)
    TESFullName::GetFullName     → static, takes const TESForm&, returns std::string_view
    TESForm::GetFormByID<T>      → lookup by numeric FormID
    TESForm::GetFormByEditorID<T> → lookup by string EditorID (lazy, safe to call post-load)
```

## SECTION 1 — TESNPC / TESActorBase  (TESBoundAnimObjects.h)

### [CONFIRMED] TESActorBase::HeadRelatedData  (sizeof = 0x18)
```text
  File: CommonLibF4/include/RE/Bethesda/TESBoundAnimObjects.h:359
  Offset on TESNPC: headRelatedData at 0x248
  Fields:
    [0x00]  BGSColorForm*   hairColor
    [0x08]  BGSColorForm*   facialHairColor
    [0x10]  BGSTextureSet*  faceDetails
```

### [CONFIRMED] TESNPC::tintingData
```text
  File: CommonLibF4/include/RE/Bethesda/TESBoundAnimObjects.h:508
  Type:  BGSCharacterTint::Entries*   (FORWARD-DECLARED ONLY — struct body not in CommonLibF4)
  Offset: 0x300
  Note:  PlayerCharacter has a separate tintingData at 0xD00 (PlayerCharacter.h:509)
  ⚠ Cannot iterate tint entries without F4EE runtime layout — see SECTION 4.
```

### [CONFIRMED] TESNPC::GetNPC() — returns TESNPC* (self or base template NPC)

### [CONFIRMED] TESNPC::IsFemale() — checks kFemale actor base flag; safe for player + NPCs

### [CONFIRMED] TESNPC::GetFormRace() — returns TESRace*

### [CONFIRMED] TESNPC::GetHeadParts(bool alternate) — returns std::span<BGSHeadPart*>

## SECTION 2 — TESRace  (TESRace.h)

### [CONFIRMED] TESRace::FaceRelatedData  (sizeof = 0x68)
```text
  File: CommonLibF4/include/RE/Bethesda/TESRace.h:114
  Access: race->faceRelatedData[sex]  (0 = male, 1 = female)
  Offset of faceRelatedData on TESRace: 0x698

  Fields (all are POINTERS — check != null before use):
    [0x00]  BGSCharacterTint::Template::Groups*         tintingTemplate
    [0x08]  BSTArray<BGSTextureSet*>*                   faceDetailsTextureSets
    [0x10]  BGSTextureSet*                              defaultFaceDetailsTextureSet
    [0x18]  BSTArray<TESNPC*>*                          presetNPCs
    [0x20]  BSTArray<BGSColorForm*>*                    availableHairColors    ← confirmed usable
    [0x28]  BGSColorForm*                               defaultHairColor
    [0x30]  BSTArray<BGSHeadPart*>*                     headParts
    [0x38]  BSTArray<BGSCharacterMorph::Group*>*        morphGroups
    [0x40]  BSTArray<BGSCharacterMorph::FacialBoneRegion*>* facialBoneRegions
    [0x48]  BGSCharacterMorph::FacialBoneRegion*        neckFatFacialBoneRegion
    [0x50]  NiPoint3                                    neckFatAdjustmentScales
    [0x60]  BSFixedString                               wrinkleMapPath

  Pattern for hair color iteration:
    auto* fd = race->faceRelatedData[sex];
    if (!fd || !fd->availableHairColors) return;
    for (auto* cf : *fd->availableHairColors) { /* cf is BGSColorForm* */ }
```

## SECTION 3 — BGSColorForm  (TESForms.h line 2913)

### [CONFIRMED] BGSColorForm  (sizeof = 0x48)
```text
  Inherits: TESForm (0x00, size 0x20), TESFullName (0x20)
  FORM_ID:  kCLFM (CLFM record type)
  VTABLE:   VTABLE::BGSColorForm (2 entries in VTABLE_IDs.h)

  Members:
    [0x30]  union { uint32_t color; float remappingIndex; }
    [0x38]  TESCondition conditions
    [0x40]  uint32_t flags

  Color packing (uint32_t color field):
    Bethesda stores face colors as ABGR in the form data.
    Unpack: r = (color >> 16) & 0xFF
            g = (color >>  8) & 0xFF
            b =  color        & 0xFF
            a = (color >> 24) & 0xFF  (often 0 for hair colors)

  APIs:
    cf->GetFormID()                           — uint32_t FormID
    RE::TESFullName::GetFullName(*cf)          — std::string_view (localized name)
    RE::TESForm::GetFormByID<RE::BGSColorForm>(formId) — lookup by ID
```

## SECTION 4 — BGSCharacterTint  (BGSCharacterTint.h)

### [CONFIRMED — FULLY DEFINED]
```text
  BGSCharacterTint::EntrySlot  — enum class (int32_t), 26 values (kForeheadMask → kBeard)
  BGSCharacterTint::Template::Entry  (sizeof = 0x20)
    [0x00]  vtable
    [0x08]  BGSLocalizedString name
    [0x10]  TESCondition chargenConditions
    [0x18]  stl::enumeration<EntrySlot, int32_t> slot
    [0x1C]  const uint16_t uniqueID
    [0x1E]  int8_t flags

  BGSCharacterTint::Template::Group  (sizeof = 0x28)
    [0x00]  BGSLocalizedString name
    [0x08]  uint32_t id
    [0x0C]  uint32_t chargenIndex
    [0x10]  BSTArray<Entry*> entries

  BGSCharacterTint::Template::Groups  (sizeof = 0x18)
    [0x00]  BSTArray<Group*> groups
```

### [⚠ FORWARD-DECLARED ONLY — NO BODY DEFINED]
```text
  BGSCharacterTint::Entries  — class body not in CommonLibF4
    Declared in TESBoundAnimObjects.h:23 and PlayerCharacter.h:65
    Contains applied/live tint state for an NPC/player.
    Known externally: field .count and .entries[] from F4EE source (CharGenInterface.cpp:1086)
    but the full struct layout is not in CommonLibF4.
    To access: requires F4EE source structs or manual reverse-engineering of the binary.
```

## SECTION 5 — BGSCharacterMorph  (BGSCharacterMorph.h)

### [CONFIRMED — FULLY DEFINED]
```text
  BGSCharacterMorph::FacialBoneRegion
    Fields: id (uint32), name (BGSLocalizedString), boneMinMaxMap (BSTHashMap<BSFixedString, TransformMinMax*>)
    Name access: region->name._data.c_str()  (BGSLocalizedString uses _data field)

  BGSCharacterMorph::Group
    Fields: name (BSFixedString), presets (BSTArray<Preset>), presetMaskID (uint16)
    Note: No explicit uint32 id field — use loop index as the morph group id
    Name access: group->name.c_str()  (BSFixedString, direct)

  BGSCharacterMorph::BoneBaseScales — in TESRace::BodyMorphData
  BGSCharacterMorph::BONE_MODIFIER_MIN_MAX — in TESRace::BodyMorphData
```

## SECTION 6 — Actor Value / Survival AVs  (ActorValueInfo, RE::ActorValue)

### [CONFIRMED] RE::ActorValue::GetSingleton()
```text
  Has: fatigue (sleep), but NOT Hunger or Dehydration — those are game-specific forms.
```

### [CONFIRMED] Survival AV lookup pattern:
```text
  static RE::ActorValueInfo* s_avHunger = nullptr;
  static RE::ActorValueInfo* s_avThirst = nullptr;
  // Call once on kGameDataReady or lazily on first use:
  s_avHunger = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>("Hunger");
  s_avThirst = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>("Dehydration");
  // Both return nullptr if vanilla survival mode is not active
```

### [CONFIRMED] GetAV helper pattern (see PrismaHUD HUDDataProvider.cpp):
```text
  Returns 0.0f safely when the AV pointer is null.
```

## SECTION 7 — F4EE Interface Layout  (vendor/F4SEPlugins-original/f4ee/)

### [CONFIRMED] F4EE globals are INLINE OBJECTS (not pointers) in main.cpp:
```text
  CharGenInterface g_charGenInterface;
  BodyGenInterface g_bodyGenInterface;
  BodyMorphInterface g_bodyMorphInterface;   ← offset from address scan / NAF
  OverlayInterface g_overlayInterface;
  StringTable g_stringTable;
  SkinInterface g_skinInterface;
  ActorUpdateManager g_actorUpdateManager;

  Access pattern: reinterpret_cast<BodyMorphInterface*>(base + offset)
  Do NOT dereference — the offset IS the object, not a pointer to one.
  (Commit 63588157 confirmed this — single indirection was wrong, zero is correct.)
```

### [CONFIRMED] BodyMorphInterface vtable layout (19 slots, no base class inheritance)
```text
  Slot 0:  Save
  Slot 1:  Load
  Slot 2:  Revert
  Slot 3:  LoadBodyGenSliderMods
  Slot 4:  ClearBodyGenSliders
  Slot 5:  LoadBodyGenSliders
  Slot 6:  ForEachSlider        ← uses std::function internally; mirror uses void*
  Slot 7:  GetTrishapeMap
  Slot 8:  GetMorphMap
  Slot 9:  SetMorph             ← primary call site
  Slot 10: GetMorph             ← primary call site
  Slot 11: GetKeywords
  Slot 12: GetMorphs
  Slot 13: RemoveMorphsByName
  Slot 14: RemoveMorphsByKeyword
  Slot 15: ClearMorphs
  Slot 16: CloneMorphs
  Slot 17: GetMorphableShapes
  Slot 18: ApplyMorphsToShapes
  Slot 19: ApplyMorphsToShape   (not in current mirror — add if needed)
  Slot 20: UpdateMorphs

  Known offsets (g_bodyMorphInterface RVA from f4ee.dll base):
    Checksum 0x5A9E430 (v1.6.18): RVA = 0xFB440  ← NAF-confirmed
    Checksum 0x59F7081 (unknown):  RVA = 0xF4DF0  ← BEST GUESS, verify in-game
```

### [CONFIRMED] OverlayInterface vtable layout — MULTIPLE INHERITANCE
```text
  Class: OverlayInterface : public BSTEventSink<TESObjectLoadedEvent>,
                            public BSTEventSink<TESLoadGameEvent>
  ⚠ Own virtual functions start at slot 3 (NOT slot 0) in the primary vtable:
    Slot 0: dtor (scalar/vector deleting destructor — MSVC one slot)
    Slot 1: ~OverlayInterface (second destructor entry — MSVC convention)
    Slot 2: ProcessEvent(TESObjectLoadedEvent&, ...)  ← BSTEventSink<A> virtual
    Slot 3: Save          ← OverlayInterface own virtuals start here
    Slot 4: Load
    Slot 5: Revert
    Slot 6: LoadOverlayMods
    Slot 7: ClearMods
    Slot 8: LoadOverlayTemplates
    Slot 9: AddOverlay    ← primary use
    Slot 10: RemoveOverlay
    Slot 11: RemoveAll
    Slot 12: ReorderOverlay
    Slot 13: ForEachOverlay
    Slot 14: ForEachOverlayBySlot
    Slot 15: ForEachOverlayTemplate
    Slot 16: UpdateOverlays(Actor*)
    Slot 17: UpdateOverlay(Actor*, uint32)
    Slot 18: CloneOverlays
    Slot 19: GetNextUID
    Slot 20: GetOverlayRoot
    Slot 21: GetTemplateByName
    Slot 22: GetOverlayDataByUID
  RVA: discovered at runtime via ScanForInterfaceRVA() in F4EEBridge.cpp
       Log: "[F4EEBridge] OverlayInterface RVA: 0xXXXXX" — hardcode once confirmed.
```

### [CONFIRMED] SkinInterface vtable layout (no base class inheritance)
```text
  Slot 0: dtor
  Slot 1: dtor2
  Slot 2: Save
  Slot 3: Load
  Slot 4: Revert
  Slot 5: LoadSkinMods
  Slot 6: LoadSkinTemplates
  Slot 7: AddSkinOverride     ← primary use
  Slot 8: GetSkinOverride
  Slot 9: GetSkinTemplate
  Slot 10: RemoveSkinOverride
  Slot 11: CloneSkinOverride
  Slot 12: UpdateSkinOverride
  Slot 13: ForEachSkinTemplate
  Slot 14: ClearMods
  RVA: discovered at runtime via ScanForInterfaceRVA() in F4EEBridge.cpp
       Log: "[F4EEBridge] SkinInterface RVA: 0xXXXXX" — hardcode once confirmed.
```

### [CONFIRMED] SimpleLock  (f4se/GameTypes.h)
```text
  sizeof = 8  (two volatile SInt32: threadID + lockCount)
  NOT ICriticalSection (CRITICAL_SECTION = 40 bytes) — different from Windows primitives
```

### [CONFIRMED] Approximate sizeof(BodyMorphInterface) = 320 bytes
```text
  vptr(8) + SimpleLock(8) + unordered_map[2](112) + SimpleLock(8) +
  unordered_map(56) + uint64(8) + uint64(8) + unordered_map[2](112) = 320
```

## SECTION 8 — Utility APIs (confirmed usable from plugin code)

### [CONFIRMED] RE::TESFullName::GetFullName(const TESForm& form, bool strict = false)
```text
  File: FormComponents.h:1351
  Returns: std::string_view
  Works for: BGSColorForm, TESNPC, TESObjectWEAP, etc. (anything inheriting TESFullName)
```

### [CONFIRMED] RE::TESForm::GetFormByID<T>(uint32_t formId)
```text
  Safe to call any time after kGameDataReady.
```

### [CONFIRMED] RE::TESForm::GetFormByEditorID<T>(std::string_view editorId)
```text
  Safe to call after kGameDataReady. Returns nullptr if not found.
  Used for: survival AVs (Hunger, Dehydration), custom game-specific forms
```

### [CONFIRMED] RE::Console::GetPickRef()
```text
  Returns ObjectRefHandle to crosshair target. Updated each frame by engine.
  Used in PrismaHUD BuildPromptJSON().
```

### [CONFIRMED] RE::TESBoundObject::GetActivateText(TESObjectREFR*, BSStringT<char>&)
```text
  Virtual at index 0x60. Returns "Activate [Name]" string.
```

### [CONFIRMED] RE::BSInputEnableManager::EnableUserEvent - per-category input masking (FO4's ToggleControls equivalent)
```text
  File: RE/B/BSInputEnableManager.h
  bool EnableUserEvent(std::uint32_t a_layerID, UserEvents::USER_EVENT_FLAG a_userEventFlags,
                        bool a_enable, UserEvents::SENDER_ID a_senderID);
  REL::VariantID{ 1432984, 2268263 } (OG id 1432984 -> RVA 0x1b21ea0, confirmed present/non-null
  in Ghidra, 2026-07-06 gamepad-input investigation for PrismaUI_F4).

  ControlMap itself has NO ToggleControls-equivalent (no vtable, exhaustive symbol search of the
  full ~199k-function table found nothing matching this shape on ControlMap). This is the real FO4
  analog of Skyrim's ControlMap::ToggleControls - it operates on the same UserEvents::USER_EVENT_FLAG
  enum (e.g. kFighting) shared across keyboard/mouse/gamepad, via a layer system:
  AllocateNewLayer(BSTSmartPointer<BSInputEnableLayer>&, debugName) (RVA 0x1b20a70) gets a layerID,
  then EnableUserEvent(layerID, USER_EVENT_FLAG::kFighting, false, senderID) masks just that
  category. This is the INTENDED mechanism, not a workaround: RE::PlayerControls is itself
  BSTEventSink<UserEventEnabledEvent> (see PlayerControls.h inheritance), i.e. vanilla player
  control is designed to react to this exact call. Companion EnableOtherEvent/OTHER_EVENT_FLAG path
  (RVA 0x1b21fc0) exists for non-user-event input.

  Finer-grained alternative also available with no RE work: RE::PlayerControls (singleton) holds
  direct pointers to each per-action handler (activateHandler, attackHandler [AttackBlockHandler],
  jumpHandler, sneakHandler, sprintHandler, movementHandler, lookHandler, etc.), each deriving from
  PlayerInputHandler -> BSInputEventUser, which carries `bool inputEventHandlingEnabled` at offset
  0x08. E.g. PlayerControls::GetSingleton()->attackHandler->inputEventHandlingEnabled = false
  disables melee attack/power-attack input specifically, leaving movement/look/jump untouched.
  Note ranged-weapon firing is NOT covered by attackHandler - use EnableUserEvent(kFighting, ...)
  for full combat-input coverage instead.

### [CONFIRMED] RE::MenuControls::RegisterHandler fires regardless of native menu state
```text
  File: RE/M/MenuControls.h
  RegisterHandler(BSInputEventUser*) / UnregisterHandler(...), backed by a real
  BSTArray<BSInputEventUser*> handlers member. Dispatch is gated only by the low-level
  BSInputDeviceManager::pollingEnabled switch (bool @ offset 0x35), NOT by ControlMap's
  per-context/menu-focus masking or RE::UI menu-stack state.

  Confirmed via real in-game evidence (docs/findings/falloutchat.md, [FIXED] 2026-06-22 MCM/mod
  hotkey leak-through bug): other mods' MenuControls-registered hotkeys kept firing while a
  PrismaUI view had engine focus (FocusMenu pushed), because BSInputEventUser dispatch has no
  "consume" mechanism - all registered handlers receive every event independently. Vanilla
  screenshotHandler/quickSaveLoadHandler (also MenuControls members) go through the identical path
  and are known to work with zero menus open, corroborating this.

  Safe hook point for any mod wanting input regardless of menu/focus state (e.g. gamepad capture
  for a UI overlay): register your own BSInputEventUser via RegisterHandler - it's additive to the
  handlers array, not a single-owner slot, so multiple mods (or plugins) can coexist on it without
  stepping on each other.
```

## SECTION 9 — Known CommonLibF4 Gaps / Forward-Declared-Only Types
```text
These types appear in headers but have no struct body defined.
Do NOT attempt to dereference members — will compile but crash or give garbage.

  BGSCharacterTint::Entries         — see SECTION 4
  RE::msvc::function<R(Args...)>    — layout-only ABI stub, no constructors
  BSContainer::ForEachResult        — empty enum stub, unusable as return value
```

## SECTION 10 — BSScript / Papyrus VM Access Pattern

### [CONFIRMED] DO NOT USE IVirtualMachine::ForEachBoundObject
```text
  Reason: takes RE::msvc::function which has no constructors — can't pass lambdas.
```

### [CONFIRMED] Correct VM access pattern:
```text
  auto* vm = RE::GameVM::GetSingleton()->GetVM().get();
  auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(vm);
  RE::BSAutoLock lock(concreteVM->attachedScriptsLock);
  auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(
      form->GetFormType(), form);
  auto it = concreteVM->attachedScripts.find(handle);
  if (it != concreteVM->attachedScripts.end()) {
      for (auto& attached : it->second) {
          auto* obj = attached.get();
          auto* prop = obj->GetProperty(RE::BSFixedString("PropertyName"));
          if (prop) { /* read: RE::BSScript::get<float>(*prop); write: *prop = 1.0f; */ }
      }
  }
```

## SECTION 11 — PrismaUI_F4 API (IVPrismaUI* interface hierarchy)
```text
Source: E:\F4SE OG\Prisma\PrismaHUD_F4\src\PrismaUI_F4_API.h
```
```text
NOTE: Not part of CommonLibF4 — this is the PrismaUI_F4 plugin's public API.
Documented here because the correct interface version is the #1 source of
type-mismatch bugs across all Prisma consumer plugins.
```

### [CONFIRMED] IVPrismaUI4 — extends IVPrismaUI3 — is the current required interface
```text
  Use IVPrismaUI4* for g_api and all SetAPI() parameters.
  Key method added in IVPrismaUI4:
    virtual void BindUIEvent(PrismaView view, const char* functionName,
                             JSListenerCallback callback) noexcept = 0;
    virtual void EnumerateViews(ViewEnumCallback callback, void* userdata) noexcept = 0;
```

### [CONFIRMED] Threading model — CRITICAL:
```text
  BindUIEvent  callbacks → fire on GAME THREAD  (safe for RE:: access, game data)
  RegisterJSListener callbacks → fire on ULTRALIGHT THREAD (NOT safe for RE:: access)
  Rule: Any listener that reads/writes game state (actor values, RE:: globals, layout
        save, HUDLayoutManager) MUST use BindUIEvent, not RegisterJSListener.
```

### [CONFIRMED] Call timing rules:
```text
  RequestPluginAPI<IVPrismaUI4>() → call on kGameDataReady (not F4SEPlugin_Load)
  CreateView()                    → call on kPostLoadGame or kNewGame (not kGameDataReady)
  RegisterJSListener / BindUIEvent → call inside OnDomReadyCallback only
  Invoke / InteropCall            → only after DOM ready
```

### [CONFIRMED] PrismaView handle:
```text
  Type: uint64_t. Value 0 = invalid.
  Always call IsValid(view) after game reload before using a stored handle.
  Recreate on each kPostLoadGame/kNewGame — do not cache across reloads without check.
```

## MAINTENANCE RULES (Claude must follow these)
```text
- Update "Last updated:" date at the top whenever this file is edited
- Add new confirmed entries with [CONFIRMED] prefix and the source file:line
- Add unresolved gaps with [⚠ FORWARD-DECLARED ONLY] or [❓ UNCONFIRMED]
- Do NOT add guesses — only entries verified by reading the actual header
- When a gap is resolved, replace the ⚠ entry with [CONFIRMED]
- If a section exceeds ~15 entries, add a sub-index at the top of that section
```

## SECTION 12 — NiObject / BSGeometry / D3D scenegraph traversal

### [CONFIRMED 2026-06-20] NiObject virtual methods safe for type checking during traversal:
```text
  NiObject::IsNode()      — virtual; returns true for NiNode and all derived nodes
  NiObject::IsGeometry()  — virtual; returns true for BSGeometry and subclasses
  These are safe to call without blind casts. Use to walk scenegraph nodes.
```

### [CONFIRMED 2026-06-20] BSShaderProperty::flags — kPipboyScreen marker:
```text
  Flag for identifying the Pip-Boy screen geometry:  kPipboyScreen = BIT64 << 60
  More reliable than string-matching NiNode names. Check this flag on BSShaderProperty
  to identify the screen surface without assuming node naming.
```

### [CONFIRMED 2026-06-20] BSGeometry::properties[] — shader property array:
```text
  BSGeometry inherits TESObjectREFR-level arrays. properties[] holds BSShaderProperty*
  entries at a confirmed layout offset. Walk with the IsGeometry() check first.
```

### [CONFIRMED 2026-06-20] BSGraphics::Texture (RE/B/BSGraphics.h approx):
```text
  Field 0x00: ID3D11ShaderResourceView* pSRView
  Field 0x08: ID3D11Texture2D*          pTexture2D
  Replacing pSRView redirects what texture the surface renders. Confirm alignment if
  referencing the vendored header from Reference/f4se/.
```

### [CONFIRMED 2026-06-20] RE::BSGraphics::GetRendererData()->device — path to D3D device:
```text
  Does NOT require PrismaUI dependency. Use for any plugin that needs the raw D3D
  device without being a Prisma consumer.
```

### [CONFIRMED 2026-06-20] Interface3D offscreen RT — BLOCKED (no SRV exposed):
```text
  RE::Interface3D::Renderer has Offscreen_Set3D / Offscreen_Enable3D (same path as
  inventory examine mode), but CommonLibF4 does NOT expose the offscreen render
  target's ID3D11ShaderResourceView. Accessing it requires binary reverse-engineering.
  RE::Inventory3DManager only loads item models, not live actors.
  Rule: DO NOT attempt to read a player character into an HTML view without first
  resolving this gap via Ghidra. Use Inventory3DManager for item previews only.
```

## SECTION 13 — Actor AI / ActorValue manipulation

### [CONFIRMED 2026-06-20] RE::ActorValue::GetSingleton()->confidence — writable from C++:
```text
  confidence is a public member on the singleton. Assigning 0.0f forces Cowardly (flee)
  behavior; restoring the original value restores NPC's normal AI package.
  Pattern (FallenWorldAI morale):
    float origConf = actor->GetActorValue(avInfo->confidence);
    actor->SetActorValue(*avInfo, 0.0f);   // trigger flee
    actor->EvaluatePackage();
    // on recovery:
    actor->SetActorValue(*avInfo, origConf);
    actor->EvaluatePackage();
  IMPORTANT: store originalConfidence on FIRST init per actor. Reading confidence a
  second time after setting it to 0.0f gives 0.0f — you've overwritten the original.
```

### [CONFIRMED 2026-06-20] RE::TESCombatEvent — forward-declared only, no GetEventSource():
```text
  TESCombatEvent::GetEventSource() does NOT exist in this CommonLibF4.
  Track combat state via actor->IsInCombat() polled in an update loop instead.
  Do not sink TESCombatEvent — stale sink registrations cause undefined behavior.
```

### [CONFIRMED 2026-06-20] FO4 NPC template inheritance breaks SPID package distribution:
```text
  SPID injects into NPC base records that own their own package list. ~12,000 of ~12,143
  NPC records use "Use Traits" template inheritance — their package list is on the
  TEMPLATE, not the base. SPID reaches only ~6 unique NPCs with self-owned packages.
  Rule: NEVER use SPID + quest alias for real-time per-actor AI behavior that must apply
  to all NPCs. Use actor->SetActorValue() / confidence manipulation directly instead.
```

## SECTION 14 — Build / Logger Gotchas

### [CONFIRMED 2026-06-20] spdlog flush_on — use flush_on(spdlog::level::info), not warn:
```text
  flush_on(warn) buffers all info-level log lines in memory. If the game crashes or
  exits before a warn fires, the buffer is never written to disk.
  Rule: every xmake F4SE plugin should set:
    logger->flush_on(spdlog::level::info);
  in the logger-initialization block (after set_default_logger in F4SEPlugin_Query).
```

### [CONFIRMED 2026-06-19] nlohmann::ordered_json is O(n²) for large collections:
```text
  ordered_json keeps insertion order by linear-scanning for duplicate keys on every
  insert. At 22,958 COBJ records this is ~260M string comparisons — hard-freezes the
  main thread for several seconds.
  Rule: any bulk JSON output (record scan, cache build) must use hand-rolled string
  concatenation, NOT ordered_json. See PrismaCrafting CobjCache.cpp for the 14.5 MB
  in 39ms hand-rolled pattern. nlohmann::json (unordered) is fine for small payloads.
```

### [CONFIRMED 2026-06-20] xmake not found in Bash tool on this machine:
```text
  xmake is only in the PowerShell PATH. Calling it via the Bash tool exits code 127.
  Always use PowerShell for xmake builds and deployment.
```

### [CONFIRMED 2026-06-20] xmake -c flag is invalid for xmake f (configure):
```text
  xmake f -c ... produces an error. Remove -c from all xmake f commands in build scripts.
```

### [CONFIRMED 2026-06-20] package.requires_lock policy — disable in build scripts:
```text
  set_policy("package.requires_lock", true) causes interactive y/n prompts that hang
  background build processes. Remove or set to false in all xmake.lua build scripts.
```

### [CONFIRMED 2026-07-06] CEF 147.0.14 requires the static /MT CRT, not this project's usual /MD:
```text
  cef-147.0.14/cmake/cef_variables.cmake:395 pins CEF_RUNTIME_LIBRARY_FLAG to "/MT". Building
  any CEF-linked xmake target (libcef_dll_wrapper, the subprocess exe, the Phase 2 harness) with
  the project's default /MD causes a CRT mismatch against /MT-built libcef.dll (separate
  heaps/TLS/locale state) that manifests as STATUS_BREAKPOINT (0x80000003) crashes deep inside
  Chromium's C++ runtime-dependent code, immediately on every child-process role (renderer,
  gpu-process), before any of our own code runs. Confirmed by testing PrismaUI_F4_CefSubprocess.exe
  --type=renderer and --type=gpu-process standalone: both crashed identically before the fix,
  neither crashed after adding set_runtimes("MT") to all three targets in xmake.lua.
  Rule: every xmake target that links libcef_dll_wrapper/libcef.dll must call set_runtimes("MT").
  Corollary: precompiled /MD xmake packages (e.g. directxtk, used elsewhere in this project for
  DirectXTK::SaveWICTextureToFile) cannot be mixed into an /MT CEF target without reintroducing the
  same class of mismatch — use raw WIC (IWICImagingFactory etc.) instead for CEF-side D3D texture
  dumps; see src/Cef/Harness/HarnessClient.cpp DumpLastFrameToFile for the pattern.
```

### [CONFIRMED 2026-07-06] CEF's /DELAYLOAD question resolved — not needed, by design:
```text
  Migration plan Phase 1 open question: does /DELAYLOAD (used for Ultralight's 4 DLLs loading from
  a non-system path) extend cleanly to libcef.dll, or does CEF need different handling?
  Answer: the question doesn't apply the way it did for Ultralight, because libcef.dll is never
  linked into PrismaUI_F4.dll itself in this architecture — it's isolated to the standalone
  subprocess exe (PrismaUI_F4_CefSubprocess.exe) and, eventually, whatever process calls
  CefInitialize (the Phase 2 harness today; the framework DLL itself in Phase 3). Both of those
  are separate executables with libcef.dll and its full resource set (locales/, *.pak, icudtl.dat,
  v8_context_snapshot.bin, ANGLE/SwiftShader DLLs) sitting directly next to them via ordinary
  implicit linking + same-directory DLL search order — no delay-load tricks needed or used.
  Confirmed empirically: both exes load libcef.dll and reach CefInitialize/CefExecuteProcess
  successfully with plain add_links("libcef"), no /DELAYLOAD flags, co-located in the SAME output
  directory as PrismaUI_F4.dll (build/windows/x64/release/) with zero conflicts against the
  existing Ultralight DLLs already delay-loaded there.
  Remaining open question for Phase 3 specifically: once CefInitialize is called from INSIDE
  PrismaUI_F4.dll (rather than a separate harness exe), whether libcef.dll should also just be a
  plain implicit link (same answer as above, likely fine) or needs delay-load for some other
  reason (e.g. avoiding load-time cost for game sessions that never touch Prisma UI) - a
  performance/packaging decision, not a functional requirement, unlike Ultralight's case.
```

### [OPEN][HIGH] CEF Phase 2 harness: GPU process exits cleanly after ~400ms, is never relaunched, renderer times out waiting for a GPU channel
```text
  Standalone Phase 2 validation harness (src/Cef/Harness/, disposable, not shipped): CefInitialize
  succeeds, but CefBrowserHost::CreateBrowserSync always returns null. CEF's own log
  (cef_harness_log.txt, LOGSEVERITY_VERBOSE) shows the browser process completing full Chrome-
  runtime bootstrap (profile manager, extensions, component updater — this CEF version defaults to
  "Chrome runtime" style, not legacy "Alloy" — CONFIRMED this is expected/normal for CEF >= M128,
  see chromiumembedded/cef#3685: the Alloy bootstrap was deleted, only Alloy *style* remains, not a
  misconfiguration on our part). A Viz/GPU service process then logs exactly one line
  ("VizNullHypothesis is disabled (not a warning)", viz_main_impl.cc:86 — CONFIRMED via upstream
  Chromium source this is dead A/B-test telemetry with "no functional purpose", tied to
  crbug.com/354724066, fires at the very start of VizMainImpl construction before real GPU service
  init — a red herring, not the actual failure point). ~15s later a DIFFERENT PID (the renderer,
  which depends on the GPU channel) logs "Terminating current process after 15 seconds with no
  connection" (child_thread_impl.cc:902) and self-terminates; browser creation then gives up and
  returns null.

  Ruled out:
  - /MT vs /MD CRT mismatch (see entry above) — fixed, did not resolve this issue.
  - GPU backend/GL choice — tested with --disable-gpu-compositing and separately
    --disable-gpu + --use-gl=swiftshader (surfaced a real but separate self-inflicted error: this
    CEF build's allowed GL implementations are locked to [(gl=egl-angle, angle=default)] only) —
    both reproduce the identical stall, so not GL-backend-specific.
  - Manual --type=renderer / --type=gpu-process invocation without real IPC pipes reliably crashes
    with STATUS_BREAKPOINT after ~5s (expected — no parent-supplied Mojo channel handles) — a
    DIFFERENT failure mode than the real-harness-launched case, don't conflate them.
  - Missing ANGLE/SwiftShader/resource files relative to the separate helper exe — this was the
    leading hypothesis from CEF forum thread #18241 (closely analogous symptom on CEF 88, GPU
    process fails when browser_subprocess_path points at a separate-directory helper exe because
    ANGLE/SwiftShader files aren't discoverable relative to it) — REFUTED by direct evidence below.

  CONFIRMED root-cause data (Process Monitor capture, 2026-07-06, filtered to PID 20968 = the one
  and only --type=gpu-process spawn in the whole run — grep-confirmed no second gpu-process spawn
  ever occurs, i.e. no retry):
  - PID 20968 successfully opens and reads real NVIDIA driver files (nvppex.dll, nvldumdx.dll from
    DriverStore), vk_swiftshader.dll, dxil.dll, dxcompiler.dll, and NVIDIA's own DX shader cache
    (%LOCALAPPDATA%\NVIDIA\DXCache\04d1a93ba15005ef.nvph) — ALL resource/DLL/driver files load
    successfully. This directly refutes the forum-thread-#18241 missing-resources hypothesis.
  - PID 20968 then CLOSES the .nvph cache file and calls Process Exit with Exit Status: 0 (clean,
    non-error exit) only ~400ms after being spawned (created 2:24:34.463 PM, exited 2:24:34.867 PM).
    This is not a hang and not a crash — it's a normal, successful-looking process lifecycle that
    simply ends without anything else picking up afterward.
  - No second --type=gpu-process process is ever spawned to replace it. The renderer process (a
    separate PID) is left waiting for a GPU channel from a GPU process that no longer exists, and
    times out at the 15s mark as a direct consequence.

  Working theory (not yet confirmed): PID 20968's short lifecycle (touch NVIDIA driver + DX shader
  cache, then exit 0) matches the shape of Chromium's "GPU info collection" sub-process pattern —
  a deliberately short-lived process spawned just to probe GPU/driver capabilities, which normally
  exits once done, expecting the browser to then launch a SEPARATE persistent GPU/Viz process using
  that info. If that second, persistent launch is the piece missing or misconfigured in our minimal
  CefSettings (as opposed to a full cefclient-style app), that would explain every symptom above
  precisely: clean exit (not a bug in the info-collection process itself), no retry (browser isn't
  treating this as "GPU process died, relaunch it" because it was never meant to be the persistent
  one), and the renderer timing out (waiting for a channel from a persistent GPU process that never
  gets created). Not yet checked: whether CefSettings or a command-line switch controls/skips this
  info-collection step, or whether our CefBrowserProcessHandler needs to implement something
  (e.g. an OnGpuProcess-related callback) that a minimal CefApp subclass currently doesn't.

  Also checked and NOT the cause: CEF's official GitHub issue tracker and forum have no exact
  match for this symptom (searched "CreateBrowserSync returns null", "Viz process hang",
  "VizNullHypothesis", "no connection" + child_thread_impl, OSR + GPU timeout — nothing conclusive).

  Version bisection result (2026-07-06): rebuilt libcef_dll_wrapper/subprocess/harness against CEF
  138.0.62+g6981a09+chromium-138.0.7204.310 (stable channel, 9 major versions older than 147.0.14,
  62 patch revisions of bake time) via the now-parameterized setup-cef-sdk.ps1
  (-ShortVersion/-FullVersion/-Sha1), both versions kept side by side under build/cef-<version>/.
  IDENTICAL failure: CreateBrowserSync still returns null, log still shows the same
  VizNullHypothesis-then-silence pattern from the GPU/Viz process. This RULES OUT a CEF-147-specific
  regression — the bug reproduces across a 9-major-version gap, so it is almost certainly in this
  project's own CefSettings/CefApp/CefClient configuration, not a CEF version bug. (xmake.lua's
  CEF_VERSION local is currently set to "138.0.62" for this test; revert to "147.0.14" — both are
  already extracted, so this is a one-line change — once the actual fix is found, since 147 has no
  known reason to be worse than 138 once the real bug is fixed.)

  This exhausts the full agreed diagnostic sequence (Process Monitor -> CEF issue tracker search ->
  version bisection) without finding a fix. All three were genuinely informative (precise failure
  mechanism via Procmon, ruled out a documented CEF bug via the tracker search, ruled out a
  version-specific regression via bisection) but none resolved it. Next steps need to go deeper into
  our own embedding code specifically: diff every CefSettings field and CefApp/CefClient override
  against a known-working minimal OSR reference embedder (not cefclient/cefsimple, which are
  windowed — need an actual OSR reference, e.g. cef-mixer or cef-spout mentioned in earlier Phase 1
  research), since the "GPU info-collection process exits cleanly, persistent GPU process never
  follows" theory points at something structural missing from a from-scratch CefApp subclass that
  a fuller reference implementation would have (a CefBrowserProcessHandler override is the leading
  suspect - our PrismaCefApp currently only implements CefApp + CefRenderProcessHandler, with no
  CefBrowserProcessHandler at all).

  ---
  CefBrowserProcessHandler attempt (2026-07-06, per user direction): compared against
  tests/cefsimple/simple_app.cc (the canonical minimal CEF embedder in the vendored distribution).
  Confirmed a REAL, previously-missing piece: every reference embedder (cefsimple included) creates
  its first browser from CefBrowserProcessHandler::OnContextInitialized(), not immediately after
  CefInitialize() returns. CefInitialize() returns once basic init is done but BEFORE the async
  browser-context setup (which owns GPU-process wiring) actually completes — our harness was calling
  CreateBrowserSync() immediately after CefInitialize(), before that context was ready. This is a
  genuine race, not a red herring.

  Implemented: PrismaCefApp now also implements CefBrowserProcessHandler
  (GetBrowserProcessHandler() -> this), with an OnContextInitialized() override that invokes an
  injectable std::function so the harness can gate CreateBrowserSync on it (see
  src/Cef/Subprocess/PrismaCefApp.h/.cpp, src/Cef/Harness/main.cpp). Retested against CEF 147.0.14
  (the shipping version, switched back from the 138.0.62 bisection build - both remain available
  under build/cef-<version>/).

  Result: PARTIAL improvement, not a full fix.
  - OnContextInitialized DOES now fire correctly (confirmed via the harness's own log line) -
    the race is real and this part of the fix is confirmed correct.
  - The prior failure mode (15s hang -> GPU-process-exits-cleanly -> renderer timeout -> harness
    access-violation crash, exit code -1073741819) is GONE. This is a real, working improvement,
    not noise.
  - CreateBrowserSync now returns null IMMEDIATELY (no hang) after OnContextInitialized fires,
    with exit code 1 (clean process exit, not a crash) instead of the crash exit code.
  - CEF's own verbose log has NO error/warning at all around this point (grepped for
    ERROR/FATAL/CreateBrowser/browser_host/alloy_browser - zero matches near the failure) - the
    failure is now silent on the CEF-log side too, just a fast nullptr return instead of a slow one.

  STATUS UPDATE -- RESOLVED (2026-07-06, same session, per user direction to keep going): compared
  CefWindowInfo/browser-creation call against tests/cefclient/browser/osr_window_win.cc (a REAL OSR
  reference, unlike windowed cefsimple) and found two concrete, confirmed-necessary differences:

  1. **CreateBrowserSync -> async CreateBrowser.** cefclient's OSR path explicitly uses the
     asynchronous CefBrowserHost::CreateBrowser (comment: "Create the browser asynchronously"), not
     CreateBrowserSync. The actual browser reference arrives later via
     CefLifeSpanHandler::OnAfterCreated on CEF's UI thread.
  2. **CefWindowInfo::SetAsWindowless needs a REAL HWND, not nullptr.** Despite the API doc implying
     the parent is optional ("If |parent| is not provided... some functionality... may not function
     correctly"), cefclient's OsrWindowWin::CreateBrowser always creates a real (event-pump-capable)
     window first (Create(parent_hwnd, rect)) and passes ITS hwnd_ to SetAsWindowless -- never
     nullptr. Its own OsrRenderHandlerWinD3D11 constructor also requires that same real hwnd_. This
     had been missed on both counts: this harness passed nullptr and used the sync call.

  Fix implemented: main.cpp now creates a real, never-shown Win32 window
  (CreateHiddenParentWindow, WS_POPUP, DefWindowProcW, own window class) purely to give CEF's OSR
  pipeline a valid HWND for monitor/DPI identification, passes it to SetAsWindowless, and uses async
  CefBrowserHost::CreateBrowser + polls HarnessClient::GetBrowser() (now mutex-guarded, set from
  OnAfterCreated) instead of relying on CreateBrowserSync's return value.

  RESULT: FULL SUCCESS, every step of the harness now passes end to end against CEF 147.0.14 (the
  shipping version):
    CefInitialize -> OnContextInitialized fires -> hidden window created -> CreateBrowser (async)
    succeeds -> OnAcceleratedPaint fires with a real shared D3D11 texture (OpenSharedResource1
    succeeds, confirmed via a screenshot showing the actual test page content rendered correctly)
    -> CefProcessMessage round trip works (browser -> renderer -> browser, sent 42 got back 43)
    -> clean CefShutdown. Exit code 0.

  Root cause, in full: TWO independent bugs compounded into one confusing symptom. (a) Calling
  CreateBrowserSync before OnContextInitialized fired was a genuine race that produced the
  15s-hang-then-crash failure mode. (b) Even after fixing (a), using CreateBrowserSync at all
  (instead of async CreateBrowser) combined with a null parent HWND (instead of a real one, as
  windowless rendering's OSR/GPU pipeline apparently still expects even though it never creates a
  visible window) caused the fast, silent, log-free null return. Neither alone was sufficient to
  explain the full behavior; both needed fixing.

  Phase 2 is now COMPLETE. Phase 3 (in-game hello-world milestone) can proceed using this same
  pattern: CefBrowserProcessHandler::OnContextInitialized gating, a real (even if never shown/never
  the actual game window) HWND passed to SetAsWindowless, and async CreateBrowser instead of sync.

### [CONFIRMED 2026-07-06] Process-isolation data point -- 3 concurrent CEF browsers, real breakdown captured
```text
  Extended the Phase 2 harness to create 3 concurrent CefBrowserHost::CreateBrowser instances (each
  its own top-level windowless browser, same proven pattern: real HWND + async CreateBrowser +
  OnContextInitialized gate), all sharing one D3D11 device (matching how the real game's
  swap-chain-hook integration will share one device across every Prisma view). All 3 loaded,
  painted via OnAcceleratedPaint, screenshot-verified, and passed independent CefProcessMessage
  round trips (view 0: 42->43, view 1: 43->44, view 2: 44->45 -- confirming each view's IPC channel
  is correctly isolated to its own renderer, not cross-talking). Exit code 0, full PASS.

  Captured the real process breakdown via `Get-CimInstance Win32_Process -Filter
  "Name='PrismaUI_F4_CefSubprocess.exe'"` while all 3 views were alive (had to poll for the
  harness's own "Pausing 10s" log line and fire the query at that instant via a single
  Start-Process+poll-loop PowerShell script -- ScheduleWakeup's granularity was too coarse to hit
  the 10s window reliably, and a naive `Remove-Item $tempFile` line in the same script tripped a
  hook that misfired on the unrelated "E:\F4SE OG" path elsewhere in the same command; dropping the
  explicit cleanup and relying on -RedirectStandardOutput's overwrite-on-open behavior avoided it).

  Result -- 7 total PrismaUI_F4_CefSubprocess.exe processes for 3 views:
    1x --type=gpu-process                                    (shared, not per-view)
    1x --type=utility --utility-sub-type=network.mojom.NetworkService   (shared, not per-view)
    1x --type=utility --utility-sub-type=storage.mojom.StorageService   (shared, not per-view)
    4x --type=renderer (--renderer-client-id=5,6,7,8)         (one per view, PLUS one extra)

  So: gpu-process and both utility services are correctly singleton/shared regardless of view
  count (matches expectation -- these are browser-instance-wide services, not per-browsing-context).
  But EVERY view got its OWN independent renderer process -- 3 views produced 3 renderer processes
  (client-ids 6/5/7 map to the 3 CreateBrowser calls), plus a 4th (client-id=8) which is almost
  certainly Chromium's "spare renderer process" pre-warm optimization (SpareRenderProcessHostManager
  keeps one idle renderer ready for the next navigation/browser-creation to reduce launch latency) --
  not a second process for the same view.

  ### IMPORTANT CAVEAT -- this does NOT yet answer the actual open design question
  This experiment used 3 INDEPENDENT top-level CefBrowserHost::CreateBrowser calls (3 separate
  browsers), not 3 <iframe> elements inside ONE shared shell browser. Independent top-level browsing
  contexts getting independent renderer processes is normal, expected Chromium behavior regardless
  of same-origin-ness -- it says nothing about whether the shared-shell architecture actually locked
  in for Phase 3 (one CEF browser, N dynamically-created file:// iframes, per the design proposal)
  will consolidate those iframes into one renderer or still spawn one per iframe (out-of-process
  iframes / site isolation policy). The design proposal's real open item ("whether Chromium actually
  process-isolates the dynamically created file:// iframes... affects how much of the process-count
  win is real") is about THAT scenario specifically, and is still unanswered -- this result is
  useful supporting data (confirms the harness/pattern scales to multiple concurrent browsers
  cleanly) but is not a substitute for testing actual iframes-in-one-shell.

  Not yet tried: (1) checking Windows Defender/AV interference with child-process named-pipe/Mojo
  channel setup (real possibility on a daily-driver machine with active AV); (2) an older/more
  stable CEF version than 147.0.14 (chromium-147.0.7727.138 is unusually high-numbered for
  "current" — may be a bleeding-edge/dev-channel build with an active regression); (3) checking
  kolebynov/PrismaUI's cef-migration branch or upstream CEF's own issue tracker for "Viz process
  hang"/"no connection" reports on recent versions; (4) running Process Monitor during a harness
  run to see exactly what the Viz process is blocked on (a named pipe, a file open, a registry
  read) at the point it stops logging.
  Blocks: Phase 3 (the in-game "hello world" milestone) cannot proceed until browser creation
  succeeds standalone first, per the migration plan's own phase-ordering rationale.
```

### [CONFIRMED 2026-07-06] Phase 3 game-spike blocked: commonlibf4 (/MD) cannot link into a /MT CEF target
```text
  Wrote the actual in-game Phase 3 target (PrismaUI_F4_CEF_GameSpike: F4SE plugin DLL getting the
  game's real ID3D11Device/HWND via RE::BSGraphics::GetRendererData(), installing its own D3D11
  Present/ResizeBuffers hook, creating the shell browser with the real HWND, compositing via a
  hand-rolled fullscreen-triangle D3D11 shader blit instead of DirectXTK -- avoiding the DirectXTK
  vs CEF CRT conflict already solved once in the Phase 2 harness). xmake target added with
  add_deps("commonlibf4", "libcef_dll_wrapper") + set_runtimes("MT") (required for libcef.dll).

  First build attempt hit a NEW, more fundamental CRT conflict than the Phase 2 one: commonlibf4
  (and its own deps commonlib-shared/spdlog) are built /MD -- there is no override for this at the
  point where a consuming target depends on them. Setting set_runtimes("MT") on the GameSpike
  target only affects ITS OWN source files; xmake does not transparently rebuild commonlibf4/spdlog
  as a second /MT variant just because a new dependent target asks for one -- it reuses the single
  already-built /MD commonlibf4.lib/spdlog.lib. Result: real linker errors, not just warnings --
    - LNK2005 duplicate symbol definitions (std::basic_streambuf/basic_ios members defined in BOTH
      msvcprt.lib's import thunks AND libcef_dll_wrapper.lib's statically-linked /MT CRT objects)
    - LNK2019 unresolved externals for __imp_towupper, __imp__gmtime64_s, __imp__localtime64_s,
      __imp__time64, __imp_remove, __imp_rename, __imp__stat64i32, __imp__filelengthi64,
      __imp__mkdir -- these are /MD-style DLL-import thunks that commonlibf4.lib/spdlog.lib were
      compiled expecting msvcrt DLLs to provide; an /MT-linked binary has no msvcrt.dll import table
      entry for them at all, so they go unresolved.
  This is NOT the same bug as the Phase 2 DirectXTK conflict (that was one precompiled /MD package
  vs /MT CEF, solved by writing raw D3D11/WIC code instead of using the package). commonlibf4 is not
  a package that can simply be swapped out -- RE::BSGraphics::GetRendererData() (the whole reason
  for depending on it: reusing the game's real device/HWND/swapchain instead of inventing one) only
  exists there. Confirmed empirically (real xmake build attempt), not inferred from first principles.

  Two viable paths forward, not yet chosen or implemented:
    (a) Two-DLL split: a "sensor" DLL (links commonlibf4 normally at /MD, gets the device/HWND/
        swapchain, installs the Present hook, does compositing) loads a separate "CEF host" DLL
        (links CEF at /MT, no commonlibf4 dependency at all, owns CefInitialize/browser creation/
        OnAcceleratedPaint) via LoadLibrary + GetProcAddress against a thin exported C-ABI (raw
        HANDLEs/PODs only, no STL/CRT types crossing the boundary) -- exactly the same pattern this
        project already uses for RequestPluginAPI (PrismaUI_F4.dll exports it, dependent plugins
        resolve it via GetProcAddress). This sidesteps the CRT question entirely: each DLL only
        needs internal consistency with its own dependencies, and a raw exported-function boundary
        is the officially-supported way to mix CRTs on Windows.
    (b) Force a second, /MT-specific build of commonlibf4 (and spdlog) specifically for this target
        (a duplicate xmake target definition pointed at the same sources with set_runtimes("MT")
        forced, and/or an aliased add_requires("spdlog", {alias=..., configs={...}}) requesting an
        /MT variant if the spdlog xrepo recipe honors a runtime override). Keeps one DLL, but doubles
        commonlibf4's build time for every build touching this target and is unverified whether
        spdlog's package recipe actually supports a runtime override this way.
  Recommendation leans toward (a): smaller blast radius (zero changes to the already-building
  commonlibf4/spdlog dependency graph), reuses an already-proven pattern in this exact codebase, and
  the CEF host DLL becomes a natural home for ALL the shell/browser-management logic the eventual
  real PrismaUI_F4.dll migration will need anyway (Phase 5's API surface work).

  Everything up to this point (shell/ Vite+TS project with 3 sandboxed iframes, GameSpikeClient.h/
  cpp shared-texture-into-game-device import + per-iframe ping/pong tracking, Compositor.h/cpp
  hand-rolled D3D11 blit, GameSpikeMain.cpp F4SE entry + hook + orchestration, xmake target) is
  written and compiles/links fine in isolation -- only the final link step combining commonlibf4 +
  libcef_dll_wrapper into one binary fails. None of it needs to be thrown away for path (a); it just
  needs to be split across the DLL boundary at the CEF-touching vs RE::-touching seam.
  Blocks: Phase 3's in-game milestone (shell+iframes rendering, process-count comparison against the
  3-standalone-browsers data above, per-iframe round-trip confirmation) until the split is done.
```

### [CONFIRMED 2026-07-06] Phase 3 game-spike CRT blocker resolved: two-DLL split, both link cleanly
```text
  Chose path (a) from the entry above: two-DLL split reusing the RequestPluginAPI GetProcAddress
  pattern PrismaUI_F4.dll already uses for its own dependents. Confirmed empirically -- both targets
  now build and link with ZERO errors (`xmake build PrismaUI_F4_CEF_Host` then
  `xmake build PrismaUI_F4_CEF_GameSpike`, both clean, no LNK2005/LNK2019, only a benign D9025
  /EHs-overridden-by-/EHa warning the main target already suppresses).

  Final architecture -- WHO OWNS WHAT AND WHY:

  PrismaUI_F4_CEF_Host.dll  (src/Cef/GameSpike/Host/, xmake target "PrismaUI_F4_CEF_Host")
    - /MT (matches libcef.dll's static CRT). Links libcef_dll_wrapper + libcef + d3d11 + dxgi.
    - Zero commonlibf4/RE::/F4SE:: dependency, and deliberately NO spdlog either -- spdlog.lib is
      ALSO a precompiled /MD package, same conflict class as commonlibf4 (confirmed by inspection,
      not tested to failure, since it was avoided from the start this time). Logs via a tiny
      hand-rolled HostLog.h/.cpp (fopen/fwprintf + OutputDebugStringW) instead -- matches how the
      Phase 2 harness/subprocess targets already avoid logger:: for the same underlying reason.
    - Owns: PrismaCefApp (shared with Harness/Subprocess), GameSpikeClient (CEF client: shell
      browser lifecycle, OnAcceleratedPaint shared-texture import INTO A DEVICE HANDED IN FROM THE
      SENSOR, per-iframe CefProcessMessage ping/pong tracking), CefInitialize/CreateBrowser
      orchestration, the Phase 3 validation checks (per-iframe round trip + subprocess-count log).
    - Exports 3 functions, extern "C", POD-only signatures (the whole point -- nothing here needs to
      be CRT-safe to cross the boundary because nothing STL-shaped ever does):
        bool  CefHost_Init(void* gameDevice, HWND gameHwnd)   -- starts CEF init + shell browser
                                                                  creation on Host's own background
                                                                  thread; returns quickly, does not
                                                                  block the caller.
        void  CefHost_NotifyResize(int width, int height)     -- forwarded from the Sensor's
                                                                  ResizeBuffers hook.
        void* CefHost_GetLatestFrameSRV()                     -- returns an ALREADY-AddRef'd
                                                                  ID3D11ShaderResourceView* (or
                                                                  nullptr). COM refcounting is
                                                                  CRT-agnostic by design, so handing
                                                                  a raw AddRef'd interface pointer
                                                                  across an /MD-vs-/MT DLL boundary
                                                                  is the officially-supported way to
                                                                  do this -- the caller just needs to
                                                                  Release() it eventually (a
                                                                  ComPtr::Attach in the Sensor's
                                                                  Present hook handles that
                                                                  automatically each frame).
    - `gameDevice` itself is the same trick in reverse: ID3D11Device* is a plain COM vtable, ABI-
      stable regardless of which CRT compiled which side, so the Sensor can hand its
      RE::BSGraphics-obtained device straight across as a void* with zero marshalling.

  PrismaUI_F4_CEF_GameSpike.dll  (src/Cef/GameSpike/Sensor/, xmake target "PrismaUI_F4_CEF_GameSpike"
                                   -- same output filename as before the split; this is the actual
                                   F4SE plugin users enable/disable)
    - Default /MD (matches commonlibf4/spdlog/minhook, same as the main PrismaUI_F4 target). Never
      links CEF at all, so there's no /MT requirement on this side of the split.
    - Owns: F4SE plugin boilerplate, RE::BSGraphics::GetRendererData() (real game device/context/
      HWND/swapchain), the D3D11 Present/ResizeBuffers MinHook vtable hook (own hook state, does NOT
      reuse the main PrismaUI_F4 target's Hooks::D3DHooks class -- still isolated per the Phase 0/1
      "never touch the shipping target" rule), the hand-rolled fullscreen-triangle D3D11 compositor
      (Sensor/Compositor.h/.cpp, unchanged from before the split -- it never needed CEF headers).
    - At kGameDataReady: installs the D3D hook, then on a background thread resolves its own module
      directory, LoadLibraryW's PrismaUI_F4_CEF_Host.dll by ABSOLUTE path (avoids relying on
      ambient DLL search order -- the two DLLs are deployed side by side in the same F4SE/Plugins
      folder), GetProcAddress's the 3 exports above, and calls CefHost_Init(device, hwnd).
    - Present hook: every frame, calls CefHost_GetLatestFrameSRV(), wraps the result in a ComPtr via
      Attach (takes ownership of the AddRef the Host already did), and feeds it to Compositor::Draw
      if non-null. ResizeBuffers hook: forwards the new size via CefHost_NotifyResize.

  Both DLLs deploy to the same Data/F4SE/Plugins/ folder (xmake after_build on the Host target also
  copies CEF's runtime DLLs/locales/pak files + the subprocess exe + shell/dist/ alongside it; the
  Sensor target's after_build just copies itself). KNOWN CONFLICT, still not solved, only documented:
  if the real PrismaUI_F4.dll is also loaded, both plugins hook the same Present/ResizeBuffers vtable
  slots -- MinHook's MH_CreateHook returns MH_ERROR_ALREADY_HOOKED for whichever installs second (a
  soft, logged failure, not a crash, but only one of the two actually renders). Disable
  PrismaUI_F4.dll in the modlist while testing this spike.

  Not yet done: actually launching the game and confirming the shell+iframes render (this needs the
  live modlist's active profile to have PrismaUI_F4_CEF_GameSpike enabled and PrismaUI_F4 disabled,
  and a human to either eyeball the render or wait for the automatic Phase 3 validation checks
  (per-iframe ping/pong + subprocess count) to land in gamespike_host_log.txt next to
  PrismaUI_F4_CEF_Host.dll) -- deferred pending the user's own MO2 profile change and play session,
  not attempted unattended since neither DLL/host log file can confirm the compositor actually drew
  anything ON SCREEN, only that CEF's own pipeline ran.
```

### [CONFIRMED 2026-07-06] xSE PluginPreloader crashes hard if CEF runtime files sit flat in F4SE\Plugins\
```text
  First real in-game launch attempt after the two-DLL split: game never got past F4SE loading (MO2
  shows the F4SE loader step, then nothing -- no crash dialog, no game window). No
  PrismaUI_F4_CEF_GameSpike.log was ever created in Documents\My Games\Fallout4\F4SE\, meaning the
  crash happened BEFORE F4SE_PLUGIN_LOAD even ran for our own plugin.

  IMPORTANT CONTEXT (per user, 2026-07-06): xSE PluginPreloader is not even supposed to be in this
  modlist -- it was meant to have been removed a while back because it conflicts with Addictol. It
  never actually got removed. That means PluginPreloader itself is a known-fragile/deprecated
  component in this specific modlist, not just an innocent bystander that any new plugin's files
  happened to trip over -- worth remembering for any FUTURE F4SE-loader-stage crash on this modlist
  (never launches, no plugin log created): check for xSE PluginPreloader's presence/log first,
  before assuming the newest plugin under test is at fault.

  Root cause, confirmed via `xSE PluginPreloader.log`: that tool flat-scans Data\F4SE\Plugins\ and
  LoadLibrary's every single .dll file it finds there, alphabetically, treating each as an F4SE
  plugin candidate (standard xSE PluginPreloader behavior -- it exists specifically to pre-load
  plugin DLLs before F4SE's own import-table hook fires, working around anti-cheat/loader-lock
  issues). Log shows it working through the real plugins fine (Addictol, AmmoRemover,
  ArmorPenetrationBugFix, AutoBeam, BackportedBA2Support, BakaFramework, BetterConsole, BodyPartsUI,
  BulletCountedReload, CCExtenderF4, CellOffsetGenerator...), then:
    [17:18:14.409] Library 'chrome_elf.dll' loaded from '...\Data\F4SE\Plugins\chrome_elf.dll'
    [17:18:14.409] Library 'chrome_elf.dll' unloaded
  ...and the log stops there. Nothing after -- the process died loading/unloading chrome_elf.dll (or
  whichever file the alphabetical scan would have hit next) outside of any real Chrome/CEF process
  context, which chrome_elf.dll is not designed to tolerate (it does crash-reporting/user-data-dir
  setup on load that assumes a real Chromium process lifecycle around it).

  This was a deployment bug, not a code bug: PrismaUI_F4_CEF_Host.dll, PrismaUI_F4_CefSubprocess.exe,
  and every CEF runtime file (libcef.dll, chrome_elf.dll, libEGL.dll, libGLESv2.dll, dxil.dll,
  dxcompiler.dll, vk_swiftshader.dll, vulkan-1.dll, d3dcompiler_47.dll, *.pak/*.bin/*.dat, locales/,
  shell/) had all been copied directly into Data\F4SE\Plugins\, same folder as
  PrismaUI_F4_CEF_GameSpike.dll itself -- exactly the folder xSE PluginPreloader flat-scans.

  Fix: only the real F4SE plugin (PrismaUI_F4_CEF_GameSpike.dll, the Sensor) belongs directly in
  Plugins\. Everything else moved one level down into a
  Plugins\PrismaUI_F4_CEF_GameSpike_Runtime\ subfolder, which the preloader does not recurse into
  (F4SE plugin discovery, like xSE PluginPreloader's scan, is non-recursive/single-directory-level
  by convention). SensorMain.cpp's LoadLibraryW call updated to include that subfolder segment;
  HostExports.cpp needed NO change at all, since it already resolves the subprocess exe/shell/index
  .html paths relative to "wherever Host.dll's own module file sits" (GetModuleFileNameW) -- moving
  Host.dll into the subfolder makes those resolve correctly automatically. xmake.lua's
  PrismaUI_F4_CEF_Host after_build deploy step updated to copy into the runtime subfolder instead of
  flat into Plugins\ for future rebuilds. The already-deployed live-modlist mod folder was also
  reorganized by hand (not just documented) so the fix didn't require waiting on a full rebuild.

  General rule worth remembering for ANY future F4SE plugin that ships supporting loose files
  (not just this CEF work): Data\F4SE\Plugins\ is NOT a safe place for arbitrary non-plugin DLLs --
  both F4SE's own plugin loader and xSE PluginPreloader (if installed) will attempt to load them.
  Always put non-plugin support files in a subfolder.
```

### [CONFIRMED 2026-07-06] libcef.dll in a subfolder fails to load (GLE 126) -- DLL dependency search path, NOT antivirus
```text
  After moving CEF's runtime into the PrismaUI_F4_CEF_GameSpike_Runtime\ subfolder (previous entry),
  the Sensor DLL's LoadLibraryW on Host.dll -- and on libcef.dll directly -- kept failing with
  GLE=126 (ERROR_MOD_NOT_FOUND). An earlier guess blamed flaky Windows Defender real-time scanning
  (libcef.dll is a 265MB unsigned freshly-written binary). THAT GUESS WAS WRONG. Proven wrong by
  adding staged probe diagnostics to the Sensor's load path, whose in-game log showed:
    Pre-flight: hostPath exists=true libcefPath exists=true          (files ARE visible to the game)
    Probe LoadLibraryExW(DONT_RESOLVE_DLL_REFERENCES) on libcef.dll: OK   (the FILE is fully valid)
    Real LoadLibraryW on libcef.dll (standalone): FAILED (GLE=126)        (full load fails)
  The DONT_RESOLVE_DLL_REFERENCES flag loads the PE image WITHOUT resolving any imports; it
  succeeding while the real full load fails proves the failure is in resolving a DEPENDENCY, not in
  the file itself. libcef.dll's own hard (non-delay) deps are KERNEL32/WS2_32/CRYPT32/WINMM/VERSION/
  ntdll/DWrite (all in System32) plus chrome_elf.dll -- which lives ONLY as a sibling in the runtime
  subfolder. When you LoadLibrary a DLL by FULL PATH, Windows does NOT add that DLL's own directory
  to the search path used to resolve ITS dependencies; that dependency search begins at the EXE's
  directory (Fallout4.exe's), where chrome_elf.dll does not exist -> GLE 126 on chrome_elf.dll,
  surfaced to the caller as "libcef.dll not found".

  This also explains the earlier "flakiness" that fed the AV red herring: a standalone PowerShell
  test that loaded chrome_elf.dll FIRST then libcef.dll had both succeed (chrome_elf was already
  resident by base name, so libcef found it); a test that loaded libcef.dll first failed. Same
  deterministic cause, not randomness.

  FIX: SetDllDirectoryW(runtimeDir) before any load. It inserts runtimeDir into the process DLL
  search path (right after the exe dir, before System32), so chrome_elf.dll and libcef's other
  bundled siblings (libEGL/libGLESv2/vk_swiftshader/dxcompiler/dxil) resolve. Left set for the whole
  session deliberately -- libcef loads those siblings lazily throughout its lifetime, not just at the
  initial load. Verified safe against shadowing: the one runtime filename that also exists in
  System32 (d3dcompiler_47.dll) is already resident by base name before we run, and Windows returns
  an already-loaded module by base name regardless of search order, so our copy never shadows the
  system one. (LOAD_WITH_ALTERED_SEARCH_PATH on the Host.dll load is a narrower alternative but only
  covers that one load operation, not libcef's later lazy sibling loads -- SetDllDirectory covers
  both.)

  GENERAL RULE: any time you ship a DLL with its own private sibling DLL dependencies in a subfolder
  and load it by full path, you MUST put that subfolder on the DLL search path (SetDllDirectory /
  AddDllDirectory / LOAD_WITH_ALTERED_SEARCH_PATH) or the sibling deps fail to resolve with a
  misleading "<the DLL you asked for> not found" error that points at the wrong file.
```

### [CONFIRMED 2026-07-06] CEF ES-module entry script silently fails to load over file:// -- use --allow-file-access-from-files
```text
  With the Host DLL finally loading (search-path fix above), CEF came up and the shell's static
  overlay rendered in-game, BUT the 3 plugin-view iframes never appeared and every per-iframe
  CefProcessMessage ping got "no frame named" (GetFrameByName returned null). Root cause: the shell
  is a Vite build whose entry is emitted as <script type="module" crossorigin src="./assets/...js">.
  Chromium BLOCKS module-script fetches from file:// origins (a file:// document is an opaque/null
  origin; ES module loading is subject to CORS, which a null origin fails), so the module -- and
  therefore main.ts, which is what dynamically creates the view iframes -- silently never executes.
  The static overlay worked anyway because it is inline HTML in index.html toggled via injected
  CefFrame::ExecuteJavaScript, which does not depend on the module loading. GetFrameByName found
  nothing because there genuinely were no iframes.

  Two things made this hard to see and both are now fixed:
    1. No CefDisplayHandler::OnConsoleMessage / CefLoadHandler::OnLoadError were implemented, so the
       JS-side failure was completely invisible. ALWAYS wire OnConsoleMessage + OnLoadError on any
       CEF client -- they are the primary window into renderer-side failures. With them added, the
       shell's own console.log ("[PrismaShell] Created 3 view iframes") and each iframe's OnLoadEnd
       (name + URL) now appear in the host log, which is also how the real per-iframe frame names got
       confirmed (prisma-view-0/1/2 -- so GetFrameByName works once the frames actually exist).
    2. FIX: append the "allow-file-access-from-files" command-line switch in
       CefApp::OnBeforeCommandLineProcessing (unconditionally, so it lands on the renderer subprocess
       too -- that's where the module fetch/CORS check happens). This treats file:// origins as able
       to access other file:// content, unblocking the module fetch WITHOUT the broader
       --disable-web-security. Confirmed 2026-07-06: after this, main.ts runs, all 3 iframes render
       (View 0/1/2 visible in-game), and per-iframe ping/pong round trips all pass.
  NOTE for the real migration (not just the spike): shipping views over file:// with modules works
  only with this flag. The cleaner long-term option is a custom CEF scheme handler (e.g. prisma://)
  serving the shell+views from a real registered origin, where modules/fetch/CORS behave normally
  without any security-relaxing flag. Decide that during the Phase 4/5 build-pipeline work.
```

### [CONFIRMED 2026-07-06] Phase 3 process-isolation answer: sandboxed file:// iframes CONSOLIDATE, they do not spawn one renderer each
```text
  THE number the shared-shell architecture decision rests on, finally measured in-game with all
  iframes confirmed rendering (View 0/1/2 visible, per-iframe ping/pong all passing). Captured live
  via Get-CimInstance Win32_Process -Filter "Name='PrismaUI_F4_CefSubprocess.exe'" at the FO4 main
  menu with the shell + 3 iframes active:

    Model                                  gpu  utility  renderer  TOTAL
    Shell only (iframes not yet rendering)  1      2        2        5     (shell main frame + 1 spare)
    Shell + 3 sandboxed file:// iframes     1      2        3        6     <-- the real shared-shell run
    3 separate top-level CefBrowsers        1      2        4        7     (Phase 2 standalone harness)

  The 3 iframes are sandbox="allow-scripts" WITHOUT allow-same-origin, so each gets a unique opaque
  origin. Interpretation of the renderer counts:
    - Going from shell-only (2 renderers = shell main frame + 1 warm spare) to shell + 3 iframes
      (3 renderers) added exactly ONE renderer for THREE iframes -- not three. So the 3 opaque-origin
      iframes were CONSOLIDATED into a single shared renderer process (their common file:// precursor
      origin keeps them same-site enough to share a process), sitting alongside the shell's own
      main-frame renderer and Chromium's one warm spare (SpareRenderProcessHostManager, same spare
      seen in Phase 2). NOT one-renderer-per-iframe.
    - Contrast the separate-browsers model: 3 independent top-level CefBrowsers needed 4 renderers
      (~1 per browser + spare). So at just 3 views the shell model already uses fewer renderers
      (3 vs 4), and because iframe views share a renderer while separate browsers do not, the gap
      widens with view count: ~15 dependent-plugin iframes cost on the order of 1-2 renderers in the
      shell model versus ~15 in the separate-browser model. The core process-count argument for the
      shared-shell architecture is VALIDATED.
    - gpu-process (1) and both utility services (network + storage) are singletons shared across all
      browsing contexts regardless of model -- as expected (browser-instance-wide services).

  Caveat / how to make it airtight if ever needed: the "3 iframes -> +1 renderer" conclusion is
  inferred from the delta against the (separate-run) shell-only count, and Chromium's warm-spare
  timing varies run to run (this run showed 3 renderers, an earlier partial run 2), so exact counts
  wobble by +/-1 spare. To nail the scaling curve definitively, add e.g. 8-10 iframes and confirm the
  renderer count stays flat (~1 shared + shell + spare) rather than climbing linearly -- but the
  1-renderer-for-3-iframes delta plus the 3-vs-4 comparison against separate browsers is already
  strong enough to validate the architecture direction.

  Also confirmed this run: per-iframe IPC isolation holds inside the shared-shell model -- each iframe
  (targeted by CefBrowser::GetFrameByName using the frame names prisma-view-0/1/2 that OnLoadEnd
  revealed) got its OWN correct CefProcessMessage round trip (100->101, 101->102, 102->103), so
  addressing/messaging a specific plugin view still works even though they share a renderer.
```

### [CONFIRMED 2026-07-06] CEF OSR + a real (standard) scheme: sandboxed iframes go OOPIF and render BLANK unless site isolation is disabled
```text
  After moving the shell + plugin views from file:// to a real prisma:// standard scheme, every view
  loaded perfectly (status 200, JS running, console output flowing) but NOTHING appeared on screen --
  a fully blank composited frame. Root cause: under a real standard scheme, Chromium's site isolation
  (site-per-process + IsolateOrigins + especially IsolateSandboxedIframes) pushes each
  sandbox="allow-scripts" (opaque-origin) iframe into its OWN renderer process (OOPIF). In WINDOWLESS
  (OSR) mode, OOPIF content does NOT paint into the PARENT browser's OnAcceleratedPaint shared texture
  -- so the root frame (an empty transparent container) is all that composites -> blank. This did NOT
  happen with file:// because file:// isn't site-isolated the same way, so those iframes stayed
  in-process and composited.
  FIX: force every frame into the one shell renderer process via CefApp::OnBeforeCommandLineProcessing
  (applies to browser + all subprocesses):
    command_line->AppendSwitch("disable-site-isolation-trials");
    // append to any existing --disable-features value, don't clobber it:
    disable-features += "site-per-process,IsolateOrigins,IsolateSandboxedIframes"
  In-process frames composite into the shell's OSR texture (as they did under file://), and it also
  maximizes the shared-shell process-count win. General rule: a windowless/OSR CEF browser that hosts
  its UI as iframes MUST disable site isolation, or the iframes render blank.
```

## [CONFIRMED 2026-07-14] PipboyDataManager — the engine's own Pip-Boy data model (schema decompiled)

`RE::PipboyDataManager::GetSingleton()->rootObject` is the tree the **vanilla Pip-Boy renders
from**. Reading it gives every Pip-Boy system (perks, radio, workshops, quests, condition) exactly
as the game computes it, instead of hand-rolling each one.

Schema below is **verified by decompiling the engine's own key statics** in the 1.10.163 unpacked
Ghidra project (`_dynamic_initializer_for__Pipboy*Data::*KeySC__` → `BSFixedString(&...,"literal")`),
not guessed:

**Root section keys** (`PipboyDataManager::*ObjectKeySC`):
`Stats` `Special` `Perks` `Inventory` `Quests` `Workshop` `Log` `Map` `Radio` `PlayerInfo` `Status`

> **`Workshop` is SINGULAR**, and `PlayerInventoryObjectKeySC` is the string `"Inventory"` (the
> static's name and its value differ — don't infer key strings from the C++ symbol name).

**Per-entry field keys** — note the engine is **inconsistent about case**:

| Section | Keys |
|---|---|
| `Radio` | `text` `active` `frequency` `inRange` |
| `Quests` | `text` `active` `completed` `objectives` `description` `failed` `formID` `instance` `type` `sortVal` `targetID` |
| `Workshop` | `text` `rating` `value` `workshopData` |
| `Perks` | `Name` `Rank` `Description` (+ `PerkChainPerks/MaxRank/Clip/SWFFile/ListVisibility`) |
| `Special` | `Name` `Value` `Description` ← the real in-game S.P.E.C.I.A.L. blurbs |
| `Stats` | `HeadCondition` `TorsoCondition` `LArmCondition` `RArmCondition` `LLegCondition` `RLegCondition` `ActiveEffects` `StimpakCount` `RadawayCount` |
| `Status` | `IsInVats` `IsLoading` `IsMenuOpen` `IsPipboyNotEquipped` `PipboyEffectColor` … (device state, not player state) |

**Two traps:**
- **`Stats` is NOT misc stats.** It holds limb condition + active effects + stimpak/radaway counts —
  i.e. it feeds STAT→STATUS. Vanilla's DATA→STATS (misc stats: locations discovered, people killed…)
  is **not in PipboyDataManager at all** — it lives in the misc-stat manager. Don't wire DATA→STATS
  to this section; it will silently show the wrong thing.
- Walk the tree by hand (dispatch on `PipboyValue::GetType()`, primitives at `+0x18`) rather than
  calling `PipboyValue::Serialize`, which drags in the game's jsoncpp and its ABI.

Used by `Projects/Prisma/UI_Systems/PrismaPipboy` (`src/GameData.cpp`, `SerializePipboyValue`).

## [CONFIRMED 2026-07-14] RE::RadioManager exists — do not hand-roll radio offsets

`RE/R/RadioManager.h` already wraps the radio, and it is **ID-based** (`REL::VariantID` from
`RE/IDs.h`), so it resolves on OG **and** NG **and** AE:

```cpp
RE::RadioManager::QPlayerRadioEnabled();          // is the Pip-Boy radio on
RE::RadioManager::QCurrentPlayerFreq();           // the frequency actually being played
RE::RadioManager::EnablePlayerRadio(bool, bool);  // turn it on/off (+ play the on/off sound)
```

Use these instead of reading the Pip-Boy data tree's `active` flag: they are the engine's own
answer to "what is playing right now", and they cost no raw address.

**Still NOT in CommonLibF4** (raw RVA required, therefore OG-ONLY):
- `PipboyRadioController::SetRadioStation(float, bool)` — selecting a station.
  1.10.163 RVA **0xC3F2C0**. (The older runtime's private symbol export says 0xC3F2B0 — wrong target, do not use.)
  The controller instance IS exposed: `PipboyManager::GetSingleton()->radioController`.
- `MiscStatManager` — not exposed at all. The stat array is a raw global.

## [CONFIRMED 2026-07-14] REL::Offset is OG-ONLY — gate it, these DLLs are multi-runtime

`CommonLibF4/xmake.lua` sets `COMMONLIB_RUNTIMECOUNT=3`, so every plugin built against it claims
to run on **OG + NG + AE**. `REL::Offset(x)` is a literal RVA for the OG binary — on NG/AE it
resolves to a *different, unrelated* function and calling it is a jump into garbage.

`REL::ID`/`REL::VariantID` (from `RE/IDs.h`) are the version-independent form. When an ID does not
exist for what you need, the correct move is to **gate the raw offset**, not to ship it as if it
were portable:

```cpp
if (!REX::FModule::IsRuntimeOG()) { /* log + skip */ return; }
```

`REX::FModule::IsRuntimeOG()` / `IsRuntimeNG()` are the runtime checks (see `RE/S/SCRIPT_FUNCTION.h`
for the pattern already in the lib).
