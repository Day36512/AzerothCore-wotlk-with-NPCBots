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
#include "ScriptedCreature.h"
#include "SpellScriptLoader.h"
#include "hellfire_ramparts.h"
#include "SpellScript.h"

enum Says
{
    SAY_TAUNT               = 0,
    SAY_HEAL                = 1,
    SAY_SURGE               = 2,
    SAY_AGGRO               = 3,
    SAY_KILL                = 4,
    SAY_DIE                 = 5
};

enum Spells
{
    SPELL_MORTAL_WOUND      = 30641,
    SPELL_SURGE             = 34645,
    SPELL_RETALIATION       = 22857,
    SPELL_FRENZY            = 28131
};

enum Misc
{
    NPC_HELLFIRE_WATCHER    = 17309
};

struct boss_watchkeeper_gargolmar : public BossAI
{
    boss_watchkeeper_gargolmar(Creature* creature) : BossAI(creature, DATA_WATCHKEEPER_GARGOLMAR)
    {
        _taunted = false;
        _hasSpoken = false;
        _frenzied = false;
        scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    void Reset() override
    {
        _Reset();
        _taunted = false;
        _hasSpoken = false;
        _frenzied = false;
        ScheduleHealthCheckEvent(50, [&]{
            Talk(SAY_HEAL);
            std::list<Creature*> clist;
            me->GetCreaturesWithEntryInRange(clist, 100.0f, NPC_HELLFIRE_WATCHER);
            for (std::list<Creature*>::const_iterator itr = clist.begin(); itr != clist.end(); ++itr)
            {
                (*itr)->AI()->SetData(NPC_HELLFIRE_WATCHER, 0);
            }
        });

        ScheduleHealthCheckEvent(20, [&]{
            DoCastSelf(SPELL_RETALIATION);
            scheduler.Schedule(30s, [this](TaskContext context)
            {
                DoCastSelf(SPELL_RETALIATION);
                context.Repeat(30s);
            });
        });
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        _JustEngagedWith();
        scheduler.Schedule(5s, [this] (TaskContext context)
        {
            DoCastVictim(SPELL_MORTAL_WOUND);
            context.Repeat(8s);
        }).Schedule(3s, [this](TaskContext context)
        {
            Talk(SAY_SURGE);
            CastSpellOnRandomTarget(SPELL_SURGE, 60.0f);
            context.Repeat(11s);
        });
        scheduler.Schedule(1s, [this](TaskContext context)
            {
                if (!_frenzied && BothHealersDead())
                {
                    _frenzied = true;
                    DoCastSelf(SPELL_FRENZY, true);
                    return;
                }
                context.Repeat(1s);
            });
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!_taunted)
        {
            if (who->IsPlayer())
            {
                _taunted = true;
                Talk(SAY_TAUNT);
            }
        }
        BossAI::MoveInLineOfSight(who);
    }

    void KilledUnit(Unit*) override
    {
        if (!_hasSpoken)
        {
            _hasSpoken = true;
            Talk(SAY_KILL);
        }
        scheduler.Schedule(6s, [this](TaskContext /*context*/)
        {
            _hasSpoken = false;
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DIE);
        _JustDied();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _taunted;
    bool _hasSpoken;
    bool _frenzied;

    void CastSpellOnRandomTarget(uint32 spellId, float range)
    {
        std::list<Unit*> targets;
        Acore::AnyUnitInObjectRangeCheck check(me, range);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, targets, check);
        Cell::VisitObjects(me, searcher, range);

        targets.remove_if([this](Unit* unit) -> bool
            {
                if (!unit || !unit->IsAlive())
                    return true;

                bool isPlayer = unit->GetTypeId() == TYPEID_PLAYER;
                bool isNpcBot = (unit->GetTypeId() == TYPEID_UNIT) && unit->ToCreature() && unit->ToCreature()->IsNPCBot();
                if (!isPlayer && !isNpcBot)
                    return true;

                if (!me->IsValidAttackTarget(unit))
                    return true;

                return false;
            });

        if (!targets.empty())
        {
            Unit* target = Acore::Containers::SelectRandomContainerElement(targets);
            DoCast(target, spellId);
        }
    }
    bool BothHealersDead()
    {
        std::list<Creature*> watchers;
        me->GetCreaturesWithEntryInRange(watchers, 100.0f, NPC_HELLFIRE_WATCHER);

        uint8 alive = 0u;
        for (Creature* c : watchers)
            if (c && c->IsAlive() && !c->isDead())
                ++alive;

        return alive == 0u && !watchers.empty(); 
    }
};

class spell_gargolmar_retalliation : public AuraScript
{
    PrepareAuraScript(spell_gargolmar_retalliation);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!eventInfo.GetActor() || !eventInfo.GetProcTarget())
        {
            return false;
        }

        return GetTarget()->isInFront(eventInfo.GetActor(), M_PI);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_gargolmar_retalliation::CheckProc);
    }
};

void AddSC_boss_watchkeeper_gargolmar()
{
    RegisterHellfireRampartsCreatureAI(boss_watchkeeper_gargolmar);
    RegisterSpellScript(spell_gargolmar_retalliation);
}
