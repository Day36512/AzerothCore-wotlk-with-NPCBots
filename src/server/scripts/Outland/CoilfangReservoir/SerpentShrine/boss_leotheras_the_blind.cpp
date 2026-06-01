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
#include "Config.h"
#include "CreatureGroups.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include "serpent_shrine.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "ThreatManager.h"
#include "bot_ai.h"
#include "botmgr.h"

#include <vector>

enum Talk
{
    SAY_AGGRO                           = 0,
    SAY_SWITCH_TO_DEMON                 = 1,
    SAY_INNER_DEMONS                    = 2,
    SAY_DEMON_SLAY                      = 3,
    SAY_NIGHTELF_SLAY                   = 4,
    SAY_FINAL_FORM                      = 5,
    SAY_DEATH                           = 6
};

enum Spells
{
    SPELL_DUAL_WIELD                    = 42459,
    SPELL_BANISH                        = 37546,
    SPELL_TAUNT                         = 37548,
    SPELL_BERSERK                       = 26662,
    SPELL_WHIRLWIND                     = 37640,
    SPELL_SUMMON_SHADOW_OF_LEOTHERAS    = 37781,

    // Demon Form
    SPELL_CHAOS_BLAST                   = 37674,
    SPELL_CHAOS_BLAST_TRIGGER           = 37675,
    SPELL_INSIDIOUS_WHISPER             = 37676,
    SPELL_METAMORPHOSIS                 = 37673,
    SPELL_SUMMON_INNER_DEMON            = 37735,
    SPELL_CONSUMING_MADNESS             = 37749,

    // Other
    SPELL_CLEAR_CONSUMING_MADNESS       = 37750,
    SPELL_SHADOW_BOLT                   = 39309,
    SPELL_FULL_HEAL                     = 17683
};

enum Misc
{
    NPC_SHADOW_OF_LEOTHERAS             = 21875,
    NPC_GREYHEART_SPELLBINDER           = 21806,

    ACTION_CHECK_SPELLBINDERS           = 1
};

enum Groups
{
    GROUP_COMBAT                        = 1,
    GROUP_DEMON                         = 2
};

namespace
{
    constexpr char const* CONFIG_DEMON_PHASE_DELAY_MS = "SerpentShrine.Leotheras.DemonPhaseDelayMs";

    Milliseconds GetLeotherasDemonPhaseDelay()
    {
        return Milliseconds(sConfigMgr->GetOption<uint32>(CONFIG_DEMON_PHASE_DELAY_MS, 60350u));
    }

    bool IsLeotherasNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsLeotherasPlayerMarkedTank(Player* player)
    {
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (Group::member_citerator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & MEMBER_FLAG_MAINTANK;

        return false;
    }

    bool IsLeotherasTank(Unit* unit, Creature* source)
    {
        if (!unit)
            return false;

        if (source && (unit == source->GetThreatMgr().GetCurrentVictim() || unit == source->GetThreatMgr().GetLastVictim() || unit == source->GetVictim()))
            return true;

        if (IsLeotherasNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsLeotherasPlayerMarkedTank(player);

        return false;
    }

    float GetLeotherasSpellRange(uint32 spellId, float rangeOverride = 0.0f)
    {
        if (rangeOverride > 0.0f)
            return rangeOverride;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        return spellInfo ? spellInfo->GetMaxRange(false) : 0.0f;
    }

    bool IsLeotherasPlayerOrBot(Unit* unit)
    {
        return unit && unit->IsAlive() && (unit->IsPlayer() || unit->IsNPCBot());
    }

    bool IsValidLeotherasRandomTarget(Creature* source, Unit* unit, float range, bool excludeTanks)
    {
        if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive())
            return false;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return false;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        if (excludeTanks && IsLeotherasTank(unit, source))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (range > 0.0f && !source->IsWithinDistInMap(unit, range))
            return false;

        if (!source->IsValidAttackTarget(unit) || !source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
            return false;

        return true;
    }

    void AddLeotherasRandomTarget(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, float range, bool excludeTanks)
    {
        if (!IsValidLeotherasRandomTarget(source, unit, range, excludeTanks))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    Player* GetLeotherasOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetLeotherasGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    void AddLeotherasOwnedBots(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player, float range, bool excludeTanks)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddLeotherasRandomTarget(source, targets, seen, pair.second, range, excludeTanks);
    }

    void AddLeotherasThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, float range, bool excludeTanks)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddLeotherasRandomTarget(source, targets, seen, ref->GetVictim(), range, excludeTanks);
        }
    }

    std::vector<Unit*> GatherLeotherasRandomTargets(Creature* source, uint32 spellId, bool excludeTanks, float rangeOverride = 0.0f)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source)
            return targets;

        float const range = GetLeotherasSpellRange(spellId, rangeOverride);
        Unit* seed = source->GetThreatMgr().GetCurrentVictim();
        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetLeotherasGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddLeotherasRandomTarget(source, targets, seen, member, range, excludeTanks);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddLeotherasOwnedBots(source, targets, seen, player, range, excludeTanks);
            }
        }
        else
        {
            AddLeotherasRandomTarget(source, targets, seen, seed, range, excludeTanks);

            if (Player* player = GetLeotherasOwnerPlayer(seed))
            {
                AddLeotherasRandomTarget(source, targets, seen, player, range, excludeTanks);
                AddLeotherasOwnedBots(source, targets, seen, player, range, excludeTanks);
            }
        }

        AddLeotherasThreatTargets(source, targets, seen, range, excludeTanks);
        return targets;
    }

    Unit* SelectLeotherasRandomTarget(Creature* source, uint32 spellId, bool excludeTanks, float rangeOverride = 0.0f)
    {
        std::vector<Unit*> targets = GatherLeotherasRandomTargets(source, spellId, excludeTanks, rangeOverride);
        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }
}

struct boss_leotheras_the_blind : public BossAI
{
    boss_leotheras_the_blind(Creature* creature) : BossAI(creature, DATA_LEOTHERAS_THE_BLIND) { }

    void Reset() override
    {
        BossAI::Reset();
        DoCastSelf(SPELL_CLEAR_CONSUMING_MADNESS, true);
        DoCastSelf(SPELL_DUAL_WIELD, true);
        me->SetReactState(REACT_PASSIVE);
        _recentlySpoken = false;

        ScheduleHealthCheckEvent(15, [&]{
            me->RemoveAurasDueToSpell(SPELL_WHIRLWIND);

            if (me->GetDisplayId() != me->GetNativeDisplayId())
            {
                //is currently in metamorphosis
                me->LoadEquipment();
                me->RemoveAurasDueToSpell(SPELL_METAMORPHOSIS);
                scheduler.RescheduleGroup(GROUP_COMBAT, 10s);
            }

            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            me->ClearTarget();
            me->SendMeleeAttackStop();
            scheduler.CancelGroup(GROUP_DEMON);
            scheduler.DelayAll(10s);

            me->SetReactState(REACT_PASSIVE);
            me->SetStandState(UNIT_STAND_STATE_KNEEL);
            me->GetMotionMaster()->Clear();
            me->StopMoving();
            Talk(SAY_FINAL_FORM);

            scheduler.Schedule(4s, [this](TaskContext)
            {
                DoCastSelf(SPELL_SUMMON_SHADOW_OF_LEOTHERAS);
            }).Schedule(6s, [this](TaskContext)
            {
                DoResetThreatList();
                me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                me->SetReactState(REACT_AGGRESSIVE);
                me->ResumeChasingVictim();

                if (me->GetVictim())
                {
                    me->SetTarget(me->GetVictim()->GetGUID());
                    me->SendMeleeAttackStart(me->GetVictim());
                }
            });
        });
    }

    void AttackStart(Unit* who) override
    {
        if (me->HasAura(SPELL_METAMORPHOSIS))
            AttackStartCaster(who, 40.0f);
        else
            ScriptedAI::AttackStart(who);
    }

    void DoAction(int32 actionId) override
    {
        if (actionId == ACTION_CHECK_SPELLBINDERS)
        {
            if (CreatureGroup* formation = me->GetFormation())
            {
                if (!formation->IsAnyMemberAlive(true))
                {
                    me->RemoveAllAuras();
                    me->LoadEquipment();
                    me->SetReactState(REACT_AGGRESSIVE);
                    me->SetStandState(UNIT_STAND_STATE_STAND);
                    me->SetInCombatWithZone();
                    Talk(SAY_AGGRO);

                    scheduler.Schedule(10min, [this](TaskContext)
                    {
                        DoCastSelf(SPELL_BERSERK);
                    });

                    ElfTime();
                }
            }
        }
    }

    void ElfTime()
    {
        DoResetThreatList();
        me->InterruptNonMeleeSpells(false);
        scheduler.Schedule(25050ms, 32550ms, GROUP_COMBAT, [this](TaskContext context)
        {
            DoCastSelf(SPELL_WHIRLWIND);
            context.Repeat(30250ms, 34900ms);
        }).Schedule(GetLeotherasDemonPhaseDelay(), GROUP_DEMON, [this](TaskContext)
        {
            DoResetThreatList();
            Talk(SAY_SWITCH_TO_DEMON);
            DemonTime();
        });
    }

    void MoveToTargetIfOutOfRange(Unit* target)
    {
        if (!me->IsWithinDistInMap(target, 40.0f))
        {
            me->GetMotionMaster()->MoveChase(target, 40.0f, 0);
            me->AddThreat(target, 0.0f);
        }
        else
            me->GetMotionMaster()->Clear();
    }

    void DemonTime()
    {
        DoResetThreatList();
        me->RemoveAurasDueToSpell(SPELL_WHIRLWIND);
        me->InterruptNonMeleeSpells(false);
        me->LoadEquipment(0, true);
        DoCastSelf(SPELL_METAMORPHOSIS, true);

        scheduler.CancelGroup(GROUP_COMBAT);
        scheduler.Schedule(1s, GROUP_DEMON, [this](TaskContext context)
        {
            MoveToTargetIfOutOfRange(me->GetVictim());
            context.Repeat(1s);
        }).Schedule(24250ms, GROUP_DEMON, [this](TaskContext)
        {
            Talk(SAY_INNER_DEMONS);
            me->CastCustomSpell(SPELL_INSIDIOUS_WHISPER, SPELLVALUE_MAX_TARGETS, 5, me, false);
        }).Schedule(60s, [this](TaskContext)
        {
            DoResetThreatList();
            me->LoadEquipment();
            me->ResumeChasingVictim();
            me->RemoveAurasDueToSpell(SPELL_METAMORPHOSIS);
            scheduler.CancelGroup(GROUP_DEMON);
            ElfTime();
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (me->GetDisplayId() == me->GetNativeDisplayId())
        {
            if (me->GetReactState() != REACT_PASSIVE)
                DoMeleeAttackIfReady();
        }
        else if (me->isAttackReady(BASE_ATTACK))
        {
            if (DoCastVictim(SPELL_CHAOS_BLAST) != SPELL_CAST_OK)
            {
                // Auto-attacks if there are no valid targets to cast his spell on f.e pet taunted.
                DoMeleeAttackIfReady();
            }
            else
                me->setAttackTimer(BASE_ATTACK, 2000);
        }
    }
private:
    bool _recentlySpoken;
};

struct npc_inner_demon : public ScriptedAI
{
    npc_inner_demon(Creature* creature) : ScriptedAI(creature)
    { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (!summoner)
            return;

        me->SetMaxHealth(me->GetMaxHealth() * 5);
        me->SetHealth(me->GetMaxHealth());
        DoCastSelf(SPELL_FULL_HEAL, true);

        if (Unit* target = summoner->ToUnit())
        {
            _targetGuid = target->GetGUID();
            AttackStart(target);
            ForceTargetBotToFightDemon(target);
        }
        else
            _targetGuid = me->GetSummonerGUID();

        scheduler.CancelAll();
        scheduler.Schedule(1s, [this](TaskContext context)
        {
            TryFixateAssignedTarget();
            context.Repeat(2s);
        }).Schedule(4s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_SHADOW_BOLT);
            context.Repeat(6s);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Unit* target = GetAssignedTarget())
            target->RemoveAurasDueToSpell(SPELL_INSIDIOUS_WHISPER);

        RestoreTargetBotAfterDemon();
    }

    bool CanBeSeen(Player const* player) override
    {
        return IsAssignedTargetRaidMember(player);
    }

    bool CanReceiveDamage(Unit* attacker)
    {
        return IsAssignedTargetRaidMember(attacker);
    }

    void OnCalculateMeleeDamageReceived(uint32& damage, Unit* attacker) override
    {
        if (!CanReceiveDamage(attacker))
        {
            damage = 0;
        }
    }

    void OnCalculateSpellDamageReceived(int32& damage, Unit* attacker) override
    {
        if (!CanReceiveDamage(attacker))
        {
            damage = 0;
        }
    }

    void OnCalculatePeriodicTickReceived(uint32& damage, Unit* attacker) override
    {
        if (!CanReceiveDamage(attacker))
        {
            damage = 0;
        }
    }

    bool CanAIAttack(Unit const* who) const override
    {
        return who && who->GetGUID() == GetAssignedTargetGUID();
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
        {
            return;
        }

        DoMeleeAttackIfReady();
    }

private:
    ObjectGuid GetAssignedTargetGUID() const
    {
        return !_targetGuid.IsEmpty() ? _targetGuid : me->GetSummonerGUID();
    }

    Unit* GetAssignedTarget() const
    {
        return ObjectAccessor::GetUnit(*me, GetAssignedTargetGUID());
    }

    Group const* GetAssignedTargetGroup(Unit const* target) const
    {
        if (!target)
            return nullptr;

        if (Player const* player = target->ToPlayer())
            return player->GetGroup();

        Creature const* creature = target->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    Player const* GetAssignedTargetOwner(Unit const* target) const
    {
        if (!target)
            return nullptr;

        if (Player const* player = target->ToPlayer())
            return player;

        Creature const* creature = target->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    bool IsAssignedTargetRaidMember(Unit const* unit) const
    {
        if (!unit)
            return false;

        Unit const* target = GetAssignedTarget();
        if (!target)
            return false;

        if (unit->GetGUID() == target->GetGUID())
            return true;

        if (unit->GetMap() != target->GetMap())
            return false;

        if (Group const* group = GetAssignedTargetGroup(target))
            return group->IsMember(unit->GetGUID());

        Player const* targetOwner = GetAssignedTargetOwner(target);
        if (!targetOwner)
            return false;

        if (Player const* player = unit->ToPlayer())
            return player->GetGUID() == targetOwner->GetGUID();

        Creature const* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() && creature->GetBotOwner() == targetOwner;
    }

    Creature* GetLeotheras() const
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            return instance->GetCreature(DATA_LEOTHERAS_THE_BLIND);

        return nullptr;
    }

    void ForceTargetBotToFightDemon(Unit* target)
    {
        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_COMBATRESET | BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY);
        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        ai->AttackStart(me);
    }

    void RestoreTargetBotAfterDemon()
    {
        Creature* bot = GetAssignedTarget() ? GetAssignedTarget()->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_COMBATRESET | BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY);

        if (Creature* leotheras = GetLeotheras())
        {
            if (leotheras->IsAlive() && leotheras->IsEngaged() && bot->IsInMap(leotheras))
            {
                ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                ai->AttackStart(leotheras);
                return;
            }
        }

        if (!ai->IAmFree())
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
    }

    void TryFixateAssignedTarget()
    {
        Unit* target = GetAssignedTarget();
        if (!target || !target->IsAlive() || !target->HasAura(SPELL_INSIDIOUS_WHISPER))
        {
            me->DespawnOrUnsummon();
            return;
        }

        if (target->GetMap() != me->GetMap())
            return;

        me->GetThreatMgr().ResetAllThreat();
        me->GetThreatMgr().AddThreat(target, 1000000.0f);

        if (me->GetVictim() != target)
            AttackStart(target);

        ForceTargetBotToFightDemon(target);
    }

    ObjectGuid _targetGuid;
};

class spell_leotheras_whirlwind : public SpellScript
{
    PrepareSpellScript(spell_leotheras_whirlwind);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetCaster()->GetThreatMgr().ResetAllThreat();

        if (roll_chance_i(33))
            if (Unit* target = SelectLeotherasRandomTarget(GetCaster()->ToCreature(), SPELL_TAUNT, true, 30.0f))
                target->CastSpell(GetCaster(), SPELL_TAUNT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_leotheras_whirlwind::HandleScriptEffect, EFFECT_2, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_leotheras_chaos_blast : public SpellScript
{
    PrepareSpellScript(spell_leotheras_chaos_blast);

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_CHAOS_BLAST_TRIGGER, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_leotheras_chaos_blast::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_leotheras_insidious_whisper : public SpellScript
{
    PrepareSpellScript(spell_leotheras_insidious_whisper);

    void FilterTargets(std::list<WorldObject*>& unitList)
    {
        Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        unitList.remove_if([caster](WorldObject* target) -> bool
        {
            Unit* unit = target ? target->ToUnit() : nullptr;
            return !IsLeotherasPlayerOrBot(unit) || IsLeotherasTank(unit, caster);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_leotheras_insidious_whisper::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_leotheras_insidious_whisper_aura : public AuraScript
{
    PrepareAuraScript(spell_leotheras_insidious_whisper_aura);

    void HandleEffectApply(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_INNER_DEMON, true);
    }

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetUnitOwner();
        if (!target || !target->IsAlive())
            return;

        Unit* killer = target;
        if (InstanceScript* instance = target->GetInstanceScript())
        {
            if (Creature* leotheras = instance->GetCreature(DATA_LEOTHERAS_THE_BLIND))
                killer = leotheras;
        }

        Unit::Kill(killer, target);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_leotheras_insidious_whisper_aura::HandleEffectApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_leotheras_insidious_whisper_aura::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_leotheras_demon_link : public AuraScript
{
    PrepareAuraScript(spell_leotheras_demon_link);

    void OnPeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        if (Unit* victim = GetUnitOwner()->GetVictim())
        {
            GetUnitOwner()->CastSpell(victim, GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell, true);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_leotheras_demon_link::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_leotheras_clear_consuming_madness : public SpellScript
{
    PrepareSpellScript(spell_leotheras_clear_consuming_madness);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
        {
            Unit::Kill(GetCaster(), target);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_leotheras_clear_consuming_madness::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_boss_leotheras_the_blind()
{
    RegisterSerpentShrineAI(boss_leotheras_the_blind);
    RegisterSerpentShrineAI(npc_inner_demon);
    RegisterSpellScript(spell_leotheras_whirlwind);
    RegisterSpellScript(spell_leotheras_chaos_blast);
    RegisterSpellAndAuraScriptPair(spell_leotheras_insidious_whisper, spell_leotheras_insidious_whisper_aura);
    RegisterSpellScript(spell_leotheras_demon_link);
    RegisterSpellScript(spell_leotheras_clear_consuming_madness);
}
