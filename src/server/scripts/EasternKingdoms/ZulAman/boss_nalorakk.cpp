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

#include "CellImpl.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "zulaman.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

enum Spells
{
    SPELL_BERSERK           = 45078,

    // Troll form
    SPELL_BRUTALSWIPE       = 42384,
    SPELL_MANGLE            = 42389,
    SPELL_MANGLEEFFECT      = 44955,
    SPELL_SURGE             = 42402,
    SPELL_BEARFORM          = 42377,

    // Bear form
    SPELL_LACERATINGSLASH   = 42395,
    SPELL_RENDFLESH         = 42397,
    SPELL_DEAFENINGROAR     = 42398
};

enum Talks
{
    SAY_WAVE1 = 0,
    SAY_WAVE2,
    SAY_WAVE3,
    SAY_WAVE4,
    SAY_AGGRO,
    SAY_SURGE,
    SAY_SHIFTEDTOBEAR,
    SAY_SHIFTEDTOTROLL,
    SAY_BERSERK,
    SAY_KILL_ONE,
    SAY_KILL_TWO,
    SAY_DEATH,
    SAY_NALORAKK_EVENT1, // Not implemented
    SAY_NALORAKK_EVENT2, // Not implemented
    SAY_RUN_AWAY,
    EMOTE_BEAR
};

enum Phases
{
    PHASE_SEND_GUARDS_1         = 0,
    PHASE_SEND_GUARDS_2         = 1,
    PHASE_SEND_GUARDS_3         = 2,
    PHASE_SEND_GUARDS_4         = 3,
    PHASE_SEND_GUARDS_5         = 4,
    PHASE_START_COMBAT          = 5
};

enum NalorakkGroups
{
    GROUP_CHECK_DEAD            = 1,
    GROUP_CHECK_EVADE           = 2,
    GROUP_MOVE                  = 3,
    GROUP_BERSERK               = 4,
    GROUP_HUMAN                 = 5,
    GROUP_BEAR                  = 6,
    GROUP_BOT_TANK_SUPPORT      = 10
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

static std::vector<Unit*> GatherEncounterTanksForCreature(Creature const* source, float range = 150.0f)
{
    std::vector<Unit*> tanks;

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
        if (IsEncounterTank(unit))
            tanks.push_back(unit);

    std::stable_sort(tanks.begin(), tanks.end(), [](Unit* left, Unit* right)
    {
        Creature* leftCreature = left ? left->ToCreature() : nullptr;
        Creature* rightCreature = right ? right->ToCreature() : nullptr;
        bot_ai* leftAI = IsValidEncounterNPCBot(leftCreature) ? leftCreature->GetBotAI() : nullptr;
        bot_ai* rightAI = IsValidEncounterNPCBot(rightCreature) ? rightCreature->GetBotAI() : nullptr;

        bool leftOffTank = leftAI && leftAI->IsOffTank();
        bool rightOffTank = rightAI && rightAI->IsOffTank();
        if (leftOffTank != rightOffTank)
            return leftOffTank;

        bool leftBotTank = leftAI && (leftAI->IsTank() || leftAI->IsOffTank() || leftAI->HasRole(BOT_ROLE_TANK));
        bool rightBotTank = rightAI && (rightAI->IsTank() || rightAI->IsOffTank() || rightAI->HasRole(BOT_ROLE_TANK));
        if (leftBotTank != rightBotTank)
            return leftBotTank;

        return left->GetGUID() < right->GetGUID();
    });

    return tanks;
}

static Unit* SelectFarthestEncounterTargetForCreature(Creature* source, float range, bool includeVictim = false)
{
    Unit* best = nullptr;
    float bestDistance = -1.0f;

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
    {
        if (!includeVictim && unit == source->GetVictim())
            continue;

        float distance = source->GetDistance(unit);
        if (!best || distance > bestDistance)
        {
            best = unit;
            bestDistance = distance;
        }
    }

    if (!best && includeVictim && source->GetVictim() && source->IsWithinDistInMap(source->GetVictim(), range))
        return source->GetVictim();

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

struct boss_nalorakk : public BossAI
{
    boss_nalorakk(Creature* creature) : BossAI(creature, DATA_NALORAKK)
    {
        _phase = PHASE_SEND_GUARDS_1;
        _ranIntro = false;
        _active = true;
        creature->SetReactState(REACT_PASSIVE);
        me->SetImmuneToAll(true);
    }

    void Reset() override
    {
        BossAI::Reset();
        scheduler.CancelGroup(GROUP_BOT_TANK_SUPPORT);
        _waveList.clear();
        _introScheduler.CancelAll();
        if (_bearForm)
        {
            me->RemoveAurasDueToSpell(SPELL_BEARFORM);
            _bearForm = false;
        }
        if (_ranIntro)
        {
            _phase = PHASE_START_COMBAT;
            me->SetReactState(REACT_AGGRESSIVE);
            _active = false;
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (who->IsPlayer() && !who->ToPlayer()->IsGameMaster() && _phase < PHASE_START_COMBAT && _active)
        {
            _active = false;
            switch (_phase)
            {
                case PHASE_SEND_GUARDS_1:
                    me->GetCreaturesWithEntryInRange(_waveList, 10.0f, NPC_AMANISHI_AXE_THROWER);
                    me->GetCreaturesWithEntryInRange(_waveList, 10.0f, NPC_AMANISHI_TRIBESMAN);
                    GroupedAttack(_waveList, who);
                    Talk(SAY_WAVE1);
                    _introScheduler.Schedule(5s, GROUP_CHECK_DEAD, [this](TaskContext context)
                    {
                        if (CheckFullyDeadGroup(_waveList))
                            if (_phase == PHASE_SEND_GUARDS_1)
                            {
                                _introScheduler.CancelGroup(GROUP_CHECK_DEAD);
                                _waveList.clear();
                                me->GetMotionMaster()->MoveWaypoint(me->GetEntry()*100+1, false);
                                Talk(SAY_RUN_AWAY);
                                _introScheduler.Schedule(5s, [this](TaskContext)
                                {
                                    me->SetFacingTo(6.25f);
                                    _active = true;
                                });
                                _phase = PHASE_SEND_GUARDS_2;
                            }
                        context.Repeat(5s);
                    });
                    break;
                case PHASE_SEND_GUARDS_2:
                    me->GetCreaturesWithEntryInRange(_waveList, 10.0f, NPC_AMANISHI_AXE_THROWER);
                    me->GetCreaturesWithEntryInRange(_waveList, 10.0f, NPC_AMANISHI_TRIBESMAN);
                    me->GetCreaturesWithEntryInRange(_waveList, 10.0f, NPC_AMANISHI_MEDICINE_MAN);
                    GroupedAttack(_waveList, who);
                    Talk(SAY_WAVE2);
                    _introScheduler.Schedule(5s, GROUP_CHECK_DEAD, [this](TaskContext context)
                    {
                        if (CheckFullyDeadGroup(_waveList))
                            if (_phase == PHASE_SEND_GUARDS_2)
                            {
                                _introScheduler.CancelGroup(GROUP_CHECK_DEAD);
                                _waveList.clear();
                                Talk(SAY_RUN_AWAY);
                                me->GetMotionMaster()->MoveWaypoint(me->GetEntry()*100+2, false);
                                _introScheduler.Schedule(6s, [this](TaskContext)
                                {
                                    me->SetFacingTo(1.54f);
                                    _active = true;
                                });
                                _phase = PHASE_SEND_GUARDS_3;
                            }
                        context.Repeat(5s);
                    });
                    break;
                case PHASE_SEND_GUARDS_3:
                    me->GetCreaturesWithEntryInRange(_waveList, 10.0f, NPC_AMANISHI_WARBRINGER);
                    GroupedAttack(_waveList, who);
                    Talk(SAY_WAVE3);
                    _introScheduler.Schedule(5s, GROUP_CHECK_DEAD, [this](TaskContext context)
                    {
                        if (CheckFullyDeadGroup(_waveList))
                            if (_phase == PHASE_SEND_GUARDS_3)
                            {
                                _introScheduler.CancelGroup(GROUP_CHECK_DEAD);
                                _waveList.clear();
                                Talk(SAY_RUN_AWAY);
                                me->GetMotionMaster()->MoveWaypoint(me->GetEntry() * 100 + 3, false);
                                _introScheduler.Schedule(6s, [this](TaskContext)
                                {
                                    me->SetFacingTo(1.54f);
                                    _active = true;
                                });
                                _phase = PHASE_SEND_GUARDS_4;
                            }
                        context.Repeat(5s);
                    });
                    break;
                case PHASE_SEND_GUARDS_4:
                    me->GetCreaturesWithEntryInRange(_waveList, 25.0f, NPC_AMANISHI_WARBRINGER);
                    me->GetCreaturesWithEntryInRange(_waveList, 25.0f, NPC_AMANISHI_MEDICINE_MAN);
                    GroupedAttack(_waveList, who);
                    Talk(SAY_WAVE4);
                    _waveList.clear();
                    _phase = PHASE_START_COMBAT;
                    _ranIntro = true;
                    me->SetImmuneToAll(false);
                    me->SetReactState(REACT_AGGRESSIVE);
                    me->SetHomePosition(me->GetPosition());
                    break;
            }
            _introScheduler.Schedule(10s, GROUP_CHECK_EVADE, [this](TaskContext context)
            {
                if (CheckAnyEvadeGroup(_waveList))
                {
                    _introScheduler.CancelGroup(GROUP_CHECK_DEAD);
                    _introScheduler.Schedule(5s, GROUP_CHECK_EVADE, [this](TaskContext context)
                    {
                        for (Creature* member : _waveList)
                            if (member->isMoving())
                            {
                                context.Repeat(1s);
                                return;
                            }
                        _active = true;
                    });
                }
                else
                    context.Repeat(10s);
            });
        }
        BossAI::MoveInLineOfSight(who);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        scheduler.Schedule(2s, GROUP_BOT_TANK_SUPPORT, [this](TaskContext context)
        {
            BrutalSwipeTankSupportTick();
            context.Repeat(2s);
        });

        scheduler.Schedule(15s, 20s, GROUP_HUMAN, [this](TaskContext context)
        {
            Talk(SAY_SURGE);
            CastSurge();
            context.Repeat();
        }).Schedule(15s, 25s, GROUP_HUMAN, [this](TaskContext context)
        {
            DoCastVictim(SPELL_BRUTALSWIPE);
            context.Repeat();
        }).Schedule(6s, 34s, GROUP_HUMAN, [this](TaskContext context)
        {
            if (me->GetVictim() && !me->GetVictim()->HasAura(SPELL_MANGLE))
            {
                DoCastVictim(SPELL_MANGLE);
                context.Repeat(1s);
            }
            else
                context.Repeat();
        }).Schedule(10min, GROUP_BERSERK, [this](TaskContext)
        {
            Talk(SAY_BERSERK);
            DoCastSelf(SPELL_BERSERK, true);
        }).Schedule(45s, 50s, GROUP_HUMAN, [this](TaskContext)
        {
            ShapeShift(_bearForm);
        });
    }

    void ShapeShift(bool currentlyInBearForm)
    {
        if (currentlyInBearForm)
        {
            Talk(SAY_SHIFTEDTOTROLL);
            scheduler.CancelGroup(GROUP_BEAR);
            _bearForm = false;

            me->SetCanDualWield(true);

            scheduler.Schedule(15s, 20s, GROUP_HUMAN, [this](TaskContext context)
            {
                Talk(SAY_SURGE);
                CastSurge();
                context.Repeat();
            }).Schedule(15s, 25s, GROUP_HUMAN, [this](TaskContext context)
            {
                DoCastVictim(SPELL_BRUTALSWIPE);
                context.Repeat();
            }).Schedule(6s, 34s, GROUP_HUMAN, [this](TaskContext context)
            {
                DoCastVictim(SPELL_MANGLE);
                context.Repeat();
            }).Schedule(10min, GROUP_BERSERK, [this](TaskContext)
            {
                Talk(SAY_BERSERK);
                DoCastSelf(SPELL_BERSERK, true);
            }).Schedule(45s, 50s, GROUP_HUMAN, [this](TaskContext)
            {
                ShapeShift(_bearForm);
            });
        }
        else
        {
            Talk(SAY_SHIFTEDTOBEAR);
            Talk(EMOTE_BEAR);
            DoCastSelf(SPELL_BEARFORM, true);
            scheduler.CancelGroup(GROUP_HUMAN);
            _bearForm = true;

            me->SetCanDualWield(false);

            scheduler.Schedule(4s, 26s, GROUP_BEAR, [this](TaskContext context)
            {
                DoCastVictim(SPELL_LACERATINGSLASH);
                context.Repeat(4s, 26s);
            }).Schedule(6s, 21s, GROUP_BEAR, [this](TaskContext context)
            {
                DoCastVictim(SPELL_RENDFLESH);
                context.Repeat(6s, 21s);
            }).Schedule(11s, 24s, GROUP_BEAR, [this](TaskContext context)
            {
                DoCastSelf(SPELL_DEAFENINGROAR);
                context.Repeat(11s, 24s);
            }).Schedule(30s, GROUP_BEAR, [this](TaskContext)
            {
                ShapeShift(_bearForm);
            });
        }
    }

    void GroupedAttack(std::list<Creature* > attackerList, Unit* trigger)
    {
        std::vector<Unit*> tanks = GatherEncounterTanksForCreature(me, 120.0f);
        uint8 index = 0;

        for (Creature* attacker : attackerList)
        {
            attacker->SetInCombatWithZone();
            Unit* tank = !tanks.empty() ? tanks[index++ % tanks.size()] : trigger;
            if (!tank || !tank->IsAlive())
                tank = me->GetVictim();

            if (!tank || !tank->IsAlive())
                continue;

            attacker->AddThreat(tank, 8000.0f);
            attacker->AI()->AttackStart(tank);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _introScheduler.Update(diff);
        BossAI::UpdateAI(diff);
    }

    bool CheckFullyDeadGroup(std::list<Creature*> groupToCheck)
    {
        for (Creature* member : groupToCheck)
        {
            if (member->IsAlive())
            {
                return false;
            }
        }
        return true;
    }

    bool CheckAnyEvadeGroup(std::list<Creature*> groupToCheck)
    {
        for (Creature* member : groupToCheck)
            if (member->IsAlive() && !member->IsInCombat())
                return true;
        return false;
    }

    void JustDied(Unit* killer) override
    {
        scheduler.CancelGroup(GROUP_BOT_TANK_SUPPORT);
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void KilledUnit(Unit* victim) override
    {
        BossAI::KilledUnit(victim);
        if (victim->IsPlayer() || IsNPCBotUnit(victim))
            Talk(urand(0, 1) ? SAY_KILL_ONE : SAY_KILL_TWO);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        scheduler.CancelGroup(GROUP_BOT_TANK_SUPPORT);
        BossAI::EnterEvadeMode(why);
    }

    Unit* SelectFarthestEncounterTarget(float range, bool includeVictim = false) const
    {
        return SelectFarthestEncounterTargetForCreature(me, range, includeVictim);
    }

    void CastSurge()
    {
        Unit* target = SelectFarthestEncounterTarget(45.0f, false);
        if (!target)
            target = SelectFarthestEncounterTarget(45.0f, true);

        if (target)
            DoCast(target, SPELL_SURGE);
    }

    Creature* FindOffTankBotForBrutalSwipe() const
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

    void BrutalSwipeTankSupportTick()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        Creature* offTank = FindOffTankBotForBrutalSwipe();
        if (!offTank)
            return;

        if (offTank->IsWithinDist2d(victim, 4.5f) && me->HasInArc(float(M_PI) / 2.0f, offTank))
            return;

        float angle = me->GetAngle(victim) + 0.35f;
        Position destination;
        destination.Relocate(victim->GetPositionX() + std::cos(angle) * 2.5f, victim->GetPositionY() + std::sin(angle) * 2.5f, victim->GetPositionZ(), offTank->GetOrientation());
        SafeBotMoveTo(offTank, destination);
    }

private:
    uint8 _phase;
    bool _ranIntro;
    bool _active;
    bool _bearForm;
    TaskScheduler _introScheduler;
    std::list<Creature *> _waveList;
};

void AddSC_boss_nalorakk()
{
    RegisterZulAmanCreatureAI(boss_nalorakk);
}
