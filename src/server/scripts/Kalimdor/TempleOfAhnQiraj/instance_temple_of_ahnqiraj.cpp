/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Creature.h"
#include "CreatureGroups.h"
#include "GridNotifiers.h"
#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TaskScheduler.h"
#include "temple_of_ahnqiraj.h"
#include "World.h"
#include "Config.h"

namespace
{
    // Map ID for AQ40
    constexpr uint32 MAP_AQ40 = MAP_AHN_QIRAJ_TEMPLE; // 531
    constexpr uint32 ENTRY_VEKNISS_DRONE = 15300;

    static constexpr uint32 HALLWAY_CLEANUP_ENTRIES[] =
    {
        4076,   // Roach (critter)
        15475,  // Qiraji Scarab
        15476,  // Scarab
        15300,  // Vekniss Drone
        15229   // Vekniss Soldier
    };

    inline bool IsCleanupEntry(uint32 entry)
    {
        for (uint32 e : HALLWAY_CLEANUP_ENTRIES)
            if (e == entry)
                return true;
        return false;
    }

    // Config toggles
    inline bool IsAQ40CleanupEnabled()
    {
        return sConfigMgr->GetOption<bool>("AQ40.FankrissCleanup.Enable", true);
    }

    inline uint32 GetAQ40CleanupRespawnBlockSeconds()
    {
        // default 7 days
        return sConfigMgr->GetOption<uint32>("AQ40.FankrissCleanup.BlockRespawnSeconds", DAY * 7);
    }

    // Set respawn way into the future for this instance and despawn cleanly.
    inline void DespawnAndBlockRespawn(Creature* c)
    {
        if (!c || !c->IsInWorld())
            return;

        uint32 block = GetAQ40CleanupRespawnBlockSeconds();
        c->SetRespawnTime(block);
        c->SaveRespawnTime();


        c->DespawnOrUnsummon();  

        if (c->IsInCombat())
        {
            c->CombatStop(true);
            c->GetThreatMgr().ClearAllThreat();
            c->ClearInCombat();
        }

        LOG_DEBUG("scripts.aq40",
            "FankrissCleanup: Despawned entry {} (GUID: {}) and blocked respawn for {}s.",
            c->GetEntry(), c->GetGUID().ToString(), block);
    }

    // Return true if Fankriss is DONE in this instance
    inline bool IsFankrissDone(Creature* me)
    {
        if (!me)
            return false;
        if (me->GetMapId() != MAP_AQ40)
            return false;

        if (InstanceScript* inst = me->GetInstanceScript())
            return inst->GetBossState(DATA_FANKRISS) == DONE;

        return false;
    }

    // Generate a set of positions around a center that will deliberately load many grids.
    // We purposely cast a wide, circular net that comfortably covers the approach hallway to Fankriss.
    // Strategy: polar fan, radii up to ~1600 yd, 16 spokes, 9 rings (16*9=144 load points).
    // That’s plenty to hit every relevant grid without needing brittle hand-measured coords.
    static void GenerateGridLoadTargets(Position const& center, std::vector<Position>& out)
    {
        out.clear();

        // radii: 0..1600 in 200 steps
        constexpr float stepR = 200.f;
        constexpr float maxR = 1600.f;
        constexpr int spokes = 16; // every 22.5 degrees

        for (float r = 0.f; r <= maxR + 1.f; r += stepR)
        {
            for (int s = 0; s < spokes; ++s)
            {
                float ang = float(M_PI) * 2.f * (float(s) / float(spokes));
                float x = center.GetPositionX() + r * std::cos(ang);
                float y = center.GetPositionY() + r * std::sin(ang);
                float z = center.GetPositionZ(); // z not used for grid load

                Position p;
                p.Relocate(x, y, z, 0.f);
                out.push_back(p);
            }
        }
    }

    // Load a batch of grids on the instance map; throttled by scheduler to avoid spikes.
    // After each batch, newly created hallway mobs will be caught by OnCreatureCreate if CleanupActive == true.
    static void ScheduleGridLoads(InstanceScript* inst, std::vector<Position> const& targets)
    {
        if (!inst || !inst->instance)
            return;

        // We’ll process 12 targets per tick, 50ms apart => roughly 12*50ms=600ms per 144 points ~ 7.2s total.
        // Gentle enough for an instance map.
        size_t batchSize = 12;
        uint32 delayMs = 50;

        // Capture by value
        std::shared_ptr<std::vector<Position>> sharedTargets = std::make_shared<std::vector<Position>>(targets);
        std::shared_ptr<size_t> idx = std::make_shared<size_t>(0);

        inst->scheduler.Schedule(1ms, [inst, sharedTargets, idx, batchSize, delayMs](TaskContext context)
            {
                Map* map = inst->instance;
                if (!map)
                    return;

                size_t end = std::min(*idx + batchSize, sharedTargets->size());

                for (; *idx < end; ++(*idx))
                {
                    Position const& p = (*sharedTargets)[*idx];

                    // Deliberately load the grid at (x,y)
                    map->LoadGrid(p.GetPositionX(), p.GetPositionY());
                }

                if (*idx < sharedTargets->size())
                    context.Repeat(Milliseconds(delayMs));
            });
    }
}

ObjectData const creatureData[] =
{
    { NPC_VEM,          DATA_VEM          },
    { NPC_KRI,          DATA_KRI          },
    { NPC_YAUJ,         DATA_YAUJ         },
    { NPC_SARTURA,      DATA_SARTURA      },
    { NPC_FANKRISS,     DATA_FANKRISS     },
    { NPC_CTHUN,        DATA_CTHUN        },
    { NPC_EYE_OF_CTHUN, DATA_EYE_OF_CTHUN },
    { NPC_OURO,         DATA_OURO         },
    { NPC_OURO_SPAWNER, DATA_OURO_SPAWNER },
    { NPC_MASTERS_EYE,  DATA_MASTERS_EYE  },
    { NPC_VEKLOR,       DATA_VEKLOR       },
    { NPC_VEKNILASH,    DATA_VEKNILASH    },
    { NPC_VISCIDUS,     DATA_VISCIDUS     },
    { 0,                0                 }
};

DoorData const doorData[] =
{
    { AQ40_DOOR_SKERAM,      DATA_SKERAM,        DOOR_TYPE_PASSAGE },
    { AQ40_DOOR_TE_ENTRANCE, DATA_TWIN_EMPERORS, DOOR_TYPE_ROOM },
    { AQ40_DOOR_TE_EXIT,     DATA_TWIN_EMPERORS, DOOR_TYPE_PASSAGE },
    { 0,                     0,                  DOOR_TYPE_ROOM}
};

class instance_temple_of_ahnqiraj : public InstanceMapScript
{
public:
    instance_temple_of_ahnqiraj() : InstanceMapScript("instance_temple_of_ahnqiraj", MAP_AQ40) {}

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_temple_of_ahnqiraj_InstanceMapScript(map);
    }

    struct instance_temple_of_ahnqiraj_InstanceMapScript : public InstanceScript
    {
        instance_temple_of_ahnqiraj_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(MAX_BOSS_NUMBER);
            LoadObjectData(creatureData, nullptr);
            LoadDoorData(doorData);
        }

        // NEW: cleanup flag + hallway GUID tracking
        bool CleanupActive = false;
        GuidVector HallwayMobGuids;

        void Initialize() override
        {
            BugTrioDeathCount = 0;
            CleanupActive = false;
            HallwayMobGuids.clear();
        }

        void OnCreatureCreate(Creature* creature) override
        {
            switch (creature->GetEntry())
            {
            case NPC_OURO_SPAWNER:
                if (GetBossState(DATA_OURO) != DONE)
                    creature->Respawn();
                break;
            case NPC_MASTERS_EYE:
                if (GetBossState(DATA_TWIN_EMPERORS) != DONE)
                    creature->Respawn(true);
                break;
            case NPC_CTHUN:
                if (!creature->IsAlive())
                {
                    for (ObjectGuid const& guid : CThunGraspGUIDs)
                        if (GameObject* cthunGrasp = instance->GetGameObject(guid))
                            cthunGrasp->DespawnOrUnsummon(1s);
                }
                break;
            default:
                break;
            }

            // Track hallway entries; if cleanup already active, kill immediately
            if (IsCleanupEntry(creature->GetEntry()))
            {
                HallwayMobGuids.push_back(creature->GetGUID());

                if (IsAQ40CleanupEnabled() && (CleanupActive || GetBossState(DATA_FANKRISS) == DONE))
                    DespawnAndBlockRespawn(creature);
            }

            InstanceScript::OnCreatureCreate(creature);
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            switch (go->GetEntry())
            {
            case GO_CTHUN_GRASP:
                CThunGraspGUIDs.push_back(go->GetGUID());
                if (Creature* CThun = GetCreature(DATA_CTHUN))
                    if (!CThun->IsAlive())
                        go->DespawnOrUnsummon(1s);
                break;
            default:
                break;
            }

            InstanceScript::OnGameObjectCreate(go);
        }

        void OnUnitDeath(Unit* unit) override
        {
            switch (unit->GetEntry())
            {
            case NPC_QIRAJI_SLAYER:
            case NPC_QIRAJI_MINDSLAYER:
                if (Creature* creature = unit->ToCreature())
                {
                    if (CreatureGroup* formation = creature->GetFormation())
                    {
                        scheduler.Schedule(100ms, [formation](TaskContext /*context*/)
                            {
                                if (!formation->IsAnyMemberAlive(true))
                                    if (Creature* leader = formation->GetLeader())
                                        if (leader->IsAlive())
                                            leader->AI()->SetData(0, 1);
                            });
                    }
                }
                break;
            case NPC_CTHUN:
                for (ObjectGuid const& guid : CThunGraspGUIDs)
                    if (GameObject* cthunGrasp = instance->GetGameObject(guid))
                        cthunGrasp->DespawnOrUnsummon(1s);
                break;
            default:
                break;
            }
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
            case DATA_BUG_TRIO_DEATH:
                return BugTrioDeathCount;
            }
            return 0;
        }

        void SetData(uint32 type, uint32 data) override
        {
            switch (type)
            {
            case DATA_BUG_TRIO_DEATH:
                if (data != 0)
                    ++BugTrioDeathCount;
                else
                    BugTrioDeathCount = 0;
                break;
            default:
                break;
            }
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            switch (type)
            {
            case DATA_OURO:
                if (state == FAIL)
                    if (Creature* ouroSpawner = GetCreature(DATA_OURO_SPAWNER))
                        ouroSpawner->Respawn();
                break;

            // --- NEW: When Fankriss dies, activate cleanup AND deliberately load grids to force spawns to appear and be cleaned ---
            case DATA_FANKRISS:
            {
                if (!IsAQ40CleanupEnabled())
                    break;

                if (state == DONE)
                {
                    CleanupActive = true;

                    LOG_INFO("scripts.aq40", "FankrissCleanup: Activating hallway cleanup (block respawns = {}s).",
                        GetAQ40CleanupRespawnBlockSeconds());

                    // 1) Clean any already-loaded hallway mobs we’ve tracked
                    for (ObjectGuid const& guid : HallwayMobGuids)
                    {
                        if (Creature* c = instance->GetCreature(guid))
                            if (IsCleanupEntry(c->GetEntry()))
                                DespawnAndBlockRespawn(c);
                    }

                    // 2) Deliberately load nearby grids in a wide radius around Fankriss
                    //    to pull in hallway spawns, which OnCreatureCreate will immediately nuke.
                    if (Creature* fankriss = GetCreature(DATA_FANKRISS))
                    {
                        std::vector<Position> targets;
                        GenerateGridLoadTargets(fankriss->GetPosition(), targets);

                        LOG_INFO("scripts.aq40", "FankrissCleanup: Scheduling deliberate grid loads ({} targets).",
                            targets.size());

                        ScheduleGridLoads(this, targets);
                    }
                    else
                    {
                        // Fallback: use first player as an origin if Fankriss object is gone
                        Map::PlayerList const& pl = instance->GetPlayers();
                        if (!pl.IsEmpty())
                        {
                            if (Player* p = pl.begin()->GetSource())
                            {
                                std::vector<Position> targets;
                                GenerateGridLoadTargets(p->GetPosition(), targets);

                                LOG_INFO("scripts.aq40", "FankrissCleanup: Scheduling grid loads from player origin ({} targets).",
                                    targets.size());

                                ScheduleGridLoads(this, targets);
                            }
                        }
                    }
                }
                break;
            }

            default:
                break;
            }

            return true;
        }

    private:
        GuidVector CThunGraspGUIDs;
        uint32 BugTrioDeathCount;
    };
};

struct npc_vekniss_drone_cleanup : public ScriptedAI
{
    explicit npc_vekniss_drone_cleanup(Creature* creature) : ScriptedAI(creature) {}

    // Called on respawn (DB respawns use this)
    void JustRespawned() override
    {
        TryCleanup();
    }

    void Reset() override
    {
        TryCleanup();
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    void TryCleanup()
    {
        if (!IsAQ40CleanupEnabled())
            return;

        if (me->GetMapId() != MAP_AQ40 || me->GetEntry() != ENTRY_VEKNISS_DRONE)
            return;

        if (IsFankrissDone(me))
            DespawnAndBlockRespawn(me);
    }
};

void AddSC_instance_temple_of_ahnqiraj()
{
    new instance_temple_of_ahnqiraj();
    RegisterTempleOfAhnQirajCreatureAI(npc_vekniss_drone_cleanup);
}
