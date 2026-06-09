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
#include "GameObjectScript.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MoveSplineInit.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "SpellScriptLoader.h"
#include "ThreatManager.h"
#include "WaypointMgr.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "the_eye.h"
#include <algorithm>
#include <limits>
#include <vector>
#include <cmath>

#include "Player.h"
#include "SpellScript.h"

enum Spells
{
    SPELL_BERSERK                   = 45078,
    SPELL_FLAME_QUILLS              = 34229,
    SPELL_QUILL_MISSILE_1           = 34269, // 21
    SPELL_QUILL_MISSILE_2           = 34314, // 3
    SPELL_CLEAR_ALL_DEBUFFS         = 34098,
    SPELL_FLAME_BUFFET              = 34121,
    SPELL_EMBER_BLAST               = 34341,
    SPELL_REBIRTH_PHASE2            = 34342,
    SPELL_MELT_ARMOR                = 35410,
    SPELL_CHARGE                    = 35412,
    SPELL_REBIRTH_DIVE              = 35369,
    SPELL_DIVE_BOMB_VISUAL          = 35367,
    SPELL_DIVE_BOMB                 = 35181,
    SPELL_ALAR_SHIELDING_PROTECTION = 600445,
    SPELL_ALAR_SHIELDING_CUBE       = 600446,

    SPELL_ALAR_BOT_TELEPORT_VISUAL  = 51347,
    SPELL_GREATER_FIRE_PROTECTION   = 17543,

    SPELL_MODEL_VISIBILITY          = 24401 // Might not be accurate
};

// @todo: Alar doesnt seem to move to waypoints but instead to the triggers in p1
const Position alarPoints[9] =
{
    {338.65f, 61.57f, 18.07f, 4.60f}, //first platform
    {393.42f, 34.25f, 20.18f, 1.61f},
    {391.83f, -34.97f, 20.18f, 0.52f},
    {340.26f, -63.58f, 18.12f, 5.71f},
    {258.959015f, -38.687099f, 20.262899f, 5.21f}, //pre-nerf only
    {259.2277997, 35.879002f, 20.263f, 4.81f}, //pre-nerf only
    {332.0f, 0.01f, 43.0f, 0.0f}, //quill
    {331.0f, 0.01f, -2.38f, 0.0f}, //middle (p2)
    {332.0f, 0.01f, 43.0f, 0.0f} // dive
};

enum Misc
{
    DISPLAYID_INVISIBLE         = 23377,
    NPC_EMBER_OF_ALAR           = 19551,
    NPC_FLAME_PATCH             = 20602,

    POINT_PLATFORM              = 0,
    POINT_QUILL                 = 6,
    POINT_MIDDLE                = 7,
    POINT_DIVE                  = 8,

    EVENT_RELOCATE_MIDDLE       = 1,
    EVENT_REBIRTH               = 2,

    EVENT_MOVE_TO_PHASE_2       = 3,
    EVENT_FINISH_DIVE           = 4,
    EVENT_INVISIBLE             = 5
};

enum GroupAlar
{
    GROUP_FLAME_BUFFET              = 1,
    GROUP_ALAR_BOT_PLATFORM_SUPPORT = 2,
    GROUP_ALAR_BOT_EMBER_SUPPORT    = 3
};

// Xinef: Ruse of the Ashtongue (10946)
enum qruseoftheAshtongue
{
    SPELL_ASHTONGUE_RUSE        = 42090,
    QUEST_RUSE_OF_THE_ASHTONGUE = 10946,
};

const float INNER_CIRCLE_RADIUS = 60.0f;

namespace
{
    Position const AlarPlatforms[4] =
    {
        { 338.65f,  61.57f, 18.07f, 4.60f },
        { 393.42f,  34.25f, 20.18f, 1.61f },
        { 391.83f, -34.97f, 20.18f, 0.52f },
        { 340.26f, -63.58f, 18.12f, 5.71f }
    };

    constexpr float ALAR_BOT_SCAN_RANGE = 180.0f;
    constexpr float ALAR_PLATFORM_RADIUS = 13.0f;
    constexpr float ALAR_PLATFORM_Z_TOLERANCE = 8.0f;
    constexpr float ALAR_PLATFORM_TANK_MELEE_RANGE = 9.0f;
    constexpr float ALAR_MIN_BOT_PLATFORM_OFFSET = 4.0f;
    constexpr float ALAR_EMBER_PICKUP_DISTANCE = 3.0f;
    constexpr float ALAR_EMBER_EXPLOSION_CLEAR_DISTANCE = 8.0f;
    constexpr uint32 ALAR_EMBER_AVOID_HEALTH_PCT = 20;
    constexpr uint32 ALAR_PLATFORM_TANK_TELEPORT_COOLDOWN_MS = 4000;
    constexpr uint32 ALAR_SHIELDING_CUBE_BOT_HOLD_MS = 13000;
    constexpr uint32 ALAR_SHIELDING_CUBE_COOLDOWN_MS = 45000;
    constexpr float ALAR_PLATFORM_TANK_THREAT = 500000.0f;
    constexpr float ALAR_EMBER_HANDLER_THREAT = 75000.0f;

    Position const& GetAlarPlatformPosition(uint8 platformIndex)
    {
        return AlarPlatforms[std::min<uint8>(platformIndex, 3)];
    }

    bool IsValidEncounterNPCBot(Creature* creature)
    {
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }

    bool IsEncounterParticipantFor(Creature const* source, Unit* unit, float range = ALAR_BOT_SCAN_RANGE)
    {
        if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(source, range))
            return false;

        if (unit->IsPlayer())
            return true;

        return IsValidEncounterNPCBot(unit->ToCreature());
    }

    std::vector<Unit*> GatherAlarEncounterUnits(Creature const* source, float range = ALAR_BOT_SCAN_RANGE)
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

    std::vector<Creature*> GatherAlarEncounterBots(Creature const* source, float range = ALAR_BOT_SCAN_RANGE)
    {
        std::vector<Creature*> bots;

        for (Unit* unit : GatherAlarEncounterUnits(source, range))
            if (Creature* creature = unit->ToCreature())
                if (IsValidEncounterNPCBot(creature))
                    bots.push_back(creature);

        return bots;
    }

    bool IsPlayerMarkedTank(Player* player)
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

    bool IsAlarNpcBotTank(Creature* bot)
    {
        bot_ai* ai = IsValidEncounterNPCBot(bot) ? bot->GetBotAI() : nullptr;
        return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK));
    }

    bool IsAlarMeleeBot(Creature* bot)
    {
        bot_ai* ai = IsValidEncounterNPCBot(bot) ? bot->GetBotAI() : nullptr;
        if (!ai)
            return false;

        if (ai->HasRole(BOT_ROLE_HEAL) || ai->HasRole(BOT_ROLE_RANGED))
            return false;

        return (MELEE_BOT_CLASSES_MASK & (1u << ai->GetBotClass())) != 0;
    }

    bool IsAlarEncounterTank(Unit* unit)
    {
        if (!unit)
            return false;

        if (Player* player = unit->ToPlayer())
            return IsPlayerMarkedTank(player);

        return IsAlarNpcBotTank(unit->ToCreature());
    }

    bool IsUnitOnAlarPlatform(Unit* unit, uint8 platformIndex)
    {
        if (!unit || platformIndex >= 4)
            return false;

        Position const& platform = GetAlarPlatformPosition(platformIndex);
        return unit->GetExactDist2d(platform.GetPositionX(), platform.GetPositionY()) <= ALAR_PLATFORM_RADIUS &&
            std::abs(unit->GetPositionZ() - platform.GetPositionZ()) <= ALAR_PLATFORM_Z_TOLERANCE;
    }

    bool IsUnitInAlarPlatformTankRange(Creature* alar, Unit* unit, uint8 platformIndex)
    {
        if (!alar || !unit || !IsUnitOnAlarPlatform(unit, platformIndex))
            return false;

        return alar->IsWithinMeleeRange(unit) || unit->GetExactDist2d(alar) <= ALAR_PLATFORM_TANK_MELEE_RANGE;
    }

    Player* SelectHumanAlarPlatformTank(Creature* alar, uint8 platformIndex)
    {
        if (!alar)
            return nullptr;

        for (Unit* unit : GatherAlarEncounterUnits(alar))
        {
            Player* player = unit ? unit->ToPlayer() : nullptr;
            if (!player)
                continue;

            if (unit != alar->GetVictim() && !IsPlayerMarkedTank(player))
                continue;

            if (IsUnitInAlarPlatformTankRange(alar, unit, platformIndex))
                return player;
        }

        return nullptr;
    }

    bool HasValidAlarPlatformTank(Creature* alar, uint8 platformIndex)
    {
        if (!alar)
            return false;

        for (Unit* unit : GatherAlarEncounterUnits(alar))
        {
            if (!unit)
                continue;

            bool const currentHumanVictim = unit->IsPlayer() && unit == alar->GetVictim();
            if (!currentHumanVictim && !IsAlarEncounterTank(unit))
                continue;

            if (IsUnitInAlarPlatformTankRange(alar, unit, platformIndex))
                return true;
        }

        return false;
    }

    Creature* SelectAlarPlatformTank(Creature* alar, uint8 platformIndex)
    {
        Creature* best = nullptr;
        float bestScore = std::numeric_limits<float>::max();
        Position const& platform = GetAlarPlatformPosition(platformIndex);

        for (Creature* bot : GatherAlarEncounterBots(alar))
        {
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (!ai || !IsAlarNpcBotTank(bot))
                continue;

            float score = bot->GetExactDist2d(platform.GetPositionX(), platform.GetPositionY());
            if (ai->IsTank())
                score -= 40.0f;
            if (ai->IsOffTank())
                score -= 20.0f;
            if (bot == alar->GetVictim())
                score -= 15.0f;
            if (IsUnitOnAlarPlatform(bot, platformIndex))
                score -= 10.0f;

            if (!best || score < bestScore)
            {
                best = bot;
                bestScore = score;
            }
        }

        return best;
    }

    void ApplyAlarFireProtection(Creature* bot)
    {
        if (!IsValidEncounterNPCBot(bot) || bot->HasAura(SPELL_GREATER_FIRE_PROTECTION))
            return;

        bot->CastSpell(bot, SPELL_GREATER_FIRE_PROTECTION, true);
    }

    uint32 ApplyAlarFireProtectionToBots(Creature* source, char const* reason)
    {
        uint32 applied = 0;

        for (Creature* bot : GatherAlarEncounterBots(source))
        {
            if (bot->HasAura(SPELL_GREATER_FIRE_PROTECTION))
                continue;

            ApplyAlarFireProtection(bot);
            ++applied;
        }

        if (applied)
            LOG_DEBUG("scripts", "Al'ar NPCBot: applied Greater Fire Protection ({}) to {} bot(s)", reason ? reason : "encounter", applied);

        return applied;
    }

    Position GetAlarPlatformBotDestination(Creature* alar, uint8 platformIndex)
    {
        Position destination = GetAlarPlatformPosition(platformIndex);

        if (!alar)
            return destination;

        if (destination.GetExactDist2d(alar->GetPositionX(), alar->GetPositionY()) < ALAR_MIN_BOT_PLATFORM_OFFSET)
        {
            float const angle = alar->GetAngle(&destination);
            destination.Relocate(
                alar->GetPositionX() + std::cos(angle) * ALAR_MIN_BOT_PLATFORM_OFFSET,
                alar->GetPositionY() + std::sin(angle) * ALAR_MIN_BOT_PLATFORM_OFFSET,
                destination.GetPositionZ());
        }

        destination.SetOrientation(destination.GetAngle(alar));
        return destination;
    }

    Position GetAlarEmberBotDestination(Creature* bot, Creature* ember, bool avoidExplosion)
    {
        Position destination = ember ? ember->GetPosition() : Position();
        if (!ember)
            return destination;

        float const distance = avoidExplosion ? ALAR_EMBER_EXPLOSION_CLEAR_DISTANCE : ALAR_EMBER_PICKUP_DISTANCE;
        float angle = bot ? ember->GetAngle(bot) : ember->GetOrientation();

        float x = ember->GetPositionX() + std::cos(angle) * distance;
        float y = ember->GetPositionY() + std::sin(angle) * distance;
        float z = ember->GetPositionZ();

        if (bot && !bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        destination.Relocate(x, y, z);
        destination.SetOrientation(destination.GetAngle(ember));
        return destination;
    }

    void TeleportBotWithAlarVisual(Creature* bot, Position const& destination)
    {
        if (!IsValidEncounterNPCBot(bot))
            return;

        bot->CastSpell(bot, SPELL_ALAR_BOT_TELEPORT_VISUAL, true);
        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        bot->NearTeleportTo(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(), destination.GetOrientation(), false);
    }

    void ForceAlarBotEngage(Creature* bot, Creature* target, float threat)
    {
        bot_ai* ai = IsValidEncounterNPCBot(bot) ? bot->GetBotAI() : nullptr;
        if (!ai || !target || !target->IsAlive())
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FOLLOW | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_ATTACK);
        ai->SetBotCommandState(BOT_COMMAND_COMBATRESET);
        bot->SetInCombatWith(target);
        target->SetInCombatWith(bot);
        target->GetThreatMgr().AddThreat(bot, threat, nullptr, true, true);
        bot->Attack(target, !ai->HasRole(BOT_ROLE_RANGED));
        bot->SetInFront(target);
    }

    void LowerOtherAlarTankBotThreat(Creature* alar, Creature* activeTank)
    {
        if (!alar || !activeTank)
            return;

        for (Creature* bot : GatherAlarEncounterBots(alar))
        {
            if (bot == activeTank || !IsAlarNpcBotTank(bot))
                continue;

            alar->GetThreatMgr().ModifyThreatByPercent(bot, -75);
        }
    }

    Creature* SelectAlarEmberHandler(Creature* ember)
    {
        Creature* best = nullptr;
        float bestScore = std::numeric_limits<float>::max();

        for (Creature* bot : GatherAlarEncounterBots(ember))
        {
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (!ai || !IsAlarMeleeBot(bot))
                continue;

            float score = bot->GetDistance(ember);
            if (ai->HasRole(BOT_ROLE_DPS))
                score -= 30.0f;
            if (ai->IsTank() || ai->IsOffTank())
                score += 15.0f;
            if (bot == ember->GetVictim())
                score -= 20.0f;

            if (!best || score < bestScore)
            {
                best = bot;
                bestScore = score;
            }
        }

        return best;
    }

    void SendMeleeBotToAlarEmber(Creature* bot, Creature* ember)
    {
        if (!IsValidEncounterNPCBot(bot) || !ember || !ember->IsAlive())
            return;

        bool const avoidExplosion = ember->HealthBelowPct(ALAR_EMBER_AVOID_HEALTH_PCT);
        ApplyAlarFireProtection(bot);
        TeleportBotWithAlarVisual(bot, GetAlarEmberBotDestination(bot, ember, avoidExplosion));

        if (!avoidExplosion)
            ForceAlarBotEngage(bot, ember, ALAR_EMBER_HANDLER_THREAT);

        if (ember->AI())
            ember->AI()->AttackStart(bot);

        LOG_DEBUG("scripts", "Al'ar NPCBot: ember handler {} ({}) sent to Ember {} at {:.2f} {:.2f} {:.2f}",
            bot->GetName(), bot->GetEntry(), ember->GetGUID().GetCounter(), ember->GetPositionX(), ember->GetPositionY(), ember->GetPositionZ());
    }

    void MoveMeleeBotsAwayFromDyingAlarEmber(Creature* ember)
    {
        if (!ember)
            return;

        for (Creature* bot : GatherAlarEncounterBots(ember))
        {
            if (!IsAlarMeleeBot(bot) || bot->GetDistance(ember) > ALAR_EMBER_EXPLOSION_CLEAR_DISTANCE)
                continue;

            ApplyAlarFireProtection(bot);
            TeleportBotWithAlarVisual(bot, GetAlarEmberBotDestination(bot, ember, true));
            bot->AttackStop();
            bot->GetBotAI()->RemoveBotCommandState(BOT_COMMAND_ATTACK);

            LOG_DEBUG("scripts", "Al'ar NPCBot: moved melee bot {} ({}) away from dying Ember {}", bot->GetName(), bot->GetEntry(), ember->GetGUID().GetCounter());
        }
    }

    void AddAlarShieldingCubeBot(std::vector<Creature*>& bots, GuidSet& seen, Player* clicker, Creature* bot)
    {
        if (!clicker || !IsValidEncounterNPCBot(bot) || !bot->IsAlive() || !bot->IsInWorld() || bot->GetMap() != clicker->GetMap())
            return;

        if (seen.count(bot->GetGUID()))
            return;

        seen.insert(bot->GetGUID());
        bots.push_back(bot);
    }

    void AddAlarShieldingCubeOwnerBots(std::vector<Creature*>& bots, GuidSet& seen, Player* clicker, Player* owner)
    {
        if (!owner)
            return;

        if (BotMgr* botMgr = owner->GetBotMgr())
            if (BotMap const* botMap = botMgr->GetBotMap())
                for (BotMap::value_type const& pair : *botMap)
                    AddAlarShieldingCubeBot(bots, seen, clicker, pair.second);
    }

    std::vector<Creature*> GatherAlarShieldingCubeBots(Player* clicker)
    {
        std::vector<Creature*> bots;
        GuidSet seen;

        if (!clicker)
            return bots;

        if (Group* group = clicker->GetGroup())
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddAlarShieldingCubeOwnerBots(bots, seen, clicker, player);
                else if (Creature* bot = member ? member->ToCreature() : nullptr)
                    AddAlarShieldingCubeBot(bots, seen, clicker, bot);
            }
        }
        else
        {
            AddAlarShieldingCubeOwnerBots(bots, seen, clicker, clicker);
        }

        return bots;
    }

    void ReleaseAlarShieldingCubeBot(Creature* bot)
    {
        if (!IsValidEncounterNPCBot(bot))
            return;

        bot->SetStandState(UNIT_STAND_STATE_STAND);

        if (bot_ai* ai = bot->GetBotAI())
        {
            ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }
    }

    void HoldAlarShieldingCubeBot(Creature* bot, Player* clicker)
    {
        if (!IsValidEncounterNPCBot(bot) || !clicker)
            return;

        Position destination = clicker->GetPosition();
        destination.SetOrientation(clicker->GetOrientation());

        TeleportBotWithAlarVisual(bot, destination);
        bot->SetStandState(UNIT_STAND_STATE_STAND);

        if (bot_ai* ai = bot->GetBotAI())
        {
            ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);
            ai->SetBotCommandState(BOT_COMMAND_STAY);
        }

        bot->m_Events.AddEventAtOffset([bot]
        {
            ReleaseAlarShieldingCubeBot(bot);
        }, Milliseconds(ALAR_SHIELDING_CUBE_BOT_HOLD_MS));
    }
}

struct boss_alar : public BossAI
{

    boss_alar(Creature* creature) : BossAI(creature, DATA_ALAR)
    {
        me->SetCombatMovement(false);
    }

    void Reset() override
    {
        BossAI::Reset();
        _canAttackCooldown = true;
        _baseAttackOverride = false;
        _spawnPhoenixes = false;
        _hasPretendedToDie = false;
        _transitionScheduler.CancelAll();
        _platform = 0;
        _noMelee = false;
        _platformRoll = 0;
        _noQuillTimes = 0;
        _platformMoveRepeatTimer = 16s;
        _currentPlatform = 0;
        _platformTankTeleportCooldown = 0;
        me->SetModelVisible(true);
        me->SetReactState(REACT_AGGRESSIVE);
        ConstructWaypointsAndMove();
        me->m_Events.KillAllEvents(false);
    }

    void JustReachedHome() override
    {
        BossAI::JustReachedHome();
        if (me->IsEngaged())
            ConstructWaypointsAndMove();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        scheduler.Schedule(0s, [this](TaskContext context)
        {
            if (roll_chance_i(20 * _noQuillTimes))
            {
                _noQuillTimes = 0;
                _platformRoll = RAND(0, 1);
                _platform = _platformRoll ? 0 : 3;
                me->GetMotionMaster()->MovePoint(POINT_QUILL, alarPoints[POINT_QUILL], FORCED_MOVEMENT_NONE, 0.f, false, true);
                _platformMoveRepeatTimer = 16s;
            }
            else
            {
                uint8 const destinationPlatform = _platform;
                if (_noQuillTimes++ > 0)
                {
                    me->SetOrientation(alarPoints[_platform].GetOrientation());
                    SpawnPhoenixes(1, me);
                }
                me->GetMotionMaster()->MovePoint(POINT_PLATFORM, alarPoints[_platform], FORCED_MOVEMENT_NONE, 0.f, false, true);
                ScheduleAlarPlatformSupport(destinationPlatform);
                _platform = (_platform+1)%4;
                _platformMoveRepeatTimer = 30s;
            }
            context.Repeat(_platformMoveRepeatTimer);
        });

        ScheduleMainSpellAttack(0s);
    }

    bool CanAIAttack(Unit const* victim) const override
    {
        if (me->isMoving())
            return true;

        return _hasPretendedToDie || me->IsWithinMeleeRange(victim);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (why == EVADE_REASON_BOUNDARY)
            BossAI::EnterEvadeMode(why);
        else if (me->GetThreatMgr().IsThreatListEmpty(true))
            BossAI::EnterEvadeMode(why);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        me->SetModelVisible(true);

        me->GetMap()->DoForAllPlayers([&](Player* player)
        {
            if (player->GetQuestStatus(QUEST_RUSE_OF_THE_ASHTONGUE) == QUEST_STATUS_INCOMPLETE && player->HasAura(SPELL_ASHTONGUE_RUSE))
                player->AreaExploredOrEventHappens(QUEST_RUSE_OF_THE_ASHTONGUE);
        });
    }

    void MoveInLineOfSight(Unit* /*who*/) override { }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damagetype*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (damage >= me->GetHealth() && _platform < POINT_MIDDLE)
        {
            damage = 0;
            if (!_hasPretendedToDie)
            {
                _hasPretendedToDie = true;
                scheduler.CancelGroup(GROUP_ALAR_BOT_PLATFORM_SUPPORT);
                DoCastSelf(SPELL_EMBER_BLAST, true);
                PretendToDie(me);
                _transitionScheduler.Schedule(1s, [this](TaskContext)
                {
                    me->SetVisible(false);
                }).Schedule(8s, [this](TaskContext)
                {
                    me->SetPosition(alarPoints[POINT_MIDDLE]);
                }).Schedule(12s, [this](TaskContext)
                {
                    me->SetStandState(UNIT_STAND_STATE_STAND);
                    me->SetVisible(true);
                    DoCastSelf(SPELL_CLEAR_ALL_DEBUFFS, true);
                    DoCastSelf(SPELL_REBIRTH_PHASE2);
                }).Schedule(16001ms, [this](TaskContext)
                {
                    me->SetHealth(me->GetMaxHealth());
                    me->SetReactState(REACT_AGGRESSIVE);
                    _noMelee = false;
                    me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    _platform = POINT_MIDDLE;
                    _currentPlatform = POINT_MIDDLE;
                    me->ResumeChasingVictim();
                    ScheduleAbilities();
                });
            }
        }
    }

    void PretendToDie(Creature* creature)
    {
        _noMelee = true;
        scheduler.CancelAll();
        creature->InterruptNonMeleeSpells(true);
        creature->RemoveAllAuras();
        creature->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        creature->SetReactState(REACT_PASSIVE);
        creature->GetMotionMaster()->MovementExpired(false);
        creature->GetMotionMaster()->MoveIdle();
        creature->SetStandState(UNIT_STAND_STATE_DEAD);
    }

    void ScheduleAbilities()
    {
        _transitionScheduler.CancelAll();
        ScheduleTimedEvent(57s, [&]
        {
            DoCastVictim(SPELL_MELT_ARMOR);
        }, 60s);
        ScheduleTimedEvent(10s, [&]
        {
            DoCastRandomTarget(SPELL_CHARGE, 0, 50.0f);
        }, 30s);
        ScheduleTimedEvent(20s, [&]
        {
            // find spell from sniffs?
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 50.0f, true))
                me->SummonCreature(NPC_FLAME_PATCH, *target, TEMPSUMMON_TIMED_DESPAWN, 2 * MINUTE * IN_MILLISECONDS);
        }, 30s);
        ScheduleTimedEvent(34s, [&]
        {
            me->GetMotionMaster()->MovePoint(POINT_DIVE, alarPoints[POINT_DIVE], FORCED_MOVEMENT_NONE, 0.f, false, true);
            scheduler.DelayAll(15s);
        }, 57s);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_BERSERK, true);
        }, 10min);

        ScheduleMainSpellAttack(0s);
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);

        if (summon && summon->GetEntry() == NPC_EMBER_OF_ALAR)
            HandleAlarEmberSpawn(summon);
    }

    void SpawnPhoenixes(uint8 count, Unit* targetToSpawnAt)
    {
        if (targetToSpawnAt)
        {
            Position spawnPosition = DeterminePhoenixPosition(targetToSpawnAt->GetPosition());
            for (uint8 i = 0; i < count; ++i)
            {
                me->SummonCreature(NPC_EMBER_OF_ALAR, spawnPosition, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 6000);
            }
        }
    }

    void DoDiveBomb()
    {
        _noMelee = true;
        ApplyAlarFireProtectionToBots(me, "Dive Bomb");
        scheduler.Schedule(2s, [this](TaskContext)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 110.0f, true))
                SpawnPhoenixes(2, target);
        }).Schedule(6s, [this](TaskContext)
        {
            me->SetModelVisible(true);
            DoCastSelf(SPELL_REBIRTH_DIVE);
        }).Schedule(10s, [this](TaskContext)
        {
            me->ResumeChasingVictim();
            _noMelee = false;
        });
        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 90.0f, true))
        {
            DoCast(target, SPELL_DIVE_BOMB);
            me->SetPosition(*target);
            me->StopMovingOnCurrentPos();
        }
        me->RemoveAurasDueToSpell(SPELL_DIVE_BOMB_VISUAL);

    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
        {
            if (type == ESCORT_MOTION_TYPE && me->movespline->Finalized() && !me->IsInCombat())
                ConstructWaypointsAndMove();

            return;
        }

        switch (id)
        {
            case POINT_QUILL:
                scheduler.CancelGroup(GROUP_FLAME_BUFFET);
                scheduler.CancelGroup(GROUP_ALAR_BOT_PLATFORM_SUPPORT);
                ApplyAlarFireProtectionToBots(me, "Flame Quills");
                scheduler.Schedule(1s, [this](TaskContext)
                {
                    DoCastSelf(SPELL_FLAME_QUILLS);
                });
                ScheduleMainSpellAttack(13s);
                break;
            case POINT_DIVE:
                scheduler.Schedule(1s, [this](TaskContext)
                {
                    DoCastSelf(SPELL_DIVE_BOMB_VISUAL);
                }).Schedule(5s, [this](TaskContext)
                {
                    DoDiveBomb();
                });
                break;
            default:
                return;
        }
    }

    void ScheduleMainSpellAttack(std::chrono::seconds timer)
    {
        scheduler.Schedule(timer, GROUP_FLAME_BUFFET, [this](TaskContext context)
        {
            if (!me->SelectNearestTarget(me->GetCombatReach()) && !me->isMoving())
                DoCastAOE(SPELL_FLAME_BUFFET);

            context.Repeat(2s);
        });
    }

    void ConstructWaypointsAndMove()
    {
        me->StopMoving();
        me->GetMotionMaster()->MovePath(me->GetWaypointPath(), FORCED_MOVEMENT_NONE, PathSource::WAYPOINT_MGR);
    }

    void UpdateAI(uint32 diff) override
    {
        _transitionScheduler.Update(diff);

        if (_platformTankTeleportCooldown > diff)
            _platformTankTeleportCooldown -= diff;
        else
            _platformTankTeleportCooldown = 0;

        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (!_noMelee)
            DoMeleeAttackIfReady();
    }

    Position DeterminePhoenixPosition(Position playerPosition)
    {
        // set finalPosition to playerPosition in case the fraction fails
        Position finalPosition = playerPosition;
        float playerXPosition = playerPosition.GetPositionX();
        float playerYPosition = playerPosition.GetPositionY();
        float centreXPosition = alarPoints[POINT_MIDDLE].GetPositionX();
        float centreYPosition = alarPoints[POINT_MIDDLE].GetPositionY();
        float deltaX = std::abs(playerXPosition-centreXPosition);
        float deltaY = std::abs(playerYPosition-centreYPosition);
        int8 signMultiplier[2] = {1, 1};
        // if fraction has x position 0.0f we get nan as a result
        if (float playerFraction = deltaX/deltaY)
        {
            // player angle based on delta X and delta Y
            float playerAngle = std::atan(playerFraction);
            float phoenixDeltaYPosition = std::cos(playerAngle)*INNER_CIRCLE_RADIUS;
            float phoenixDeltaXPosition = std::sin(playerAngle)*INNER_CIRCLE_RADIUS;
            // as calculations are absolute values we have to multiply in the end
            // should be negative if player position was further down than centre
            if (playerXPosition < centreXPosition)
                signMultiplier[0] = -1;
            if (playerYPosition < centreYPosition)
                signMultiplier[1] = -1;
            // phoenix position based on set distance
            finalPosition = {centreXPosition+signMultiplier[0]*phoenixDeltaXPosition, centreYPosition+signMultiplier[1]*phoenixDeltaYPosition, 0.0f, 0.0f};
        }
        return finalPosition;
    }

    void ScheduleAlarPlatformSupport(uint8 platformIndex)
    {
        _currentPlatform = platformIndex;
        scheduler.CancelGroup(GROUP_ALAR_BOT_PLATFORM_SUPPORT);

        LOG_DEBUG("scripts", "Al'ar NPCBot: phase 1 platform transition to {} ({:.2f} {:.2f} {:.2f})",
            uint32(platformIndex + 1),
            GetAlarPlatformPosition(platformIndex).GetPositionX(),
            GetAlarPlatformPosition(platformIndex).GetPositionY(),
            GetAlarPlatformPosition(platformIndex).GetPositionZ());

        scheduler.Schedule(2500ms, GROUP_ALAR_BOT_PLATFORM_SUPPORT, [this, platformIndex](TaskContext context)
        {
            if (!me->IsInCombat() || _hasPretendedToDie || _platform >= POINT_MIDDLE || _currentPlatform != platformIndex)
                return;

            HandleAlarPlatformTankSupport(platformIndex);
            context.Repeat(2s);
        });
    }

    void HandleAlarPlatformTankSupport(uint8 platformIndex)
    {
        if (platformIndex >= 4)
            return;

        if (SelectHumanAlarPlatformTank(me, platformIndex))
            return;

        if (HasValidAlarPlatformTank(me, platformIndex))
            return;

        if (_platformTankTeleportCooldown)
            return;

        Creature* tank = SelectAlarPlatformTank(me, platformIndex);
        if (!tank)
            return;

        TeleportBotToAlarPlatform(tank, platformIndex);
    }

    void TeleportBotToAlarPlatform(Creature* bot, uint8 platformIndex)
    {
        if (!IsValidEncounterNPCBot(bot) || platformIndex >= 4)
            return;

        Position destination = GetAlarPlatformBotDestination(me, platformIndex);
        ApplyAlarFireProtection(bot);
        TeleportBotWithAlarVisual(bot, destination);
        ForceAlarBotEngage(bot, me, ALAR_PLATFORM_TANK_THREAT);
        LowerOtherAlarTankBotThreat(me, bot);

        _platformTankTeleportCooldown = ALAR_PLATFORM_TANK_TELEPORT_COOLDOWN_MS;

        LOG_DEBUG("scripts", "Al'ar NPCBot: teleported tank bot {} ({}) to platform {} at {:.2f} {:.2f} {:.2f}",
            bot->GetName(), bot->GetEntry(), uint32(platformIndex + 1),
            destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    }

    void HandleAlarEmberSpawn(Creature* ember)
    {
        Creature* handler = SelectAlarEmberHandler(ember);
        if (!handler)
        {
            LOG_DEBUG("scripts", "Al'ar NPCBot: no melee bot handler available for Ember {}", ember ? ember->GetGUID().GetCounter() : 0);
            return;
        }

        SendMeleeBotToAlarEmber(handler, ember);

        ObjectGuid const emberGuid = ember->GetGUID();
        scheduler.Schedule(1s, GROUP_ALAR_BOT_EMBER_SUPPORT, [this, emberGuid](TaskContext context)
        {
            Creature* ember = ObjectAccessor::GetCreature(*me, emberGuid);
            if (!ember || !ember->IsAlive())
                return;

            if (ember->HealthBelowPct(ALAR_EMBER_AVOID_HEALTH_PCT))
            {
                MoveMeleeBotsAwayFromDyingAlarEmber(ember);
                return;
            }

            context.Repeat(1s);
        });
    }

private:
    bool _hasPretendedToDie;
    bool _canAttackCooldown;
    bool _baseAttackOverride;
    bool _spawnPhoenixes;
    bool _noMelee;
    uint8 _platform;
    uint8 _currentPlatform;
    uint8 _platformRoll;
    uint8 _noQuillTimes;
    uint32 _platformTankTeleportCooldown;
    std::chrono::seconds _platformMoveRepeatTimer;
    TaskScheduler _transitionScheduler;
};

class CastQuill : public BasicEvent
{
public:
    CastQuill(Unit* caster, uint32 spellId) : _caster(caster), _spellId(spellId){ }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        _caster->CastSpell(_caster, _spellId, true);
        return true;
    }
private:
    Unit* _caster;
    uint32 _spellId;
};

class spell_alar_flame_quills : public AuraScript
{
    PrepareAuraScript(spell_alar_flame_quills);

    void HandlePeriodic(AuraEffect const*  /*aurEff*/)
    {
        PreventDefaultAction();

        // 24 spells in total
        for (uint8 i = 0; i < 21; ++i)
            GetUnitOwner()->m_Events.AddEventAtOffset(new CastQuill(GetUnitOwner(), SPELL_QUILL_MISSILE_1 + i), Milliseconds(i * 40));
        GetUnitOwner()->m_Events.AddEventAtOffset(new CastQuill(GetUnitOwner(), SPELL_QUILL_MISSILE_2 + 0), Milliseconds(22 * 40));
        GetUnitOwner()->m_Events.AddEventAtOffset(new CastQuill(GetUnitOwner(), SPELL_QUILL_MISSILE_2 + 1), Milliseconds(23 * 40));
        GetUnitOwner()->m_Events.AddEventAtOffset(new CastQuill(GetUnitOwner(), SPELL_QUILL_MISSILE_2 + 2), Milliseconds(24 * 40));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_alar_flame_quills::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_alar_quill_missile : public SpellScript
{
    PrepareSpellScript(spell_alar_quill_missile);

    void ReduceShieldedTargetDamage()
    {
        Unit* target = GetHitUnit();
        if (!target || (!target->IsPlayer() && !target->IsNPCBot()) || !target->HasAura(SPELL_ALAR_SHIELDING_PROTECTION))
            return;

        int32 damage = GetHitDamage();
        if (damage <= 0)
            return;

        SetHitDamage(std::max<int32>(1, damage * 5 / 100));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_alar_quill_missile::ReduceShieldedTargetDamage);
    }
};

class spell_alar_ember_blast : public SpellScript
{
    PrepareSpellScript(spell_alar_ember_blast);

    void HandleCast()
    {
        if (InstanceScript* instance = GetCaster()->GetInstanceScript())
            if (Creature* alar = instance->GetCreature(DATA_ALAR))
                if (!alar->HasAura(SPELL_MODEL_VISIBILITY))
                    Unit::DealDamage(GetCaster(), alar, alar->CountPctFromMaxHealth(2));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_alar_ember_blast::HandleCast);
    }
};

class spell_alar_dive_bomb : public AuraScript
{
    PrepareAuraScript(spell_alar_dive_bomb);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->SetModelVisible(false);
        GetUnitOwner()->SetDisplayId(DISPLAYID_INVISIBLE);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_alar_dive_bomb::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class go_alar_shielding_cube : public GameObjectScript
{
public:
    go_alar_shielding_cube() : GameObjectScript("go_alar_shielding_cube") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        if (!player || !go)
            return true;

        if (go->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
            return true;

        go->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
        go->m_Events.AddEventAtOffset([go]
        {
            if (go->IsInWorld())
                go->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
        }, Milliseconds(ALAR_SHIELDING_CUBE_COOLDOWN_MS));

        player->CastSpell(player, SPELL_ALAR_SHIELDING_CUBE, true);

        uint32 movedBots = 0;
        for (Creature* bot : GatherAlarShieldingCubeBots(player))
        {
            HoldAlarShieldingCubeBot(bot, player);
            ++movedBots;
        }

        LOG_DEBUG("scripts", "Al'ar Shielding Cube: {} clicked cube, gathered {} NPCBot(s), and disabled cube for {} ms", player->GetName(), movedBots, ALAR_SHIELDING_CUBE_COOLDOWN_MS);

        return true;
    }
};

void AddSC_boss_alar()
{
    RegisterTheEyeAI(boss_alar);
    RegisterSpellScript(spell_alar_flame_quills);
    RegisterSpellScript(spell_alar_quill_missile);
    RegisterSpellScript(spell_alar_ember_blast);
    RegisterSpellScript(spell_alar_dive_bomb);
    new go_alar_shielding_cube();
}
