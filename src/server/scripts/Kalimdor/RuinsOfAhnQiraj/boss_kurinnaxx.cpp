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

#include "CreatureScript.h"
#include "CreatureTextMgr.h"
#include "GameObjectAI.h"
#include "GameObjectScript.h"
#include "ScriptedCreature.h"
#include "TaskScheduler.h"
#include "ruins_of_ahnqiraj.h"

enum Spells
{
    SPELL_MORTAL_WOUND      = 25646,
    SPELL_SAND_TRAP         = 25648,
    SPELL_ENRAGE            = 26527,
    SPELL_SUMMON_PLAYER     = 26446,
    SPELL_WIDE_SLASH        = 25814,
    SPELL_THRASH            = 3391
};

enum Texts
{
    SAY_KURINNAXX_DEATH     = 5 // Yell by 'Ossirian the Unscarred'
};

struct boss_kurinnaxx : public BossAI
{
    boss_kurinnaxx(Creature* creature) : BossAI(creature, DATA_KURINNAXX) {}

    void InitializeAI() override
    {
        me->m_CombatDistance = 50.0f;
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        scheduler
        .Schedule(8s, 10s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_MORTAL_WOUND);
            context.Repeat(8s, 10s);
        })
        .Schedule(5s, 15s, [this](TaskContext context)
        {
            if (Unit* target = SelectSandTrapTarget())
            {
                target->CastSpell(target, SPELL_SAND_TRAP, true, nullptr, nullptr, me->GetGUID());
            }
            context.Repeat(5s, 15s);
        })
        .Schedule(10s, 15s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_WIDE_SLASH);
            context.Repeat(12s, 15s);
        })
        .Schedule(16s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_THRASH);
            context.Repeat(16s);
        });

        ScheduleHealthCheckEvent(30, [&]
        {
            DoCastSelf(SPELL_ENRAGE);
        });
    }

    void JustDied(Unit* killer) override
    {
        if (killer)
        {
            if (Player* player = killer->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                if (Creature* creature = player->SummonCreature(NPC_ANDOROV, -8538.177f, 1486.0956f, 32.39054f, 3.7638654f, TEMPSUMMON_CORPSE_DESPAWN, 0))
                {
                    creature->setActive(true);
                }
            }
        }

        if (Creature* ossirian = instance->GetCreature(DATA_OSSIRIAN))
        {
            ossirian->setActive(true);
            if (ossirian->GetAI())
                ossirian->AI()->Talk(SAY_KURINNAXX_DEATH);
        }

        BossAI::JustDied(killer);
    }

private:
    static constexpr float SAND_TRAP_RANGE = 100.0f;

    bool IsValidSandTrapTarget(Unit* unit) const
    {
        return unit
            && unit->IsAlive()
            && me->IsValidAttackTarget(unit)
            && me->IsWithinDistInMap(unit, SAND_TRAP_RANGE);
    }

    Unit* SelectSandTrapTarget()
    {
        GuidVector candidates;
        candidates.reserve(32);

        me->GetMap()->DoForAllPlayers([&](Player* player)
        {
            if (player->IsGameMaster())
                return;

            if (IsValidSandTrapTarget(player))
                candidates.push_back(player->GetGUID());
        });

        std::list<Unit*> nearby;
        {
            Acore::AnyUnitInObjectRangeCheck check(me, SAND_TRAP_RANGE);
            Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, nearby, check);
            Cell::VisitObjects(me, searcher, SAND_TRAP_RANGE);
        }

        for (Unit* u : nearby)
        {
            if (u->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* c = u->ToCreature();
            if (!c)
                continue;

            if (!c->IsNPCBot())
                continue;

            if (IsValidSandTrapTarget(c))
                candidates.push_back(c->GetGUID());
        }

        if (candidates.empty())
            return nullptr;

        ObjectGuid chosen = Acore::Containers::SelectRandomContainerElement(candidates);
        return ObjectAccessor::GetUnit(*me, chosen);
    }
};

struct go_sand_trap : public GameObjectAI
{
    go_sand_trap(GameObject* go) : GameObjectAI(go) { }

    void Reset() override
    {
        _scheduler.Schedule(5s, [this](TaskContext /*context*/)
        {
            if (InstanceScript* instance = me->GetInstanceScript())
                if (Creature* kurinnaxx = instance->GetCreature(DATA_KURINNAXX))
                    me->Use(kurinnaxx);
        });
    }

    void UpdateAI(uint32 const diff) override
    {
        _scheduler.Update(diff);
    }

protected:
    TaskScheduler _scheduler;
};

void AddSC_boss_kurinnaxx()
{
    RegisterRuinsOfAhnQirajCreatureAI(boss_kurinnaxx);
    RegisterGameObjectAI(go_sand_trap);
}
