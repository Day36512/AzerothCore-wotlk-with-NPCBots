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
#include "ScriptedCreature.h"
#include "hyjal.h"

#include <list>

enum Spells
{
    SPELL_FROST_ARMOR           = 31256,
    SPELL_DEATH_AND_DECAY       = 31258,
    SPELL_FROST_NOVA            = 31250,
    SPELL_ICEBOLT               = 31249,
    SPELL_ENRAGE                = 26662
};

enum Texts
{
    SAY_ONDEATH                 = 0,
    SAY_ONSLAY                  = 1,
    SAY_DECAY                   = 2,
    SAY_NOVA                    = 3,
    SAY_ONSPAWN                 = 4
};

enum Misc
{
    GROUP_FROST                 = 1,
    PATH_RAGE_WINTERCHILL       = 177670,
    POINT_COMBAT_START          = 7
};

struct boss_rage_winterchill : public BossAI
{
public:
    boss_rage_winterchill(Creature* creature) : BossAI(creature, DATA_WINTERCHILL)
    {
        _recentlySpoken = false;
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        scheduler.Schedule(18s, 24s, [this](TaskContext context)
        {
            context.SetGroup(GROUP_FROST);

            DoCastSelf(SPELL_FROST_ARMOR);
            context.Repeat(30s, 45s);
        }).Schedule(5s, 9s, [this](TaskContext context)
        {
            context.SetGroup(GROUP_FROST);

            DoCastRandomPlayerOrBotTarget(SPELL_ICEBOLT);
            context.Repeat(9s, 15s);
        }).Schedule(12s, 17s, [this](TaskContext context)
        {
            context.SetGroup(GROUP_FROST);

            if (DoCastRandomPlayerOrBotTarget(SPELL_FROST_NOVA, 0, 45.f) == SPELL_CAST_OK)
                Talk(SAY_NOVA);

            context.Repeat(25s, 30s);
        }).Schedule(21s, 28s, [this](TaskContext context)
        {
            if (DoCastRandomPlayerOrBotTarget(SPELL_DEATH_AND_DECAY, 0, 40.f) == SPELL_CAST_OK)
            {
                Talk(SAY_DECAY);
                context.DelayGroup(GROUP_FROST, 15s);
            }

            context.Repeat(45s);
        }).Schedule(10min, [this](TaskContext context)
        {
            DoCastSelf(SPELL_ENRAGE);
            context.Repeat(5min);
        });
    }

    void DoAction(int32 action) override
    {
        Talk(SAY_ONSPAWN, 1200ms);

        if (action == DATA_WINTERCHILL)
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
        if (!_recentlySpoken && IsPlayerOrValidNPCBot(victim) && me->IsAlive())
        {
            Talk(SAY_ONSLAY);
            _recentlySpoken = true;

            scheduler.Schedule(6s, [this](TaskContext)
            {
                _recentlySpoken = false;
            });
        }
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_ONDEATH);
        BossAI::JustDied(killer);
    }

private:
    static bool IsValidNPCBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }

    static bool IsPlayerOrValidNPCBot(Unit const* unit)
    {
        if (!unit)
            return false;

        if (unit->IsPlayer())
            return true;

        return IsValidNPCBot(unit->ToCreature());
    }

    bool IsValidRandomPlayerOrBotTarget(Unit const* target, float dist, bool withTank) const
    {
        if (!target || !target->IsInWorld() || !target->IsAlive() || !target->IsInCombat() || target->GetMap() != me->GetMap())
            return false;

        if (target->HasUnitState(UNIT_STATE_ISOLATED))
            return false;

        if (!withTank && target == me->GetThreatMgr().GetLastVictim())
            return false;

        if (dist > 0.0f && !me->IsWithinCombatRange(target, dist))
            return false;

        if (dist < 0.0f && me->IsWithinCombatRange(target, -dist))
            return false;

        if (!me->IsValidAttackTarget(target))
            return false;

        return IsPlayerOrValidNPCBot(target);
    }

    Unit* SelectRandomPlayerOrBotTarget(uint32 threatTablePosition = 0, float dist = 0.0f, bool withTank = true)
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, threatTablePosition, [this, dist, withTank](Unit* target)
        {
            return IsValidRandomPlayerOrBotTarget(target, dist, withTank);
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

void AddSC_boss_rage_winterchill()
{
    RegisterHyjalAI(boss_rage_winterchill);
}
