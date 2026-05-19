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
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "TaskScheduler.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "gruuls_lair.h"
#include "SpellMgr.h"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

enum HighKingMaulgar
{
    SAY_AGGRO                   = 0,
    SAY_ENRAGE                  = 1,
    SAY_OGRE_DEATH              = 2,
    SAY_SLAY                    = 3,
    SAY_DEATH                   = 4,

    // High King Maulgar
    SPELL_ARCING_SMASH          = 39144,
    SPELL_MIGHTY_BLOW           = 33230,
    SPELL_WHIRLWIND             = 33238,
    SPELL_BERSERKER_C           = 26561,
    SPELL_ROAR                  = 16508,
    SPELL_FLURRY                = 33232,

    // Olm the Summoner
    SPELL_DARK_DECAY            = 33129,
    SPELL_DEATH_COIL            = 33130,
    SPELL_SUMMON_WFH            = 33131,

    // Kiggler the Craed
    SPELL_GREATER_POLYMORPH     = 33173,
    SPELL_LIGHTNING_BOLT        = 36152,
    SPELL_ARCANE_SHOCK          = 33175,
    SPELL_ARCANE_EXPLOSION      = 33237,

    // Blindeye the Seer
    SPELL_GREATER_PW_SHIELD     = 33147,
    SPELL_HEAL                  = 33144,
    SPELL_PRAYER_OH             = 33152,

    // Krosh Firehand
    SPELL_GREATER_FIREBALL      = 33051,
    SPELL_SPELLSHIELD           = 33054,
    SPELL_BLAST_WAVE            = 33061,

    ACTION_ADD_DEATH            = 1
};

namespace
{
    enum MaulgarBotSupport
    {
        MAULGAR_THREAT_SUPPORT_TIMER = 2 * IN_MILLISECONDS,

        MAULGAR_THREAT_MAIN_TANK     = 450000,
        MAULGAR_THREAT_OLM           = 300000,
        MAULGAR_THREAT_KIGGLER       = 250000,
        MAULGAR_THREAT_KROSH         = 350000,
        MAULGAR_THREAT_BLINDEYE      = 180000,
        MAULGAR_THREAT_FEL_HUNTER    = 400000
    };

    bool IsUsableEncounterUnitForThreat(Unit* unit, Creature const* source)
    {
        return unit &&
            source &&
            unit->IsInWorld() &&
            unit->IsAlive() &&
            unit->GetMap() == source->GetMap() &&
            !unit->HasUnitState(UNIT_STATE_ISOLATED) &&
            unit->IsWithinDist(source, 180.0f);
    }

    bool IsPlayerMarkedTank(Player* player)
    {
        if (!player)
            return false;

        Group* group = player->GetGroup();
        if (!group)
            return false;

        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::member_citerator itr = slots.begin(); itr != slots.end(); ++itr)
        {
            if (itr->guid == player->GetGUID())
                return itr->flags & (MEMBER_FLAG_MAINTANK | MEMBER_FLAG_MAINASSIST);
        }

        return false;
    }

    bot_ai* GetNpcBotAI(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return nullptr;

        return creature->GetBotAI();
    }

    bool IsNpcBotTank(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsNpcBotMainTank(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        return ai && ai->IsTank() && !ai->IsOffTank();
    }

    bool IsNpcBotOffTank(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        return ai && ai->IsOffTank();
    }

    bool IsNpcBotMage(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        return ai && ai->GetBotClass() == BOT_CLASS_MAGE;
    }

    bool IsNpcBotRanged(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        return ai && ai->HasRole(BOT_ROLE_RANGED);
    }

    bool IsNpcBotRangedDps(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        return ai && ai->HasRole(BOT_ROLE_RANGED) && ai->HasRole(BOT_ROLE_DPS);
    }

    bool IsNpcBotCaster(Unit* unit)
    {
        bot_ai* ai = GetNpcBotAI(unit);
        if (!ai)
            return false;

        return (CASTING_BOT_CLASSES_MASK & (1u << ai->GetBotClass())) != 0;
    }

    bool IsNpcBotCasterRanged(Unit* unit)
    {
        return IsNpcBotRanged(unit) && IsNpcBotCaster(unit);
    }

    bool IsEncounterMainTank(Unit* unit)
    {
        if (!unit)
            return false;

        if (IsNpcBotMainTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsPlayerMarkedTank(player);

        return false;
    }

    bool IsEncounterTank(Unit* unit)
    {
        if (!unit)
            return false;

        if (IsNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsPlayerMarkedTank(player);

        return false;
    }

    void TryAddEncounterUnit(Unit* unit, Creature* source, GuidSet& seen, std::vector<Unit*>& units)
    {
        if (!IsUsableEncounterUnitForThreat(unit, source))
            return;

        if (seen.count(unit->GetGUID()))
            return;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return;

        seen.insert(unit->GetGUID());
        units.push_back(unit);
    }

    std::vector<Unit*> GatherEncounterUnits(Creature* source)
    {
        std::vector<Unit*> units;
        GuidSet seen;

        if (!source || !source->GetMap())
            return units;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            TryAddEncounterUnit(player, source, seen, units);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    TryAddEncounterUnit(member, source, seen, units);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
            {
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    TryAddEncounterUnit(pair.second, source, seen, units);
            }
        }

        return units;
    }

    template <class Predicate>
    Unit* SelectEncounterUnit(Creature* source, Predicate predicate, ObjectGuid excludedGuid = ObjectGuid::Empty)
    {
        Unit* selected = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        for (Unit* unit : GatherEncounterUnits(source))
        {
            if (!unit || unit->GetGUID() == excludedGuid)
                continue;

            if (!predicate(unit))
                continue;

            float distance = source->GetDistance(unit);
            if (!selected || distance < bestDistance)
            {
                selected = unit;
                bestDistance = distance;
            }
        }

        return selected;
    }

    Unit* SelectNearestEncounterUnit(Creature* source, ObjectGuid excludedGuid = ObjectGuid::Empty)
    {
        return SelectEncounterUnit(source, [](Unit*) { return true; }, excludedGuid);
    }

    Unit* SelectMaulgarTank(Creature* source)
    {
        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return u->IsPlayer() && IsPlayerMarkedTank(u->ToPlayer()); }))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotMainTank(u); }))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsEncounterTank(u); }))
            return unit;

        return SelectNearestEncounterUnit(source);
    }

    Unit* SelectOlmTank(Creature* source)
    {
        ObjectGuid mainTankGuid = ObjectGuid::Empty;
        if (Unit* mainTank = SelectMaulgarTank(source))
            mainTankGuid = mainTank->GetGUID();

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotOffTank(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsEncounterTank(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotOffTank(u); }))
            return unit;

        return SelectNearestEncounterUnit(source, mainTankGuid);
    }

    Unit* SelectKigglerTank(Creature* source)
    {
        ObjectGuid mainTankGuid = ObjectGuid::Empty;
        if (Unit* mainTank = SelectMaulgarTank(source))
            mainTankGuid = mainTank->GetGUID();

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotOffTank(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotRangedDps(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsEncounterTank(u); }, mainTankGuid))
            return unit;

        return SelectNearestEncounterUnit(source, mainTankGuid);
    }

    Unit* SelectKroshTank(Creature* source)
    {
        ObjectGuid mainTankGuid = ObjectGuid::Empty;
        if (Unit* mainTank = SelectMaulgarTank(source))
            mainTankGuid = mainTank->GetGUID();

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotMage(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotCasterRanged(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotRangedDps(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotOffTank(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsEncounterTank(u); }, mainTankGuid))
            return unit;

        return SelectNearestEncounterUnit(source, mainTankGuid);
    }

    Unit* SelectBlindeyeTank(Creature* source)
    {
        ObjectGuid mainTankGuid = ObjectGuid::Empty;
        if (Unit* mainTank = SelectMaulgarTank(source))
            mainTankGuid = mainTank->GetGUID();

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotOffTank(u); }, mainTankGuid))
            return unit;

        if (Unit* unit = SelectEncounterUnit(source, [](Unit* u) { return IsEncounterTank(u); }, mainTankGuid))
            return unit;

        return SelectNearestEncounterUnit(source, mainTankGuid);
    }

    Unit* SelectThreatTargetFor(Creature* creature)
    {
        if (!creature)
            return nullptr;

        switch (creature->GetEntry())
        {
            case NPC_MAULGAR:
                return SelectMaulgarTank(creature);
            case NPC_OLM_THE_SUMMONER:
                return SelectOlmTank(creature);
            case NPC_KIGGLER_THE_CRAZED:
                return SelectKigglerTank(creature);
            case NPC_KROSH_FIREHAND:
                return SelectKroshTank(creature);
            case NPC_BLINDEYE_THE_SEER:
                return SelectBlindeyeTank(creature);
            default:
                return SelectOlmTank(creature);
        }
    }

    float GetThreatSupportAmount(Creature* creature)
    {
        if (!creature)
            return 0.0f;

        switch (creature->GetEntry())
        {
            case NPC_MAULGAR:
                return float(MAULGAR_THREAT_MAIN_TANK);
            case NPC_OLM_THE_SUMMONER:
                return float(MAULGAR_THREAT_OLM);
            case NPC_KIGGLER_THE_CRAZED:
                return float(MAULGAR_THREAT_KIGGLER);
            case NPC_KROSH_FIREHAND:
                return float(MAULGAR_THREAT_KROSH);
            case NPC_BLINDEYE_THE_SEER:
                return float(MAULGAR_THREAT_BLINDEYE);
            default:
                return float(MAULGAR_THREAT_FEL_HUNTER);
        }
    }

    void AddEncounterThreat(Creature* creature, Unit* target, float threatValue, bool forceAttack)
    {
        if (!creature || !creature->IsAlive() || !IsUsableEncounterUnitForThreat(target, creature))
            return;

        creature->SetInCombatWithZone();
        creature->SetInCombatWith(target);
        target->SetInCombatWith(creature);
        creature->GetThreatMgr().AddThreat(target, threatValue);

        if (forceAttack && creature->AI() && creature->GetVictim() != target)
            creature->AI()->AttackStart(target);
    }

    Unit* ReinforceEncounterThreat(Creature* creature, bool forceAttack)
    {
        Unit* target = SelectThreatTargetFor(creature);
        AddEncounterThreat(creature, target, GetThreatSupportAmount(creature), forceAttack);
        return target;
    }

    Unit* UpdateEncounterThreatSupport(Creature* creature, uint32 diff, uint32& timer, bool forceAttack)
    {
        if (!creature || !creature->IsEngaged())
            return nullptr;

        if (timer > diff)
        {
            timer -= diff;
            return nullptr;
        }

        timer = MAULGAR_THREAT_SUPPORT_TIMER;
        return ReinforceEncounterThreat(creature, forceAttack);
    }

    bool EncounterHasMageBot(Creature* source)
    {
        return SelectEncounterUnit(source, [](Unit* u) { return IsNpcBotMage(u); }) != nullptr;
    }

    void SupportKroshFallbackShield(Creature* krosh, Unit* target)
    {
        if (!krosh || krosh->GetEntry() != NPC_KROSH_FIREHAND || !target || EncounterHasMageBot(krosh))
            return;

        if (!target->IsNPCBot() || target->HasAura(SPELL_SPELLSHIELD))
            return;

        target->CastSpell(target, SPELL_SPELLSHIELD, true);
    }

    Unit* SelectKigglerPolymorphTarget(Creature* kiggler)
    {
        Unit* victim = kiggler ? kiggler->GetVictim() : nullptr;
        if (!kiggler || !victim)
            return nullptr;

        Unit* selected = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        for (Unit* unit : GatherEncounterUnits(kiggler))
        {
            if (!unit || unit == victim)
                continue;

            if (IsEncounterMainTank(unit) || IsNpcBotTank(unit))
                continue;

            if (!kiggler->IsValidAttackTarget(unit) || !unit->isTargetableForAttack(false))
                continue;

            if (!unit->InSamePhase(kiggler) || !kiggler->IsWithinLOSInMap(unit))
                continue;

            float distance = kiggler->GetDistance(unit);
            if (distance > 45.0f)
                continue;

            if (!selected || distance < bestDistance)
            {
                selected = unit;
                bestDistance = distance;
            }
        }

        if (selected)
            return selected;

        return (IsEncounterMainTank(victim) || IsNpcBotTank(victim)) ? nullptr : victim;
    }

    bool IsNonTankNpcBot(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return !ai || (!ai->IsTank() && !ai->IsOffTank());
    }
}

struct boss_high_king_maulgar : public BossAI
{
    boss_high_king_maulgar(Creature* creature) : BossAI(creature, DATA_MAULGAR)
    {    }

    void Reset() override
    {
        _Reset();
        _recentlySpoken = false;
        _threatSupportTimer = 0;
        me->SetLootMode(0);
        ScheduleHealthCheckEvent(50, [&]{
            Talk(SAY_ENRAGE);
            DoCastSelf(SPELL_FLURRY);

            scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastRandomTarget(SPELL_BERSERKER_C);
                context.Repeat(35s);
            }).Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_ROAR);
                context.Repeat(20600ms, 29100ms);
            });
        });
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType /*damageType*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (damage && me->HasAura(SPELL_WHIRLWIND) && IsNonTankNpcBot(victim))
            damage = CalculatePct(damage, 45);
    }

    void KilledUnit(Unit*  /*victim*/) override
    {
        if (!_recentlySpoken)
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;
        }

        scheduler.Schedule(5s, [this](TaskContext)
        {
            _recentlySpoken = false;
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        if (instance->GetData(DATA_ADDS_KILLED) == MAX_ADD_NUMBER)
            _JustDied();
    }

    void DoAction(int32 actionId) override
    {
        if (me->IsAlive())
        {
            Talk(SAY_OGRE_DEATH);
            if (actionId == MAX_ADD_NUMBER)
            {
                me->AddLootMode(1);
            }
        }
        else if (actionId == MAX_ADD_NUMBER)
        {
            me->AddLootMode(1);
            me->loot.clear();
            me->loot.FillLoot(me->GetCreatureTemplate()->lootid, LootTemplates_Creature, me->GetLootRecipient(), false, false, me->GetLootMode(), me);
            me->SetDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
            _JustDied();
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(9500ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_ARCING_SMASH);
            context.Repeat(9500ms, 12s);
        }).Schedule(15700ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_MIGHTY_BLOW);
            context.Repeat(16200ms, 19s);
        }).Schedule(67s, [this](TaskContext context)
        {
            scheduler.DelayAll(15s);
            DoCastSelf(SPELL_WHIRLWIND);
            context.Repeat(45s, 60s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        UpdateEncounterThreatSupport(me, diff, _threatSupportTimer, true);

        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }
private:
    bool _recentlySpoken;
    uint32 _threatSupportTimer;
};

struct boss_olm_the_summoner : public ScriptedAI
{
    boss_olm_the_summoner(Creature* creature) : ScriptedAI(creature), summons(me)
    {
        instance = creature->GetInstanceScript();
        _scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    SummonList summons;
    InstanceScript* instance;

    void Reset() override
    {
        _scheduler.CancelAll();
        summons.DespawnAll();
        _threatSupportTimer = 0;
        instance->SetBossState(DATA_MAULGAR, NOT_STARTED);
    }

    void AttackStart(Unit* who) override
    {
        if (!who)
            return;

        if (me->Attack(who, true))
            me->GetMotionMaster()->MoveChase(who, 25.0f);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->SetInCombatWithZone();
        instance->SetBossState(DATA_MAULGAR, IN_PROGRESS);
        ReinforceEncounterThreat(me, true);
        _threatSupportTimer = MAULGAR_THREAT_SUPPORT_TIMER;

        _scheduler.Schedule(1200ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SUMMON_WFH);
            context.Repeat(48500ms);
        }).Schedule(6050ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_DARK_DECAY);
            context.Repeat(6050ms);
        }).Schedule(6500ms, [this](TaskContext context)
        {
            if (me->HealthBelowPct(90))
            {
                DoCastRandomTarget(SPELL_DEATH_COIL);
            }
            context.Repeat(6s, 13500ms);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        instance->SetData(DATA_ADDS_KILLED, 1);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);

        Unit* tank = SelectOlmTank(me);
        AddEncounterThreat(summon, tank, float(MAULGAR_THREAT_FEL_HUNTER), true);

        ObjectGuid summonGuid = summon->GetGUID();
        ObjectGuid tankGuid = tank ? tank->GetGUID() : ObjectGuid::Empty;
        _scheduler.Schedule(1500ms, [this, summonGuid, tankGuid](TaskContext)
        {
            Creature* summon = ObjectAccessor::GetCreature(*me, summonGuid);
            Unit* tank = tankGuid ? ObjectAccessor::GetUnit(*me, tankGuid) : SelectOlmTank(me);
            AddEncounterThreat(summon, tank, float(MAULGAR_THREAT_FEL_HUNTER), true);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        UpdateEncounterThreatSupport(me, diff, _threatSupportTimer, true);

        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }
private:
    TaskScheduler _scheduler;
    uint32 _threatSupportTimer;
};

struct boss_kiggler_the_crazed : public ScriptedAI
{
    boss_kiggler_the_crazed(Creature* creature) : ScriptedAI(creature)
    {
        instance = creature->GetInstanceScript();
        _scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    InstanceScript* instance;

    void Reset() override
    {
        _scheduler.CancelAll();
        _threatSupportTimer = 0;
        instance->SetBossState(DATA_MAULGAR, NOT_STARTED);
    }

    void AttackStart(Unit* who) override
    {
        if (!who)
            return;

        if (me->Attack(who, true))
            me->GetMotionMaster()->MoveChase(who, 25.0f);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->SetInCombatWithZone();
        instance->SetBossState(DATA_MAULGAR, IN_PROGRESS);
        ReinforceEncounterThreat(me, true);
        _threatSupportTimer = MAULGAR_THREAT_SUPPORT_TIMER;

        _scheduler.Schedule(1200ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_LIGHTNING_BOLT);
            context.Repeat(2400ms);
        }).Schedule(29s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_ARCANE_SHOCK);
            context.Repeat(7200ms, 20600ms);
        }).Schedule(23s, [this](TaskContext context)
        {
            if (Unit* target = SelectKigglerPolymorphTarget(me))
                DoCast(target, SPELL_GREATER_POLYMORPH);
            context.Repeat(10900ms);
        }).Schedule(30s, [this](TaskContext context)
        {
            if (me->SelectNearestTarget(15.0f))
            {
                    DoCastAOE(SPELL_ARCANE_EXPLOSION);
            }
            context.Repeat(30s);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        instance->SetData(DATA_ADDS_KILLED, 1);
    }

    void UpdateAI(uint32 diff) override
    {
        UpdateEncounterThreatSupport(me, diff, _threatSupportTimer, true);

        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }
private:
    TaskScheduler _scheduler;
    uint32 _threatSupportTimer;
};

struct boss_blindeye_the_seer : public ScriptedAI
{
    boss_blindeye_the_seer(Creature* creature) : ScriptedAI(creature)
    {
        instance = creature->GetInstanceScript();
        _scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    InstanceScript* instance;

    void Reset() override
    {
        _scheduler.CancelAll();
        _threatSupportTimer = 0;
        instance->SetBossState(DATA_MAULGAR, NOT_STARTED);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->SetInCombatWithZone();
        instance->SetBossState(DATA_MAULGAR, IN_PROGRESS);
        ReinforceEncounterThreat(me, true);
        _threatSupportTimer = MAULGAR_THREAT_SUPPORT_TIMER;

        _scheduler.Schedule(7200ms, [this](TaskContext context)
        {
            if (Unit* target = DoSelectLowestHpFriendly(60.0f, 50000))
            {
                DoCast(target, SPELL_HEAL);
            }
            context.Repeat(7200ms);
        }).Schedule(37500ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_GREATER_PW_SHIELD);
            _scheduler.Schedule(1200ms, [this](TaskContext)
            {
                DoCastSelf(SPELL_PRAYER_OH);
            });
            context.Repeat(54500ms, 63s);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        instance->SetData(DATA_ADDS_KILLED, 1);
    }

    void UpdateAI(uint32 diff) override
    {
        UpdateEncounterThreatSupport(me, diff, _threatSupportTimer, true);

        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }
private:
    TaskScheduler _scheduler;
    uint32 _threatSupportTimer;
};

struct boss_krosh_firehand : public ScriptedAI
{
    boss_krosh_firehand(Creature* creature) : ScriptedAI(creature)
    {
        instance = creature->GetInstanceScript();
        _scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING);
        });
    }

    InstanceScript* instance;

    void Reset() override
    {
        _scheduler.CancelAll();
        _threatSupportTimer = 0;
        instance->SetBossState(DATA_MAULGAR, NOT_STARTED);
    }

    void AttackStart(Unit* who) override
    {
        if (!who)
            return;

        if (me->Attack(who, true))
            me->GetMotionMaster()->MoveChase(who, 25.0f);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->SetInCombatWithZone();
        instance->SetBossState(DATA_MAULGAR, IN_PROGRESS);
        Unit* kroshTank = ReinforceEncounterThreat(me, true);
        SupportKroshFallbackShield(me, kroshTank);
        _threatSupportTimer = MAULGAR_THREAT_SUPPORT_TIMER;

        _scheduler.Schedule(1200ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SPELLSHIELD);
            context.Repeat(30300ms);
        }).Schedule(3600ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_GREATER_FIREBALL);
            context.Repeat(3600ms);
        }).Schedule(7200ms, [this](TaskContext context)
        {
            if (me->SelectNearestTarget(15.0f))
            {
                    DoCastAOE(SPELL_BLAST_WAVE);
            }
            context.Repeat(7200ms);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        instance->SetData(DATA_ADDS_KILLED, 1);
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType /*damageType*/, SpellSchoolMask damageSchoolMask) override
    {
        if (damage && victim && victim->IsNPCBot() && !victim->HasAura(SPELL_SPELLSHIELD) && (damageSchoolMask & SPELL_SCHOOL_MASK_FIRE))
            damage = CalculatePct(damage, 55);
    }

    void UpdateAI(uint32 diff) override
    {
        if (Unit* kroshTank = UpdateEncounterThreatSupport(me, diff, _threatSupportTimer, true))
            SupportKroshFallbackShield(me, kroshTank);

        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }
private:
    TaskScheduler _scheduler;
    uint32 _threatSupportTimer;
};

void AddSC_boss_high_king_maulgar()
{
    RegisterGruulsLairAI(boss_high_king_maulgar);
    RegisterGruulsLairAI(boss_kiggler_the_crazed);
    RegisterGruulsLairAI(boss_blindeye_the_seer);
    RegisterGruulsLairAI(boss_olm_the_summoner);
    RegisterGruulsLairAI(boss_krosh_firehand);
}
