/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * at your option any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Creature.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ObjectAccessor.h"
#include "the_slave_pens.h"

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

#include <chrono>
#include <vector>
#include <algorithm>

using namespace std::chrono_literals;

enum QuagmirranSpells
{
    SPELL_ACID_SPRAY = 38153,
    SPELL_CLEAVE = 40504,
    SPELL_POISON_BOLT_VOLLEY = 34780,
    SPELL_UPPERCUT = 32055,

    SPELL_TOXIC_LINK_DOT = 300387,    // Poison DoT aura
    SPELL_TOXIC_LINK_VISUAL = 39920   // Visual tether, triggered from one linked target to the other
};

struct boss_quagmirran : public BossAI
{
    boss_quagmirran(Creature* creature) : BossAI(creature, DATA_QUAGMIRRAN)
    {
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    ObjectGuid _linkTarget1;
    ObjectGuid _linkTarget2;
    bool _linkActive = false;

    static constexpr float LINK_BREAK_DISTANCE = 25.0f;

    static bool IsPlayer(Unit* u)
    {
        return u && u->IsAlive() && u->GetTypeId() == TYPEID_PLAYER;
    }

    static bool IsNPCBot(Unit* u)
    {
        if (!u || !u->IsAlive() || u->GetTypeId() != TYPEID_UNIT)
            return false;

        if (Creature* c = u->ToCreature())
            return c->IsNPCBot();

        return false;
    }

    static bool IsPlayerOrNPCBot(Unit* u)
    {
        return IsPlayer(u) || IsNPCBot(u);
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

    void RemoveToxicLinkVisual(Unit* a, Unit* b)
    {
        if (a)
            a->RemoveAurasDueToSpell(SPELL_TOXIC_LINK_VISUAL);

        if (b)
            b->RemoveAurasDueToSpell(SPELL_TOXIC_LINK_VISUAL);
    }

    void EndToxicLink()
    {
        if (!_linkActive)
            return;

        Unit* a = ObjectAccessor::GetUnit(*me, _linkTarget1);
        Unit* b = ObjectAccessor::GetUnit(*me, _linkTarget2);

        if (a)
        {
            a->RemoveAurasDueToSpell(SPELL_TOXIC_LINK_DOT);
            a->RemoveAurasDueToSpell(SPELL_TOXIC_LINK_VISUAL);
        }

        if (b)
        {
            b->RemoveAurasDueToSpell(SPELL_TOXIC_LINK_DOT);
            b->RemoveAurasDueToSpell(SPELL_TOXIC_LINK_VISUAL);
        }

        RemoveToxicLinkVisual(a, b);

        _linkTarget1.Clear();
        _linkTarget2.Clear();
        _linkActive = false;
    }

    void ApplyToxicLinkVisual(Unit* first, Unit* second)
    {
        if (!first || !second)
            return;

        // Keep the damaging part as a DoT only.  Spell 39920 is used only as the
        // visible connection between linked targets.  Cast from one linked target
        // to the other so the beam appears between the pair instead of from boss.
        first->CastSpell(second, SPELL_TOXIC_LINK_VISUAL, true);
    }

    void TryStartToxicLink()
    {
        if (_linkActive)
            return;

        std::vector<Unit*> players;
        std::vector<Unit*> bots;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (IsPlayer(u))
                players.push_back(u);
            else if (IsNPCBot(u))
                bots.push_back(u);
        }

        if (players.size() + bots.size() < 2)
            return;

        Unit* first = nullptr;
        Unit* second = nullptr;

        if (players.size() >= 2)
        {
            uint32 i1 = urand(0u, static_cast<uint32>(players.size() - 1));
            uint32 i2;
            do
            {
                i2 = urand(0u, static_cast<uint32>(players.size() - 1));
            } while (i2 == i1);

            first = players[i1];
            second = players[i2];
        }
        else if (!players.empty() && !bots.empty())
        {
            first = players[0];
            second = bots[urand(0u, static_cast<uint32>(bots.size() - 1))];
        }
        else
        {
            if (bots.size() < 2)
                return;

            uint32 i1 = urand(0u, static_cast<uint32>(bots.size() - 1));
            uint32 i2;
            do
            {
                i2 = urand(0u, static_cast<uint32>(bots.size() - 1));
            } while (i2 == i1);

            first = bots[i1];
            second = bots[i2];
        }

        if (!first || !second || first == second)
            return;

        _linkTarget1 = first->GetGUID();
        _linkTarget2 = second->GetGUID();
        _linkActive = true;

        me->CastSpell(first, SPELL_TOXIC_LINK_DOT, true);
        me->CastSpell(second, SPELL_TOXIC_LINK_DOT, true);

        ApplyToxicLinkVisual(first, second);
    }

    void Reset() override
    {
        scheduler.CancelAll();
        EndToxicLink();
        _Reset();
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();
        EndToxicLink();
        _JustDied();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler
            .Schedule(9100ms, [this](TaskContext ctx)
                {
                    DoCastVictim(SPELL_CLEAVE);
                    ctx.Repeat(18800ms, 24800ms);
                })
            .Schedule(20300ms, [this](TaskContext ctx)
                {
                    DoCastVictim(SPELL_UPPERCUT);
                    ctx.Repeat(21800ms);
                })
            .Schedule(25200ms, [this](TaskContext ctx)
                {
                    if (Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false))
                        me->CastSpell(target, SPELL_ACID_SPRAY, false);
                    else
                        DoCastRandomTarget(SPELL_ACID_SPRAY);

                    ctx.Repeat(25s);
                })
            .Schedule(31800ms, [this](TaskContext ctx)
                {
                    DoCastAOE(SPELL_POISON_BOLT_VOLLEY);
                    ctx.Repeat(24400ms);
                });

        // Toxic Link start loop
        scheduler.Schedule(20s, [this](TaskContext ctx)
            {
                TryStartToxicLink();
                ctx.Repeat(25s, 32s);
            });

        // Toxic Link heartbeat:
        // No extra damage spell is cast here.  The mechanic is only:
        //  - both linked targets have the DoT
        //  - one linked target casts the visual tether onto the other
        //  - moving far enough apart removes both DoTs and the visual aura
        scheduler.Schedule(1s, [this](TaskContext ctx)
            {
                if (_linkActive)
                {
                    Unit* a = ObjectAccessor::GetUnit(*me, _linkTarget1);
                    Unit* b = ObjectAccessor::GetUnit(*me, _linkTarget2);

                    if (!a || !b || !a->IsAlive() || !b->IsAlive() || !me->IsInCombat())
                    {
                        EndToxicLink();
                    }
                    else if (!a->HasAura(SPELL_TOXIC_LINK_DOT) || !b->HasAura(SPELL_TOXIC_LINK_DOT))
                    {
                        // If the DoT was removed by any non-distance condition, clean the visual too.
                        EndToxicLink();
                    }
                    else if (a->GetDistance(b) >= LINK_BREAK_DISTANCE)
                    {
                        EndToxicLink();
                    }
                }

                ctx.Repeat(1s);
            });
    }
};

void AddSC_boss_quagmirran()
{
    RegisterTheSlavePensCreatureAI(boss_quagmirran);
}
