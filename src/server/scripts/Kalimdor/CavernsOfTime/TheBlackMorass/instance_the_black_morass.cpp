/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * at your option any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CellImpl.h"
#include "Config.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "Unit.h"
#include "WorldStateDefines.h"
#include "the_black_morass.h"

#include <list>
#include <vector>

namespace BlackMorassConfig
{
    // Delay after Chrono Lord Deja / Temporus die before portal spawning resumes.
    // Default preserves the previous intended post-boss delay: 2m30s = 150000 ms.
    inline Milliseconds GetPostBossGateRespawnDelay()
    {
        uint32 delayMs = sConfigMgr->GetOption<uint32>("BlackMorass.GateRespawnDelayMs", 150000);

        // Safety clamp: allow instant resume, but avoid absurdly large values from config mistakes.
        if (delayMs > 30 * MINUTE * IN_MILLISECONDS)
            delayMs = 30 * MINUTE * IN_MILLISECONDS;

        return Milliseconds(delayMs);
    }

    inline Milliseconds GetTrashPortalFallbackDelay(uint8 currentRift)
    {
        return currentRift >= 13 ? 2min : 90s;
    }

    inline bool IsBossPortalRift(uint8 currentRift)
    {
        return currentRift == 6 || currentRift == 12 || currentRift == 18;
    }
}

const Position PortalLocation[4] =
{
    { -2030.8318f, 7024.9443f, 23.071817f, 3.14159f },
    { -1961.7335f, 7029.5280f, 21.811401f, 2.12931f },
    { -1887.6950f, 7106.5570f, 22.049500f, 4.95673f },
    { -1930.9106f, 7183.5970f, 23.007639f, 3.59537f }
};

ObjectData const creatureData[] =
{
    { NPC_MEDIVH, DATA_MEDIVH },
    { 0,          0           }
};

class instance_the_black_morass : public InstanceMapScript
{
public:
    instance_the_black_morass() : InstanceMapScript("instance_the_black_morass", MAP_OPENING_OF_THE_DARK_PORTAL) {}

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_the_black_morass_InstanceMapScript(map);
    }

    struct instance_the_black_morass_InstanceMapScript : public InstanceScript
    {
        instance_the_black_morass_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, nullptr);

            _currentRift = 0;
            _shieldPercent = 100;
            _encounterNPCs.clear();
            _portalSpawningEnabled = true;
            _encounterStartBeastsKilled = false;
            _eventStatus = EVENT_PREPARE;
            _lastPortalPosition = Position(0.0f, 0.0f, 0.0f, 0.0f);
        }

        void CleanupInstance()
        {
            _currentRift = 0;
            _shieldPercent = 100;

            _availableRiftPositions.clear();
            _scheduler.CancelAll();

            for (Position const& pos : PortalLocation)
                _availableRiftPositions.push_back(pos);

            // Prevent getting stuck if event fails during a boss break.
            _portalSpawningEnabled = true;
            _encounterStartBeastsKilled = false;
            _lastPortalPosition = Position(0.0f, 0.0f, 0.0f, 0.0f);

            if (Creature* medivh = GetCreature(DATA_MEDIVH))
                medivh->Respawn();

            for (ObjectGuid const& guid : _encounterNPCs)
            {
                if (guid.GetEntry() == NPC_DP_BEAM_STALKER)
                {
                    if (Creature* creature = instance->GetCreature(guid))
                        creature->Respawn();

                    break;
                }
            }
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            if (state == DONE)
            {
                switch (type)
                {
                case DATA_AEONUS:
                {
                    if (Creature* medivh = GetCreature(DATA_MEDIVH))
                        medivh->AI()->DoAction(ACTION_OUTRO);

                    instance->DoForAllPlayers([&](Player* player)
                        {
                            if (player->GetQuestStatus(QUEST_OPENING_PORTAL) == QUEST_STATUS_INCOMPLETE)
                                player->AreaExploredOrEventHappens(QUEST_OPENING_PORTAL);

                            if (player->GetQuestStatus(QUEST_MASTER_TOUCH) == QUEST_STATUS_INCOMPLETE)
                                player->AreaExploredOrEventHappens(QUEST_MASTER_TOUCH);
                        });

                    for (ObjectGuid const& guid : _encounterNPCs)
                    {
                        if (Creature* creature = instance->GetCreature(guid))
                        {
                            switch (creature->GetEntry())
                            {
                            case NPC_RIFT_KEEPER_WARLOCK:
                            case NPC_RIFT_KEEPER_MAGE:
                            case NPC_RIFT_LORD:
                            case NPC_RIFT_LORD_2:
                            case NPC_INFINITE_ASSASSIN:
                            case NPC_INFINITE_ASSASSIN_2:
                            case NPC_INFINITE_WHELP:
                            case NPC_INFINITE_CHRONOMANCER:
                            case NPC_INFINITE_CHRONOMANCER_2:
                            case NPC_INFINITE_EXECUTIONER:
                            case NPC_INFINITE_EXECUTIONER_2:
                            case NPC_INFINITE_VANQUISHER:
                            case NPC_INFINITE_VANQUISHER_2:
                                creature->DespawnOrUnsummon(1ms);
                                break;
                            default:
                                break;
                            }
                        }
                    }

                    break;
                }
                case DATA_CHRONO_LORD_DEJA:
                case DATA_TEMPORUS:
                {
                    // Boss portals are not allowed to advance from the trash fallback timer.
                    // Once the boss dies, this is the only place that resumes the event.
                    _portalSpawningEnabled = false;
                    _scheduler.CancelGroup(CONTEXT_GROUP_RIFTS);

                    Milliseconds const delay = BlackMorassConfig::GetPostBossGateRespawnDelay();

                    _scheduler.Schedule(delay, [this](TaskContext)
                        {
                            if (_eventStatus != EVENT_IN_PROGRESS || _currentRift >= 18)
                                return;

                            _portalSpawningEnabled = true;
                            ScheduleNextPortal(0s, _lastPortalPosition);
                        });

                    break;
                }
                default:
                    break;
                }
            }

            return true;
        }

        void FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& packet) override
        {
            packet.Worldstates.emplace_back(WORLD_STATE_BLACK_MORASS, _eventStatus);
            packet.Worldstates.emplace_back(WORLD_STATE_BLACK_MORASS_SHIELD, _shieldPercent);
            packet.Worldstates.emplace_back(WORLD_STATE_BLACK_MORASS_RIFT, _currentRift);
        }

        void OnPlayerEnter(Player* player) override
        {
            if (instance->GetPlayersCountExceptGMs() <= 1 && GetBossState(DATA_AEONUS) != DONE && _eventStatus != EVENT_IN_PROGRESS)
                CleanupInstance();

            player->SendUpdateWorldState(WORLD_STATE_BLACK_MORASS, _eventStatus);
            player->SendUpdateWorldState(WORLD_STATE_BLACK_MORASS_SHIELD, _shieldPercent);
            player->SendUpdateWorldState(WORLD_STATE_BLACK_MORASS_RIFT, _currentRift);
        }

        void ScheduleNextPortal(Milliseconds time, Position lastPosition)
        {
            // Only one rift timer can be scheduled at any time.
            _scheduler.CancelGroup(CONTEXT_GROUP_RIFTS);

            _scheduler.Schedule(time, [this, lastPosition](TaskContext context)
                {
                    context.SetGroup(CONTEXT_GROUP_RIFTS);

                    if (GetCreature(DATA_MEDIVH))
                    {
                        // Spawning prevented: post-boss delay, event failed/not started, or last portal spawned.
                        if (!_portalSpawningEnabled || _eventStatus == EVENT_PREPARE || _currentRift >= 18)
                            return;

                        Position spawnPos;
                        if (!_availableRiftPositions.empty())
                        {
                            if (_availableRiftPositions.size() > 1)
                            {
                                auto spawnPosItr = Acore::Containers::SelectRandomContainerElementIf(_availableRiftPositions, [&](Position const& pos) -> bool
                                    {
                                        return pos != lastPosition;
                                    });

                                if (spawnPosItr != _availableRiftPositions.end())
                                    spawnPos = *spawnPosItr;
                            }
                            else
                            {
                                spawnPos = Acore::Containers::SelectRandomContainerElement(_availableRiftPositions);
                            }

                            _availableRiftPositions.remove(spawnPos);
                            _lastPortalPosition = spawnPos;

                            DoUpdateWorldState(WORLD_STATE_BLACK_MORASS_RIFT, ++_currentRift);

                            instance->SummonCreature(NPC_TIME_RIFT, spawnPos);

                            // Trash waves keep the original safety timer.
                            // Boss waves stop here and resume only from SetBossState(..., DONE).
                            if (!BlackMorassConfig::IsBossPortalRift(_currentRift))
                                context.Repeat(BlackMorassConfig::GetTrashPortalFallbackDelay(_currentRift));
                        }
                        // If no rift positions are available, the next rift will be scheduled in OnCreatureRemove.
                    }
                });
        }

        void OnCreatureCreate(Creature* creature) override
        {
            switch (creature->GetEntry())
            {
            case NPC_TIME_RIFT:
            case NPC_CHRONO_LORD_DEJA:
            case NPC_INFINITE_CHRONO_LORD:
            case NPC_TEMPORUS:
            case NPC_INFINITE_TIMEREAVER:
            case NPC_AEONUS:
            case NPC_RIFT_KEEPER_WARLOCK:
            case NPC_RIFT_KEEPER_MAGE:
            case NPC_RIFT_LORD:
            case NPC_RIFT_LORD_2:
            case NPC_INFINITE_ASSASSIN:
            case NPC_INFINITE_ASSASSIN_2:
            case NPC_INFINITE_WHELP:
            case NPC_INFINITE_CHRONOMANCER:
            case NPC_INFINITE_CHRONOMANCER_2:
            case NPC_INFINITE_EXECUTIONER:
            case NPC_INFINITE_EXECUTIONER_2:
            case NPC_INFINITE_VANQUISHER:
            case NPC_INFINITE_VANQUISHER_2:
            case NPC_DP_EMITTER_STALKER:
            case NPC_DP_CRYSTAL_STALKER:
            case NPC_DP_BEAM_STALKER:
                _encounterNPCs.insert(creature->GetGUID());
                break;
            default:
                break;
            }

            InstanceScript::OnCreatureCreate(creature);
        }

        void OnCreatureRemove(Creature* creature) override
        {
            switch (creature->GetEntry())
            {
            case NPC_TIME_RIFT:
            {
                _availableRiftPositions.push_back(creature->GetHomePosition());

                if (_eventStatus == EVENT_IN_PROGRESS && _portalSpawningEnabled)
                    ScheduleNextPortal(0s, creature->GetHomePosition());

                _encounterNPCs.erase(creature->GetGUID());
                break;
            }
            case NPC_CHRONO_LORD_DEJA:
            case NPC_INFINITE_CHRONO_LORD:
            case NPC_TEMPORUS:
            case NPC_INFINITE_TIMEREAVER:
            case NPC_AEONUS:
            case NPC_RIFT_KEEPER_WARLOCK:
            case NPC_RIFT_KEEPER_MAGE:
            case NPC_RIFT_LORD:
            case NPC_RIFT_LORD_2:
            case NPC_INFINITE_ASSASSIN:
            case NPC_INFINITE_ASSASSIN_2:
            case NPC_INFINITE_WHELP:
            case NPC_INFINITE_CHRONOMANCER:
            case NPC_INFINITE_CHRONOMANCER_2:
            case NPC_INFINITE_EXECUTIONER:
            case NPC_INFINITE_EXECUTIONER_2:
            case NPC_INFINITE_VANQUISHER:
            case NPC_INFINITE_VANQUISHER_2:
            case NPC_DP_EMITTER_STALKER:
                _encounterNPCs.erase(creature->GetGUID());
                break;
            default:
                break;
            }

            InstanceScript::OnCreatureRemove(creature);
        }

        void SetData(uint32 type, uint32 data) override
        {
            switch (type)
            {
            case DATA_MEDIVH:
            {
                _eventStatus = EVENT_IN_PROGRESS;

                KillEncounterStartBeasts();

                DoUpdateWorldState(WORLD_STATE_BLACK_MORASS, _eventStatus);
                DoUpdateWorldState(WORLD_STATE_BLACK_MORASS_SHIELD, _shieldPercent);
                DoUpdateWorldState(WORLD_STATE_BLACK_MORASS_RIFT, _currentRift);

                ScheduleNextPortal(3s, Position(0.0f, 0.0f, 0.0f, 0.0f));
                break;
            }
            case DATA_DAMAGE_SHIELD:
            {
                if (_shieldPercent <= 0)
                    return;

                _shieldPercent -= data;
                if (_shieldPercent < 0)
                    _shieldPercent = 0;

                DoUpdateWorldState(WORLD_STATE_BLACK_MORASS_SHIELD, _shieldPercent);

                if (!_shieldPercent)
                {
                    _eventStatus = EVENT_PREPARE;

                    if (Creature* medivh = GetCreature(DATA_MEDIVH))
                    {
                        if (medivh->IsAlive() && medivh->IsAIEnabled)
                        {
                            medivh->SetImmuneToNPC(true);
                            medivh->AI()->Talk(SAY_MEDIVH_DEATH);

                            for (ObjectGuid const& guid : _encounterNPCs)
                            {
                                if (Creature* creature = instance->GetCreature(guid))
                                    creature->InterruptNonMeleeSpells(true);
                            }

                            // Step 1 - Medivh loses all auras.
                            _scheduler.Schedule(4s, [this](TaskContext)
                                {
                                    if (Creature* medivh = GetCreature(DATA_MEDIVH))
                                        medivh->RemoveAllAuras();

                                    // Step 2 - Medivh dies and visual effect NPCs are despawned.
                                    _scheduler.Schedule(500ms, [this](TaskContext)
                                        {
                                            if (Creature* medivh = GetCreature(DATA_MEDIVH))
                                            {
                                                medivh->KillSelf(false);

                                                GuidSet encounterNPCSCopy = _encounterNPCs;
                                                for (ObjectGuid const& guid : encounterNPCSCopy)
                                                {
                                                    switch (guid.GetEntry())
                                                    {
                                                    case NPC_TIME_RIFT:
                                                    case NPC_DP_EMITTER_STALKER:
                                                    case NPC_DP_CRYSTAL_STALKER:
                                                    case NPC_DP_BEAM_STALKER:
                                                        if (Creature* creature = instance->GetCreature(guid))
                                                            creature->DespawnOrUnsummon();
                                                        break;
                                                    default:
                                                        break;
                                                    }
                                                }
                                            }

                                            // Step 3 - All summoned creatures despawn.
                                            _scheduler.Schedule(2s, [this](TaskContext)
                                                {
                                                    GuidSet encounterNPCSCopy = _encounterNPCs;
                                                    for (ObjectGuid const& guid : encounterNPCSCopy)
                                                    {
                                                        // Don't despawn permanent visual effect NPC twice or it won't respawn correctly.
                                                        if (guid.GetEntry() == NPC_DP_BEAM_STALKER)
                                                            continue;

                                                        if (Creature* creature = instance->GetCreature(guid))
                                                        {
                                                            creature->CastSpell(creature, SPELL_TELEPORT_VISUAL, true);
                                                            creature->DespawnOrUnsummon(1200ms, 0s);
                                                        }
                                                    }

                                                    _scheduler.CancelAll();

                                                    // Step 4 - Schedule instance cleanup without player interaction.
                                                    _scheduler.Schedule(300s, [this](TaskContext)
                                                        {
                                                            CleanupInstance();

                                                            DoUpdateWorldState(WORLD_STATE_BLACK_MORASS, _eventStatus);
                                                            DoUpdateWorldState(WORLD_STATE_BLACK_MORASS_SHIELD, _shieldPercent);
                                                            DoUpdateWorldState(WORLD_STATE_BLACK_MORASS_RIFT, _currentRift);
                                                        });
                                                });
                                        });
                                });
                        }
                    }
                }

                break;
            }
            default:
                break;
            }
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
            case DATA_SHIELD_PERCENT:
                return _shieldPercent;
            case DATA_RIFT_NUMBER:
                return _currentRift;
            default:
                break;
            }

            return 0;
        }

        void Update(uint32 diff) override
        {
            _scheduler.Update(diff);
        }

    private:
        bool ShouldKillEncounterStartCreature(Creature* creature) const
        {
            if (!creature || !creature->IsInWorld() || !creature->IsAlive())
                return false;

            if (creature->FindMap() != instance)
                return false;

            // Black Morass wildlife uses hostile faction 16.  Matching faction here is more reliable
            // than a local radius/cell scan because it catches every loaded creature currently in this
            // instance map when Medivh starts the event.
            if (creature->GetFaction() != 16)
                return false;

            // Safety guards.  These should not normally match faction 16, but keep them here so this
            // cleanup can never kill player-owned units if a custom template is misconfigured.
            if (creature->IsNPCBotOrPet())
                return false;

            if (creature->IsPet())
                return false;

            if (creature->IsControlledByPlayer())
                return false;

            if (creature->GetCharmerOrOwnerPlayerOrPlayerItself())
                return false;

            return true;
        }

        void KillEncounterStartBeasts()
        {
            if (_encounterStartBeastsKilled)
                return;

            _encounterStartBeastsKilled = true;

            std::vector<ObjectGuid> creatureGuids;
            creatureGuids.reserve(instance->GetCreatureBySpawnIdStore().size());

            for (auto const& pair : instance->GetCreatureBySpawnIdStore())
            {
                Creature* creature = pair.second;
                if (ShouldKillEncounterStartCreature(creature))
                    creatureGuids.push_back(creature->GetGUID());
            }

            uint32 killedCount = 0;
            for (ObjectGuid const& guid : creatureGuids)
            {
                Creature* creature = instance->GetCreature(guid);
                if (!ShouldKillEncounterStartCreature(creature))
                    continue;

                creature->KillSelf(false);
                ++killedCount;
            }

            LOG_INFO("scripts", "The Black Morass: killed {} loaded faction 16 creature(s) at encounter start.", killedCount);
        }

    protected:
        std::list<Position> _availableRiftPositions;
        GuidSet _encounterNPCs;
        Position _lastPortalPosition;
        uint8 _currentRift;
        int8 _shieldPercent;
        bool _portalSpawningEnabled;
        bool _encounterStartBeastsKilled;
        EventStatus _eventStatus;
        TaskScheduler _scheduler;
    };
};

void AddSC_instance_the_black_morass()
{
    new instance_the_black_morass();
}
