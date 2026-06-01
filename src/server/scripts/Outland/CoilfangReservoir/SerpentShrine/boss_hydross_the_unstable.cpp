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

#include "Config.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "Group.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScriptLoader.h"
#include "serpent_shrine.h"
#include "SpellScript.h"
#include "ThreatManager.h"
#include "bot_ai.h"
#include "botmgr.h"

#include <vector>

enum Talk
{
    SAY_AGGRO                   = 0,
    SAY_SWITCH_TO_CLEAN         = 1,
    SAY_CLEAN_SLAY              = 2,
    SAY_CLEAN_DEATH             = 3,
    SAY_SWITCH_TO_CORRUPT       = 4,
    SAY_CORRUPT_SLAY            = 5,
    SAY_CORRUPT_DEATH           = 6
};

enum Spells
{
    SPELL_CLEANSING_FIELD_AURA  = 37935,
    SPELL_CLEANSING_FIELD       = 37934,
    SPELL_BLUE_BEAM             = 38015,
    SPELL_ELEMENTAL_SPAWNIN     = 25035,
    SPELL_PURIFY_ELEMENTAL      = 36461,
    SPELL_SUMMON_ELEMENTAL      = 36459,

    SPELL_SUMMON_CORRUPTED1     = 38188,
    SPELL_SUMMON_CORRUPTED2     = 38189,
    SPELL_SUMMON_CORRUPTED3     = 38190,
    SPELL_SUMMON_CORRUPTED4     = 38191,
    SPELL_SUMMON_PURIFIED1      = 38198,
    SPELL_SUMMON_PURIFIED2      = 38199,
    SPELL_SUMMON_PURIFIED3      = 38200,
    SPELL_SUMMON_PURIFIED4      = 38201,

    SPELL_CORRUPTION            = 37961,
    SPELL_WATER_TOMB            = 38235,
    SPELL_VILE_SLUDGE           = 38246,
    SPELL_ENRAGE                = 27680,

    SPELL_MARK_OF_HYDROSS1      = 38215,
    SPELL_MARK_OF_HYDROSS2      = 38216,
    SPELL_MARK_OF_HYDROSS3      = 38217,
    SPELL_MARK_OF_HYDROSS4      = 38218,
    SPELL_MARK_OF_HYDROSS5      = 38231,
    SPELL_MARK_OF_HYDROSS6      = 40584,
    SPELL_MARK_OF_CORRUPTION1   = 38219,
    SPELL_MARK_OF_CORRUPTION2   = 38220,
    SPELL_MARK_OF_CORRUPTION3   = 38221,
    SPELL_MARK_OF_CORRUPTION4   = 38222,
    SPELL_MARK_OF_CORRUPTION5   = 38230,
    SPELL_MARK_OF_CORRUPTION6   = 40583,
};

enum Misc
{
    GROUP_ABILITIES                 = 1,
    GROUP_OOC_PURIFY_ELEMENTALS     = 2,

    NPC_PURIFIED_WATER_ELEMENTAL    = 21260,
    NPC_PURE_SPAWN_OF_HYDROSS       = 22035,
    NPC_TAINTED_HYDROSS_ELEMENTAL   = 21253
};

enum WaterElementalPathIds
{
    PATH_CENTER                     = 6,
    PATH_END                        = 13
};

namespace
{
    constexpr char const* CONFIG_WATER_TOMB_DIRECT_TARGET_ONLY = "SerpentShrine.Hydross.WaterTomb.DirectTargetOnly";
    constexpr char const* CONFIG_WATER_TOMB_DAMAGE_MULTIPLIER = "SerpentShrine.Hydross.WaterTomb.DamageMultiplier";

    bool IsWaterTombDirectTargetOnlyEnabled()
    {
        return sConfigMgr->GetOption<bool>(CONFIG_WATER_TOMB_DIRECT_TARGET_ONLY, true);
    }

    float GetWaterTombDamageMultiplier()
    {
        float multiplier = sConfigMgr->GetOption<float>(CONFIG_WATER_TOMB_DAMAGE_MULTIPLIER, 0.5f);
        if (multiplier < 0.0f)
            return 0.0f;

        if (multiplier > 1.0f)
            return 1.0f;

        return multiplier;
    }

    bool IsHydrossNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsHydrossPlayerMarkedTank(Player* player)
    {
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (Group::member_citerator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & MEMBER_FLAG_MAINTANK;

        return false;
    }

    bool IsHydrossTank(Unit* unit, Creature* source)
    {
        if (!unit)
            return false;

        if (source && (unit == source->GetThreatMgr().GetCurrentVictim() || unit == source->GetThreatMgr().GetLastVictim() || unit == source->GetVictim()))
            return true;

        if (IsHydrossNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsHydrossPlayerMarkedTank(player);

        return false;
    }

    bool IsHydrossValidRandomTarget(Creature* source, Unit* unit, float range)
    {
        if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive())
            return false;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return false;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        if (IsHydrossTank(unit, source))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (range > 0.0f && !source->IsWithinDistInMap(unit, range))
            return false;

        if (!source->IsValidAttackTarget(unit) || !source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
            return false;

        return true;
    }

    void AddHydrossRandomTarget(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, float range)
    {
        if (!IsHydrossValidRandomTarget(source, unit, range))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    Player* GetHydrossOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetHydrossGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    void AddHydrossOwnedBots(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player, float range)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddHydrossRandomTarget(source, targets, seen, pair.second, range);
    }

    void AddHydrossThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, float range)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddHydrossRandomTarget(source, targets, seen, ref->GetVictim(), range);
        }
    }

    float GetHydrossSpellRange(uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        return spellInfo ? spellInfo->GetMaxRange(false) : 0.0f;
    }

    std::vector<Unit*> GatherHydrossRandomTargets(Creature* source, uint32 spellId)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source)
            return targets;

        float const range = GetHydrossSpellRange(spellId);
        Unit* seed = source->GetThreatMgr().GetCurrentVictim();
        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetHydrossGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddHydrossRandomTarget(source, targets, seen, member, range);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddHydrossOwnedBots(source, targets, seen, player, range);
            }
        }
        else
        {
            AddHydrossRandomTarget(source, targets, seen, seed, range);

            if (Player* player = GetHydrossOwnerPlayer(seed))
            {
                AddHydrossRandomTarget(source, targets, seen, player, range);
                AddHydrossOwnedBots(source, targets, seen, player, range);
            }
        }

        AddHydrossThreatTargets(source, targets, seen, range);
        return targets;
    }

    SpellCastResult CastHydrossRandomNonTankTarget(Creature* source, uint32 spellId, bool triggered)
    {
        std::vector<Unit*> targets = GatherHydrossRandomTargets(source, spellId);
        if (targets.empty())
            return SPELL_FAILED_BAD_TARGETS;

        return source->CastSpell(Acore::Containers::SelectRandomContainerElement(targets), spellId, triggered);
    }
}

struct boss_hydross_the_unstable : public BossAI
{
    boss_hydross_the_unstable(Creature* creature) : BossAI(creature, DATA_HYDROSS_THE_UNSTABLE), _recentlySpoken(false) { }

    void Reset() override
    {
        _Reset();
        _recentlySpoken = false;
        SummonTaintedElementalOOC();
    }

    void SummonTaintedElementalOOC()
    {
        me->m_Events.AddEventAtOffset([this] {
            DoCastAOE(SPELL_SUMMON_ELEMENTAL);
            SummonTaintedElementalOOC();
        }, 12s, 12s, GROUP_OOC_PURIFY_ELEMENTALS);
    }

    void JustReachedHome() override
    {
        BossAI::JustReachedHome();
        if (!me->HasAura(SPELL_BLUE_BEAM))
        {
            me->RemoveAurasDueToSpell(SPELL_CLEANSING_FIELD_AURA);
        }
    }

    void SummonMovementInform(Creature* summon, uint32 movementType, uint32 pathId) override
    {
        if (movementType == WAYPOINT_MOTION_TYPE)
        {
            if (pathId == PATH_CENTER)
            {
                summon->SetFacingToObject(me);
                DoCast(summon, SPELL_PURIFY_ELEMENTAL);

                // Happens even if Hydross is dead, so completely detached to the spell, which is nothing but a dummy anyways.
                summon->m_Events.AddEventAtOffset([summon] {
                    summon->UpdateEntry(NPC_PURIFIED_WATER_ELEMENTAL);
                }, 1s);
            }
            else if (pathId == PATH_END)
            {
                summon->DespawnOrUnsummon();
            }
        }
    }

    void SetForm(bool corrupt, bool initial)
    {
        scheduler.CancelGroup(GROUP_ABILITIES);
        DoResetThreatList();

        if (corrupt)
        {
            me->SetMeleeDamageSchool(SPELL_SCHOOL_NATURE);
            me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FROST, false);
            me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_NATURE, true);
            DoCastSelf(SPELL_CORRUPTION, true);

            scheduler.Schedule(15s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_CORRUPTION1);
            }).Schedule(30s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_CORRUPTION2);
            }).Schedule(45s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_CORRUPTION3);
            }).Schedule(60s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_CORRUPTION4);
            }).Schedule(75s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_CORRUPTION5);
            }).Schedule(90s, GROUP_ABILITIES, [this](TaskContext context)
            {
                DoCastSelf(SPELL_MARK_OF_CORRUPTION6);
                context.Repeat(15s);
            }).Schedule(12150ms, GROUP_ABILITIES, [this](TaskContext context)
            {
                CastHydrossRandomNonTankTarget(me, SPELL_VILE_SLUDGE, true);
                context.Repeat(9700ms, 32800ms);
            });
        }
        else
        {
            me->SetMeleeDamageSchool(SPELL_SCHOOL_FROST);
            me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FROST, true);
            me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_NATURE, false);
            me->RemoveAurasDueToSpell(SPELL_CORRUPTION);

            scheduler.Schedule(15s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_HYDROSS1);
            }).Schedule(30s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_HYDROSS2);
            }).Schedule(45s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_HYDROSS3);
            }).Schedule(60s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_HYDROSS4);
            }).Schedule(75s, GROUP_ABILITIES, [this](TaskContext)
            {
                DoCastSelf(SPELL_MARK_OF_HYDROSS5);
            }).Schedule(90s, GROUP_ABILITIES, [this](TaskContext context)
            {
                DoCastSelf(SPELL_MARK_OF_HYDROSS6);
                context.Repeat(15s);
            }).Schedule(12150ms, GROUP_ABILITIES, [this](TaskContext context)
            {
                CastHydrossRandomNonTankTarget(me, SPELL_WATER_TOMB, true);
                context.Repeat(9700ms, 32800ms);
            });
        }

        if (initial)
        {
            return;
        }

        if (corrupt)
        {
            Talk(SAY_SWITCH_TO_CORRUPT);
            for (uint32 spellId = SPELL_SUMMON_CORRUPTED1; spellId <= SPELL_SUMMON_CORRUPTED4; ++spellId)
            {
                DoCastSelf(spellId, true);
            }
        }
        else
        {
            Talk(SAY_SWITCH_TO_CLEAN);
            for (uint32 spellId = SPELL_SUMMON_PURIFIED1; spellId <= SPELL_SUMMON_PURIFIED4; ++spellId)
            {
                DoCastSelf(spellId, true);
            }
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        SetForm(false, true);
        me->m_Events.CancelEventGroup(GROUP_OOC_PURIFY_ELEMENTALS);

        scheduler.Schedule(1s, [this](TaskContext context)
        {
            if (me->HasAura(SPELL_BLUE_BEAM) == me->HasAura(SPELL_CORRUPTION))
            {
                SetForm(!me->HasAura(SPELL_BLUE_BEAM), false);
            }
            context.Repeat(1s);
        }).Schedule(10min, [this](TaskContext)
        {
            DoCastSelf(SPELL_ENRAGE, true);
        });
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (!_recentlySpoken)
        {
            Talk(me->HasAura(SPELL_CORRUPTION) ? SAY_CORRUPT_SLAY : SAY_CLEAN_SLAY);
            _recentlySpoken = true;
        }
        scheduler.Schedule(6s, [this](TaskContext)
        {
            _recentlySpoken = false;
        });
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);

        summon->CastSpell(summon, SPELL_ELEMENTAL_SPAWNIN, true);

        if (summon->GetEntry() == NPC_PURE_SPAWN_OF_HYDROSS)
        {
            summon->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FROST, true);
        }
        else if (summon->GetEntry() == NPC_TAINTED_HYDROSS_ELEMENTAL)
        {
            summon->setActive(true);
            summon->GetMotionMaster()->MoveWaypoint(summon->GetEntry() * 10, false);
        }
        else
        {
            summon->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_NATURE, true);
        }
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        summons.Despawn(summon);
    }

    void JustDied(Unit* killer) override
    {
        Talk(me->HasAura(SPELL_CORRUPTION) ? SAY_CORRUPT_DEATH : SAY_CLEAN_DEATH);
        BossAI::JustDied(killer);
    }
private:
    bool _recentlySpoken;
};

class spell_hydross_cleansing_field_aura : public AuraScript
{
    PrepareAuraScript(spell_hydross_cleansing_field_aura);

    void HandleEffectApply(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTarget()->GetEntry() == NPC_HYDROSS_THE_UNSTABLE)
            if (Unit* caster = GetCaster())
                caster->CastSpell(caster, SPELL_CLEANSING_FIELD, true);
    }

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTarget()->GetEntry() == NPC_HYDROSS_THE_UNSTABLE)
            if (Unit* caster = GetCaster())
                caster->CastSpell(caster, SPELL_CLEANSING_FIELD, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_hydross_cleansing_field_aura::HandleEffectApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_hydross_cleansing_field_aura::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_hydross_cleansing_field_command : public AuraScript
{
    PrepareAuraScript(spell_hydross_cleansing_field_command);

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTarget()->HasUnitState(UNIT_STATE_CASTING))
            GetTarget()->InterruptNonMeleeSpells(false);
        else
            GetTarget()->CastSpell(GetTarget(), SPELL_BLUE_BEAM, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectApplyFn(spell_hydross_cleansing_field_command::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_hydross_mark_of_hydross : public AuraScript
{
    PrepareAuraScript(spell_hydross_mark_of_hydross);

    void HandleEffectApply(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, GetCasterGUID(), GetAura());
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_hydross_mark_of_hydross::HandleEffectApply, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_hydross_water_tomb : public SpellScript
{
    PrepareSpellScript(spell_hydross_water_tomb);

    void KeepAuraOnDirectTargetOnly(SpellEffIndex /*effIndex*/)
    {
        if (!IsWaterTombDirectTargetOnlyEnabled())
            return;

        Unit* target = GetHitUnit();
        Unit* explicitTarget = GetExplTargetUnit();
        if (!target || !explicitTarget || target == explicitTarget)
            return;

        if (!target->IsPlayer() && !target->IsNPCBot())
            return;

        PreventHitAura();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hydross_water_tomb::KeepAuraOnDirectTargetOnly, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
        OnEffectHitTarget += SpellEffectFn(spell_hydross_water_tomb::KeepAuraOnDirectTargetOnly, EFFECT_1, SPELL_EFFECT_APPLY_AURA);
    }
};

class spell_hydross_water_tomb_aura : public AuraScript
{
    PrepareAuraScript(spell_hydross_water_tomb_aura);

    void CalculatePeriodicDamage(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount = int32(float(amount) * GetWaterTombDamageMultiplier());
        if (amount < 0)
            amount = 0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hydross_water_tomb_aura::CalculatePeriodicDamage, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

void AddSC_boss_hydross_the_unstable()
{
    RegisterSerpentShrineAI(boss_hydross_the_unstable);
    RegisterSpellScript(spell_hydross_cleansing_field_aura);
    RegisterSpellScript(spell_hydross_cleansing_field_command);
    RegisterSpellScript(spell_hydross_mark_of_hydross);
    RegisterSpellAndAuraScriptPair(spell_hydross_water_tomb, spell_hydross_water_tomb_aura);
}
