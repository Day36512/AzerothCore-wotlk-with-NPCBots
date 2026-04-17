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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "blood_furnace.h"
#include <cmath>
#include <vector>
#include <limits>

#include "bot_ai.h"
#include "botmgr.h"

namespace
{
    static Position const KELIDAN_CENTER = { 326.04f, -87.20f, -24.65f, 4.86f };
    static constexpr float KELIDAN_LEASH_RADIUS = 65.0f;
    static constexpr float PI = 3.14159265358979323846f;
}

enum Says
{
    SAY_WAKE = 0,
    SAY_ADD_AGGRO = 1,
    SAY_KILL = 2,
    SAY_NOVA = 3,
    SAY_DIE = 4
};

enum Spells
{
    SPELL_CORRUPTION = 30938,
    SPELL_EVOCATION = 30935,
    SPELL_FIRE_NOVA = 300380,   // huge 500y nova
    SPELL_SHADOW_BOLT_VOLLEY = 28599,
    SPELL_BURNING_NOVA = 300381,   // 11s immunity channel/aura
    SPELL_VORTEX = 37370,
    SPELL_ANTI_MAGIC_ZONE = 300379    // 12s, -95% spell dmg
};

enum Misc
{
    NPC_SHADOWMOON_CHANNELER = 17653
};

enum Actions
{
    ACTION_CHANNELER_DIED = 1,
    ACTION_CHANNELER_AGGRO = 2
};

struct boss_kelidan_the_breaker : public BossAI
{
    boss_kelidan_the_breaker(Creature* creature) : BossAI(creature, DATA_KELIDAN)
    {
        scheduler.SetValidator([this] { return !me->HasUnitState(UNIT_STATE_CASTING); });
    }

    std::vector<Position> _amzSpots;

    void ClearAllTimersAndState()
    {
        scheduler.CancelAll();
        _amzSpots.clear();

        me->RemoveAurasDueToSpell(SPELL_BURNING_NOVA);
        me->CombatStop(true);
        me->GetThreatMgr().ClearAllThreat();
        me->AttackStop();

        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        ApplyImmunities(true);

        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveTargetedHome();
    }

    bool AnyAliveRaidMembersOnThreat() const
    {
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (u->GetTypeId() == TYPEID_PLAYER)
                return true;

            if (u->GetTypeId() == TYPEID_UNIT)
            {
                Creature* c = u->ToCreature();
                if (c && c->IsNPCBot())
                    return true;
            }
        }

        return false;
    }

    // -------- lifecycle --------
    void Reset() override
    {
        _amzSpots.clear();
        scheduler.CancelAll();

        _Reset();
        ApplyImmunities(true);
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        DoCastSelf(SPELL_EVOCATION); 

        me->SetHomePosition(KELIDAN_CENTER);
        if (me->GetExactDist2d(KELIDAN_CENTER.GetPositionX(), KELIDAN_CENTER.GetPositionY()) > 0.5f)
            me->NearTeleportTo(KELIDAN_CENTER.GetPositionX(), KELIDAN_CENTER.GetPositionY(),
                KELIDAN_CENTER.GetPositionZ(), KELIDAN_CENTER.GetOrientation());
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ClearAllTimersAndState();
        BossAI::EnterEvadeMode(why);
        DoCastSelf(SPELL_EVOCATION);
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();
        Talk(SAY_DIE);
        _JustDied();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        scheduler.CancelAll();

        Talk(SAY_WAKE);
        _JustEngagedWith();
        me->InterruptNonMeleeSpells(false); 

        scheduler.Schedule(1s, [this](TaskContext ctx)
            {
                DoCastAOE(SPELL_SHADOW_BOLT_VOLLEY);
                ctx.Repeat(8s, 13s);
            });

        scheduler.Schedule(5s, [this](TaskContext ctx)
            {
                DoCastAOE(SPELL_CORRUPTION);
                ctx.Repeat(30s, 50s);
            });

        scheduler.Schedule(15s, [this](TaskContext ctx)
            {
                if (me->GetExactDist2d(KELIDAN_CENTER.GetPositionX(), KELIDAN_CENTER.GetPositionY()) > 0.5f)
                {
                    me->CastSpell(me, 64446, true);
                    me->NearTeleportTo(KELIDAN_CENTER.GetPositionX(), KELIDAN_CENTER.GetPositionY(),
                        KELIDAN_CENTER.GetPositionZ(), KELIDAN_CENTER.GetOrientation());
                }

                Talk(SAY_NOVA);

                ApplyImmunities(false);
                me->AddAura(SPELL_BURNING_NOVA, me); // lasts 11s
                ApplyImmunities(true);

                if (IsHeroic())
                    DoCastAOE(SPELL_VORTEX);

                SpawnAntiMagicZones();
                MoveBotsIntoAMZ();

                scheduler.Schedule(10s, [this](TaskContext /*fire*/)
                    {
                        DoCastSelf(SPELL_FIRE_NOVA, true);
                    });

                scheduler.Schedule(12s, [this](TaskContext /*follow*/)
                    {
                        SetBotsToFollow();
                        _amzSpots.clear();
                    });

                ctx.Repeat(25s, 32s);
            });
    }

    // Dumb bullshit
    void MoveInLineOfSight(Unit* who) override
    {
        if (!me->IsAlive())
            return;

        if (!me->IsInCombat())
            return;

        BossAI::MoveInLineOfSight(who);
    }

    void UpdateAI(uint32 diff) override
    {
        // Lots of bullshit
        if (!me->IsInCombat())
        {
            if (!me->HasAura(SPELL_EVOCATION))
            DoCastSelf(SPELL_EVOCATION);
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            return;
        }

        // Leash reset
        if (me->GetExactDist2d(KELIDAN_CENTER.GetPositionX(), KELIDAN_CENTER.GetPositionY()) > KELIDAN_LEASH_RADIUS)
        {
            EnterEvadeMode(EVADE_REASON_BOUNDARY);
            return;
        }

        // More bullshit
        if (!AnyAliveRaidMembersOnThreat())
        {
            EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
            return;
        }

        if (!UpdateVictim())
            return;

        scheduler.Update(diff, [this] { DoMeleeAttackIfReady(); });
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (urand(0, 1))
            Talk(SAY_KILL);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_CHANNELER_DIED)
        {
            if (!me->FindNearestCreature(NPC_SHADOWMOON_CHANNELER, 100.0f))
            {
                me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                me->SetReactState(REACT_AGGRESSIVE);
                me->SetInCombatWithZone();
            }
        }
        else if (param == ACTION_CHANNELER_AGGRO)
        {
            Talk(SAY_ADD_AGGRO);
        }
    }

    void ApplyImmunities(bool apply)
    {
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_CHARM, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISORIENTED, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISTRACT, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_ROOT, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SILENCE, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SLEEP, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SNARE, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_STUN, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FREEZE, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_KNOCKOUT, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_BANISH, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SHACKLE, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DAZE, apply);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SAPPED, apply);
    }

    // --- Helpers: AMZ & bots ---
    void SpawnAntiMagicZones()
    {
        _amzSpots.clear();

        int count = IsHeroic() ? 1 : 3;
        float base = frand(0.f, 2.0f * PI);
        for (int i = 0; i < count; ++i)
        {
            float angle = base + (count > 1 ? i * (2.0f * PI / count) : 0.0f);
            float radius = frand(28.0f, 40.0f);

            float x = me->GetPositionX() + radius * std::cos(angle);
            float y = me->GetPositionY() + radius * std::sin(angle);
            float z = me->GetPositionZ();
            me->UpdateGroundPositionZ(x, y, z);

            me->CastSpell(x, y, z, SPELL_ANTI_MAGIC_ZONE, true);

            Position p;
            p.m_positionX = x;
            p.m_positionY = y;
            p.m_positionZ = z;
            p.m_orientation = 0.f;
            _amzSpots.push_back(p);
        }
    }

    void MoveBotsIntoAMZ()
    {
        if (_amzSpots.empty())
            return;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive() || u->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* c = u->ToCreature();
            if (!c || !c->IsNPCBot())
                continue;

            float best = std::numeric_limits<float>::max();
            Position bestPos;
            for (auto const& p : _amzSpots)
            {
                float d = c->GetExactDist(p.GetPositionX(), p.GetPositionY(), p.GetPositionZ());
                if (d < best)
                {
                    best = d;
                    bestPos = p;
                }
            }

            float jitter = frand(0.5f, 2.0f);
            float ang = frand(0.f, 2.f * PI);
            bestPos.m_positionX += std::cos(ang) * jitter;
            bestPos.m_positionY += std::sin(ang) * jitter;

            if (c->IsNonMeleeSpellCast(false, false, false, false, false))
                c->InterruptNonMeleeSpells(false, 0, true, false);

            if (bot_ai* ai = c->GetBotAI())
                ai->MoveToSendPosition(bestPos);
        }
    }

    void SetBotsToFollow()
    {
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive() || u->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* c = u->ToCreature();
            if (!c || !c->IsNPCBot())
                continue;

            if (bot_ai* ai = c->GetBotAI())
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }
    }
};

void AddSC_boss_kelidan_the_breaker()
{
    RegisterBloodFurnaceCreatureAI(boss_kelidan_the_breaker);
}
