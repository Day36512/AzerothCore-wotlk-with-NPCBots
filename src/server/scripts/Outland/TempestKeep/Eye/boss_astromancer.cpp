/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Cell.h"
#include "CellImpl.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "the_eye.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceBombOnMe(Creature* bot, uint32 spellId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Yells
{
    SAY_AGGRO                           = 0,
    SAY_SUMMON                          = 1,
    SAY_KILL                            = 2,
    SAY_DEATH                           = 3,
    SAY_VOID                            = 4
};

enum Spells
{
    SPELL_SOLARIAN_TRANSFORM            = 39117,
    SPELL_ARCANE_MISSILES               = 33031,
    SPELL_WRATH_OF_THE_ASTROMANCER      = 42783,
    SPELL_BLINDING_LIGHT                = 33009,
    SPELL_PSYCHIC_SCREAM                = 34322,
    SPELL_VOID_BOLT                     = 39329,
    SPELL_TRUE_BEAM                     = 33365,
    SPELL_TELEPORT_START_POSITION       = 33244, // Serverside
};

enum Misc
{
    DISPLAYID_INVISIBLE                 = 11686,
    NPC_HIGH_ASTROMANCER_SOLARIAN       = 18805,
    NPC_ASTROMANCER_SOLARIAN_SPOTLIGHT  = 18928,
    NPC_SOLARIUM_AGENT                  = 18925,
    NPC_SOLARIUM_PRIEST                 = 18806
};

#define INNER_PORTAL_RADIUS         14.0f
#define OUTER_PORTAL_RADIUS         28.0f
#define CENTER_X                    432.909f
#define CENTER_Y                    -373.424f
#define CENTER_Z                    17.9608f
#define CENTER_O                    1.06421f
#define PORTAL_Z                    17.005f
#define START_POSITION_X            432.74f
#define START_POSITION_Y            -373.645f
#define START_POSITION_Z            18.0138f

namespace
{
    constexpr float SOLARIAN_ENCOUNTER_RANGE = 150.0f;
    constexpr float SOLARIAN_ROOM_RADIUS = 95.0f;
    constexpr float SOLARIAN_WRATH_SAFE_CLEAR_RADIUS = 25.0f;
    constexpr float SOLARIAN_WRATH_SEARCH_RADIUS_MAX = 120.0f;
    constexpr float SOLARIAN_ADD_PRIORITY_RANGE = 120.0f;
    constexpr float SOLARIAN_ADD_TANK_THREAT = 75000.0f;
    constexpr float SOLARIAN_PRIEST_DPS_THREAT = 15000.0f;
    constexpr float SOLARIAN_BOSS_TANK_THREAT = 75000.0f;
    constexpr float SOLARIAN_FINAL_TANK_THREAT = 125000.0f;

    bool IsValidSolarianEncounterBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && creature->GetBotAI() && creature->IsAlive() && creature->IsInWorld() &&
            !creature->IsTempBot() && !creature->IsFreeBot();
    }

    bool IsValidSolarianEncounterUnit(Unit const* unit)
    {
        if (!unit || !unit->IsAlive() || !unit->IsInWorld() || unit->HasUnitState(UNIT_STATE_ISOLATED))
            return false;

        if (Player const* player = unit->ToPlayer())
            return !player->IsGameMaster();

        return IsValidSolarianEncounterBot(unit->ToCreature());
    }

    bool IsInSolarianRoom(Unit const* unit)
    {
        if (!unit)
            return false;

        return unit->GetExactDist2d(CENTER_X, CENTER_Y) <= SOLARIAN_ROOM_RADIUS &&
            std::abs(unit->GetPositionZ() - CENTER_Z) <= 12.0f;
    }

    bool IsInSolarianRoom(Position const& position)
    {
        float dx = position.GetPositionX() - CENTER_X;
        float dy = position.GetPositionY() - CENTER_Y;
        return std::sqrt(dx * dx + dy * dy) <= SOLARIAN_ROOM_RADIUS &&
            std::abs(position.GetPositionZ() - CENTER_Z) <= 12.0f;
    }

    bool IsSolarianEncounterParticipantFor(Creature const* source, Unit const* unit, float range = SOLARIAN_ENCOUNTER_RANGE)
    {
        if (!source || !IsValidSolarianEncounterUnit(unit) || unit == source)
            return false;

        if (source->GetMap() != unit->GetMap() || !source->IsWithinDistInMap(unit, range))
            return false;

        if (source->GetVictim() == unit)
            return true;

        if (unit->IsInCombatWith(source))
            return true;

        if (source->GetThreatMgr().GetThreat(unit, true) > 0.0f)
            return true;

        return source->IsInCombat() && unit->IsInCombat() && IsInSolarianRoom(unit);
    }

    std::vector<Unit*> GatherSolarianEncounterUnits(Creature const* source, float range = SOLARIAN_ENCOUNTER_RANGE)
    {
        std::vector<Unit*> units;

        auto addUnit = [&](Unit* unit)
        {
            if (IsSolarianEncounterParticipantFor(source, unit, range))
                units.push_back(unit);
        };

        if (!source || !source->GetMap())
            return units;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            addUnit(player);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    addUnit(member);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
                if (BotMap const* botMap = botMgr->GetBotMap())
                    for (BotMap::value_type const& pair : *botMap)
                        addUnit(pair.second);
        }

        std::sort(units.begin(), units.end(), [](Unit const* left, Unit const* right)
        {
            return left->GetGUID() < right->GetGUID();
        });

        units.erase(std::unique(units.begin(), units.end()), units.end());
        return units;
    }

    std::vector<Creature*> GatherSolarianEncounterBots(Creature const* source, float range = SOLARIAN_ENCOUNTER_RANGE)
    {
        std::vector<Creature*> bots;

        for (Unit* unit : GatherSolarianEncounterUnits(source, range))
            if (Creature* bot = unit ? unit->ToCreature() : nullptr)
                if (IsValidSolarianEncounterBot(bot))
                    bots.push_back(bot);

        return bots;
    }

    bool IsSolarianTankBot(Creature const* bot)
    {
        bot_ai const* ai = IsValidSolarianEncounterBot(bot) ? bot->GetBotAI() : nullptr;
        return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK) || ai->HasRole(BOT_ROLE_TANK_OFF));
    }

    bool IsSolarianDpsBot(Creature const* bot)
    {
        bot_ai const* ai = IsValidSolarianEncounterBot(bot) ? bot->GetBotAI() : nullptr;
        return ai && ai->HasRole(BOT_ROLE_DPS) && !ai->HasRole(BOT_ROLE_HEAL);
    }

    bool IsSolarianMeleeDpsBot(Creature const* bot)
    {
        bot_ai const* ai = IsValidSolarianEncounterBot(bot) ? bot->GetBotAI() : nullptr;
        return ai && ai->HasRole(BOT_ROLE_DPS) && !ai->HasRole(BOT_ROLE_HEAL) && !ai->HasRole(BOT_ROLE_RANGED) &&
            (MELEE_BOT_CLASSES_MASK & (1u << ai->GetBotClass())) != 0;
    }

    Unit* SelectSolarianRandomRaidTarget(Creature* source, float range, uint32 excludeAura = 0)
    {
        std::vector<Unit*> targets = GatherSolarianEncounterUnits(source, range);

        targets.erase(std::remove_if(targets.begin(), targets.end(), [excludeAura](Unit* unit)
        {
            return !unit || (excludeAura && unit->HasAura(excludeAura));
        }), targets.end());

        if (targets.empty())
            return nullptr;

        return targets[urand(0, uint32(targets.size() - 1))];
    }

    bool IsSolarianAdd(Creature const* creature)
    {
        if (!creature || !creature->IsAlive())
            return false;

        return creature->GetEntry() == NPC_SOLARIUM_AGENT || creature->GetEntry() == NPC_SOLARIUM_PRIEST;
    }

    void ForceSolarianBotEngage(Creature* bot, Creature* target, float threat)
    {
        bot_ai* ai = IsValidSolarianEncounterBot(bot) ? bot->GetBotAI() : nullptr;
        if (!ai || !target || !target->IsAlive() || !target->CanHaveThreatList())
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FOLLOW | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_ATTACK);
        bot->SetInCombatWith(target);
        target->SetInCombatWith(bot);
        target->GetThreatMgr().AddThreat(bot, threat, nullptr, true, true);
        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        ai->AttackStart(target);
    }

    Creature* SelectSolarianTankBotFor(Creature* source)
    {
        Creature* best = nullptr;
        float bestScore = std::numeric_limits<float>::max();

        for (Creature* bot : GatherSolarianEncounterBots(source))
        {
            if (!IsSolarianTankBot(bot) || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            float score = source->GetDistance(bot);
            if (bot == source->GetVictim())
                score -= 40.0f;
            if (bot->GetBotAI()->IsTank())
                score -= 30.0f;
            if (bot->GetBotAI()->IsOffTank())
                score -= 15.0f;

            if (!best || score < bestScore)
            {
                best = bot;
                bestScore = score;
            }
        }

        return best;
    }
}

struct boss_high_astromancer_solarian : public BossAI
{
    boss_high_astromancer_solarian(Creature* creature) : BossAI(creature, DATA_ASTROMANCER)
    {
        callForHelpRange = 105.0f;
    }

    void AssignSolarianBossTank(char const* reason, float threat)
    {
        if (!me->CanHaveThreatList())
            return;

        if (Unit* victim = me->GetVictim())
            if (victim->IsPlayer())
                return;

        Creature* tankBot = SelectSolarianTankBotFor(me);
        if (!tankBot)
            return;

        ForceSolarianBotEngage(tankBot, me, threat);
        LOG_DEBUG("scripts", "Solarian NPCBot: assigned {} to tank Solarian ({})", tankBot->GetName(), reason ? reason : "encounter");
    }

    void CommandSolarianDpsToBoss()
    {
        if (!me->CanHaveThreatList() || !me->IsAlive())
            return;

        for (Creature* bot : GatherSolarianEncounterBots(me))
        {
            if (!IsSolarianDpsBot(bot) || IsSolarianTankBot(bot) || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai)
                continue;

            bot->SetInCombatWith(me);
            me->SetInCombatWith(bot);
            if (me->GetThreatMgr().GetThreat(bot, true) <= 0.0f)
                me->GetThreatMgr().AddThreat(bot, 1000.0f, nullptr, true, true);

            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(me);
        }
    }

    void SeedSolarianAddThreat(Creature* add)
    {
        if (!add || !IsSolarianAdd(add) || !add->CanHaveThreatList())
            return;

        Creature* tankBot = SelectSolarianTankBotFor(add);
        if (tankBot)
        {
            ForceSolarianBotEngage(tankBot, add, SOLARIAN_ADD_TANK_THREAT);
            if (add->AI())
                add->AI()->AttackStart(tankBot);
        }

        for (Creature* bot : GatherSolarianEncounterBots(me))
        {
            if (!bot || bot == tankBot || !add->IsValidAttackTarget(bot))
                continue;

            if (add->GetEntry() == NPC_SOLARIUM_PRIEST && IsSolarianMeleeDpsBot(bot))
            {
                ForceSolarianBotEngage(bot, add, SOLARIAN_PRIEST_DPS_THREAT);
                continue;
            }

            if (add->GetEntry() == NPC_SOLARIUM_AGENT && IsSolarianDpsBot(bot))
            {
                bot->SetInCombatWith(add);
                add->SetInCombatWith(bot);
                if (add->GetThreatMgr().GetThreat(bot, true) <= 0.0f)
                    add->GetThreatMgr().AddThreat(bot, 1000.0f, nullptr, true, true);
            }
        }

        LOG_DEBUG("scripts", "Solarian NPCBot: seeded bot threat for {} {}", add->GetName(), add->GetGUID().GetCounter());
    }

    void CommandSolarianBotsToPrioritizeAdds(uint32 entry)
    {
        std::list<Creature*> adds;
        me->GetCreatureListWithEntryInGrid(adds, entry, SOLARIAN_ADD_PRIORITY_RANGE);

        uint32 handled = 0;
        for (Creature* add : adds)
        {
            if (!add || !add->IsAlive())
                continue;

            SeedSolarianAddThreat(add);
            ++handled;
        }

        if (handled)
            LOG_DEBUG("scripts", "Solarian NPCBot: refreshed priority handling for {} Solarium add(s) entry {}", handled, entry);
    }

    void HandleSolarianFinalTransform()
    {
        AssignSolarianBossTank("final transform", SOLARIAN_FINAL_TANK_THREAT);
        CommandSolarianDpsToBoss();
        LOG_DEBUG("scripts", "Solarian NPCBot: final Voidwalker phase support activated.");
    }

    void Reset() override
    {
        BossAI::Reset();
        me->SetModelVisible(true);
        me->SetReactState(REACT_AGGRESSIVE);

        ScheduleHealthCheckEvent(20, [&]{
            Talk(SAY_VOID);
            me->InterruptNonMeleeSpells(false);
            scheduler.CancelAll();
            me->SetModelVisible(true);
            me->ResumeChasingVictim();
            scheduler.Schedule(3s, [this](TaskContext context)
            {
                DoCastVictim(SPELL_VOID_BOLT);
                context.Repeat(7s);
            }).Schedule(7s, [this](TaskContext context)
            {
                DoCastSelf(SPELL_PSYCHIC_SCREAM);
                context.Repeat(12s);
            });
            DoCastSelf(SPELL_SOLARIAN_TRANSFORM, true);
            HandleSolarianFinalTransform();
        });
    }

    void AttackStart(Unit* who) override
    {
        if (who && me->Attack(who, true))
        {
            me->GetMotionMaster()->MoveChase(who, 0.0f);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && roll_chance_i(50))
        {
            Talk(SAY_KILL);
        }
    }

    void JustDied(Unit* killer) override
    {
        me->SetModelVisible(true);
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO);
        BossAI::JustEngagedWith(who);
        me->GetMotionMaster()->Clear();
        AssignSolarianBossTank("engage", SOLARIAN_BOSS_TANK_THREAT);

        scheduler.Schedule(3650ms, [this](TaskContext context)
        {
            me->GetMotionMaster()->Clear();
            if (Unit* target = SelectSolarianRandomRaidTarget(me, 40.0f, SPELL_WRATH_OF_THE_ASTROMANCER))
            {
                DoCast(target, SPELL_ARCANE_MISSILES);
                LOG_DEBUG("scripts", "Solarian NPCBot: Arcane Missiles selected {} target {}", target->IsPlayer() ? "player" : "NPCBot", target->GetName());
            }
            else
            {
                //no targets in required range
                me->GetMotionMaster()->MoveChase(me->GetVictim(), 30.0f);
                me->CastStop();
            }
            context.Repeat(800ms, 7300ms);
        }).Schedule(21800ms, [this](TaskContext context)
        {
            if (Unit* target = SelectSolarianRandomRaidTarget(me, 100.0f, SPELL_WRATH_OF_THE_ASTROMANCER))
            {
                DoCast(target, SPELL_WRATH_OF_THE_ASTROMANCER);
                LOG_DEBUG("scripts", "Solarian NPCBot: Wrath of the Astromancer selected {} target {}", target->IsPlayer() ? "player" : "NPCBot", target->GetName());
            }
            context.Repeat(21800ms, 23350ms);
        }).Schedule(33900ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_BLINDING_LIGHT);
            context.Repeat(33900ms, 48100ms);
        }).Schedule(52100ms, [this](TaskContext context)
        {
            me->SetReactState(REACT_PASSIVE);
            scheduler.DelayAll(22s);
            DoCastSelf(SPELL_TELEPORT_START_POSITION);
            scheduler.Schedule(1s, [this](TaskContext)
            {
                for (uint8 i = 0; i < 3; ++i)
                {
                    float o = rand_norm() * 2 * M_PI;
                    if (i == 0)
                    {
                        me->SummonCreature(NPC_ASTROMANCER_SOLARIAN_SPOTLIGHT, CENTER_X + cos(o)*INNER_PORTAL_RADIUS, CENTER_Y + std::sin(o)*INNER_PORTAL_RADIUS, CENTER_Z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 25000);
                    }
                    else
                    {
                        me->SummonCreature(NPC_ASTROMANCER_SOLARIAN_SPOTLIGHT, CENTER_X + cos(o)*OUTER_PORTAL_RADIUS, CENTER_Y + std::sin(o)*OUTER_PORTAL_RADIUS, PORTAL_Z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 25000);
                    }
                }
            }).Schedule(2s, [this](TaskContext)
            {
                Talk(SAY_SUMMON);
            }).Schedule(3s, [this](TaskContext)
            {
                me->RemoveAllAuras();
                me->SetModelVisible(false);
            }).Schedule(7s, [this](TaskContext)
            {
                summons.DoForAllSummons([&](WorldObject* summon)
                {
                    if (Creature* light = summon->ToCreature())
                    {
                        if (light->GetEntry() == NPC_ASTROMANCER_SOLARIAN_SPOTLIGHT)
                        {
                            if (light->GetDistance2d(CENTER_X, CENTER_Y) < 20.0f)
                            {
                                DoCast(light, SPELL_TRUE_BEAM);
                                me->SetPosition(*light);
                                me->StopMovingOnCurrentPos();
                            }
                            for (uint8 j = 0; j < 4; ++j)
                            {
                                me->SummonCreature(NPC_SOLARIUM_AGENT, light->GetPositionX() + frand(-3.0f, 3.0f), light->GetPositionY() + frand(-3.0f, 3.0f), light->GetPositionZ(), 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
                            }
                        }
                    }
                });
                CommandSolarianBotsToPrioritizeAdds(NPC_SOLARIUM_AGENT);
            }).Schedule(23s, [this](TaskContext)
            {
                me->SetReactState(REACT_AGGRESSIVE);
                DoResetThreatList();
                summons.DoForAllSummons([&](WorldObject* summon)
                {
                    if (Creature* light = summon->ToCreature())
                    {
                        if (light->GetEntry() == NPC_ASTROMANCER_SOLARIAN_SPOTLIGHT)
                        {
                            light->RemoveAllAuras();
                            if (light->GetDistance2d(CENTER_X, CENTER_Y) < 20.0f)
                            {
                                me->RemoveAllAuras();
                                me->SetModelVisible(true);
                            }
                            else
                            {
                                me->SummonCreature(NPC_SOLARIUM_PRIEST, light->GetPositionX() + frand(-3.0f, 3.0f), light->GetPositionY() + frand(-3.0f, 3.0f), light->GetPositionZ(), 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
                            }
                        }
                    }
                });
                CommandSolarianBotsToPrioritizeAdds(NPC_SOLARIUM_PRIEST);
                AssignSolarianBossTank("reappear", SOLARIAN_BOSS_TANK_THREAT);
            });
            context.Repeat(87500ms, 91200ms);
        });
    }

     void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (me->GetReactState() == REACT_AGGRESSIVE)
        {
            DoMeleeAttackIfReady();
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (!summon->IsTrigger())
        {
            summon->SetInCombatWithZone();
            if (IsSolarianAdd(summon))
                SeedSolarianAddThreat(summon);
        }
    }
};

class spell_astromancer_wrath_of_the_astromancer : public AuraScript
{
    PrepareAuraScript(spell_astromancer_wrath_of_the_astromancer);

    void CollectNearbyUnits(Unit* center, std::vector<Unit*>& out)
    {
        out.clear();

        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck check(center, SOLARIAN_WRATH_SEARCH_RADIUS_MAX);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(center, nearby, check);
        Cell::VisitObjects(center, searcher, SOLARIAN_WRATH_SEARCH_RADIUS_MAX);

        for (Unit* unit : nearby)
            if (unit && unit->IsAlive())
                out.push_back(unit);
    }

    void MovePetIfOwned(Unit* owner, Position const& destination)
    {
        if (!owner)
            return;

        ObjectGuid ownerGuid = owner->GetGUID();

        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck check(owner, 40.0f);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(owner, nearby, check);
        Cell::VisitObjects(owner, searcher, 40.0f);

        for (Unit* unit : nearby)
        {
            if (!unit || !unit->IsAlive() || unit->GetOwnerGUID() != ownerGuid)
                continue;

            if (Creature* creature = unit->ToCreature())
            {
                creature->AttackStop();
                creature->GetMotionMaster()->Clear();
                creature->GetMotionMaster()->MovePoint(0, destination);
            }
        }
    }

    bool IsCandidateClear(Position const& position, Unit* carrier, std::vector<Unit*> const& crowd)
    {
        ObjectGuid ownerGuid = carrier->GetGUID();

        for (Unit* unit : crowd)
        {
            if (!unit || unit == carrier || unit->GetOwnerGUID() == ownerGuid)
                continue;

            if (unit->GetDistance2d(position.GetPositionX(), position.GetPositionY()) < SOLARIAN_WRATH_SAFE_CLEAR_RADIUS)
                return false;
        }

        return true;
    }

    bool IsPathableAndInLOS(Unit* carrier, Position& candidate)
    {
        Map* map = carrier ? carrier->GetMap() : nullptr;
        if (!map)
            return false;

        float x = candidate.GetPositionX();
        float y = candidate.GetPositionY();
        float z = candidate.GetPositionZ();

        if (!map->CanReachPositionAndGetValidCoords(carrier, x, y, z, true, true))
            return false;

        candidate.Relocate(x, y, z, candidate.GetOrientation());

        if (!IsInSolarianRoom(candidate))
            return false;

        if (!carrier->IsWithinLOS(x, y, z))
            return false;

        if (!map->isInLineOfSight(
            carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ(),
            x, y, z,
            carrier->GetPhaseMask(),
            LineOfSightChecks::LINEOFSIGHT_ALL_CHECKS,
            VMAP::ModelIgnoreFlags::M2))
        {
            return false;
        }

        return true;
    }

    bool FindSafeSpot(Unit* carrier, Position& out)
    {
        if (!carrier)
            return false;

        std::vector<Unit*> crowd;
        CollectNearbyUnits(carrier, crowd);

        float const baseX = carrier->GetPositionX();
        float const baseY = carrier->GetPositionY();
        float const baseZ = carrier->GetPositionZ();
        float baseAngle = std::atan2(baseY - CENTER_Y, baseX - CENTER_X);
        if (baseAngle == 0.0f)
            baseAngle = carrier->GetOrientation();

        static constexpr float radii[] = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f, 55.0f, 60.0f, 65.0f, 70.0f };
        static constexpr int32 angleOffsets[] = { 0, 15, -15, 30, -30, 45, -45, 60, -60, 90, -90, 120, -120, 150, -150, 180 };

        for (float radius : radii)
        {
            for (int32 offsetDegrees : angleOffsets)
            {
                float angle = baseAngle + float(offsetDegrees) * float(M_PI) / 180.0f;
                float x = baseX + radius * std::cos(angle);
                float y = baseY + radius * std::sin(angle);
                float z = baseZ;

                carrier->UpdateAllowedPositionZ(x, y, z);

                Position candidate;
                candidate.Relocate(x, y, z, angle);

                if (IsInSolarianRoom(candidate) && IsCandidateClear(candidate, carrier, crowd) && IsPathableAndInLOS(carrier, candidate))
                {
                    out = candidate;
                    return true;
                }
            }
        }

        return false;
    }

    void MoveWrathBotAway(Creature* bot)
    {
        if (!IsValidSolarianEncounterBot(bot))
            return;

        if (bot_ai* ai = bot->GetBotAI())
        {
            Position destination;
            if (!FindSafeSpot(bot, destination))
            {
                LOG_DEBUG("scripts", "Solarian NPCBot: no safe Wrath position found for {}", bot->GetName());
                return;
            }

            bot->InterruptNonMeleeSpells(false);
            bot->AttackStop();
            bot->BotStopMovement();
            ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW | BOT_COMMAND_ATTACK);
            ai->SetBotCommandState(BOT_COMMAND_STAY);
            ai->MoveToSendPosition(destination);
            MovePetIfOwned(bot, destination);

            LOG_DEBUG("scripts", "Solarian NPCBot: moved Wrath bot {} to {:.2f} {:.2f} {:.2f}", bot->GetName(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
        }
    }

    void ReturnWrathBot(Unit* target)
    {
        Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target);
        if (!IsValidSolarianEncounterBot(bot))
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_COMBATRESET);

        if (Creature* solarian = bot->FindNearestCreature(NPC_HIGH_ASTROMANCER_SOLARIAN, SOLARIAN_ENCOUNTER_RANGE, true))
        {
            if (solarian->IsAlive() && solarian->IsInCombat() && !ai->HasRole(BOT_ROLE_HEAL))
            {
                ForceSolarianBotEngage(bot, solarian, IsSolarianTankBot(bot) ? SOLARIAN_BOSS_TANK_THREAT : 1000.0f);
                LOG_DEBUG("scripts", "Solarian NPCBot: returned Wrath bot {} to attack Solarian.", bot->GetName());
                return;
            }
        }

        ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        LOG_DEBUG("scripts", "Solarian NPCBot: returned Wrath bot {} to follow.", bot->GetName());
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target || target->IsPlayer())
            return;

        Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target);
        if (!IsValidSolarianEncounterBot(bot))
            return;

        DBMFTABotCallouts::AnnounceBombOnMe(bot, SPELL_WRATH_OF_THE_ASTROMANCER, "Wrath of the Astromancer", DBMFTABotCallouts::GetCooldownMs());
        LOG_DEBUG("scripts", "Solarian NPCBot: Wrath of the Astromancer applied to bot {}; callout sent through DBM-FTA BotCallouts.", bot->GetName());
        MoveWrathBotAway(bot);
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        bool const expired = GetTargetApplication() && GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE;

        Unit* target = GetUnitOwner();
        if (expired && target)
            target->CastSpell(target, GetSpellInfo()->Effects[EFFECT_1].CalcValue(), false, nullptr, nullptr, GetCaster() ? GetCaster()->GetGUID() : ObjectGuid::Empty);

        ReturnWrathBot(target);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_astromancer_wrath_of_the_astromancer::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_astromancer_wrath_of_the_astromancer::AfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_astromancer_solarian_transform : public AuraScript
{
    PrepareAuraScript(spell_astromancer_solarian_transform);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->ApplyStatPctModifier(UNIT_MOD_ARMOR, TOTAL_PCT, 400.0f);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->ApplyStatPctModifier(UnitMods(UNIT_MOD_ARMOR), TOTAL_PCT, -80.0f);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_astromancer_solarian_transform::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_astromancer_solarian_transform::OnRemove, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_high_astromancer_solarian()
{
    RegisterTheEyeAI(boss_high_astromancer_solarian);
    RegisterSpellScript(spell_astromancer_wrath_of_the_astromancer);
    RegisterSpellScript(spell_astromancer_solarian_transform);
}
