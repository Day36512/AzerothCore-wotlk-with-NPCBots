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

#include "Containers.h"
#include "CreatureScript.h"
#include "Group.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScriptLoader.h"
#include "serpent_shrine.h"
#include "SpellScript.h"
#include "ThreatManager.h"
#include "bot_ai.h"
#include "botmgr.h"

enum Yells
{
    SAY_AGGRO                       = 0,
    SAY_SUMMON                      = 1,
    SAY_SUMMON_BUBBLE               = 2,
    SAY_SLAY                        = 3,
    SAY_DEATH                       = 4,
    EMOTE_WATERY_GRAVE              = 5,
    EMOTE_EARTHQUAKE                = 6,
    EMOTE_WATERY_GLOBULES           = 7
};

enum Spells
{
    SPELL_TIDAL_WAVE                = 37730,
    SPELL_WATERY_GRAVE              = 38028,
    SPELL_WATERY_GRAVE_1            = 38023,
    SPELL_WATERY_GRAVE_2            = 38024,
    SPELL_WATERY_GRAVE_3            = 38025,
    SPELL_WATERY_GRAVE_4            = 37850,
    SPELL_EARTHQUAKE                = 37764,
    SPELL_SUMMON_MURLOC1            = 39813,
    SPELL_SUMMON_WATER_GLOBULE_1    = 37854,
    SPELL_SUMMON_WATER_GLOBULE_2    = 37858,
    SPELL_SUMMON_WATER_GLOBULE_3    = 37860,
    SPELL_SUMMON_WATER_GLOBULE_4    = 37861
};

const uint32 wateryGraveIds[4] = {SPELL_WATERY_GRAVE_1, SPELL_WATERY_GRAVE_2, SPELL_WATERY_GRAVE_3, SPELL_WATERY_GRAVE_4};
const uint32 waterGlobuleIds[4] = {SPELL_SUMMON_WATER_GLOBULE_1, SPELL_SUMMON_WATER_GLOBULE_2, SPELL_SUMMON_WATER_GLOBULE_3, SPELL_SUMMON_WATER_GLOBULE_4};

namespace
{
    constexpr uint32 NPC_TIDEWALKER_LURKER = 21920;
    constexpr float MOROGRIM_MURLOC_TANK_THREAT_NUDGE = 5000.0f;

    bool IsMorogrimNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsMorogrimPlayerMarkedTank(Player* player)
    {
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (Group::member_citerator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & MEMBER_FLAG_MAINTANK;

        return false;
    }

    bool IsMorogrimTank(Unit* unit, Creature* source)
    {
        if (!unit)
            return false;

        if (source && (unit == source->GetThreatMgr().GetCurrentVictim() || unit == source->GetThreatMgr().GetLastVictim() || unit == source->GetVictim()))
            return true;

        if (IsMorogrimNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsMorogrimPlayerMarkedTank(player);

        return false;
    }

    bool IsMorogrimPlayerOrBot(Unit* unit)
    {
        return unit && unit->IsAlive() && (unit->IsPlayer() || unit->IsNPCBot());
    }

    Group* GetMorogrimGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    bool TryAddMorogrimMurlocTankThreat(Creature* boss, Creature* summon, Unit* target, GuidSet& seen)
    {
        if (!target || !target->IsAlive() || !target->IsInMap(summon) || !IsMorogrimTank(target, boss))
            return false;

        if (!seen.insert(target->GetGUID()).second)
            return false;

        summon->AddThreat(target, MOROGRIM_MURLOC_TANK_THREAT_NUDGE);
        return true;
    }

    void AddMorogrimMurlocTankThreat(Creature* boss, Creature* summon)
    {
        if (!boss || !summon || summon->GetEntry() != NPC_TIDEWALKER_LURKER)
            return;

        GuidSet seen;
        bool addedThreat = false;
        Unit* seed = boss->GetThreatMgr().GetCurrentVictim();
        if (!seed)
            seed = boss->GetVictim();

        if (Group* group = GetMorogrimGroup(seed))
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
                addedThreat |= TryAddMorogrimMurlocTankThreat(boss, summon, member, seen);

        for (ThreatReference const* ref : boss->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            addedThreat |= TryAddMorogrimMurlocTankThreat(boss, summon, ref->GetVictim(), seen);
        }

        if (!addedThreat)
            if (Unit* victim = boss->GetVictim())
                summon->AddThreat(victim, MOROGRIM_MURLOC_TANK_THREAT_NUDGE);
    }
}

struct boss_morogrim_tidewalker : public BossAI
{
    boss_morogrim_tidewalker(Creature* creature) : BossAI(creature, DATA_MOROGRIM_TIDEWALKER)
    {    }

    void Reset() override
    {
        BossAI::Reset();
        _recentlySpoken = false;
    }

    void KilledUnit(Unit*) override
    {
        if (!_recentlySpoken)
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;
        }

        scheduler.Schedule(6s, [this](TaskContext)
        {
            _recentlySpoken = false;
        });
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        summon->SetInCombatWithZone();
        AddMorogrimMurlocTankThreat(me, summon);
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);

        scheduler.Schedule(10s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_TIDAL_WAVE);
            context.Repeat(20s);
        }).Schedule(20s, [this](TaskContext context)
        {
            Talk(SAY_SUMMON_BUBBLE);
            if (me->HealthAbovePct(25))
            {
                Talk(EMOTE_WATERY_GRAVE);
                me->CastCustomSpell(SPELL_WATERY_GRAVE, SPELLVALUE_MAX_TARGETS, 4, me, false);
            }
            else
            {
                Talk(EMOTE_WATERY_GLOBULES);
                for (uint32 waterGlobuleId : waterGlobuleIds)
                {
                    DoCastSelf(waterGlobuleId, true);
                }
            }
            context.Repeat(25s);
        }).Schedule(40s, [this](TaskContext context)
        {
            Talk(EMOTE_EARTHQUAKE);

            DoCastSelf(SPELL_EARTHQUAKE);
            scheduler.Schedule(8s, [this](TaskContext)
            {
                Talk(SAY_SUMMON);
                for (uint32 murlocSpellId = SPELL_SUMMON_MURLOC1; murlocSpellId < SPELL_SUMMON_MURLOC1 + 11; ++murlocSpellId)
                {
                    DoCastSelf(murlocSpellId, true);
                }
            });
            context.Repeat(45s, 60s);
        });
    }
private:
    bool _recentlySpoken;
};

class spell_morogrim_tidewalker_watery_grave : public SpellScript
{
    PrepareSpellScript(spell_morogrim_tidewalker_watery_grave);

    bool Load() override
    {
        _targetNumber = 0;
        return true;
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        uint8 maxSize = 4;
        Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;

        targets.remove_if([caster](WorldObject* target) -> bool
            {
                Unit* unit = target ? target->ToUnit() : nullptr;
                return !IsMorogrimPlayerOrBot(unit) || IsMorogrimTank(unit, caster);
            });

        if (targets.size() > maxSize)
        {
            Acore::Containers::RandomResize(targets, maxSize);
        }
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            if (_targetNumber < 4)
                GetCaster()->CastSpell(target, wateryGraveIds[_targetNumber++], true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_morogrim_tidewalker_watery_grave::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_morogrim_tidewalker_watery_grave::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }

private:
    uint8 _targetNumber;
};

class spell_morogrim_tidewalker_water_globule_new_target : public SpellScript
{
    PrepareSpellScript(spell_morogrim_tidewalker_water_globule_new_target);

    void FilterTargets(std::list<WorldObject*>& unitList)
    {
        unitList.remove_if([](WorldObject* target) -> bool
        {
            return !IsMorogrimPlayerOrBot(target ? target->ToUnit() : nullptr);
        });

        Acore::Containers::RandomResize(unitList, 1);
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        // Xinef: if we have target we currently follow, return
        if (Unit* target = GetCaster()->GetVictim())
            if (GetCaster()->GetThreatMgr().GetThreat(target) >= 100000.0f)
                return;

        // Xinef: acquire new target
        // TODO: sniffs to see how this actually happens
        if (Unit* target = GetHitUnit())
            GetCaster()->AddThreat(target, 1000000.0f);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_morogrim_tidewalker_water_globule_new_target::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_morogrim_tidewalker_water_globule_new_target::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_morogrim_tidewalker()
{
    RegisterSerpentShrineAI(boss_morogrim_tidewalker);
    RegisterSpellScript(spell_morogrim_tidewalker_watery_grave);
    RegisterSpellScript(spell_morogrim_tidewalker_water_globule_new_target);
}
