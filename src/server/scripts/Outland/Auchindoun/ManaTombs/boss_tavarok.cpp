/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "mana_tombs.h"

 // NPCBots
#include "bot_ai.h"

// Grid search helpers
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"

#include <cmath>

using namespace std::chrono_literals;

enum Spells
{
    SPELL_EARTHQUAKE = 33919,
    SPELL_CRYSTAL_PRISON = 32361,
    SPELL_ARCING_SMASH = 8374,

    // Custom
    SPELL_CRYSTAL_PILLAR_VISUAL = 300393,      // visual on the pillar
    SPELL_CRYSTAL_PILLAR_EXPLOSION = 300394    // AoE damage when pillar explodes
};

enum Misc
{
    NPC_CRYSTAL_PILLAR = 819188
};

enum Groups
{
    GROUP_GRAVITY_WELL = 1,
    GROUP_CRYSTAL_PRISON = 2,
    GROUP_ARCING_SMASH = 3,
    GROUP_CRYSTAL_PILLARS = 4
};

struct boss_tavarok : public BossAI
{
    boss_tavarok(Creature* creature) : BossAI(creature, DATA_TAVAROK)
    {
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    void Reset() override
    {
        _Reset();
        scheduler.CancelAll();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler
            // Gravity Well -> pull -> bots run out -> Earthquake 4s later
            .Schedule(15s, 25s, GROUP_GRAVITY_WELL, [this](TaskContext context)
                {
                    DoGravityWell();
                    context.Repeat(35s, 45s);
                })
            // Crystal Prison (skip the current tank)
            .Schedule(12s, 22s, GROUP_CRYSTAL_PRISON, [this](TaskContext context)
                {
                    DoCastOnRandomPlayerOrNPCBot(SPELL_CRYSTAL_PRISON, 100.0f, true);
                    context.Repeat(15s, 22s);
                })
            // Arcing Smash on current victim
            .Schedule(5900ms, GROUP_ARCING_SMASH, [this](TaskContext context)
                {
                    DoCastVictim(SPELL_ARCING_SMASH);
                    context.Repeat(8s, 12s);
                })
            // Crystal Pillars: periodically spawn on two random targets
            .Schedule(20s, 30s, GROUP_CRYSTAL_PILLARS, [this](TaskContext context)
                {
                    SummonCrystalPillars();
                    context.Repeat(30s, 40s);
                });
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
    }

    void KilledUnit(Unit* /*victim*/) override
    {
    }

private:
    Unit* SelectRandomPlayerOrNPCBot(float range = 0.0f, bool skipCurrentTank = false)
    {
        std::list<Unit*> targets;

        Unit* currentTank = skipCurrentTank ? me->GetVictim() : nullptr;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* target = ref->GetVictim();
            if (!target || !target->IsAlive())
                continue;

            if (currentTank && target == currentTank)
                continue;

            if (range > 0.0f && !me->IsWithinDistInMap(target, range))
                continue;

            if (!target->IsPlayer() && !target->IsNPCBot())
                continue;

            targets.push_back(target);
        }

        if (targets.empty())
            return nullptr;

        uint32 index = urand(0, static_cast<uint32>(targets.size() - 1));
        auto it = targets.begin();
        std::advance(it, index);
        return *it;
    }

    void DoCastOnRandomPlayerOrNPCBot(uint32 spellId, float range = 0.0f, bool skipCurrentTank = false)
    {
        if (Unit* target = SelectRandomPlayerOrNPCBot(range, skipCurrentTank))
            DoCast(target, spellId);
    }

    void DoGravityWell()
    {
        // Push pillar events away from this Earthquake window
        // so they don't land right before/during Quake.
        scheduler.DelayGroup(GROUP_CRYSTAL_PILLARS, 8s);

        // Immediately pull all hostile players + NPCBots toward Tavarok.
        PullHostilesToBoss(80.0f);

        // Give a short moment for the pull animation to "complete" and then send bots out.
        scheduler.Schedule(500ms, [this](TaskContext /*context*/)
            {
                MoveBotsToSafePositions(40.0f);
            });

        // 4s after the pull, cast Earthquake.
        scheduler.Schedule(4s, [this](TaskContext /*context*/)
            {
                DoCastSelf(SPELL_EARTHQUAKE);
            });

        // 1.5s after Earthquake (5.5s total), put bots back on follow.
        scheduler.Schedule(5500ms, [this](TaskContext /*context*/)
            {
                SetBotsToFollow();
            });
    }

    void PullHostilesToBoss(float radius)
    {
        float mx = me->GetPositionX();
        float my = me->GetPositionY();
        float mz = me->GetPositionZ();

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* target = ref->GetVictim();
            if (!target || !target->IsAlive())
                continue;

            if (!me->IsWithinDistInMap(target, radius))
                continue;

            if (target == me)
                continue;

            target->GetMotionMaster()->MoveJump(mx, my, mz, 10.0f, 10.0f);
        }
    }

    void MoveBotsToSafePositions(float desiredDist)
    {
        float mx = me->GetPositionX();
        float my = me->GetPositionY();

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (!u->IsNPCBot())
                continue;

            Creature* bot = u->ToCreature();
            if (!bot)
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai)
                continue;

            if (ai->HasRole(BOT_ROLE_TANK))
                continue;

            float ux = bot->GetPositionX();
            float uy = bot->GetPositionY();
            float uz = bot->GetPositionZ();

            float dx = ux - mx;
            float dy = uy - my;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001f)
            {
                dx = 1.0f;
                dy = 0.0f;
                len = 1.0f;
            }

            dx /= len;
            dy /= len;

            float x = mx + dx * desiredDist;
            float y = my + dy * desiredDist;
            float z = uz;

            // Adjust Z to ground to avoid floating.
            bot->UpdateAllowedPositionZ(x, y, z);

            // Try to prefer line-of-sight; if no LOS at full distance, try half distance.
            if (!me->IsWithinLOS(x, y, z))
            {
                float halfDist = desiredDist * 0.5f;
                x = mx + dx * halfDist;
                y = my + dy * halfDist;
                z = uz;
                bot->UpdateAllowedPositionZ(x, y, z);
            }

            Position dest;
            dest.Relocate(x, y, z, bot->GetOrientation());

            ai->MoveToSendPosition(dest);
        }
    }

    // Sapphiron-style: find *all* nearby NPCBots and force follow, not just those on threat list.
    void SetBotsToFollow()
    {
        std::list<Unit*> nearby;
        Bcore::AnyUnitInObjectRangeCheck check(me, 100.0f);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(me, nearby, check);
        Cell::VisitObjects(me, searcher, 100.0f);

        for (Unit* u : nearby)
        {
            if (!u || !u->IsAlive())
                continue;

            if (!u->IsNPCBot())
                continue;

            Creature* bot = u->ToCreature();
            if (!bot)
                continue;

            if (bot_ai* ai = bot->GetBotAI())
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }
    }

    void SummonCrystalPillars()
    {
        // Build a pool of all valid player/bot targets in range
        std::vector<Unit*> pool;
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* t = ref->GetVictim();
            if (!t || !t->IsAlive())
                continue;

            if (!t->IsPlayer() && !t->IsNPCBot())
                continue;

            if (!me->IsWithinDistInMap(t, 60.0f))
                continue;

            pool.push_back(t);
        }

        if (pool.empty())
            return;

        // We only want up to 2 unique targets per wave
        uint8 maxTargets = std::min<uint8>(2u, static_cast<uint8>(pool.size()));

        for (uint8 i = 0; i < maxTargets; ++i)
        {
            if (pool.empty())
                break;

            uint32 index = urand(0, static_cast<uint32>(pool.size() - 1));
            Unit* target = pool[index];
            pool.erase(pool.begin() + index); // avoid duplicates

            if (!target || !target->IsAlive())
                continue;

            DoCast(target, SPELL_CRYSTAL_PILLAR_VISUAL, true);

            Position basePos = target->GetPosition();

            float x = basePos.GetPositionX();
            float y = basePos.GetPositionY();
            float z = basePos.GetPositionZ();

            me->UpdateAllowedPositionZ(x, y, z);

            Position spawnPos;
            spawnPos.Relocate(x, y, z, basePos.GetOrientation());

            // Pillar still used as the explosion anchor
            me->SummonCreature(NPC_CRYSTAL_PILLAR, spawnPos, TEMPSUMMON_TIMED_DESPAWN, 10000);
        }
    }
};

struct npc_tavarok_crystal_pillar : public ScriptedAI
{
    npc_tavarok_crystal_pillar(Creature* creature) : ScriptedAI(creature) {}

    EventMap events;

    void Reset() override
    {
        events.Reset();
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        me->AddUnitState(UNIT_STATE_ROOT);

        // Pillar visual and timed explosion
        DoCastSelf(SPELL_CRYSTAL_PILLAR_VISUAL, true);
        events.ScheduleEvent(1, 8s);
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        DoCastSelf(SPELL_CRYSTAL_PILLAR_VISUAL, true);
        events.Reset();
        events.ScheduleEvent(1, 8s);
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            if (eventId == 1)
            {
                // Explosion, then despawn shortly after
                DoCastSelf(SPELL_CRYSTAL_PILLAR_EXPLOSION, true);
                me->DespawnOrUnsummon(1000ms);
            }
        }
    }
};

void AddSC_boss_tavarok()
{
    RegisterManaTombsCreatureAI(boss_tavarok);
    RegisterCreatureAI(npc_tavarok_crystal_pillar);
}
