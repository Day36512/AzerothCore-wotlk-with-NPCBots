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
#include "GridNotifiers.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "hyjal.h"

#include <list>

enum Spells
{
    SPELL_MALEVOLENT_CLEAVE = 31436,
    SPELL_WAR_STOMP         = 31480,
    SPELL_CRIPPLE           = 31477,
    SPELL_MARK              = 31447,
    SPELL_MARK_DAMAGE       = 31463,
    SPELL_SUPER_MANA_POTION = 28499,
    SPELL_DARK_RUNE         = 27869,
    SPELL_DEMONIC_RUNE      = 16666,
    SPELL_DRUMS_RESTORATION = 35478,
    SPELL_TINNITUS          = 51120
};

enum Texts
{
    SAY_ONSLAY          = 0,
    SAY_MARK            = 1,
    SAY_ONSPAWN         = 2,
};

enum Sounds
{
    SOUND_ONDEATH       = 11018,
};

namespace
{
    bool IsValidKazrogalNPCBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI() && !creature->IsInEvadeMode();
    }

    bool IsKazrogalPlayerOrBot(Unit const* unit)
    {
        if (!unit)
            return false;

        if (unit->IsPlayer())
            return true;

        return IsValidKazrogalNPCBot(unit->ToCreature());
    }

    bool IsValidKazrogalEncounterTarget(Unit const* target, Creature const* source, float dist = 0.0f, bool withTank = true)
    {
        if (!target || !source || !target->IsInWorld() || !target->IsAlive() || !target->IsInCombat() || target->GetMap() != source->GetMap())
            return false;

        if (target->HasUnitState(UNIT_STATE_ISOLATED))
            return false;

        if (!withTank && target == source->GetThreatMgr().GetLastVictim())
            return false;

        if (dist > 0.0f && !source->IsWithinCombatRange(target, dist))
            return false;

        if (dist < 0.0f && source->IsWithinCombatRange(target, -dist))
            return false;

        if (!source->IsValidAttackTarget(target))
            return false;

        return IsKazrogalPlayerOrBot(target);
    }

    bool IsValidKazrogalMarkTarget(Unit const* target, Creature const* source)
    {
        return IsValidKazrogalEncounterTarget(target, source) && target->getPowerType() == POWER_MANA;
    }
}

struct boss_kazrogal : public BossAI
{
public:
    boss_kazrogal(Creature* creature) : BossAI(creature, DATA_KAZROGAL)
    {    }

    void Reset() override
    {
        _recentlySpoken = false;
        _markCounter = 0;
        BossAI::Reset();
    }

    void JustEngagedWith(Unit * who) override
    {
        BossAI::JustEngagedWith(who);

        scheduler.Schedule(6s, 21s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_MALEVOLENT_CLEAVE);
            context.Repeat();
        }).Schedule(12s, 18s, [this](TaskContext context)
        {
            if (SelectRandomPlayerOrBotTarget(0, 12.f))
            {
                DoCastAOE(SPELL_WAR_STOMP);
                context.Repeat(15s, 30s);
            }
            else
                context.Repeat(1200ms);
        }).Schedule(15s, [this](TaskContext context)
        {
            DoCastRandomPlayerOrBotTarget(SPELL_CRIPPLE, 0, 20.f);
            context.Repeat(12s, 20s);
        }).Schedule(45s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_MARK);
            Talk(SAY_MARK);
            context.Repeat(GetMarkRepeatTimer());
        });
    }

    Milliseconds GetMarkRepeatTimer()
    {
        ++_markCounter;
        Milliseconds timer = 45s - (5s * _markCounter);
        if (timer <= 10s)
            return 10s;
        else
            return timer;
    }

    void DoAction(int32 action) override
    {
        Talk(SAY_ONSPAWN, 1200ms);

        if (action == DATA_KAZROGAL)
            me->GetMotionMaster()->MoveWaypoint(HORDE_BOSS_PATH, false);
    }

    void KilledUnit(Unit * victim) override
    {
        if (!_recentlySpoken && IsKazrogalPlayerOrBot(victim) && me->IsAlive())
        {
            Talk(SAY_ONSLAY);
            _recentlySpoken = true;

            scheduler.Schedule(6s, [this](TaskContext)
                {
                    _recentlySpoken = false;
                });
        }
    }

    void JustDied(Unit * killer) override
    {
        me->PlayDirectSound(SOUND_ONDEATH);
        BossAI::JustDied(killer);
    }

private:
    Unit* SelectRandomPlayerOrBotTarget(uint32 threatTablePosition = 0, float dist = 0.0f, bool withTank = true)
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, threatTablePosition, [this, dist, withTank](Unit* target)
        {
            return IsValidKazrogalEncounterTarget(target, me, dist, withTank);
        });

        return targets.empty() ? nullptr : targets.front();
    }

    SpellCastResult DoCastRandomPlayerOrBotTarget(uint32 spellId, uint32 threatTablePosition = 0, float dist = 0.0f, bool triggered = false, bool withTank = true)
    {
        if (Unit* target = SelectRandomPlayerOrBotTarget(threatTablePosition, dist, withTank))
            return DoCast(target, spellId, triggered);

        SpellCastResult result = DoCastRandomTarget(spellId, threatTablePosition, dist, true, triggered, withTank);
        if (result != SPELL_FAILED_BAD_TARGETS)
            return result;

        if (Unit* victim = me->GetVictim())
            return DoCast(victim, spellId, triggered);

        return result;
    }

    bool _recentlySpoken;
    uint8 _markCounter;
};

class spell_mark_of_kazrogal : public SpellScript
{
    PrepareSpellScript(spell_mark_of_kazrogal);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;

        targets.remove_if([caster](WorldObject* object)
        {
            Unit* target = object ? object->ToUnit() : nullptr;
            return !IsValidKazrogalMarkTarget(target, caster);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_mark_of_kazrogal::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_mark_of_kazrogal_aura : public AuraScript
{
    PrepareAuraScript(spell_mark_of_kazrogal_aura);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_MARK_DAMAGE });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        Creature* bot = target ? target->ToCreature() : nullptr;

        if (!IsValidKazrogalNPCBot(bot) || !roll_chance_i(50))
            return;

        uint32 spellIds[4] =
        {
            SPELL_SUPER_MANA_POTION,
            SPELL_DARK_RUNE,
            SPELL_DEMONIC_RUNE,
            SPELL_DRUMS_RESTORATION
        };
        uint32 spellCount = target->HasAura(SPELL_TINNITUS) ? 3 : 4;

        target->CastSpell(target, spellIds[urand(0, spellCount - 1)], true);
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        if ((int32)target->GetPower(POWER_MANA) < aurEff->GetBaseAmount())
        {
            target->CastSpell(target, SPELL_MARK_DAMAGE, true, nullptr, aurEff);
            // Remove aura
            SetDuration(0);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_mark_of_kazrogal_aura::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_MANA_LEECH, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_mark_of_kazrogal_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_MANA_LEECH);
    }
};

void AddSC_boss_kazrogal()
{
    RegisterHyjalAI(boss_kazrogal);
    RegisterSpellAndAuraScriptPair(spell_mark_of_kazrogal, spell_mark_of_kazrogal_aura);
}
