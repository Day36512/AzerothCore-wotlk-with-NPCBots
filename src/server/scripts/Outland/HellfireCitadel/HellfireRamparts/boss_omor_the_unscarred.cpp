/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellScriptLoader.h"
#include "hellfire_ramparts.h"
#include "Containers.h"
#include "SpellAuras.h"
#include "ObjectAccessor.h"

 // Dinkle custom: needed for random-target helpers
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Says
{
    SAY_AGGRO = 0,
    SAY_SUMMON = 1,
    SAY_CURSE = 2,
    SAY_KILL = 3,
    SAY_DIE = 4,
    SAY_WIPE = 5
};

enum Spells
{
    SPELL_SHADOW_BOLT = 30686,
    SPELL_SUMMON_FIENDISH_HOUND = 30707,
    SPELL_TREACHEROUS_AURA = 30695,
    SPELL_DEMONIC_SHIELD = 31901,
    SPELL_BERSERK = 300362
};

static constexpr float  HOUND_FIXATE_THREAT = 8000.0f;
static constexpr uint32 HOUND_HP_NORMAL = 16220;
static constexpr uint32 HOUND_HP_HEROIC = 22750;
static constexpr float  HOUND_SCALE_MULT = 2.0f;
static constexpr Milliseconds HOUND_TUNE_REAPPLY_DELAY = 150ms;

// Treacherous Aura timing.
// Original behavior was initial 7s, repeat 13s-17s.
// This keeps the mechanic active but makes it much less spammy.
static constexpr Milliseconds TREACHEROUS_AURA_INITIAL_DELAY = 12s;
static constexpr Milliseconds TREACHEROUS_AURA_REPEAT_MIN = 22s;
static constexpr Milliseconds TREACHEROUS_AURA_REPEAT_MAX = 28s;
static constexpr Milliseconds TREACHEROUS_AURA_SPREAD_DELAY = 6s;
static constexpr float TREACHEROUS_AURA_SPREAD_RADIUS = 8.0f;
static constexpr uint8 TREACHEROUS_AURA_MAX_EXTRA_TARGETS = 2;

struct boss_omor_the_unscarred : public BossAI
{
    boss_omor_the_unscarred(Creature* creature)
        : BossAI(creature, DATA_OMOR_THE_UNSCARRED), _hasSpoken(false), _shieldActive(false)
    {
        me->SetCombatMovement(false);

        // Dinkle custom
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    void Reset() override
    {
        Talk(SAY_WIPE);
        _Reset();
        _hasSpoken = false;
        _lastCurseTarget.Clear();
        _shieldActive = false;

        _activeHounds.clear();
        me->RemoveAurasDueToSpell(SPELL_BERSERK);

        ScheduleHealthCheckEvent(66, [&] { StartShieldCycle(); });
        ScheduleHealthCheckEvent(33, [&] { StartShieldCycle(); });
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        _JustEngagedWith();

        scheduler.Schedule(5s, [this](TaskContext context)
            {
                if (Unit* v = me->GetVictim())
                    DoCast(v, SPELL_SHADOW_BOLT, false);

                context.Repeat(6s, 8s);
            });

        scheduler.Schedule(TREACHEROUS_AURA_INITIAL_DELAY, [this](TaskContext context)
            {
                if (roll_chance_i(40))
                    Talk(SAY_CURSE);

                if (Unit* mark = SelectRandomEligibleTarget(60.0f))
                {
                    ObjectGuid markGuid = mark->GetGUID();
                    _lastCurseTarget = markGuid;

                    DoCast(mark, SPELL_TREACHEROUS_AURA, false);
                    if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(mark))
                        DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(bot, SPELL_TREACHEROUS_AURA, "DBM-Party-BC", "528", "Treacherous Aura", DBMFTABotCallouts::GetCooldownMs());

                    scheduler.Schedule(TREACHEROUS_AURA_SPREAD_DELAY, [this, markGuid](TaskContext /*ctx*/)
                        {
                            if (Unit* anchor = ObjectAccessor::GetUnit(*me, markGuid))
                                SpreadTreacherousAura(anchor, TREACHEROUS_AURA_SPREAD_RADIUS, TREACHEROUS_AURA_MAX_EXTRA_TARGETS);

                            if (_lastCurseTarget == markGuid)
                                _lastCurseTarget.Clear();
                        });
                }

                context.Repeat(TREACHEROUS_AURA_REPEAT_MIN, TREACHEROUS_AURA_REPEAT_MAX);
            });

        scheduler.Schedule(10s, [this](TaskContext context)
            {
                DoCastSelf(SPELL_SUMMON_FIENDISH_HOUND, true);
                context.Repeat(24s, 30s);
            });
    }

    void KilledUnit(Unit*) override
    {
        if (!_hasSpoken)
        {
            _hasSpoken = true;
            Talk(SAY_KILL);

            scheduler.Schedule(6s, [this](TaskContext /*context*/)
                {
                    _hasSpoken = false;
                });
        }
    }

    void JustSummoned(Creature* summon) override
    {
        Talk(SAY_SUMMON);
        summons.Summon(summon);
        summon->SetInCombatWithZone();

        ApplyHoundTuning(summon);

        ObjectGuid sguid = summon->GetGUID();
        scheduler.Schedule(HOUND_TUNE_REAPPLY_DELAY, [this, sguid](TaskContext /*ctx*/)
            {
                if (Creature* s = ObjectAccessor::GetCreature(*me, sguid))
                    ApplyHoundTuning(s);
            });

        _activeHounds.insert(summon->GetGUID());
        UpdateBerserkStacks();

        Unit* target = SelectRandomEligibleTarget(60.0f, true);
        if (!target)
            target = me->GetVictim();

        if (target)
        {
            summon->AI()->AttackStart(target);
            summon->AddThreat(target, HOUND_FIXATE_THREAT);

            scheduler.Schedule(1s, [summon, target](TaskContext /*ctx*/)
                {
                    if (summon->IsAlive() && target->IsAlive())
                        summon->AddThreat(target, HOUND_FIXATE_THREAT * 0.25f);
                });
        }
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _activeHounds.erase(summon->GetGUID());
        UpdateBerserkStacks();
        summons.Despawn(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        _activeHounds.erase(summon->GetGUID());
        UpdateBerserkStacks();
        summons.Despawn(summon);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DIE);
        _activeHounds.clear();
        me->RemoveAurasDueToSpell(SPELL_BERSERK);
        _JustDied();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (_shieldActive)
            return;

        if (!me->GetVictim() || !me->isAttackReady())
            return;

        if (me->IsWithinMeleeRange(me->GetVictim()))
        {
            me->GetMotionMaster()->MoveChase(me->GetVictim());
            DoMeleeAttackIfReady();
        }
        else
        {
            me->GetMotionMaster()->Clear();
            DoCastVictim(SPELL_SHADOW_BOLT, false);
            me->resetAttackTimer();
        }
    }

private:
    bool _hasSpoken;
    bool _shieldActive;
    ObjectGuid _lastCurseTarget;
    std::unordered_set<ObjectGuid> _activeHounds;

    void ApplyHoundTuning(Creature* hound)
    {
        if (!hound || !hound->IsAlive())
            return;

        float nativeScale = hound->GetNativeObjectScale();
        hound->SetObjectScale(nativeScale * HOUND_SCALE_MULT);

        bool heroic = me->GetMap() && me->GetMap()->IsHeroic();
        uint32 hp = heroic ? HOUND_HP_HEROIC : HOUND_HP_NORMAL;
        hound->SetMaxHealth(hp);
        hound->SetHealth(hp);
    }

    // Dinkle custom
    Unit* SelectRandomEligibleTarget(float range, bool preferNonVictim = false)
    {
        std::list<Unit*> targets;
        Acore::AnyUnitInObjectRangeCheck check(me, range);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, targets, check);
        Cell::VisitObjects(me, searcher, range);

        Unit* currentVictim = me->GetVictim();

        targets.remove_if([this, currentVictim, preferNonVictim](Unit* u)
            {
                if (!u || !u->IsAlive())
                    return true;

                bool const isPlayer = u->GetTypeId() == TYPEID_PLAYER;
                bool const isNpcBot = (u->GetTypeId() == TYPEID_UNIT) && u->ToCreature() && u->ToCreature()->IsNPCBot();

                if (!isPlayer && !isNpcBot)
                    return true;

                if (!me->IsValidAttackTarget(u))
                    return true;

                if (preferNonVictim && currentVictim && u->GetGUID() == currentVictim->GetGUID())
                    return true;

                return false;
            });

        if (targets.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(targets);
    }

    // Dinkle custom
    void SpreadTreacherousAura(Unit* anchor, float radius, uint8 maxExtra)
    {
        if (!anchor || !anchor->IsAlive())
            return;

        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck check(anchor, radius);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(anchor, nearby, check);
        Cell::VisitObjects(anchor, searcher, radius);

        nearby.remove_if([this, anchor](Unit* u)
            {
                if (!u || !u->IsAlive())
                    return true;

                bool const isPlayer = u->GetTypeId() == TYPEID_PLAYER;
                bool const isNpcBot = (u->GetTypeId() == TYPEID_UNIT) && u->ToCreature() && u->ToCreature()->IsNPCBot();

                if (!isPlayer && !isNpcBot)
                    return true;

                if (!me->IsValidAttackTarget(u))
                    return true;

                if (u->GetGUID() == anchor->GetGUID())
                    return true;

                return false;
            });

        Acore::Containers::RandomResize(nearby, std::min<size_t>(maxExtra, nearby.size()));

        for (Unit* u : nearby)
        {
            DoCast(u, SPELL_TREACHEROUS_AURA, true);
            if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(u))
                DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(bot, SPELL_TREACHEROUS_AURA, "DBM-Party-BC", "528", "Treacherous Aura", DBMFTABotCallouts::GetCooldownMs());
        }
    }

    void StartShieldCycle()
    {
        if (_shieldActive || !me->IsAlive())
            return;

        _shieldActive = true;

        me->SetCombatMovement(false);
        me->StopMoving();

        DoCastSelf(SPELL_DEMONIC_SHIELD, true);

        scheduler.Schedule(200ms, [this](TaskContext /*ctx*/)
            {
                if (Aura* a = me->GetAura(SPELL_DEMONIC_SHIELD))
                {
                    a->SetMaxDuration(12000);
                    a->SetDuration(12000);
                }
            });

        DoCastSelf(SPELL_SUMMON_FIENDISH_HOUND, true);

        scheduler.Schedule(12s, [this](TaskContext /*ctx*/)
            {
                _shieldActive = false;
                me->SetCombatMovement(true);

                if (Unit* v = me->GetVictim())
                    me->GetMotionMaster()->MoveChase(v);
            });
    }

    void UpdateBerserkStacks()
    {
        uint32 stacks = static_cast<uint32>(_activeHounds.size());

        if (stacks == 0)
        {
            me->RemoveAurasDueToSpell(SPELL_BERSERK);
            return;
        }

        if (Aura* aura = me->GetAura(SPELL_BERSERK))
        {
            aura->SetStackAmount(stacks);
        }
        else
        {
            me->CastSpell(me, SPELL_BERSERK, true);

            if (Aura* a2 = me->GetAura(SPELL_BERSERK))
                a2->SetStackAmount(stacks);
        }
    }
};

void AddSC_boss_omor_the_unscarred()
{
    RegisterHellfireRampartsCreatureAI(boss_omor_the_unscarred);
}
