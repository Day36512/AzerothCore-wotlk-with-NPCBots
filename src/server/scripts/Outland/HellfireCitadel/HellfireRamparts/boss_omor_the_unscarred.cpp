/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellScriptLoader.h"
#include "hellfire_ramparts.h"
#include "Containers.h"       // Acore::Containers helpers
#include "SpellAuras.h"       // Aura, SetDuration/SetMaxDuration

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
    SPELL_SHADOW_BOLT = 30686, // single target nuke
    SPELL_SUMMON_FIENDISH_HOUND = 30707, // summons 1 hound
    SPELL_TREACHEROUS_AURA = 30695, // classic Omor curse
    SPELL_DEMONIC_SHIELD = 31901  // damage reduction shield (base 10s)
};

// Tunables
static constexpr float HOUND_FIXATE_THREAT = 8000.0f;
static constexpr uint32 HOUND_HP_NORMAL = 21220; // normal 5-man
static constexpr uint32 HOUND_HP_HEROIC = 38750; // heroic 5-man
static constexpr float HOUND_SCALE_MULT = 2.0f;

struct boss_omor_the_unscarred : public BossAI
{
    boss_omor_the_unscarred(Creature* creature) : BossAI(creature, DATA_OMOR_THE_UNSCARRED), _hasSpoken(false)
    {
        me->SetCombatMovement(false);
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

        scheduler.Schedule(7s, [this](TaskContext context)
            {
                if (roll_chance_i(40))
                    Talk(SAY_CURSE);

                if (Unit* mark = SelectRandomEligibleTarget(60.0f))
                {
                    _lastCurseTarget = mark->GetGUID();
                    DoCast(mark, SPELL_TREACHEROUS_AURA, false);

                    scheduler.Schedule(6s, [this](TaskContext /*ctx*/)
                        {
                            if (Unit* anchor = ObjectAccessor::GetUnit(*me, _lastCurseTarget))
                                SpreadTreacherousAura(anchor, 8.0f /*radius*/, 2 /*max extra*/);
                            _lastCurseTarget.Clear();
                        });
                }
                context.Repeat(13s, 17s);
            });

        scheduler.Schedule(10s, [this](TaskContext context)
            {
                DoCastSelf(SPELL_SUMMON_FIENDISH_HOUND, true);
                context.Repeat(20s, 24s);
            });

        scheduler.Schedule(12s, [this](TaskContext context)
            {
                LaunchShadowVolley(3 /*bolts*/, 1200ms /*gap*/, 60.0f /*range*/);
                context.Repeat(22s, 28s);
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

        summon->SetObjectScale(summon->GetObjectScale() * HOUND_SCALE_MULT);

        bool heroic = me->GetMap() && me->GetMap()->IsHeroic();
        uint32 hp = heroic ? HOUND_HP_HEROIC : HOUND_HP_NORMAL;
        summon->SetMaxHealth(hp);
        summon->SetHealth(hp);

        Unit* target = SelectRandomEligibleTarget(60.0f, /*preferNonVictim=*/true);
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

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DIE);
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

                const bool isPlayer = u->GetTypeId() == TYPEID_PLAYER;
                const bool isNpcBot = (u->GetTypeId() == TYPEID_UNIT) && u->ToCreature() && u->ToCreature()->IsNPCBot();
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

    void SpreadTreacherousAura(Unit* anchor, float radius, uint8 maxExtra)
    {
        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck check(anchor, radius);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(anchor, nearby, check);
        Cell::VisitObjects(anchor, searcher, radius);

        nearby.remove_if([this, anchor](Unit* u)
            {
                if (!u || !u->IsAlive())
                    return true;

                const bool isPlayer = u->GetTypeId() == TYPEID_PLAYER;
                const bool isNpcBot = (u->GetTypeId() == TYPEID_UNIT) && u->ToCreature() && u->ToCreature()->IsNPCBot();
                if (!isPlayer && !isNpcBot)
                    return true;

                if (!me->IsValidAttackTarget(u))
                    return true;

                if (u->GetGUID() == anchor->GetGUID())
                    return true;

                return false;
            });

        Acore::Containers::RandomResize(nearby, std::min<uint8>(maxExtra, nearby.size()));
        for (Unit* u : nearby)
            DoCast(u, SPELL_TREACHEROUS_AURA, true);
    }

    void LaunchShadowVolley(uint8 bolts, Milliseconds gap, float range)
    {
        for (uint8 i = 0; i < bolts; ++i)
        {
            scheduler.Schedule(gap * i, [this, range](TaskContext /*ctx*/)
                {
                    if (Unit* tgt = SelectRandomEligibleTarget(range, /*preferNonVictim=*/true))
                        DoCast(tgt, SPELL_SHADOW_BOLT, true);
                });
        }
    }

    void StartShieldCycle()
    {
        if (_shieldActive || !me->IsAlive())
            return;

        _shieldActive = true;

        me->SetCombatMovement(false);
        me->StopMoving();

        // Cast shield (base 10s), then extend to 12s by setting durations on the aura.
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
        LaunchShadowVolley(2, 1200ms, 60.0f);

        scheduler.Schedule(12s, [this](TaskContext /*ctx*/)
            {
                _shieldActive = false;
                me->SetCombatMovement(true);
                if (Unit* v = me->GetVictim())
                    me->GetMotionMaster()->MoveChase(v);
            });
    }
};

void AddSC_boss_omor_the_unscarred()
{
    RegisterHellfireRampartsCreatureAI(boss_omor_the_unscarred);
}
