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

#include "Creature.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "the_slave_pens.h"

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

#include <vector>
#include <list>
#include <chrono>

using namespace std::chrono_literals;

enum MennuSpells
{
    SPELL_LIGHTNING_BOLT = 35010,
    SPELL_HEALING_WARD = 34980,
    SPELL_EARTHGRAB_TOTEM = 31981,
    SPELL_STONESKIN_TOTEM = 31985,
    SPELL_NOVA_TOTEM = 31991,
    SPELL_CHAIN_LIGHTNING = 300385,
    SPELL_HEX = 16097,
    SPELL_BLOODLUST = 2825,   
    SPELL_SUMMON_FIRE_ELEMENTAL = 63774   // heroic-only periodic fire elemental
};

enum MennuText
{
    SAY_AGGRO = 1,
    SAY_KILL = 2,
    SAY_JUST_DIED = 3
};

enum MennuNPCs
{
    NPC_FIRE_ELEMENTAL_CUSTOM = 33838
};

struct boss_mennu_the_betrayer : public BossAI
{
    boss_mennu_the_betrayer(Creature* creature) : BossAI(creature, DATA_MENNU_THE_BETRAYER)
    {
        // Do not schedule abilities while casting
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    // Low-HP phase flag
    bool _totemicRage = false;

    static bool IsPlayerOrNPCBot(Unit* u)
    {
        if (!u || !u->IsAlive())
            return false;

        if (u->GetTypeId() == TYPEID_PLAYER)
            return true;

        if (u->GetTypeId() == TYPEID_UNIT)
        {
            if (Creature* c = u->ToCreature())
                return c->IsNPCBot();
        }

        return false;
    }

    static bool IsManaUser(Unit* u)
    {
        if (!u || !u->IsAlive())
            return false;

        if (u->getPowerType() != POWER_MANA)
            return false;

        return u->GetPower(POWER_MANA) > 0;
    }

    Unit* SelectRandomPlayerOrNPCBotFromThreat(bool excludeVictim = true)
    {
        std::vector<Unit*> pool;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (excludeVictim && u == me->GetVictim())
                continue;

            if (!IsPlayerOrNPCBot(u))
                continue;

            pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0u, static_cast<uint32>(pool.size() - 1))];
    }

    Unit* SelectRandomManaUserFromThreat(bool excludeVictim = true)
    {
        std::vector<Unit*> pool;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (excludeVictim && u == me->GetVictim())
                continue;

            if (!IsPlayerOrNPCBot(u))
                continue;

            if (!IsManaUser(u))
                continue;

            pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0u, static_cast<uint32>(pool.size() - 1))];
    }

    void DespawnNearbyFireElementals(float radius)
    {
        std::list<Creature*> elems;
        GetCreatureListWithEntryInGrid(elems, me, NPC_FIRE_ELEMENTAL_CUSTOM, radius);

        for (Creature* elem : elems)
            elem->DespawnOrUnsummon();
    }

    void Reset() override
    {
        scheduler.CancelAll();
        _Reset();
        _totemicRage = false;

        // Clean up any leftover fire elementals
        DespawnNearbyFireElementals(80.0f);

        // Health-based phases

        ScheduleHealthCheckEvent(75, [this]
            {
                DoCastSelf(SPELL_EARTHGRAB_TOTEM);
                DoCastSelf(SPELL_STONESKIN_TOTEM);
            });

        ScheduleHealthCheckEvent(60, [this]
            {
                DoCastSelf(SPELL_HEALING_WARD);
            });

        ScheduleHealthCheckEvent(40, [this]
            {
                DoCastSelf(SPELL_EARTHGRAB_TOTEM);
                DoCastSelf(SPELL_NOVA_TOTEM);
            });

        ScheduleHealthCheckEvent(25, [this]
            {
                if (_totemicRage)
                    return;

                _totemicRage = true;

                me->Yell("The tides will drag you under!", LANG_UNIVERSAL);
                me->CastSpell(me, SPELL_BLOODLUST, true);

                // Lightning spam
                scheduler.Schedule(1s, [this](TaskContext ctx)
                    {
                        Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false);
                        if (!target)
                            target = me->GetVictim();

                        if (target)
                            me->CastSpell(target, SPELL_LIGHTNING_BOLT, false);

                        ctx.Repeat(3s, 4s);
                    });

                // Faster Nova spam
                scheduler.Schedule(2s, [this](TaskContext ctx)
                    {
                        DoCastSelf(SPELL_NOVA_TOTEM);
                        ctx.Repeat(16s, 20s);
                    });
            });
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(5s, 8s, [this](TaskContext ctx)
            {
                Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false);
                if (!target)
                    target = me->GetVictim();

                if (target)
                    me->CastSpell(target, SPELL_LIGHTNING_BOLT, false);

                ctx.Repeat(7s, 10s);
            });

        scheduler.Schedule(8s, 12s, [this](TaskContext ctx)
            {
                Unit* target = SelectRandomPlayerOrNPCBotFromThreat(true);
                if (!target)
                    target = me->GetVictim();

                if (target)
                    me->CastSpell(target, SPELL_CHAIN_LIGHTNING, true);

                ctx.Repeat(14s, 18s);
            });

        scheduler.Schedule(15s, 20s, [this](TaskContext ctx)
            {
                if (Unit* target = SelectRandomManaUserFromThreat(true))
                    me->CastSpell(target, SPELL_HEX, false);

                ctx.Repeat(22s, 30s);
            });

        scheduler.Schedule(12s, 18s, [this](TaskContext ctx)
            {
                if (urand(0, 1) == 0)
                    DoCastSelf(SPELL_EARTHGRAB_TOTEM);

                DoCastSelf(SPELL_NOVA_TOTEM);
                ctx.Repeat(18s, 24s);
            });

        scheduler.Schedule(18s, 24s, [this](TaskContext ctx)
            {
                DoCastSelf(SPELL_STONESKIN_TOTEM);
                ctx.Repeat(26s, 32s);
            });

        scheduler.Schedule(22s, 28s, [this](TaskContext ctx)
            {
                if (me->HealthBelowPct(60))
                    DoCastSelf(SPELL_HEALING_WARD);

                ctx.Repeat(25s, 35s);
            });

        if (me->GetMap()->IsHeroic() && !me->GetMap()->IsRaid())
        {
            scheduler.Schedule(10s, [this](TaskContext ctx)
                {
                    me->CastSpell(me, SPELL_SUMMON_FIRE_ELEMENTAL, true);
                    ctx.Repeat(20s);
                });
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();
        _JustDied();
        Talk(SAY_JUST_DIED);
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_KILL);
    }
};

void AddSC_boss_mennu_the_betrayer()
{
    RegisterTheSlavePensCreatureAI(boss_mennu_the_betrayer);
}
