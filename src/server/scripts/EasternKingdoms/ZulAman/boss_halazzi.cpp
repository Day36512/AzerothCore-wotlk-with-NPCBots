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
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "zulaman.h"
#include <cmath>
#include <limits>
#include <vector>

enum Spells
{
    SPELL_DUAL_WIELD            = 29651,
    SPELL_SABER_LASH            = 43267,
    SPELL_FRENZY                = 43139,
    SPELL_FLAMESHOCK            = 43303,
    SPELL_EARTHSHOCK            = 43305,
    SPELL_SUMMON_LYNX           = 43143,
    SPELL_SUMMON_TOTEM          = 43302,
    SPELL_BERSERK               = 45078,
    SPELL_LYNX_FRENZY           = 43290, // Used by Spirit Lynx
    SPELL_SHRED_ARMOR           = 43243, // Used by Spirit Lynx
    SPELL_TRANSFORM_DUMMY       = 43615, // Used by Spirit Lynx

    SPELL_TRANSFIGURE           = 44054,
    SPELL_TRANSFORM_TO_LYNX_75  = 43145,
    SPELL_TRANSFORM_TO_LYNX_50  = 43271,
    SPELL_TRANSFORM_TO_LYNX_25  = 43272
};

enum UniqueEvents
{
    EVENT_BERSERK                = 1
};

enum Hal_CreatureIds
{
    NPC_HALAZZI_TROLL            = 24144, // dummy creature - used to update model, stats
    NPC_TOTEM                    = 24224
};

enum PhaseHalazzi
{
    PHASE_NONE                   = 0,
    PHASE_LYNX                   = 1,
    PHASE_HUMAN                  = 2,
    PHASE_MERGE                  = 3,
    PHASE_ENRAGE                 = 4
};

enum Yells
{
    SAY_AGGRO                    = 0,
    SAY_KILL                     = 1,
    SAY_SABER                    = 2,
    SAY_SPLIT                    = 3,
    SAY_MERGE                    = 4,
    SAY_DEATH                    = 5
};

enum Groups
{
    GROUP_LYNX                   = 0,
    GROUP_HUMAN                  = 1,
    GROUP_MERGE                  = 3,
    GROUP_SPLIT                  = 4,
    GROUP_BOT_HALAZZI_POSITIONING = 10
};

enum Actions
{
    ACTION_MERGE                 = 0
};

static bool IsNPCBotUnit(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && creature->IsNPCBot() && creature->GetBotAI();
}

static bool IsValidEncounterNPCBot(Creature* creature)
{
    return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
}

static bool IsEncounterParticipantFor(Creature const* source, Unit* unit, float range)
{
    if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
        return false;

    if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(source, range))
        return false;

    if (unit->IsPlayer())
        return true;

    return IsValidEncounterNPCBot(unit->ToCreature());
}

static std::vector<Unit*> GatherEncounterUnitsForCreature(Creature const* source, float range = 150.0f)
{
    std::vector<Unit*> units;
    GuidSet seen;

    auto addUnit = [&](Unit* unit)
    {
        if (!unit || seen.count(unit->GetGUID()) || !IsEncounterParticipantFor(source, unit, range))
            return;

        seen.insert(unit->GetGUID());
        units.push_back(unit);
    };

    if (!source || !source->GetMap())
        return units;

    Map::PlayerList const& players = source->GetMap()->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* player = itr->GetSource();
        if (!player)
            continue;

        addUnit(player);

        if (Group* group = player->GetGroup())
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
                addUnit(member);

        if (BotMgr* botMgr = player->GetBotMgr())
            if (BotMap const* botMap = botMgr->GetBotMap())
                for (BotMap::value_type const& pair : *botMap)
                    addUnit(pair.second);
    }

    return units;
}

static std::vector<Creature*> GatherEncounterBotsForCreature(Creature const* source, float range = 150.0f)
{
    std::vector<Creature*> bots;

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
        if (Creature* creature = unit->ToCreature())
            if (IsValidEncounterNPCBot(creature))
                bots.push_back(creature);

    return bots;
}

static Unit* SelectRandomEncounterTargetForCreature(Creature* source, float range, bool includeVictim = true)
{
    std::vector<Unit*> candidates;

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
    {
        if (!includeVictim && unit == source->GetVictim())
            continue;

        candidates.push_back(unit);
    }

    if (candidates.empty() && includeVictim && source->GetVictim() && source->IsWithinDistInMap(source->GetVictim(), range))
        return source->GetVictim();

    if (candidates.empty())
        return nullptr;

    return Acore::Containers::SelectRandomContainerElement(candidates);
}

static bool IsPlayerMarkedTank(Player* player)
{
    if (!player)
        return false;

    Group* group = player->GetGroup();
    if (!group)
        return false;

    Group::MemberSlotList const& slots = group->GetMemberSlots();
    for (Group::member_citerator itr = slots.begin(); itr != slots.end(); ++itr)
        if (itr->guid == player->GetGUID())
            return itr->flags & (MEMBER_FLAG_MAINTANK | MEMBER_FLAG_MAINASSIST);

    return false;
}

static bool IsEncounterTank(Unit* unit)
{
    if (!unit)
        return false;

    if (Player* player = unit->ToPlayer())
        return IsPlayerMarkedTank(player);

    Creature* bot = unit->ToCreature();
    bot_ai* ai = IsValidEncounterNPCBot(bot) ? bot->GetBotAI() : nullptr;
    return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK));
}

static Unit* SelectPreferredEncounterTankForCreature(Creature* source, float range = 150.0f)
{
    Unit* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
    {
        if (!IsEncounterTank(unit))
            continue;

        float score = unit->GetDistance(source);
        if (Creature* bot = unit->ToCreature())
            if (bot_ai* ai = bot->GetBotAI())
            {
                if (ai->IsOffTank())
                    score -= 30.0f;
                else if (ai->IsTank())
                    score -= 15.0f;
            }

        if (!best || score < bestScore)
        {
            best = unit;
            bestScore = score;
        }
    }

    if (!best && source->GetVictim() && source->GetVictim()->IsAlive())
        best = source->GetVictim();

    return best;
}

static void SafeBotMoveTo(Creature* bot, Position const& destination)
{
    if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
        return;

    bot_ai* ai = bot->GetBotAI();
    if (!ai)
        return;

    float x = destination.GetPositionX();
    float y = destination.GetPositionY();
    float z = destination.GetPositionZ();

    if (!bot->CanFly())
        bot->UpdateAllowedPositionZ(x, y, z);

    Position finalPosition;
    finalPosition.Relocate(x, y, z, destination.GetOrientation());

    ai->MoveToSendPosition(finalPosition);
}

struct boss_halazzi : public BossAI
{
    boss_halazzi(Creature* creature) : BossAI(creature, DATA_HALAZZI)
    {    }

    void Reset() override
    {
        scheduler.CancelGroup(GROUP_BOT_HALAZZI_POSITIONING);
        me->UpdateEntry(NPC_HALAZZI);
        BossAI::Reset();
        _transformCount = 0;
        _phase = PHASE_NONE;
        SetInvincibility(true);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        scheduler.Schedule(2s, GROUP_BOT_HALAZZI_POSITIONING, [this](TaskContext context)
        {
            SaberLashTankSupportTick();
            context.Repeat(2s);
        });

        ScheduleUniqueTimedEvent(10min, [&]
        {
            DoCastSelf(SPELL_BERSERK, true);
        }, EVENT_BERSERK);
        EnterPhase(PHASE_LYNX);
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask) override
    {
        BossAI::DamageTaken(attacker, damage, damagetype, damageSchoolMask);

        if (_phase == PHASE_LYNX)
        {
            uint32 _healthCheckPercentage = 25 * (3 - _transformCount);
            if (me->HealthBelowPctDamaged(_healthCheckPercentage, damage))
                EnterPhase(PHASE_HUMAN);
        }
        else if (_phase == PHASE_HUMAN)
        {
            if (me->HealthBelowPctDamaged(20, damage))
                EnterPhase(PHASE_MERGE);
        }
    }

    void SpellHit(Unit*, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_TRANSFORM_DUMMY)
            me->UpdateEntry(NPC_HALAZZI_TROLL);
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);

        if (summon->GetEntry() == NPC_TOTEM)
            summon->Attack(me->GetVictim(), false);
        else if (summon->GetEntry() == NPC_SPIRIT_LYNX)
            AssignSpiritLynxTank(summon);
    }

    void AttackStart(Unit* who) override
    {
        if (_phase != PHASE_MERGE)
            BossAI::AttackStart(who);
    }

    void EnterPhase(PhaseHalazzi nextPhase)
    {
        _phase = nextPhase;

        switch (nextPhase)
        {
            case PHASE_LYNX:
            {
                summons.DespawnAll();

                if (_transformCount)
                {
                    me->UpdateEntry(NPC_HALAZZI);
                    switch (_transformCount)
                    {
                        case 1:
                            DoCastSelf(SPELL_TRANSFORM_TO_LYNX_75, true);
                            break;
                        case 2:
                            DoCastSelf(SPELL_TRANSFORM_TO_LYNX_50, true);
                            break;
                        case 3:
                            DoCastSelf(SPELL_TRANSFORM_TO_LYNX_25, true);
                            break;
                        default:
                            break;
                    }
                }

                me->ResumeChasingVictim();

                scheduler.CancelGroup(GROUP_MERGE);
                scheduler.Schedule(5s, 15s, GROUP_LYNX, [this](TaskContext context)
                {
                    Talk(SAY_SABER);
                    DoCastVictim(SPELL_SABER_LASH, true);
                    context.Repeat();
                }).Schedule(20s, 35s, GROUP_LYNX, [this](TaskContext context)
                {
                    DoCastSelf(SPELL_FRENZY);
                    context.Repeat();
                });
                break;
            }
            case PHASE_HUMAN:
                Talk(SAY_SPLIT);
                DoCastSelf(SPELL_TRANSFIGURE, true);
                scheduler.Schedule(3s, GROUP_SPLIT, [this](TaskContext /*context*/)
                {
                    DoCastSelf(SPELL_SUMMON_LYNX, true);
                });
                _phase = PHASE_HUMAN;

                scheduler.CancelGroup(GROUP_MERGE);
                scheduler.CancelGroup(GROUP_LYNX);
                scheduler.Schedule(10s, GROUP_HUMAN, [this](TaskContext context)
                {
                    if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                    {
                        if (target->IsNonMeleeSpellCast(false))
                            DoCast(target, SPELL_EARTHSHOCK);
                        else
                            DoCast(target, SPELL_FLAMESHOCK);
                    }
                    context.Repeat(10s, 15s);
                }).Schedule(12s, GROUP_HUMAN, [this](TaskContext context)
                {
                    DoCastSelf(SPELL_SUMMON_TOTEM);
                    context.Repeat(20s);
                });
                break;
            case PHASE_MERGE:
                if (Creature* lynx = instance->GetCreature(DATA_SPIRIT_LYNX))
                {
                    Talk(SAY_MERGE);
                    scheduler.CancelGroup(GROUP_HUMAN);
                    lynx->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    lynx->GetMotionMaster()->Clear();
                    lynx->GetMotionMaster()->MoveFollow(me, 0, 0);
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveFollow(lynx, 0, 0);
                    ++_transformCount;
                    scheduler.Schedule(2s, GROUP_MERGE, [this, lynx](TaskContext context)
                    {
                        if (lynx)
                        {
                            if (me->IsWithinDistInMap(lynx, 6.0f))
                            {
                                EnterPhase(PHASE_LYNX);

                                // Enrage phase
                                if (_transformCount == 3)
                                {
                                    _phase = PHASE_ENRAGE;
                                    SetInvincibility(false);
                                    scheduler.Schedule(12s, GROUP_LYNX, [this](TaskContext context)
                                    {
                                        DoCastSelf(SPELL_SUMMON_TOTEM);
                                        context.Repeat(20s);
                                    });
                                }
                            }
                            else
                                context.Repeat(2s);
                        }
                    });
                }
                break;
            default:
                break;
        }
    }

    void KilledUnit(Unit* victim) override
    {
        BossAI::KilledUnit(victim);
        if (victim->IsPlayer() || IsNPCBotUnit(victim))
            Talk(SAY_KILL);
    }

    void DoAction(int32 actionId) override
    {
        if (actionId == ACTION_MERGE)
            EnterPhase(PHASE_MERGE);
    }

    void JustDied(Unit* killer) override
    {
        scheduler.CancelGroup(GROUP_BOT_HALAZZI_POSITIONING);
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        scheduler.CancelGroup(GROUP_BOT_HALAZZI_POSITIONING);
        BossAI::EnterEvadeMode(why);
    }

    Unit* SelectRandomEncounterTarget(float range, bool includeVictim = true) const
    {
        return SelectRandomEncounterTargetForCreature(me, range, includeVictim);
    }

    Creature* FindOffTankBotForSaberLash() const
    {
        Unit* victim = me->GetVictim();
        Creature* best = nullptr;
        float bestScore = std::numeric_limits<float>::max();

        for (Creature* bot : GatherEncounterBotsForCreature(me, 80.0f))
        {
            if (!bot || bot == victim || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai || !(ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK)))
                continue;

            float score = bot->GetDistance(me);
            if (ai->IsOffTank())
                score -= 20.0f;

            if (!best || score < bestScore)
            {
                best = bot;
                bestScore = score;
            }
        }

        return best;
    }

    void SaberLashTankSupportTick()
    {
        if (_phase == PHASE_MERGE || !me->GetVictim())
            return;

        Creature* offTank = FindOffTankBotForSaberLash();
        if (!offTank)
            return;

        Unit* victim = me->GetVictim();
        if (offTank->IsWithinDist2d(victim, 4.5f) && me->HasInArc(float(M_PI) / 2.0f, offTank))
            return;

        float angle = me->GetAngle(victim) + 0.35f;
        Position destination;
        destination.Relocate(victim->GetPositionX() + std::cos(angle) * 2.5f, victim->GetPositionY() + std::sin(angle) * 2.5f, victim->GetPositionZ(), offTank->GetOrientation());
        SafeBotMoveTo(offTank, destination);
    }

    void AssignSpiritLynxTank(Creature* lynx)
    {
        if (!lynx || !lynx->IsAlive())
            return;

        Unit* tank = SelectPreferredEncounterTankForCreature(me, 120.0f);
        if (!tank || !tank->IsAlive())
            return;

        lynx->SetInCombatWithZone();
        lynx->AddThreat(tank, 10000.0f);
        lynx->AI()->AttackStart(tank);
    }

private:
    uint8 _transformCount;
    PhaseHalazzi _phase;
};

void AddSC_boss_halazzi()
{
    RegisterZulAmanCreatureAI(boss_halazzi);
}
