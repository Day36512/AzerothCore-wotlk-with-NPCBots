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
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "hyjal.h"

#include <list>
#include <string>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceDebuffOnMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Spells
{
    SPELL_RAIN_OF_FIRE          = 31340,
    SPELL_DOOM                  = 31347,
    SPELL_HOWL_OF_AZGALOR       = 31344,
    SPELL_CLEAVE                = 31345,
    SPELL_BERSERK               = 26662
};

enum Texts
{
    SAY_ONDEATH             = 0,
    SAY_ONSLAY              = 1,
    SAY_DOOM                = 2, // Not used?
    SAY_ONSPAWN             = 3,

    SAY_ARCHIMONDE_INTRO    = 8
};

namespace
{
    char const* const DBM_HYJAL_MODULE_FOLDER = "DBM-Hyjal";
    char const* const DBM_AZGALOR_MODULE_ID = "Azgalor";

    bool IsValidAzgalorNPCBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI() && !creature->IsInEvadeMode();
    }

    bool IsAzgalorPlayerOrBot(Unit const* unit)
    {
        if (!unit)
            return false;

        if (unit->IsPlayer())
            return true;

        return IsValidAzgalorNPCBot(unit->ToCreature());
    }

    bool IsValidAzgalorRandomTarget(Unit const* target, Creature const* source, float dist = 0.0f, bool withTank = true)
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

        return IsAzgalorPlayerOrBot(target);
    }
}

struct boss_azgalor : public BossAI
{
public:
    boss_azgalor(Creature* creature) : BossAI(creature, DATA_AZGALOR)
    {
        _recentlySpoken = false;
    }

    void JustEngagedWith(Unit * who) override
    {
        BossAI::JustEngagedWith(who);

        scheduler.Schedule(10s, 16s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_CLEAVE);
            context.Repeat(8s, 16s);
        }).Schedule(20s, 25s, [this](TaskContext context)
        {
            DoCastRandomPlayerOrBotTarget(SPELL_RAIN_OF_FIRE, 0, 40.f);
            context.Repeat(12s, 35s);
        }).Schedule(30s, [this](TaskContext context)
        {
            DoCastAOE(SPELL_HOWL_OF_AZGALOR);
            context.Repeat(18s, 20s);
        }).Schedule(45s, 55s, [this](TaskContext context)
        {
            DoCastAOE(SPELL_DOOM);
            Talk(SAY_DOOM);
            context.Repeat();
        }).Schedule(10min, [this](TaskContext context)
        {
            DoCastSelf(SPELL_BERSERK);
            context.Repeat(5min);
        });
    }

    void DoAction(int32 action) override
    {
        Talk(SAY_ONSPAWN, 1200ms);

        if (action == DATA_AZGALOR)
            me->GetMotionMaster()->MoveWaypoint(HORDE_BOSS_PATH, false);
    }

    void KilledUnit(Unit * victim) override
    {
        if (!_recentlySpoken && IsAzgalorPlayerOrBot(victim) && me->IsAlive())
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
        Talk(SAY_ONDEATH);
        // If Archimonde has not yet been initialized, this won't trigger
        if (Creature* archi = instance->GetCreature(DATA_ARCHIMONDE))
        {
            archi->AI()->DoAction(ACTION_BECOME_ACTIVE_AND_CHANNEL);
            archi->AI()->Talk(SAY_ARCHIMONDE_INTRO, 25s);
        }
        BossAI::JustDied(killer);
    }

private:
    Unit* SelectRandomPlayerOrBotTarget(uint32 threatTablePosition = 0, float dist = 0.0f, bool withTank = true)
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, threatTablePosition, [this, dist, withTank](Unit* target)
        {
            return IsValidAzgalorRandomTarget(target, me, dist, withTank);
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
};

class spell_azgalor_doom : public SpellScript
{
    PrepareSpellScript(spell_azgalor_doom);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        Unit* victim = GetCaster() ? GetCaster()->GetVictim() : nullptr;

        targets.remove_if([caster, victim](WorldObject* object)
        {
            Unit* target = object ? object->ToUnit() : nullptr;
            return !target || target == victim || !IsValidAzgalorRandomTarget(target, caster);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_azgalor_doom::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_azgalor_doom_aura : public AuraScript
{
    PrepareAuraScript(spell_azgalor_doom_aura);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(GetTarget()))
            DBMFTABotCallouts::AnnounceDebuffOnMeForModule(bot, SPELL_DOOM, DBM_HYJAL_MODULE_FOLDER, DBM_AZGALOR_MODULE_ID, "Doom", DBMFTABotCallouts::GetCooldownMs());
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEATH && !IsExpired())
        {
            target->CastSpell(target, GetSpellInfo()->Effects[EFFECT_0].TriggerSpell, true);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_azgalor_doom_aura::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_azgalor_doom_aura::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_azgalor()
{
    RegisterHyjalAI(boss_azgalor);
    RegisterSpellAndAuraScriptPair(spell_azgalor_doom, spell_azgalor_doom_aura);
}
