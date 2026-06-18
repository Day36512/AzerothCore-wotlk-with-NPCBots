# Custom Code Worldserver MS Audit

Original audit scope: custom code under `src/server/scripts/Custom` and `modules`.

The original audit was based on static inspection. The mitigation pass below changed only ALE/NpcBotBanter performance paths and NpcBotBanter config defaults; it did not touch NPCBots core behavior. Line numbers refer to the current workspace snapshot.

## Applied mitigation pass

The first performance pass applied the following targeted changes:

- `modules/mod-ale`: world/gameobject update paths now skip work when ALE is disabled, empty per-object event processors no longer update every tick, and world objects no longer allocate an `ALEEventProcessor` until Lua registers a timed event on that object.
- `modules/mod-npcbot-banter`: proximity checks now run through a round-robin player budget instead of every online player each world tick pass; global proximity cooldown short-circuits the expensive proximity path; default `WorldTickMs` is now 5000, default proximity global cooldown is 8000 ms, and default proximity player budget is 8.
- Config copies updated in `modules/mod-npcbot-banter/conf`, `out/install/x64-Debug/configs/modules`, and `F:\Desktop\Dinklepack\configs\modules`.

## Executive summary - top 5 suspects

1. `modules/mod-ale` is the biggest structural risk when enabled. It hooks world update, every map update, every world-object update, and every gameobject update, then runs Lua/event-processor plumbing from those hooks.
2. `modules/mod-npcbot-banter` defaults enabled and performs a global online-player walk every world tick interval, then evaluates player state/proximity banter and may do additional grid/global searches.
3. `modules/mod-timewalking` subscribes to all creature updates. Active timewalking dungeons run creature scale/aura checks from that path, plus combat damage hooks.
4. Custom NPCBot encounter directors, especially Thaddius and Kel'Thuzad, can bunch duplicate bot enumeration, wide grid scans, and forced bot movement commands into boss-mechanic ticks.
5. `src/server/scripts/Custom/Custom_NPC/fake_player_ambience.cpp` can create persistent per-creature AI overhead when many fake-player ambience NPCs are active.

## Ranked findings

### 1. High - ALE global update fan-out

- File/function: `modules/mod-ale/src/ALE_SC.cpp`, `ALE_WorldScript::OnUpdate`, `ALE_AllMapScript::OnMapUpdate`, `ALE_AllGameObjectScript::OnGameObjectUpdate`, `ALE_WorldObjectScript::OnWorldObjectUpdate`.
- Evidence:
  - `ALE_WorldScript` subscribes to `WORLDHOOK_ON_UPDATE` and calls `sALE->OnWorldUpdate(diff)` at `ALE_SC.cpp:1103-1155`.
  - `ALE_AllMapScript` subscribes to `ALLMAPHOOK_ON_MAP_UPDATE` and calls `sALE->OnUpdate(map, diff)` at `ALE_SC.cpp:255-302`.
  - `ALE_AllGameObjectScript::OnGameObjectUpdate` calls `sALE->UpdateAI(go, diff)` at `ALE_SC.cpp:109-127`.
  - `ALE_WorldObjectScript::OnWorldObjectSetMap` allocates `ALEEventProcessor` for world objects and `OnWorldObjectUpdate` calls `object->ALEEvents->Update(diff)` at `ALE_SC.cpp:1091-1099`.
  - `ALE::OnWorldUpdate` updates global events, HTTP responses, query callbacks, and Lua world-update bindings at `LuaEngine/hooks/ServerHooks.cpp:252-267`.
  - `ALE::UpdateAI(GameObject*)` always updates the object's ALE event processor before checking gameobject AI-update bindings at `LuaEngine/hooks/GameObjectHooks.cpp:43-50`.
  - Module config has `ALE.Enabled = true` in `modules/mod-ale/conf/mod_ale.conf.dist:63`, although C++ config fallback is false in `LuaEngine/ALEConfig.cpp:18-23`.
- Frequency: every world tick, every map update, every world-object update, and every gameobject update while ALE is enabled.
- Blast radius: server-wide. Even empty or light Lua usage still pays C++ hook dispatch, config checks, event processor updates, and potential lock/binding-map work across many objects.
- Fix direction: make the hot update hooks cheap when no script has registered relevant bindings or events. Consider per-hook active counters, lazy object event processors only when a Lua event is registered on that object, and a production config default of disabled unless Lua content is required.

### 2. High - NpcBotBanter global player sweep and proximity scans

- File/function: `modules/mod-npcbot-banter/src/npcbot_banter.cpp`, `BanterMgr::OnWorldUpdate`, `TryProximityBanter`, `GetSayRecipients`, `FindPlayerByKey`, `FindProximityTarget`.
- Evidence:
  - Defaults enable the module with `NpcBotBanter.WorldTickMs = 3000` at `npcbot_banter.cpp:589-639` and `modules/mod-npcbot-banter/conf/mod_npcbot_banter.conf.dist:10-19`.
  - `OnWorldUpdate` runs every world update, does queue/sequence maintenance, and every 3 seconds takes the global player lock, iterates `ObjectAccessor::GetPlayers()`, then calls `UpdatePlayerState` and `TryProximityBanter` for each player at `npcbot_banter.cpp:1219-1261`.
  - Say recipient selection loops all online players again under the global player lock at `npcbot_banter.cpp:1613-1628`.
  - `FindPlayerByKey` searches all online players by low GUID at `npcbot_banter.cpp:2242-2255`.
  - Response dispatch can call `GetCreatureListWithEntryInGrid` at `npcbot_banter.cpp:2288-2298`.
  - Proximity target search can call `GetGameObjectListWithEntryInGrid` for each viable proximity line at `npcbot_banter.cpp:2868-2885`.
  - `TryProximityBanter` loops configured proximity lines per player before selecting speakers/targets at `npcbot_banter.cpp:2921-2976`.
- Frequency: world update bookkeeping every tick, plus full online-player scan every `WorldTickMs`; more work when chat responses, proximity lines, or sequence queues are active.
- Blast radius: server-wide, scaling with online players and configured banter/proximity data. Can create lock contention and grid-search bursts.
- Fix direction: keep per-map or per-zone player lists, cache `guid -> Player*` lookups where safe, short-circuit proximity when no nearby configured speaker entries exist, and expose a conservative production profile that disables proximity or raises `WorldTickMs`.

### 3. Medium-high - Timewalking all-creature update path

- File/function: `modules/mod-timewalking/src/TimewalkingScripts.cpp`, `Timewalking_AllCreatureScript::OnAllCreatureUpdate`; `modules/mod-timewalking/src/TimewalkingMgr.cpp`, `TimewalkingMgr::OnCreatureUpdate`, `ScaleCreature`, `ApplyPlayerBotAura`.
- Evidence:
  - `Timewalking_AllCreatureScript::OnAllCreatureUpdate` calls `sTimewalking->OnCreatureUpdate(creature, diff)` for every creature update at `TimewalkingScripts.cpp:105-128`.
  - `OnCreatureUpdate` checks whether the creature is an NPCBot, applies the player/bot aura if so, otherwise calls `ScaleCreature(creature, false)` at `TimewalkingMgr.cpp:1549-1558`.
  - `ScaleCreature` exits when timewalking is inactive, but active dungeon creatures run map-data lookup, creature-data lookup, selected-level computation, cached-state checks, and sometimes stat regeneration/update at `TimewalkingMgr.cpp:1669-1799`.
  - NPCBot detection includes entry-set checks, configured entry-range checks, `IsNPCBotOrPet`, and script-name/AI-name string checks at `TimewalkingMgr.cpp:1410-1428`.
  - Combat damage/healing hooks also call into timewalking multiplier checks at `TimewalkingScripts.cpp:131-165` and `TimewalkingMgr.cpp:1801-1863`.
- Frequency: every creature update globally; meaningful work mainly in active dungeon maps.
- Blast radius: medium globally, high inside active timewalking dungeons with many creatures/NPCBots. Cached scale data limits repeated stat writes, but the hook still executes constantly.
- Fix direction: move creature scaling to add-world/select-level/map-enter events where possible, track active timewalking maps and creature GUIDs, avoid script-name style NPCBot detection from per-update paths, and throttle NPCBot/player aura refreshes.

### 4. High encounter spike - NPCBot raid directors duplicate bot work

- File/function: `src/server/scripts/Custom/Custom_NPC/Naxx40/boss_thaddius_40_custom.cpp`, `TeleportBotsNear`, `SendBotsToPolaritySpots`, `BuildPlan`, `ReturnAllBotsToFollow`, polarity target filter; `boss_kelthuzad_40_custom.cpp`, `Director_SpreadTick` and target fallbacks; `boss_sapphiron_40_custom.cpp`, ice-block movement helpers.
- Evidence:
  - Thaddius iterates map players, then `BotMgr::GetAllGroupMembers(plr)` for each player in multiple helpers at `boss_thaddius_40_custom.cpp:141-162`, `174-210`, `637-659`, `784-805`, and `1194-1212`.
  - `SendBotsToPolaritySpots` can `AttackStop`, interrupt casting, and call `MoveToSendPosition` for qualifying bots during polarity handling at `boss_thaddius_40_custom.cpp:174-210`.
  - Thaddius initialization teleports bots near the boss at `boss_thaddius_40_custom.cpp:404-409`; polarity repeats every 30 seconds at `boss_thaddius_40_custom.cpp:435-437`.
  - Kel'Thuzad fallback target selection scans 100-200 yards with `Cell::VisitObjects` at `boss_kelthuzad_40_custom.cpp:200-203`, `266-269`, `301-304`, `529-532`, and `917-920`.
  - Kel'Thuzad's director spread tick is scheduled at 1500 ms, rescheduled by config, and poked 500 ms after fissures at `boss_kelthuzad_40_custom.cpp:712-769` and `853-861`.
  - Sapphiron gathers bots and ice blocks using 150 yard scans during flight/ice-block handling at `boss_sapphiron_40_custom.cpp:175-181` and `385-405`.
- Frequency: boss-mechanic driven, not server-wide. Spikes can align with encounter events, especially Thaddius polarity/teleport and Kel'Thuzad fissure/spread ticks.
- Blast radius: raid instances with many players and NPCBots. Duplicate enumeration can revisit the same bot once per player if `GetAllGroupMembers` returns the full group for each member.
- Fix direction: collect unique NPCBot GUIDs once per map/encounter tick, cache role buckets during a mechanic window, and issue movement only when destination/command actually changed. For Kel'Thuzad/Sapphiron, prefer threat-list or maintained encounter-bot sets before wide fallback scans.

### 5. Medium - fake player ambience per-creature AI and cell scans

- File/function: `src/server/scripts/Custom/Custom_NPC/fake_player_ambience.cpp`, fake player `UpdateAI`, `HasRealPlayerNearby`, marker selection, crowd counting.
- Evidence:
  - Active fake players run `UpdateIndoorOutdoorState`, combat-stop safety, event processing, and idle recovery in every AI update at `fake_player_ambience.cpp:1003-1057`.
  - Forced behavior can schedule an immediate `EVENT_DECIDE` at 1 ms at `fake_player_ambience.cpp:990-992`.
  - Indoor/outdoor sampling calls `me->IsOutdoors()` periodically when VMAP indoor checks are enabled at `fake_player_ambience.cpp:1154-1181`.
  - Visible-player checks use `Cell::VisitObjects` with only a 1500 ms cache at `fake_player_ambience.cpp:1248-1263`.
  - Marker selection scans nearby creatures and gameobjects at `fake_player_ambience.cpp:2039-2066`.
  - Destination crowd checks scan nearby fake players at `fake_player_ambience.cpp:2180-2188`.
- Frequency: every active fake-player creature update, with heavier work on decisions/routes/marker selection.
- Blast radius: maps with many spawned fake players. Cost is mostly local but can be persistent background work.
- Fix direction: stagger decisions, add coarser per-creature update buckets, share visible-player results by grid or map cell, and cap active decision/scanning work per tick.

### 6. Medium - global aura repair sweep in custom timecycle

- File/function: `src/server/scripts/Custom/Custom_Area_Auras/mod_timecycle.cpp`, `WorldScript::OnUpdate`, `ApplyToAll`.
- Evidence:
  - `ApplyToAll` calls `sMapMgr->DoForAllMaps`, iterates every map's player list, and iterates tracked NPCBots for aura application at `mod_timecycle.cpp:472-508`.
  - `OnUpdate` gates the full sweep to aura changes, repair cadence, or force apply at `mod_timecycle.cpp:523-570`.
  - NPCBot tracking is event based in `AllCreatureScript` add/remove hooks at `mod_timecycle.cpp:623-660`.
- Frequency: periodic or on aura state changes, not every tick.
- Blast radius: server-wide at repair/change moments. Usually a spike rather than constant load.
- Fix direction: chunk `ApplyToAll` over multiple world updates, separate player and NPCBot repair cadences, and log counts/duration for each sweep before changing behavior.

### 7. Medium-low - skill/profession database writes on event hooks

- File/function: `modules/mod-shared-professions/src/SharedProfessions.cpp`, `OnPlayerUpdateSkill`, `OnPlayerLearnSpell`, `OnPlayerLogin`; `modules/mod-profession-experience/src/ProfessionExp.cpp`, skill update hooks.
- Evidence:
  - Shared professions writes to `CharacterDatabase` on profession skill updates at `SharedProfessions.cpp:147-170`, and login saves professions/recipes by scanning DBC rows at `SharedProfessions.cpp:101-118` and `173-210`.
  - Profession XP listens to crafting/gathering/fishing skill updates at `ProfessionExp.cpp:131-227`.
- Frequency: event-driven skill changes/login, not frame update.
- Blast radius: narrow. Can spike on login or profession training/crafting batches, but unlikely to explain ordinary world update diff spikes.
- Fix direction: keep as lower priority; instrument login and skill-update DB write durations if profession actions correlate with lag.

## Hot-path inventory

- World update hooks:
  - `modules/mod-ale/src/ALE_SC.cpp:1103-1155` - ALE world update.
  - `modules/mod-npcbot-banter/src/npcbot_banter.cpp:1219-1261` and `3596-3604` - NpcBotBanter world update.
  - `modules/mod-autofish/src/mod_autofish.cpp:470-510` - AutoFish bounded pending timer update.
  - `src/server/scripts/Custom/Custom_Area_Auras/mod_timecycle.cpp:523-570` - timecycle aura sweep gate.
- Per-player update hooks:
  - `src/server/scripts/Custom/Custom_Npc_Bots/npcbot_auto_hide_on_logout.cpp:58-71` - logout helper, but registration is commented out at `npcbot_auto_hide_on_logout.cpp:79-82`.
- All-creature/update style hooks:
  - `modules/mod-timewalking/src/TimewalkingScripts.cpp:105-128` - all creature update into timewalking manager.
  - `modules/mod-ale/src/ALE_SC.cpp:90-105` - all-creature select-level hooks.
- All-map/gameobject/world-object hooks:
  - `modules/mod-ale/src/ALE_SC.cpp:255-302` - all map update.
  - `modules/mod-ale/src/ALE_SC.cpp:109-127` - all gameobject update.
  - `modules/mod-ale/src/ALE_SC.cpp:1086-1100` - world-object event processor allocation/update.
- Creature AI/event maps:
  - `src/server/scripts/Custom/Custom_NPC/fake_player_ambience.cpp:1003-1057`.
  - Naxx40 custom boss scripts under `src/server/scripts/Custom/Custom_NPC/Naxx40`.
- Wide scans/global loops:
  - `ObjectAccessor::GetPlayers()` in NpcBotBanter at `npcbot_banter.cpp:1244`, `1615`, `1684`, `2248`.
  - `sMapMgr->DoForAllMaps` in timecycle at `mod_timecycle.cpp:474`.
  - `Cell::VisitObjects` in fake-player ambience, NPCBot encounter directors, Naxx boss helpers, and ALE Lua-exposed methods.

## NPCBot-specific concerns

- Duplicate group-member enumeration: Thaddius repeatedly walks every map player and then all group members for each player. This should be converted to a unique-bot collection helper before any future behavior change.
- Forced movement churn: Thaddius polarity uses `AttackStop`, interrupt, and `MoveToSendPosition`; Twins/Anub/Gluth/Kel'Thuzad/Sapphiron helpers also issue movement from encounter events. These should check command/destination deltas before sending new movement orders.
- Wide fallback scans: Kel'Thuzad and Sapphiron use 100-200 yard `Cell::VisitObjects` scans to recover target/bot lists. Prefer maintained encounter-bot sets and threat-list candidates first.
- NPCBot detection in hot paths: Timewalking does several checks, including script/AI name tests, from `OnCreatureUpdate`. Cache NPCBot classification per creature where possible.
- Aura sweep interactions: Timecycle and timewalking both manage player/NPCBot auras. Their sweeps are mostly gated, but instrumentation should confirm they do not coincide with large NPCBot raid moments.

## PlayerBots leftover check

No unguarded active PlayerBots runtime dependency stood out in the audited scope.

- `modules/mod-ale/src/LuaEngine/methods/PlayerMethods.h:5125-5131` has a Playerbot/RNDBot helper, but it is compile-gated by `#if defined(MOD_PLAYERBOTS)`.
- `modules/mod-individual-progression/src/IndividualProgression.cpp:942` loads `IndividualProgression.BotAccountsRegex` with default `^RNDBOT.*`; this is config/account filtering, not a PlayerBots runtime manager call.
- `modules/mod-individual-progression` README/config comments and `modules/mod-easy-respawn/conf/mod_easy_respawn.conf.dist:12` mention Playerbots in documentation/comments.
- `modules/mod-timewalking/src/TimewalkingMgr.cpp:1376` names a method `ApplyPlayerBotAura`, but the implementation applies to real players and NPCBots through `IsNpcBotUnit`; this is a naming artifact, not an active PlayerBots dependency.

## Probably not the issue

- `modules/mod-ashen-diagnostics-exporter` has been removed from the current workspace. It was previously a high-risk per-player update suspect, but it is no longer an active hot path in this tree.
- `modules/mod-autofish/src/mod_autofish.cpp` is now bounded by pending timer maps. `OnUpdate` calls `TickPendingCasts`, `TickBobbers`, `TickAutoLoot`, and `TickRecast` at `mod_autofish.cpp:501-510`; spell casts schedule bobber lookup only when the spell summons a configured bobber at `mod_autofish.cpp:518-525`. It no longer appears to be an all-player/all-gameobject scan risk.
- `src/server/scripts/Custom/Custom_Npc_Bots/npcbot_auto_hide_on_logout.cpp` has an `OnPlayerUpdate`, but `AddSC_npcbot_auto_hide_on_logout` does not instantiate it at `npcbot_auto_hide_on_logout.cpp:79-82`.
- `modules/mod-profession-experience` and `modules/mod-shared-professions` are event-driven by skill/login/learn-spell hooks, not normal update ticks.
- Many Naxx40 `UpdateAI` methods are normal boss event loops. The risk is specific helper scans/movement bursts, not the mere presence of `UpdateAI`.

## Recommended instrumentation pass

Add temporary, config-gated timing around the top suspects before optimizing behavior. Suggested config: `CustomPerf.Enabled`, `CustomPerf.WarnMs`, `CustomPerf.SampleEveryMs`, and per-module toggles.

Measure these first:

- ALE: `ALE::OnWorldUpdate`, `ALE::OnUpdate(Map*)`, `ALE::UpdateAI(GameObject*)`, `ALE_WorldObjectScript::OnWorldObjectUpdate`. Log elapsed time, map id, object type, and binding/event counts if available.
- NpcBotBanter: `OnWorldUpdate`, `UpdatePlayerState`, `TryProximityBanter`, `SelectSpeaker`, `FindProximityTarget`, `DispatchQueuedResponses`. Log online player count, proximity line count, selected speaker scans, queued responses.
- Timewalking: `OnCreatureUpdate`, `ScaleCreature`, `ApplyPlayerBotAura`. Log active map id, creature count sampled, NPCBot classification count, scale cache hits/misses.
- NPCBot encounter directors: Thaddius `TeleportBotsNear`, `SendBotsToPolaritySpots`, `BuildPlan`; Kel'Thuzad `Director_SpreadTick`; Sapphiron ice-block movement. Log unique bots touched, duplicate visits avoided, scan result counts, movement commands issued.
- Fake-player ambience: fake player `UpdateAI`, `DecideNextBehavior`, marker scans, visible-player checks, crowd checks. Log active fake-player count and scan counts.
- Timecycle: `ApplyToAll`. Log map count, player count, NPCBot count, and elapsed ms.

Test scenarios:

1. Idle world with normal online count.
2. NPCBot raid with Thaddius/Kel'Thuzad/Sapphiron mechanics.
3. Active timewalking dungeon with many creatures and NPCBots.
4. High fake-player ambience spawn count in one outdoor map.

Stop condition for the instrumentation pass: identify any hook exceeding `CustomPerf.WarnMs` or contributing repeated 1-2 ms slices on normal ticks. Optimize only the measured offenders first.
