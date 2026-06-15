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
    void AnnounceMoveAwayFromMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Spells
{
    SPELL_CARRION_SWARM       = 31306,
    SPELL_SLEEP               = 31298,
    SPELL_INFERNO             = 31299,
    SPELL_VAMPIRIC_AURA       = 31317,
    SPELL_VAMPIRIC_AURA_HEAL  = 31285,
    SPELL_ENRAGE              = 26662,
    SPELL_INFERNAL_STUN       = 31302,
    SPELL_INFERNAL_IMMOLATION = 31304
};

enum Texts
{
    SAY_ONDEATH         = 0,
    SAY_ONSLAY          = 1,
    SAY_SWARM           = 2,
    SAY_SLEEP           = 3,
    SAY_INFERNO         = 4,
    SAY_ONSPAWN         = 5,
};

namespace
{
    char const* const DBM_HYJAL_MODULE_FOLDER = "DBM-Hyjal";
    char const* const DBM_ANETHERON_MODULE_ID = "Anetheron";

    bool IsValidAnetheronNPCBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI() && !creature->IsInEvadeMode();
    }

    bool IsAnetheronPlayerOrBot(Unit const* unit)
    {
        if (!unit)
            return false;

        if (unit->IsPlayer())
            return true;

        return IsValidAnetheronNPCBot(unit->ToCreature());
    }

    bool IsValidAnetheronRandomTarget(Unit const* target, Creature const* source, float dist = 0.0f, bool withTank = true)
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

        return IsAnetheronPlayerOrBot(target);
    }
}

struct boss_anetheron : public BossAI
{
public:
    boss_anetheron(Creature* creature) : BossAI(creature, DATA_ANETHERON)
    {
        _recentlySpoken = false;
    }

    void JustEngagedWith(Unit * who) override
    {
        BossAI::JustEngagedWith(who);

        scheduler.Schedule(20s, 28s, [this](TaskContext context)
        {
            if (DoCastRandomPlayerOrBotTarget(SPELL_CARRION_SWARM, 0, 60.f) == SPELL_CAST_OK)
                Talk(SAY_SWARM);
            context.Repeat(10s, 15s);
        }).Schedule(25s, 32s, [this](TaskContext context)
        {
            Talk(SAY_SLEEP);
            DoCastRandomPlayerOrBotTarget(SPELL_SLEEP, 0, 0.0f, false, false);
            context.Repeat(35s, 48s);
        }).Schedule(30s, 48s, [this](TaskContext context)
        {
            Unit* target = nullptr;
            if (DoCastRandomPlayerOrBotTarget(SPELL_INFERNO, 0, 0.0f, false, true, &target) == SPELL_CAST_OK)
            {
                Talk(SAY_INFERNO);
                AnnounceInfernoTarget(target);
            }

            context.Repeat(50s, 55s);
        }).Schedule(10min, [this](TaskContext context)
            {
                DoCastSelf(SPELL_ENRAGE);
                context.Repeat(5min);
            });
    }

    void JustSummoned(Creature* summon) override
    {
        if (summon)
        {
            summon->AI()->DoCast(SPELL_INFERNAL_STUN);
            summon->SetInCombatWithZone();
        }
        BossAI::JustSummoned(summon);
    }

    void DoAction(int32 action) override
    {
        Talk(SAY_ONSPAWN, 1200ms);

        if (action == DATA_ANETHERON)
            me->GetMotionMaster()->MoveWaypoint(urand(ALLIANCE_BASE_CHARGE_1, ALLIANCE_BASE_CHARGE_3), false);
    }

    void PathEndReached(uint32 pathId) override
    {
        switch (pathId)
        {
        case ALLIANCE_BASE_CHARGE_1:
        case ALLIANCE_BASE_CHARGE_2:
        case ALLIANCE_BASE_CHARGE_3:
            me->m_Events.AddEventAtOffset([this]()
                {
                    me->GetMotionMaster()->MoveWaypoint(urand(ALLIANCE_BASE_PATROL_1, ALLIANCE_BASE_PATROL_3), true);
                }, 1s);
            break;
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (!_recentlySpoken && IsAnetheronPlayerOrBot(victim) && me->IsAlive())
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
        BossAI::JustDied(killer);
    }

private:
    Unit* SelectRandomPlayerOrBotTarget(uint32 threatTablePosition = 0, float dist = 0.0f, bool withTank = true)
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, threatTablePosition, [this, dist, withTank](Unit* target)
        {
            return IsValidAnetheronRandomTarget(target, me, dist, withTank);
        });

        return targets.empty() ? nullptr : targets.front();
    }

    SpellCastResult DoCastRandomPlayerOrBotTarget(uint32 spellId, uint32 threatTablePosition = 0, float dist = 0.0f, bool triggered = false, bool withTank = true, Unit** selectedTarget = nullptr)
    {
        if (selectedTarget)
            *selectedTarget = nullptr;

        if (Unit* target = SelectRandomPlayerOrBotTarget(threatTablePosition, dist, withTank))
        {
            SpellCastResult result = DoCast(target, spellId, triggered);
            if (result == SPELL_CAST_OK && selectedTarget)
                *selectedTarget = target;

            return result;
        }

        SpellCastResult result = DoCastRandomTarget(spellId, threatTablePosition, dist, true, triggered, withTank);
        if (result != SPELL_FAILED_BAD_TARGETS)
            return result;

        if (Unit* victim = me->GetVictim())
        {
            result = DoCast(victim, spellId, triggered);
            if (result == SPELL_CAST_OK && selectedTarget)
                *selectedTarget = victim;

            return result;
        }

        return result;
    }

    void AnnounceInfernoTarget(Unit* target)
    {
        if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target))
            DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(bot, SPELL_INFERNO, DBM_HYJAL_MODULE_FOLDER, DBM_ANETHERON_MODULE_ID, "Inferno", DBMFTABotCallouts::GetCooldownMs());
    }

    bool _recentlySpoken;
};

class spell_anetheron_sleep : public SpellScript
{
    PrepareSpellScript(spell_anetheron_sleep);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (!targets.empty())
        {
            Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;
            Unit* victim = GetCaster() ? GetCaster()->GetVictim() : nullptr;

            targets.remove_if([caster, victim](WorldObject* object)
            {
                Unit* target = object ? object->ToUnit() : nullptr;
                return !target || target == victim || !IsValidAnetheronRandomTarget(target, caster);
            });

            Acore::Containers::RandomResize(targets, 3);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_anetheron_sleep::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 31317 - Vampiric Aura
class spell_anetheron_vampiric_aura : public AuraScript
{
    PrepareAuraScript(spell_anetheron_vampiric_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VAMPIRIC_AURA_HEAL });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return;

        Unit* actor = eventInfo.GetActor();
        int32 bp = damageInfo->GetDamage() * 3;
        actor->CastCustomSpell(SPELL_VAMPIRIC_AURA_HEAL, SPELLVALUE_BASE_POINT0, bp, actor, true, nullptr, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_anetheron_vampiric_aura::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

void AddSC_boss_anetheron()
{
    RegisterHyjalAI(boss_anetheron);
    RegisterSpellScript(spell_anetheron_sleep);
    RegisterSpellScript(spell_anetheron_vampiric_aura);
}
