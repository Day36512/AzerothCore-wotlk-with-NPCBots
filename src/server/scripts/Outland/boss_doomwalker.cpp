/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or at your option any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

#include "bot_ai.h"

#include <vector>

enum Texts
{
    SAY_AGGRO = 0,
    SAY_EARTHQUAKE = 1,
    SAY_OVERRUN = 2,
    SAY_SLAY = 3,
    SAY_DEATH = 4
};

enum Spells
{
    SPELL_EARTHQUAKE = 32686,
    SPELL_SUNDER_ARMOR = 33661,
    SPELL_CHAIN_LIGHTNING = 33665,
    SPELL_ENRAGE = 33653,
    SPELL_MARK_DEATH = 37128,
    SPELL_AURA_DEATH = 37131,
    SPELL_KNOCKDOWN = 13360,

    // Dinkle custom Overrun replacement.
    SPELL_CUSTOM_OVERRUN_BUFF = 502645, // 5 sec Overrun buff, cast on self
    SPELL_CUSTOM_OVERRUN_CHARGE = 502646  // charge selected random target
};

struct boss_doomwalker : public ScriptedAI
{
    boss_doomwalker(Creature* creature) : ScriptedAI(creature)
    {
    }

    void Reset() override
    {
        _inEnrage = false;
        _overrunning = false;

        scheduler.CancelAll();

        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveAurasDueToSpell(SPELL_CUSTOM_OVERRUN_BUFF);
    }

    void KilledUnit(Unit* victim) override
    {
        if (!victim)
            return;

        victim->CastSpell(victim, SPELL_MARK_DEATH, true);

        if (roll_chance_i(25) && victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();

        _inEnrage = false;
        _overrunning = false;

        me->RemoveAurasDueToSpell(SPELL_CUSTOM_OVERRUN_BUFF);

        Talk(SAY_DEATH);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);

        scheduler.Schedule(1s, [this](TaskContext context)
            {
                if (!me->IsAlive())
                    return;

                if (!HealthAbovePct(20))
                {
                    if (!me->HasAura(SPELL_ENRAGE))
                        DoCastSelf(SPELL_ENRAGE);

                    _inEnrage = true;
                    context.Repeat(6s);
                    return;
                }

                context.Repeat(1s);
            });

        scheduler.Schedule(5s, 13s, [this](TaskContext context)
            {
                if (_overrunning)
                {
                    context.Repeat(1s);
                    return;
                }

                DoCastVictim(SPELL_SUNDER_ARMOR);
                context.Repeat(10s, 25s);
            });

        scheduler.Schedule(10s, 30s, [this](TaskContext context)
            {
                if (_overrunning)
                {
                    context.Repeat(1s);
                    return;
                }

                DoCastRandomTarget(SPELL_CHAIN_LIGHTNING, 0, 0.0f, true, false, false);
                context.Repeat(7s, 27s);
            });

        scheduler.Schedule(25s, 35s, [this](TaskContext context)
            {
                if (_overrunning)
                {
                    context.Repeat(1s);
                    return;
                }

                Talk(SAY_EARTHQUAKE);

                // Avoid enrage + earthquake turning the raid into a meat chandelier.
                // The enrage scheduler will reapply it afterward if Doomwalker is still below 20%.
                if (_inEnrage)
                    me->RemoveAurasDueToSpell(SPELL_ENRAGE);

                DoCastSelf(SPELL_EARTHQUAKE);

                context.Repeat(30s, 55s);
            });

        scheduler.Schedule(30s, 45s, [this](TaskContext context)
            {
                if (_overrunning)
                {
                    context.Repeat(1s);
                    return;
                }

                StartOverrun();
                context.Repeat(25s, 40s);
            });
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (who && who->IsPlayer() && me->IsValidAttackTarget(who))
        {
            if (who->HasAura(SPELL_MARK_DEATH) && !who->HasAura(27827)) // Spirit of Redemption
                who->CastSpell(who, SPELL_AURA_DEATH, true);
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (_overrunning)
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _inEnrage = false;
    bool _overrunning = false;

    bool IsValidOverrunTarget(Unit* unit) const
    {
        if (!unit || !unit->IsAlive() || unit == me)
            return false;

        if (!me->IsValidAttackTarget(unit))
            return false;

        if (unit->GetTypeId() == TYPEID_PLAYER)
            return true;

        if (unit->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = unit->ToCreature();
            if (creature && creature->IsNPCBot())
                return true;
        }

        return false;
    }

    Unit* SelectRandomOverrunTarget()
    {
        std::vector<Unit*> candidates;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* unit = ref->GetVictim();
            if (!IsValidOverrunTarget(unit))
                continue;

            candidates.push_back(unit);
        }

        if (!candidates.empty())
            return candidates[urand(0, static_cast<uint32>(candidates.size() - 1))];

        Unit* victim = me->GetVictim();
        if (IsValidOverrunTarget(victim))
            return victim;

        return nullptr;
    }

    void StartOverrun()
    {
        if (!me->IsAlive() || !me->GetVictim())
            return;

        Unit* target = SelectRandomOverrunTarget();
        if (!target)
            return;

        Talk(SAY_OVERRUN);

        _overrunning = true;

        me->InterruptNonMeleeSpells(false);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);

        // Custom shenanigans version:
        // 1. Doomwalker gets his 5 second Overrun buff.
        // 2. Doomwalker charges a random hostile target, including NPCBots on threat.
        DoCastSelf(SPELL_CUSTOM_OVERRUN_BUFF, true);
        DoCast(target, SPELL_CUSTOM_OVERRUN_CHARGE);

        scheduler.Schedule(5500ms, [this](TaskContext /*context*/)
            {
                if (!me->IsAlive())
                    return;

                _overrunning = false;
                me->SetReactState(REACT_AGGRESSIVE);

                if (Unit* victim = me->GetVictim())
                    me->GetMotionMaster()->MoveChase(victim);
            });
    }
};

// 32686 - Earthquake
class spell_doomwalker_earthquake : public AuraScript
{
    PrepareAuraScript(spell_doomwalker_earthquake);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_KNOCKDOWN });
    }

    void HandleKnockdown(AuraEffect const* /*aurEff*/)
    {
        if (roll_chance_i(50))
            GetTarget()->CastSpell(GetTarget(), SPELL_KNOCKDOWN, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(
            spell_doomwalker_earthquake::HandleKnockdown,
            EFFECT_1,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_boss_doomwalker()
{
    RegisterCreatureAI(boss_doomwalker);
    RegisterSpellScript(spell_doomwalker_earthquake);
}
