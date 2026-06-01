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
    SAY_AGGRO                       = 0,
    SAY_GAIN_BLESSING               = 1,
    SAY_GAIN_ABILITY1               = 2,
    SAY_GAIN_ABILITY2               = 3,
    SAY_GAIN_ABILITY3               = 4,
    SAY_SLAY                        = 5,
    SAY_DEATH                       = 6
};

enum Spells
{
    //Fathomlord Karathress
    SPELL_CATACLYSMIC_BOLT          = 38441,
    SPELL_SEAR_NOVA                 = 38445,
    SPELL_ENRAGE                    = 24318,
    SPELL_BLESSING_OF_THE_TIDES     = 38449,
    //Fathomguard Sharkkis
    SPELL_HURL_TRIDENT              = 38374,
    SPELL_LEECHING_THROW            = 29436,
    SPELL_MULTI_TOSS                = 38366,
    SPELL_SUMMON_FATHOM_SPOREBAT    = 38431,
    SPELL_SUMMON_FATHOM_LURKER      = 38433,
    SPELL_THE_BEAST_WITHIN          = 38373,
    SPELL_BESTIAL_WRATH             = 38371,
    SPELL_POWER_OF_SHARKKIS         = 38455,
    //Fathomguard Tidalvess
    SPELL_FROST_SHOCK               = 38234,
    SPELL_EARTHBIND_TOTEM           = 38304,
    SPELL_POISON_CLEANSING_TOTEM    = 38306,
    SPELL_SPITFIRE_TOTEM            = 38236,
    SPELL_POWER_OF_TIDALVESS        = 38452,
    //Fathomguard Caribdis
    SPELL_SUMMON_CYCLONE            = 38337,
    SPELL_WATER_BOLT_VOLLEY         = 38335,
    SPELL_TIDAL_SURGE               = 38358,
    SPELL_HEALING_WAVE              = 38330,
    SPELL_POWER_OF_CARIBDIS         = 38451,
    //Spitfire Totem
    SPELL_ATTACK                    = 38296
};

enum Misc
{
    GO_CAGE                         = 185474,
    ACTION_RESET_ADDS               = 1
};

const Position olumWalk = { 456.17194f, -544.31866f, -7.5470476f, 0.00f };

namespace
{
    bool IsKarathressNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsKarathressPlayerMarkedTank(Player* player)
    {
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (Group::member_citerator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & MEMBER_FLAG_MAINTANK;

        return false;
    }

    bool IsKarathressTank(Unit* unit, Creature* source)
    {
        if (!unit)
            return false;

        if (source && (unit == source->GetThreatMgr().GetCurrentVictim() || unit == source->GetThreatMgr().GetLastVictim() || unit == source->GetVictim()))
            return true;

        if (IsKarathressNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsKarathressPlayerMarkedTank(player);

        return false;
    }

    float GetKarathressSpellRange(uint32 spellId, float rangeOverride = 0.0f)
    {
        if (rangeOverride > 0.0f)
            return rangeOverride;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        return spellInfo ? spellInfo->GetMaxRange(false) : 0.0f;
    }

    bool IsKarathressAnyTarget(Unit*)
    {
        return true;
    }

    template <typename Predicate>
    bool IsValidKarathressRandomTarget(Creature* source, Unit* unit, float range, bool excludeTanks, Predicate const& predicate)
    {
        if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive())
            return false;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return false;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        if (excludeTanks && IsKarathressTank(unit, source))
            return false;

        if (!predicate(unit))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (range > 0.0f && !source->IsWithinDistInMap(unit, range))
            return false;

        if (!source->IsValidAttackTarget(unit) || !source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
            return false;

        return true;
    }

    template <typename Predicate>
    void AddKarathressRandomTarget(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, float range, bool excludeTanks, Predicate const& predicate)
    {
        if (!IsValidKarathressRandomTarget(source, unit, range, excludeTanks, predicate))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    Player* GetKarathressOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetKarathressGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    template <typename Predicate>
    void AddKarathressOwnedBots(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player, float range, bool excludeTanks, Predicate const& predicate)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddKarathressRandomTarget(source, targets, seen, pair.second, range, excludeTanks, predicate);
    }

    template <typename Predicate>
    void AddKarathressThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, float range, bool excludeTanks, Predicate const& predicate)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddKarathressRandomTarget(source, targets, seen, ref->GetVictim(), range, excludeTanks, predicate);
        }
    }

    template <typename Predicate>
    std::vector<Unit*> GatherKarathressRandomTargets(Creature* source, uint32 spellId, bool excludeTanks, float rangeOverride, Predicate const& predicate)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source)
            return targets;

        float const range = GetKarathressSpellRange(spellId, rangeOverride);
        Unit* seed = source->GetThreatMgr().GetCurrentVictim();
        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetKarathressGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddKarathressRandomTarget(source, targets, seen, member, range, excludeTanks, predicate);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddKarathressOwnedBots(source, targets, seen, player, range, excludeTanks, predicate);
            }
        }
        else
        {
            AddKarathressRandomTarget(source, targets, seen, seed, range, excludeTanks, predicate);

            if (Player* player = GetKarathressOwnerPlayer(seed))
            {
                AddKarathressRandomTarget(source, targets, seen, player, range, excludeTanks, predicate);
                AddKarathressOwnedBots(source, targets, seen, player, range, excludeTanks, predicate);
            }
        }

        AddKarathressThreatTargets(source, targets, seen, range, excludeTanks, predicate);
        return targets;
    }

    template <typename Predicate>
    Unit* SelectKarathressRandomTarget(Creature* source, uint32 spellId, bool excludeTanks, float rangeOverride, Predicate const& predicate)
    {
        std::vector<Unit*> targets = GatherKarathressRandomTargets(source, spellId, excludeTanks, rangeOverride, predicate);
        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }

    Unit* SelectKarathressRandomTarget(Creature* source, uint32 spellId, bool excludeTanks)
    {
        return SelectKarathressRandomTarget(source, spellId, excludeTanks, 0.0f, IsKarathressAnyTarget);
    }
}

struct boss_fathomlord_karathress : public BossAI
{
    boss_fathomlord_karathress(Creature* creature) : BossAI(creature, DATA_FATHOM_LORD_KARATHRESS){ }

    void JustReachedHome() override
    {
        instance->DoForAllMinions(DATA_FATHOM_LORD_KARATHRESS, [&](Creature* fathomguard)
        {
            if (!fathomguard->IsAlive())
            {
                fathomguard->Respawn(true);
            }
        });
    }

    void Reset() override
    {
        BossAI::Reset();
        _recentlySpoken = false;

        ScheduleHealthCheckEvent(75, [&]{
            instance->DoForAllMinions(DATA_FATHOM_LORD_KARATHRESS, [&](Creature* fathomguard) {
                if (fathomguard->IsAlive())
                {
                    fathomguard->CastSpell(me, SPELL_BLESSING_OF_THE_TIDES, true);
                }
            });
            if (me->HasAura(SPELL_BLESSING_OF_THE_TIDES))
            {
                Talk(SAY_GAIN_BLESSING);
            }
        });
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        summon->Attack(me->GetVictim(), false);
        summon->SetInCombatWithZone();
    }

    void KilledUnit(Unit* /*victim*/) override
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

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
        if (GameObject* cage = me->FindNearestGameObject(GO_CAGE, 100.0f))
        {
            cage->SetGoState(GO_STATE_ACTIVE);
        }
        if (Creature* olum = instance->GetCreature(DATA_SEER_OLUM))
        {
            olum->SetWalk(true);
            olum->GetMotionMaster()->MovePoint(0, olumWalk, FORCED_MOVEMENT_NONE, 0.f, false);
            olum->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            olum->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);

        instance->DoForAllMinions(DATA_FATHOM_LORD_KARATHRESS, [&](Creature* fathomguard) {
            fathomguard->SetInCombatWithZone();
        });

        scheduler.Schedule(10s, [this](TaskContext context)
        {
            if (Unit* target = SelectKarathressRandomTarget(me, SPELL_CATACLYSMIC_BOLT, true, 50.0f, [](Unit* unit) { return unit->getPowerType() == POWER_MANA; }))
            {
                me->CastSpell(target, SPELL_CATACLYSMIC_BOLT);
            }
            context.Repeat(6s);
        }).Schedule(25s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SEAR_NOVA);
            context.Repeat(20s, 40s);
        }).Schedule(10min, [this](TaskContext)
        {
            DoCastSelf(SPELL_ENRAGE, true);
        });
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_RESET_ADDS)
        {
            EnterEvadeMode();
            instance->DoForAllMinions(DATA_FATHOM_LORD_KARATHRESS, [&](Creature* fathomguard)
            {
                fathomguard->DespawnOrUnsummon();
            });
        }
    }
private:
    bool _recentlySpoken;
};

struct boss_fathomguard_sharkkis : public ScriptedAI
{
    boss_fathomguard_sharkkis(Creature* creature) : ScriptedAI(creature), _summons(creature)
    {
        _instance = creature->GetInstanceScript();
        SetBoundary(_instance->GetBossBoundary(DATA_FATHOM_LORD_KARATHRESS));
    }

    void Reset() override
    {
        scheduler.CancelAll();

        _summons.DespawnAll();
        _summons.clear();
    }

    void JustSummoned(Creature* summon) override
    {
        summon->SetInCombatWithZone();
        _summons.Summon(summon);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            karathress->Attack(who, false);
        }

        scheduler.Schedule(2500ms, [this](TaskContext context)
        {
            if (Unit* target = SelectKarathressRandomTarget(me, SPELL_HURL_TRIDENT, true))
                DoCast(target, SPELL_HURL_TRIDENT);

            context.Repeat(5s);
        }).Schedule(20650ms, [this](TaskContext context)
        {
            if (Unit* target = SelectKarathressRandomTarget(me, SPELL_MULTI_TOSS, true))
                DoCast(target, SPELL_MULTI_TOSS);

            context.Repeat(12150ms, 26350ms);
        }).Schedule(6050ms, [this](TaskContext context)
        {
            if (Unit* target = SelectKarathressRandomTarget(me, SPELL_LEECHING_THROW, true, 50.0f, [](Unit* unit) { return unit->getPowerType() == POWER_MANA; }))
            {
                me->CastSpell(target, SPELL_LEECHING_THROW);
            }
            context.Repeat(6050ms, 22250ms);
        }).Schedule(41250ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_THE_BEAST_WITHIN);
            _summons.DoForAllSummons([&](WorldObject* summon)
            {
                me->CastSpell(summon->ToCreature(), SPELL_BESTIAL_WRATH, true);
            });
            context.Repeat(39950ms, 46050ms);
        }).Schedule(14550ms, [this](TaskContext context)
        {
            DoCastSelf(urand(0, 1) ? SPELL_SUMMON_FATHOM_LURKER : SPELL_SUMMON_FATHOM_SPOREBAT);
            context.Repeat(30300ms);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            me->CastSpell(karathress, SPELL_POWER_OF_SHARKKIS, true);
            karathress->AI()->Talk(SAY_GAIN_ABILITY2);
            me->DespawnOrUnsummon(1s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            return;
        }

        scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            karathress->AI()->DoAction(ACTION_RESET_ADDS);
        }
        ScriptedAI::EnterEvadeMode(why);
    }

private:
    InstanceScript* _instance;
    SummonList _summons;
};

enum NPCTotems
{
    NPC_SPITFIRE_TOTEM                  = 22091,
    NPC_GREATER_EARTHBIND_TOTEM         = 22486,
    NPC_GREATER_POISON_CLEANSING_TOTEM  = 22487
};

enum TidalActions
{
    ACTION_REMOVE_SPITFIRE  = 1,
    ACTION_REMOVE_EARTHBIND = 2,
    ACTION_REMOVE_CLEANSING = 3
};

enum TotemChoice
{
    SPITFIRE  = 1,
    EARTHBIND = 2,
    CLEANSING = 3
};

struct boss_fathomguard_tidalvess : public ScriptedAI
{
    boss_fathomguard_tidalvess(Creature* creature) : ScriptedAI(creature), _summons(creature)
    {
        _instance = creature->GetInstanceScript();
        SetBoundary(_instance->GetBossBoundary(DATA_FATHOM_LORD_KARATHRESS));

        _scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    void Reset() override
    {
        _scheduler.CancelAll();
        _choice = 0;

        _summons.DespawnAll();

        _entryList.clear();

        _entryList = { NPC_SPITFIRE_TOTEM, NPC_GREATER_EARTHBIND_TOTEM, NPC_GREATER_POISON_CLEANSING_TOTEM };
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        summon->Attack(me->GetVictim(), false);
        summon->SetInCombatWithZone();
    }

    void ScheduleRemoval(uint32 entry)
    {
        std::chrono::seconds timer = 0s;
        int32 action = 0;
        uint8 group = 0;

        switch (entry)
        {
            case NPC_SPITFIRE_TOTEM:
                timer = 59s;
                action = ACTION_REMOVE_SPITFIRE;
                group = SPITFIRE;
                break;
            case NPC_GREATER_EARTHBIND_TOTEM:
                timer = 44s;
                action = ACTION_REMOVE_EARTHBIND;
                group = EARTHBIND;
                break;
            case NPC_GREATER_POISON_CLEANSING_TOTEM:
                timer = 29s;
                action = ACTION_REMOVE_CLEANSING;
                group = CLEANSING;
                break;
            default:
                timer = 29s;
        }
        _totemScheduler.Schedule(timer, group, [this, action](TaskContext)
        {
            me->AI()->DoAction(action);
        });
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_REMOVE_SPITFIRE:
                _totemScheduler.CancelGroup(SPITFIRE);
                _entryList.push_back(NPC_SPITFIRE_TOTEM);
                break;
            case ACTION_REMOVE_EARTHBIND:
                _totemScheduler.CancelGroup(EARTHBIND);
                _entryList.push_back(NPC_GREATER_EARTHBIND_TOTEM);
                break;
            case ACTION_REMOVE_CLEANSING:
                _totemScheduler.CancelGroup(CLEANSING);
                _entryList.push_back(NPC_GREATER_POISON_CLEANSING_TOTEM);
                break;
            default:
                return;
        }
    }

    void SummonTotem(uint32 entry)
    {
        switch (entry)
        {
            case NPC_SPITFIRE_TOTEM:
                DoCastSelf(SPELL_SPITFIRE_TOTEM);
                break;
            case NPC_GREATER_EARTHBIND_TOTEM:
                DoCastSelf(SPELL_EARTHBIND_TOTEM);
                break;
            case NPC_GREATER_POISON_CLEANSING_TOTEM:
                DoCastSelf(SPELL_POISON_CLEANSING_TOTEM);
                break;
            default:
                return;
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            karathress->Attack(who, false);
        }
        _scheduler.Schedule(10900ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_FROST_SHOCK);
            context.Repeat(10900ms, 14700ms);
        }).Schedule(15800ms, [this](TaskContext context)
        {
            if (_entryList.size() != 0) //don't summon when all totems are up
            {
                uint32 totemEntry = _entryList.front();
                _entryList.pop_front();
                SummonTotem(totemEntry);
                ScheduleRemoval(totemEntry);
            }
            context.Repeat(13350ms, 24250ms);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            me->CastSpell(karathress, SPELL_POWER_OF_TIDALVESS, true);
            karathress->AI()->Talk(SAY_GAIN_ABILITY1);
            me->DespawnOrUnsummon(1s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            return;
        }

        _scheduler.Update(diff);
        _totemScheduler.Update(diff);

        DoMeleeAttackIfReady();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            karathress->AI()->DoAction(ACTION_RESET_ADDS);
        }
        ScriptedAI::EnterEvadeMode(why);
    }

private:
    TaskScheduler _scheduler;
    TaskScheduler _totemScheduler;
    InstanceScript* _instance;
    uint8 _choice;
    SummonList _summons;
    std::list<uint32> _entryList;
};

struct boss_fathomguard_caribdis : public ScriptedAI
{
    boss_fathomguard_caribdis(Creature* creature) : ScriptedAI(creature), _summons(creature)
    {
        _instance = creature->GetInstanceScript();
        SetBoundary(_instance->GetBossBoundary(DATA_FATHOM_LORD_KARATHRESS));

        _scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    void Reset() override
    {
        _scheduler.CancelAll();

        _summons.DespawnAll();
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            karathress->Attack(who, false);
        }
        _scheduler.Schedule(27900ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_WATER_BOLT_VOLLEY);
            context.Repeat(6050ms, 19750ms);
        }).Schedule(23050ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_TIDAL_SURGE);
            context.Repeat(24250ms, 33250ms);
        }).Schedule(15750ms, [this](TaskContext context)
        {
            if (Unit* target = SelectKarathressRandomTarget(me, SPELL_SUMMON_CYCLONE, true))
                DoCast(target, SPELL_SUMMON_CYCLONE);

            context.Repeat(47250ms, 51550ms);
        }).Schedule(20s, [this](TaskContext context)
        {
            if (Unit* target = DoSelectLowestHpFriendly(60.0f, 150000))
            {
                DoCast(target, SPELL_HEALING_WAVE);
            }
            context.Repeat(20s);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            me->CastSpell(karathress, SPELL_POWER_OF_CARIBDIS, true);
            karathress->AI()->Talk(SAY_GAIN_ABILITY3);
            me->DespawnOrUnsummon(1s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            return;
        }

        _scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (Creature* karathress = _instance->GetCreature(DATA_FATHOM_LORD_KARATHRESS))
        {
            karathress->AI()->DoAction(ACTION_RESET_ADDS);
        }
        ScriptedAI::EnterEvadeMode(why);
    }

private:
    TaskScheduler _scheduler;
    InstanceScript* _instance;
    SummonList _summons;
};

class spell_karathress_power_of_tidalvess : public AuraScript
{
    PrepareAuraScript(spell_karathress_power_of_tidalvess);

    void OnPeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        GetUnitOwner()->CastSpell(GetUnitOwner(), GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_karathress_power_of_tidalvess::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_karathress_power_of_caribdis : public AuraScript
{
    PrepareAuraScript(spell_karathress_power_of_caribdis);

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
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_karathress_power_of_caribdis::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_boss_fathomlord_karathress()
{
    RegisterSerpentShrineAI(boss_fathomlord_karathress);
    RegisterSerpentShrineAI(boss_fathomguard_sharkkis);
    RegisterSerpentShrineAI(boss_fathomguard_tidalvess);
    RegisterSerpentShrineAI(boss_fathomguard_caribdis);
    RegisterSpellScript(spell_karathress_power_of_tidalvess);
    RegisterSpellScript(spell_karathress_power_of_caribdis);
}
