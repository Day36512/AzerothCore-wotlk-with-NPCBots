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
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "zulaman.h"
#include <cmath>
#include <limits>
#include <vector>

enum Yells
{
    SAY_AGGRO                   = 0,
    SAY_FIRE_BOMBS              = 1,
    SAY_SUMMON_HATCHER          = 2,
    SAY_ALL_EGGS                = 3,
    SAY_BERSERK                 = 4,
    SAY_SLAY                    = 5,
    SAY_DEATH                   = 6,
    SAY_EVENT_STRANGERS         = 7,
    SAY_EVENT_FRIENDS           = 8
};

enum Spells
{
    // Jan'alai
    SPELL_FLAME_BREATH          = 43140,
    SPELL_FIRE_WALL             = 43113,
    SPELL_ENRAGE                = 44779,
    SPELL_SUMMON_PLAYERS_DUMMY  = 43096,
    SPELL_SUMMON_PLAYERS        = 43097,
    SPELL_TELE_TO_CENTER        = 43098, // coord
    SPELL_HATCH_ALL             = 43144,
    SPELL_BERSERK               = 45078,

    // Fire Bob Spells
    SPELL_FIRE_BOMB_CHANNEL     = 42621, // last forever
    SPELL_FIRE_BOMB_THROW       = 42628, // throw visual
    SPELL_FIRE_BOMB_DUMMY       = 42629, // bomb visual
    SPELL_FIRE_BOMB_DAMAGE      = 42630,

    // Hatcher Spells
    SPELL_HATCH_EGG_ALL         = 42471,
    SPELL_HATCH_EGG_SINGULAR    = 43734,
    SPELL_SUMMON_HATCHLING      = 42493,

    // Hatchling Spells
    SPELL_FLAMEBUFFET           = 43299
};

enum Creatures
{
    NPC_AMANI_HATCHER           = 23818,
    NPC_EGG                     = 23817,
    NPC_FIRE_BOMB               = 23920
};

const int area_dx = 44;
const int area_dy = 51;

const Position janalainPos = {-33.93f, 1149.27f, 19.0f, 0.0f};

const Position fireWallCoords[4] =
{
    {-10.13f, 1149.27f, 19, 3.1415f},
    {-33.93f, 1123.90f, 19, 0.5f * 3.1415f},
    {-54.80f, 1150.08f, 19, 0.0f},
    {-33.93f, 1175.68f, 19, 1.5f * 3.1415f}
};

const Position hatcherway[2][5] =
{
    {
        {-87.46f, 1170.09f, 6.0f, 0.0f},
        {-74.41f, 1154.75f, 6.0f, 0.0f},
        {-52.74f, 1153.32f, 19.0f, 0.0f},
        {-33.37f, 1172.46f, 19.0f, 0.0f},
        {-33.09f, 1203.87f, 19.0f, 0.0f}
    },
    {
        {-86.57f, 1132.85f, 6.0f, 0.0f},
        {-73.94f, 1146.00f, 6.0f, 0.0f},
        {-52.29f, 1146.51f, 19.0f, 0.0f},
        {-33.57f, 1125.72f, 19.0f, 0.0f},
        {-34.29f, 1095.22f, 19.0f, 0.0f}
    }
};

enum HatchActions
{
    HATCH_RESET = 0,
    HATCH_ALL   = 1
};

enum Misc
{
    MAX_BOMB_COUNT              = 40,
    GROUP_ENRAGE                = 1,
    GROUP_HATCHING              = 2,
    GROUP_BOT_BOMB_DIRECTOR     = 10,
    DATA_ALL_EGGS_HATCHED       = 0
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

static bool IsBotCastingUninterruptible(Creature* bot)
{
    if (!bot)
        return false;

    if (Spell* spell = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (SpellInfo const* spellInfo = spell->GetSpellInfo())
            if (!(spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT))
                return true;

    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        if (SpellInfo const* spellInfo = spell->GetSpellInfo())
            if (!(spellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT))
                return true;

    return false;
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

struct boss_janalai : public BossAI
{
    boss_janalai(Creature* creature) : BossAI(creature, DATA_JANALAI)
    {    }

    void Reset() override
    {
        StopBotBombAvoidance();
        BossAI::Reset();
        HatchAllEggs(HATCH_RESET);
        _isBombing = false;
        _isFlameBreathing = false;

        ScheduleHealthCheckEvent(35, [&]{
            Talk(SAY_ALL_EGGS);
            me->AttackStop();
            me->GetMotionMaster()->Clear();
            me->SetPosition(janalainPos);
            me->StopMovingOnCurrentPos();
            DoCastAOE(SPELL_HATCH_ALL);
        });

        ScheduleHealthCheckEvent(20, [&] {
            if (!me->HasAura(SPELL_ENRAGE))
                DoCastSelf(SPELL_ENRAGE, true);
            me->m_Events.CancelEventGroup(GROUP_ENRAGE);
        });

        me->m_Events.KillAllEvents(false);
        _sideHatched[0] = false;
        _sideHatched[1] = false;
    }

    void JustDied(Unit* killer) override
    {
        StopBotBombAvoidance();
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void KilledUnit(Unit* victim) override
    {
        BossAI::KilledUnit(victim);
        if (victim->IsPlayer() || IsNPCBotUnit(victim))
            Talk(SAY_SLAY);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        StopBotBombAvoidance();
        BossAI::EnterEvadeMode(why);
    }

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() == NPC_AMANI_HATCHLING)
        {
            if (summon->GetPositionY() > 1150)
                summon->GetMotionMaster()->MovePoint(0, hatcherway[0][3].GetPositionX() + rand() % 4 - 2, 1150.0f + rand() % 4 - 2, hatcherway[0][3].GetPositionY());
            else
                summon->GetMotionMaster()->MovePoint(0, hatcherway[1][3].GetPositionX() + rand() % 4 - 2, 1150.0f + rand() % 4 - 2, hatcherway[1][3].GetPositionY());

            if (Unit* tank = SelectPreferredEncounterTankForCreature(me, 120.0f))
            {
                summon->SetInCombatWithZone();
                summon->AddThreat(tank, 8000.0f);
                summon->AI()->AttackStart(tank);
            }
        }

        BossAI::JustSummoned(summon);
    }

    void DamageDealt(Unit* target, uint32& damage, DamageEffectType /*damagetype*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (_isFlameBreathing)
        {
            if (!me->HasInArc(M_PI / 6, target))
                damage = 0;
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);

        //schedule abilities
        ScheduleTimedEvent(30s, [&]{
            StartBombing();
        }, 20s, 40s);

        scheduler.Schedule(10s, GROUP_HATCHING, [this](TaskContext context)
        {
            if (_sideHatched[0] && _sideHatched[1])
                return;

            Talk(SAY_SUMMON_HATCHER);

            if (_sideHatched[0] && !_sideHatched[1])
            {
                me->SummonCreature(NPC_AMANI_HATCHER, hatcherway[1][0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
                me->SummonCreature(NPC_AMANI_HATCHER, hatcherway[1][0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
            }
            else if (!_sideHatched[0] && _sideHatched[1])
            {
                me->SummonCreature(NPC_AMANI_HATCHER, hatcherway[0][0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
                me->SummonCreature(NPC_AMANI_HATCHER, hatcherway[0][0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
            }
            else
            {
                me->SummonCreature(NPC_AMANI_HATCHER, hatcherway[0][0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
                me->SummonCreature(NPC_AMANI_HATCHER, hatcherway[1][0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
            }

            context.Repeat(90s);
        });

        ScheduleTimedEvent(8s, [&]{
            if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
            {
                me->AttackStop();
                me->GetMotionMaster()->Clear();
                DoCast(target, SPELL_FLAME_BREATH);
                me->StopMoving();
                _isFlameBreathing = true;
                // placeholder time idk yet
                scheduler.Schedule(2s, [this](TaskContext)
                {
                    _isFlameBreathing = false;
                });
            }
        }, 8s);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_ENRAGE, true);
        }, 5min, 5min, GROUP_ENRAGE);

        me->m_Events.AddEventAtOffset([&] {
            Talk(SAY_BERSERK);
            DoCastSelf(SPELL_BERSERK);
        }, 10min);
    }

    void SetData(uint32 index, uint32 data) override
    {
        if (index == DATA_ALL_EGGS_HATCHED)
            _sideHatched[data] = true;
    }

    bool HatchAllEggs(uint32 hatchAction)
    {
        std::list<Creature* > eggList;
        me->GetCreaturesWithEntryInRange(eggList, 100.0f, NPC_EGG);
        if (eggList.empty())
            return false;

        if (hatchAction == HATCH_RESET)
        {
            for (Creature* egg : eggList)
                egg->Respawn();

            summons.DespawnEntry(NPC_AMANI_HATCHLING);
        }
        else if (hatchAction == HATCH_ALL)
            DoCastSelf(SPELL_HATCH_EGG_ALL);

        eggList.clear();
        return true;
    }

    void FireWall()
    {
        for (uint8 i = 0; i < 4; ++i)
        {
            uint8 wallNum = i == 0 || i == 2 ? 3 : 2;

            for (uint8 j = 0; j < wallNum; j++)
            {
                Creature* wall = wallNum == 3
                        ? me->SummonCreature(NPC_FIRE_BOMB, fireWallCoords[i].GetPositionX(), fireWallCoords[i].GetPositionY() + 5 * (j - 1), fireWallCoords[i].GetPositionZ(), fireWallCoords[i].GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 15000)
                        : me->SummonCreature(NPC_FIRE_BOMB, fireWallCoords[i].GetPositionX() - 2 + 4 * j, fireWallCoords[i].GetPositionY(), fireWallCoords[i].GetPositionZ(), fireWallCoords[i].GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 15000);

                if (wall)
                    wall->AI()->DoCastSelf(SPELL_FIRE_WALL, true);
            }
        }
    }

    void SpawnBombs()
    {
        float dx, dy;
        for (int i = 0; i < MAX_BOMB_COUNT; ++i)
        {
            dx = float(irand(-area_dx / 2, area_dx / 2));
            dy = float(irand(-area_dy / 2, area_dy / 2));
            DoSpawnCreature(NPC_FIRE_BOMB, dx, dy, 0, 0, TEMPSUMMON_TIMED_DESPAWN, 15000);
        }
    }

    void Boom()
    {
        summons.DoForAllSummons([&](WorldObject* summon) {
            if (summon->GetEntry() == NPC_FIRE_BOMB)
            {
                if (Creature* bomb = summon->ToCreature())
                {
                    bomb->AI()->DoCastSelf(SPELL_FIRE_BOMB_DAMAGE, true);
                    bomb->RemoveAllAuras();
                }
            }
        });
    }

    void StartBombing()
    {
        Talk(SAY_FIRE_BOMBS);
        me->AttackStop();
        me->GetMotionMaster()->Clear();
        me->SetPosition(janalainPos);
        me->StopMovingOnCurrentPos();
        DoCastSelf(SPELL_FIRE_BOMB_CHANNEL);

        FireWall();
        SpawnBombs();
        _isBombing = true;

        DoCastSelf(SPELL_TELE_TO_CENTER);
        DoCastAOE(SPELL_SUMMON_PLAYERS_DUMMY, true);
        StartBotBombAvoidance();

        //DoCast(Temp, SPELL_SUMMON_PLAYERS, true) // core bug, spell does not work if too far
        ThrowBombs();

        scheduler.Schedule(11s, [this](TaskContext)
        {
            Boom();
            _isBombing = false;
            StopBotBombAvoidance();

            me->RemoveAurasDueToSpell(SPELL_FIRE_BOMB_CHANNEL);
        });
    }

    void ThrowBombs()
    {
        std::chrono::milliseconds bombTimer = 100ms;

        summons.DoForAllSummons([this, &bombTimer](WorldObject* summon) {
            if (summon->GetEntry() == NPC_FIRE_BOMB)
            {
                if (Creature* bomb = summon->ToCreature())
                {
                    bomb->m_Events.AddEventAtOffset([this, bomb] {
                        bomb->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                        DoCast(bomb, SPELL_FIRE_BOMB_THROW, true);
                        bomb->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    }, bombTimer);
                }

                bombTimer += 100ms;
            }
        });
    }

    bool CheckEvadeIfOutOfCombatArea() const override
    {
        return me->GetPositionZ() <= 12.0f;
    }

    Unit* SelectRandomEncounterTarget(float range, bool includeVictim = true) const
    {
        return SelectRandomEncounterTargetForCreature(me, range, includeVictim);
    }

    void StartBotBombAvoidance()
    {
        _botBombDirectorActive = true;
        scheduler.CancelGroup(GROUP_BOT_BOMB_DIRECTOR);
        scheduler.Schedule(500ms, GROUP_BOT_BOMB_DIRECTOR, [this](TaskContext context)
        {
            MoveBotsToSafeBombPositions();
            context.Repeat(500ms);
        });
    }

    void StopBotBombAvoidance()
    {
        _botBombDirectorActive = false;
        scheduler.CancelGroup(GROUP_BOT_BOMB_DIRECTOR);
        _botBombLocks.clear();
    }

    bool IsSafeFromBombs(Position const& position, std::vector<Creature*> const& bombs) const
    {
        static constexpr float DangerRadius = 6.0f;

        if (std::fabs(position.GetPositionX() - janalainPos.GetPositionX()) > float(area_dx) * 0.5f - 3.0f)
            return false;

        if (std::fabs(position.GetPositionY() - janalainPos.GetPositionY()) > float(area_dy) * 0.5f - 3.0f)
            return false;

        for (Creature* bomb : bombs)
            if (bomb && bomb->IsInWorld() && bomb->GetExactDist2d(position.GetPositionX(), position.GetPositionY()) < DangerRadius)
                return false;

        return true;
    }

    void MoveBotsToSafeBombPositions()
    {
        if (!_botBombDirectorActive)
            return;

        std::vector<Creature*> bombs;
        summons.DoForAllSummons([&](WorldObject* summon)
        {
            if (summon->GetEntry() == NPC_FIRE_BOMB)
                if (Creature* bomb = summon->ToCreature())
                    if (bomb->IsInWorld())
                        bombs.push_back(bomb);
        });

        if (bombs.empty())
            return;

        std::vector<Position> candidates;
        for (float dx = -18.0f; dx <= 18.0f; dx += 4.0f)
        {
            for (float dy = -22.0f; dy <= 22.0f; dy += 4.0f)
            {
                Position candidate;
                candidate.Relocate(janalainPos.GetPositionX() + dx, janalainPos.GetPositionY() + dy, janalainPos.GetPositionZ(), 0.0f);
                if (IsSafeFromBombs(candidate, bombs))
                    candidates.push_back(candidate);
            }
        }

        if (candidates.empty())
            return;

        std::vector<Creature*> bots = GatherEncounterBotsForCreature(me, 150.0f);
        for (Creature* bot : bots)
        {
            if (!bot || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            if (IsBotCastingUninterruptible(bot))
                continue;

            Position current;
            current.Relocate(bot);
            if (IsSafeFromBombs(current, bombs))
                continue;

            Position const* best = nullptr;
            float bestScore = std::numeric_limits<float>::max();
            for (Position const& candidate : candidates)
            {
                float score = bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
                if (score < bestScore)
                {
                    best = &candidate;
                    bestScore = score;
                }
            }

            if (!best || bot->GetExactDist2d(best->GetPositionX(), best->GetPositionY()) <= 2.0f)
                continue;

            bot->InterruptNonMeleeSpells(false);
            SafeBotMoveTo(bot, *best);
            _botBombLocks.insert(bot->GetGUID());
        }
    }

private:
    bool _isBombing;
    bool _isFlameBreathing;
    bool _sideHatched[2];
    bool _botBombDirectorActive = false;
    GuidSet _botBombLocks;
};

struct npc_janalai_hatcher : public ScriptedAI
{
    npc_janalai_hatcher(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        ScriptedAI::Reset();
        scheduler.CancelAll();
        _side = (me->GetPositionY() < 1150);
        _waypoint = 0;
        _repeatCount = 1;
        _isHatching = false;
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(0, hatcherway[_side][0]);
    }

    void MovementInform(uint32, uint32) override
    {
        if (_waypoint == 5)
        {
            _isHatching = true;

            scheduler.Schedule(1500ms, [this](TaskContext context)
            {
                me->CastCustomSpell(SPELL_HATCH_EGG_ALL, SPELLVALUE_MAX_TARGETS, _repeatCount);

                ++_repeatCount;

                if (me->FindNearestCreature(NPC_EGG, 100.0f))
                    context.Repeat(5s);
                else
                {
                    if (WorldObject* summoner = GetSummoner())
                        if (Creature* janalai = summoner->ToCreature())
                            janalai->AI()->SetData(DATA_ALL_EGGS_HATCHED, _side);

                    _side = _side ? 0 : 1;
                    _isHatching = false;
                    _waypoint = 3;
                    MoveToNewWaypoint(_waypoint);
                }
            });
        }
        else
        {
            MoveToNewWaypoint(_waypoint);
            ++_waypoint;
        }
    }

    void MoveToNewWaypoint(uint8 waypoint)
    {
        if (!_isHatching)
        {
            scheduler.Schedule(100ms, [this, waypoint](TaskContext)
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePoint(0, hatcherway[_side][waypoint]);
            });
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

    void JustEngagedWith(Unit* /*who*/) override { }
    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }

private:
    uint8 _side;
    uint8 _waypoint;
    uint32 _repeatCount;
    bool _isHatching;
};

class spell_summon_all_players_dummy: public SpellScript
{
    PrepareSpellScript(spell_summon_all_players_dummy);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_PLAYERS });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Position pos = GetCaster()->GetPosition();
        GuidSet seen;
        for (WorldObject* target : targets)
            if (target)
                seen.insert(target->GetGUID());

        if (Creature* caster = GetCaster()->ToCreature())
        {
            for (Unit* unit : GatherEncounterUnitsForCreature(caster, 120.0f))
            {
                if (!unit || seen.count(unit->GetGUID()) || unit->IsWithinBox(pos, 22.0f, 28.0f, 28.0f))
                    continue;

                targets.push_back(unit);
                seen.insert(unit->GetGUID());
            }
        }

        targets.remove_if([&, pos](WorldObject* target) -> bool
        {
            return target->IsWithinBox(pos, 22.0f, 28.0f, 28.0f);
        });
    }

    void OnHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_SUMMON_PLAYERS, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_summon_all_players_dummy::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_summon_all_players_dummy::OnHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_janalai()
{
    RegisterZulAmanCreatureAI(boss_janalai);
    RegisterZulAmanCreatureAI(npc_janalai_hatcher);
    RegisterSpellScript(spell_summon_all_players_dummy);
}
