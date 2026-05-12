/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or at your option any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "sethekk_halls.h"
#include "SpellMgr.h"

#include "bot_ai.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    static Position const IKISS_HOME_POSITION = { 44.50f, 286.85f, 25.06f, 0.0f };

    static std::array<Position, 4> const IKISS_AMZ_POSITIONS =
    {
        Position { 15.73f, 286.92f, 26.69f, 0.0f },
        Position { 73.76f, 287.01f, 26.63f, 0.0f },
        Position { 45.04f, 318.55f, 26.62f, 0.0f },
        Position { 45.07f, 257.82f, 26.65f, 0.0f }
    };

    static constexpr float PI = 3.14159265358979323846f;
}

enum Text
{
    SAY_INTRO = 0,
    SAY_AGGRO = 1,
    SAY_SLAY = 2,
    SAY_DEATH = 3,
    EMOTE_ARCANE_EXP = 4
};

enum Spells
{
    SPELL_BLINK = 38194,
    SPELL_BLINK_TELEPORT = 38203,
    SPELL_MANA_SHIELD = 38151,
    SPELL_ARCANE_BUBBLE = 9438,
    SPELL_SLOW = 35032,
    SPELL_POLYMORPH = 38245,
    SPELL_ARCANE_VOLLEY = 35059,
    SPELL_ARCANE_EXPLOSION = 38197,

    SPELL_TELEPORT_VISUAL = 64446,
    SPELL_ANTI_MAGIC_ZONE = 300379
};

struct boss_talon_king_ikiss : public BossAI
{
    boss_talon_king_ikiss(Creature* creature) :
        BossAI(creature, DATA_IKISS),
        _spoken(false),
        _arcaneExplosionPhase(false)
    {
    }

    void Reset() override
    {
        _Reset();

        _arcaneExplosionPhase = false;
        _amzSpots.clear();

        me->SetHomePosition(IKISS_HOME_POSITION);
        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveAurasDueToSpell(SPELL_ARCANE_BUBBLE);

        ScheduleHealthCheckEvent({ 80, 50, 25 }, [&]
            {
                StartArcaneExplosionPhase();
            });

        ScheduleHealthCheckEvent(20, [&]
            {
                if (!_arcaneExplosionPhase)
                    DoCastSelf(SPELL_MANA_SHIELD);
            });
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!_spoken && who->IsPlayer())
        {
            Talk(SAY_INTRO);
            _spoken = true;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(5s, [this](TaskContext context)
            {
                if (_arcaneExplosionPhase)
                {
                    context.Repeat(1s);
                    return;
                }

                DoCastAOE(SPELL_ARCANE_VOLLEY);
                context.Repeat(7s, 12s);
            });

        scheduler.Schedule(8s, [this](TaskContext context)
            {
                if (_arcaneExplosionPhase)
                {
                    context.Repeat(1s);
                    return;
                }

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_POLYMORPH);
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, [&](Unit* target) -> bool
                    {
                        return target && !target->IsImmunedToSpell(spellInfo) && target != me->GetThreatMgr().GetCurrentVictim();
                    }))
                {
                    DoCast(target, SPELL_POLYMORPH);
                }

                context.Repeat(15s, 17500ms);
            });

        if (IsHeroic())
        {
            scheduler.Schedule(15s, 25s, [this](TaskContext context)
                {
                    if (_arcaneExplosionPhase)
                    {
                        context.Repeat(1s);
                        return;
                    }

                    DoCastAOE(SPELL_SLOW);
                    context.Repeat(15s, 30s);
                });
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);

        _arcaneExplosionPhase = false;
        _amzSpots.clear();
        me->RemoveAurasDueToSpell(SPELL_ARCANE_BUBBLE);

        if (GameObject* coffer = instance->GetGameObject(DATA_GO_TALON_KING_COFFER))
            coffer->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE | GO_FLAG_INTERACT_COND);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && urand(0, 1))
            Talk(SAY_SLAY);
    }

private:
    bool _spoken;
    bool _arcaneExplosionPhase;
    std::vector<Position> _amzSpots;

    bool IsSpecialPhaseTarget(Unit* unit) const
    {
        if (!unit || !unit->IsAlive() || unit == me)
            return false;

        if (unit->GetTypeId() == TYPEID_PLAYER)
            return true;

        if (unit->GetTypeId() == TYPEID_UNIT)
        {
            if (Creature* creature = unit->ToCreature())
                return creature->IsNPCBot();
        }

        return false;
    }

    void StartArcaneExplosionPhase()
    {
        if (_arcaneExplosionPhase || !me->IsAlive())
            return;

        _arcaneExplosionPhase = true;
        _amzSpots.clear();

        me->InterruptNonMeleeSpells(false);
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->StopMoving();
        me->GetMotionMaster()->Clear();

        DoCastSelf(SPELL_TELEPORT_VISUAL, true);

        TeleportBossHome();
        PullSpecialPhaseTargetsToBoss();

        SpawnAntiMagicZones();
        MoveBotsIntoAMZ();

        Talk(EMOTE_ARCANE_EXP);

        // This is intentionally immediate.
        // Bubble is triggered, then Ikiss starts the real Arcane Explosion cast right away.
        // The spell's own cast time is the escape window.
        DoCastSelf(SPELL_ARCANE_BUBBLE, true);
        DoCastAOE(SPELL_ARCANE_EXPLOSION);

        // Cleanup only. This does not delay the cast.
        scheduler.Schedule(6500ms, [this](TaskContext /*context*/)
            {
                if (!me->IsAlive())
                    return;

                _arcaneExplosionPhase = false;
                _amzSpots.clear();

                me->RemoveAurasDueToSpell(SPELL_ARCANE_BUBBLE);
                me->SetReactState(REACT_AGGRESSIVE);
                me->GetThreatMgr().ResetAllThreat();

                SetBotsToFollow();
            });
    }

    void TeleportBossHome()
    {
        Position const& home = me->GetHomePosition();

        me->NearTeleportTo(
            home.GetPositionX(),
            home.GetPositionY(),
            home.GetPositionZ(),
            home.GetOrientation());
    }

    void PullSpecialPhaseTargetsToBoss()
    {
        std::vector<Unit*> targets;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* unit = ref->GetVictim();
            if (!IsSpecialPhaseTarget(unit))
                continue;

            targets.push_back(unit);
        }

        for (Unit* unit : targets)
        {
            if (!unit || !unit->IsAlive())
                continue;

            float angle = frand(0.0f, 2.0f * PI);
            float radius = frand(1.5f, 3.5f);
            float x = me->GetPositionX() + std::cos(angle) * radius;
            float y = me->GetPositionY() + std::sin(angle) * radius;
            float z = me->GetPositionZ();

            me->UpdateGroundPositionZ(x, y, z);

            unit->InterruptNonMeleeSpells(false, 0, true, false);
            unit->NearTeleportTo(x, y, z, unit->GetOrientation());
        }
    }

    void SpawnAntiMagicZones()
    {
        _amzSpots.clear();

        std::array<uint8, 4> indices = { 0, 1, 2, 3 };

        uint8 firstSwap = urand(0, 3);
        std::swap(indices[0], indices[firstSwap]);

        uint8 secondSwap = urand(1, 3);
        std::swap(indices[1], indices[secondSwap]);

        for (uint8 i = 0; i < 2; ++i)
        {
            Position pos = IKISS_AMZ_POSITIONS[indices[i]];

            float z = pos.GetPositionZ();
            me->UpdateGroundPositionZ(pos.GetPositionX(), pos.GetPositionY(), z);
            pos.m_positionZ = z;

            me->CastSpell(
                pos.GetPositionX(),
                pos.GetPositionY(),
                pos.GetPositionZ(),
                SPELL_ANTI_MAGIC_ZONE,
                true);

            _amzSpots.push_back(pos);
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

            Unit* unit = ref->GetVictim();
            if (!unit || !unit->IsAlive() || unit->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsNPCBot())
                continue;

            float bestDistance = std::numeric_limits<float>::max();
            Position bestPosition;

            for (Position const& pos : _amzSpots)
            {
                float distance = creature->GetExactDist(
                    pos.GetPositionX(),
                    pos.GetPositionY(),
                    pos.GetPositionZ());

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestPosition = pos;
                }
            }

            float jitter = frand(0.5f, 2.0f);
            float angle = frand(0.0f, 2.0f * PI);

            bestPosition.m_positionX += std::cos(angle) * jitter;
            bestPosition.m_positionY += std::sin(angle) * jitter;

            if (creature->IsNonMeleeSpellCast(false, false, false, false, false))
                creature->InterruptNonMeleeSpells(false, 0, true, false);

            if (bot_ai* ai = creature->GetBotAI())
                ai->MoveToSendPosition(bestPosition);
        }
    }

    void SetBotsToFollow()
    {
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* unit = ref->GetVictim();
            if (!unit || !unit->IsAlive() || unit->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsNPCBot())
                continue;

            if (bot_ai* ai = creature->GetBotAI())
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }
    }
};

// 38194 - Blink
class spell_talon_king_ikiss_blink : public SpellScript
{
    PrepareSpellScript(spell_talon_king_ikiss_blink);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return sSpellMgr->GetSpellInfo(SPELL_BLINK);
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        uint8 maxSize = 1;
        if (targets.size() > maxSize)
            Acore::Containers::RandomResize(targets, maxSize);
    }

    void HandleDummyHitTarget(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetCaster()->CastSpell(GetHitUnit(), SPELL_BLINK_TELEPORT, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_talon_king_ikiss_blink::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_talon_king_ikiss_blink::HandleDummyHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_talon_king_ikiss()
{
    RegisterSethekkHallsCreatureAI(boss_talon_king_ikiss);
    RegisterSpellScript(spell_talon_king_ikiss_blink);
}
