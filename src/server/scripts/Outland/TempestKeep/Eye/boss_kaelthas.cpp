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

#include "Cell.h"
#include "CellImpl.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "WorldPacket.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "the_eye.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceCustomForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& message, uint32 cooldownMs = 5000);
}

enum KTYells
{
    // Kael'thas Speech
    SAY_INTRO                           = 0,
    SAY_INTRO_CAPERNIAN                 = 1,
    SAY_INTRO_TELONICUS                 = 2,
    SAY_INTRO_THALADRED                 = 3,
    SAY_INTRO_SANGUINAR                 = 4,
    SAY_PHASE2_WEAPON                   = 5,
    SAY_PHASE3_ADVANCE                  = 6,
    SAY_PHASE4_INTRO2                   = 7,
    SAY_PHASE5_NUTS                     = 8,
    SAY_SLAY                            = 9,
    SAY_MINDCONTROL                     = 10,
    SAY_GRAVITYLAPSE                    = 11,
    SAY_SUMMON_PHOENIX                  = 12,
    SAY_DEATH                           = 13,
    SAY_PYROBLAST                       = 14,

    // Advisors
    SAY_ADVISOR_AGGRO                   = 0,
    SAY_ADVISOR_DEATH                   = 1,
    EMOTE_THALADRED_FIXATE              = 2
};

enum KTSpells
{
    // _phase 2 spells
    SPELL_SUMMON_WEAPONS                = 36976,
    SPELL_SUMMON_WEAPONA                = 36958,
    SPELL_SUMMON_WEAPONB                = 36959,
    SPELL_SUMMON_WEAPONC                = 36960,
    SPELL_SUMMON_WEAPOND                = 36961,
    SPELL_SUMMON_WEAPONE                = 36962,
    SPELL_SUMMON_WEAPONF                = 36963,
    SPELL_SUMMON_WEAPONG                = 36964,

    // _phase 3 spells
    SPELL_RESURRECTION                  = 36450,

    // _phase 4 spells
    SPELL_FIREBALL                      = 36805,
    SPELL_ARCANE_DISRUPTION             = 36834,
    SPELL_PHOENIX                       = 36723,
    SPELL_MIND_CONTROL                  = 36797,
    SPELL_FROST_BLAST_CUSTOM            = 600449,
    SPELL_FROST_BLAST_DAMAGE            = 29879,
    SPELL_SHOCK_BARRIER                 = 36815,
    SPELL_PYROBLAST                     = 36819,
    SPELL_FLAME_STRIKE                  = 36735,
    SPELL_FLAME_STRIKE_DAMAGE           = 36731,
    SPELL_FIRE_BOMB_CHANNEL             = 42621,
    SPELL_FIRE_BOMB_THROW               = 42628,
    SPELL_FIRE_BOMB_DAMAGE              = 42630,

    // transition scene spells
    SPELL_NETHERBEAM_AURA1              = 36364,
    SPELL_NETHERBEAM_AURA2              = 36370,
    SPELL_NETHERBEAM_AURA3              = 36371,
    SPELL_NETHERBEAM1                   = 36089,
    SPELL_NETHERBEAM2                   = 36090,
    SPELL_KAEL_GAINING_POWER            = 36091,
    SPELL_KAEL_EXPLODES1                = 36376,
    SPELL_KAEL_EXPLODES2                = 36375,
    SPELL_KAEL_EXPLODES3                = 36373,
    SPELL_KAEL_EXPLODES4                = 36354,
    SPELL_KAEL_EXPLODES5                = 36092,
    SPELL_GROW                          = 36184,
    SPELL_KAEL_STUNNED                  = 36185,
    SPELL_KAEL_FULL_POWER               = 36187,
    SPELL_FLOATING_DROWNED              = 36550,
    SPELL_DARK_BANISH_STATE             = 52241, // wrong visual apparently
    SPELL_ARCANE_EXPLOSION_VISUAL       = 34807,

    SPELL_PURE_NETHER_BEAM1             = 36196,
    SPELL_PURE_NETHER_BEAM2             = 36197,
    SPELL_PURE_NETHER_BEAM3             = 36198,
    SPELL_PURE_NETHER_BEAM4             = 36201,
    SPELL_PURE_NETHER_BEAM5             = 36290,
    SPELL_PURE_NETHER_BEAM6             = 36291,

    // _phase 5 spells
    SPELL_GRAVITY_LAPSE                 = 35941,
    SPELL_GRAVITY_LAPSE_TELEPORT1       = 35966,
    SPELL_GRAVITY_LAPSE_KNOCKBACK       = 34480,
    SPELL_GRAVITY_LAPSE_AURA            = 39432,
    SPELL_SUMMON_NETHER_VAPOR           = 35865,
    SPELL_NETHER_BEAM                   = 35869,
    SPELL_NETHER_BEAM_DAMAGE            = 35873,

    SPELL_REMOTE_TOY_STUN               = 37029,
    SPELL_REMOTE_TOY_RESPONSE           = 36480,
    SPELL_REMOVE_ENCHANTED_WEAPONS      = 39497,

    // Advisors
    // Universal
    SPELL_KAEL_PHASE_TWO                = 36709,
    SPELL_PERMANENT_FEIGN_DEATH         = 29266,            // placed upon advisors on fake death

    // Sanguinar
    SPELL_BELLOWING_ROAR                = 44863,

    // Capernian
    SPELL_CAPERNIAN_FIREBALL            = 36971,
    SPELL_CONFLAGRATION                 = 37018,
    SPELL_ARCANE_BURST                  = 36970,

    // Telonicus
    SPELL_BOMB                          = 37036,
    SPELL_REMOTE_TOY                    = 37027,

    // Thaladred
    SPELL_PSYCHIC_BLOW                  = 36966,
    SPELL_REND                          = 36965,
    SPELL_SILENCE                       = 30225
};

enum KTPhases
{
    PHASE_NONE                          = 0,
    PHASE_SINGLE_ADVISOR                = 1,
    PHASE_WEAPONS                       = 2,
    PHASE_TRANSITION                    = 3,
    PHASE_ALL_ADVISORS                  = 4,
    PHASE_FINAL                         = 5
};

enum KTMisc
{
    POINT_MIDDLE                        = 1,
    POINT_AIR                           = 2,
    POINT_START_LAST_PHASE              = 3,

    DATA_RESURRECT_CAST                 = 1,

    NPC_WORLD_TRIGGER                   = 19871,
    NPC_NETHER_VAPOR                    = 21002,
    NPC_PHOENIX                         = 21362,
    NPC_PHOENIX_EGG                     = 21364,
    NPC_FIRE_BOMB                       = 23920,
    NPC_NETHERSTRAND_LONGBOW            = 21268,
    NPC_STAFF_OF_DISINTEGRATION         = 21274,
};

enum KTPreFightEvents
{
    EVENT_PREFIGHT_PHASE1_01              = 1,
    EVENT_PREFIGHT_PHASE5_01              = 2,
    EVENT_PREFIGHT_PHASE5_02              = 3,
    EVENT_PREFIGHT_PHASE6_02              = 4,
    EVENT_PREFIGHT_PHASE6_03              = 5,
};

enum KTTransitionScene
{
    EVENT_SCENE_1                       = 50, // NYI
    EVENT_SCENE_2                       = 51,
    EVENT_SCENE_3                       = 52,
    EVENT_SCENE_4                       = 53,
    EVENT_SCENE_5                       = 54,
    EVENT_SCENE_6                       = 55,
    EVENT_SCENE_7                       = 56,
    EVENT_SCENE_8                       = 57,
    EVENT_SCENE_9                       = 58,
    EVENT_SCENE_10                      = 59,
    EVENT_SCENE_11                      = 60,
    EVENT_SCENE_12                      = 61,
    EVENT_SCENE_13                      = 62,
    EVENT_SCENE_14                      = 63,
    EVENT_SCENE_15                      = 64,
    EVENT_SCENE_16                      = 65,
    EVENT_SCENE_17                      = 66,
    EVENT_SCENE_18                      = 67
};

enum KTActions
{
    ACTION_START_THALADRED              = 0,
    ACTION_START_SANGUINAR              = 1,
    ACTION_START_CAPERNIAN              = 2,
    ACTION_START_TELONICUS              = 3,
    ACTION_START_WEAPONS                = 4,
    ACTION_PROGRESS_PHASE_CHECK         = 5
};

enum KTSpellGroups
{
    GROUP_PROGRESS_PHASE                = 0,
    GROUP_PYROBLAST                     = 1,
    GROUP_SHOCK_BARRIER                 = 2,
    GROUP_NETHER_BEAM                   = 3,
    GROUP_FIRE_BOMB                     = 4,
    GROUP_BOT_FIRE_BOMB_DIRECTOR        = 5
};

const Position triggersPos[6] =
{
    {799.11f, -38.95f, 85.0f, 0.0f},
    {800.16f, 37.65f, 85.0f, 0.0f},
    {847.64f, -16.19f, 64.05f, 0.0f},
    {847.53f, 15.01f, 63.69f, 0.0f},
    {843.44f, -7.87f, 67.14f, 0.0f},
    {843.35f, 6.35f, 67.14f, 0.0f}
};

constexpr uint8 KAEL_THALADRED_KITE_POINT_COUNT = 4;

Position const KaelThaladredKitePoints[KAEL_THALADRED_KITE_POINT_COUNT] =
{
    // TODO: Fill these with user-provided Thaladred kite points.
    { 662.68f, -36.82f, 46.78f, 0.0f }, // Kite point 1
    { 653.21f, 27.38f, 46.78f, 0.0f }, // Kite point 2
    { 786.85f, 41.96f, 46.78f, 0.0f }, // Kite point 3
    { 785.87f, -46.04f, 46.78f, 0.0f }  // Kite point 4
};

namespace
{
    constexpr float KAEL_ENCOUNTER_RANGE = 170.0f;
    constexpr float KAEL_REMOTE_TOY_RESPONSE_RANGE = 100.0f;
    constexpr float KAEL_THALADRED_DANGER_DISTANCE = 18.0f;
    constexpr uint32 KAEL_REMOTE_TOY_RESPONSE_LOCK_MS = 4000;
    constexpr uint32 KAEL_THALADRED_MOVE_LOCK_MS = 4500;
    constexpr uint32 KAEL_THALADRED_KITE_RELEASE_MS = 10500;
    constexpr uint32 KAEL_DOMINION_TICK_MS = 1000;
    constexpr uint8 KAEL_DOMINION_TICK_COUNT = 5;
    constexpr uint32 KAEL_DOMINION_DAMAGE_PCT = 8;
    constexpr uint32 KAEL_FROST_BLAST_DAMAGE_PCT = 18;
    constexpr uint32 KAEL_FIRE_BOMB_ACTIVE_MS = 11000;
    constexpr uint32 KAEL_FIRE_BOMB_DESPAWN_MS = 15000;
    constexpr uint32 KAEL_FIRE_BOMB_COUNT = 95;
    constexpr float KAEL_FIRE_BOMB_AREA_X = 135.0f;
    constexpr float KAEL_FIRE_BOMB_AREA_Y = 110.0f;
    constexpr float KAEL_FIRE_BOMB_SAFE_MARGIN = 8.0f;
    constexpr float KAEL_FIRE_BOMB_DANGER_RADIUS = 7.0f;
    constexpr float KAEL_FIRE_BOMB_CANDIDATE_STEP = 5.0f;
    constexpr float KAEL_FIRE_BOMB_BOT_MOVE_RANGE = 170.0f;

    GuidSet KaelAllAdvisorsTelonicusGuids;
    GuidSet KaelRemoteToyResponseLocks;
    GuidSet KaelThaladredMoveLocks;

    bool IsValidKaelEncounterBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && creature->GetBotAI() && creature->IsAlive() && creature->IsInWorld() &&
            !creature->IsTempBot() && !creature->IsFreeBot();
    }

    bool IsValidKaelEncounterUnit(Unit const* unit)
    {
        if (!unit || !unit->IsAlive() || !unit->IsInWorld() || unit->HasUnitState(UNIT_STATE_ISOLATED))
            return false;

        if (Player const* player = unit->ToPlayer())
            return !player->IsGameMaster();

        return IsValidKaelEncounterBot(unit->ToCreature());
    }

    bool IsKaelEncounterParticipantFor(Creature const* source, Unit* unit, float range = KAEL_ENCOUNTER_RANGE)
    {
        if (!source || !IsValidKaelEncounterUnit(unit) || unit == source)
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source) || !source->IsWithinDistInMap(unit, range))
            return false;

        if (!source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
            return false;

        if (source->GetVictim() == unit || unit->IsInCombatWith(source))
            return true;

        if (source->CanHaveThreatList() && source->GetThreatMgr().GetThreat(unit, true) > 0.0f)
            return true;

        return source->IsInCombat() && unit->IsInCombat();
    }

    void AddKaelEncounterTarget(Creature const* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, float range)
    {
        if (!IsKaelEncounterParticipantFor(source, unit, range))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    void AddKaelOwnedBots(Creature const* source, std::vector<Unit*>& targets, GuidSet& seen, Player* owner, float range)
    {
        if (!owner)
            return;

        if (BotMgr* botMgr = owner->GetBotMgr())
            if (BotMap const* botMap = botMgr->GetBotMap())
                for (BotMap::value_type const& pair : *botMap)
                    AddKaelEncounterTarget(source, targets, seen, pair.second, range);
    }

    void AddKaelThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, float range)
    {
        if (!source || !source->CanHaveThreatList())
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddKaelEncounterTarget(source, targets, seen, ref->GetVictim(), range);
        }
    }

    std::vector<Unit*> GatherKaelEncounterTargets(Creature* source, float range = KAEL_ENCOUNTER_RANGE)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source || !source->GetMap())
            return targets;

        AddKaelThreatTargets(source, targets, seen, range);

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            AddKaelEncounterTarget(source, targets, seen, player, range);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                {
                    AddKaelEncounterTarget(source, targets, seen, member, range);

                    if (Player* memberPlayer = member ? member->ToPlayer() : nullptr)
                        AddKaelOwnedBots(source, targets, seen, memberPlayer, range);
                }
            }

            AddKaelOwnedBots(source, targets, seen, player, range);
        }

        return targets;
    }

    std::vector<Creature*> GatherKaelEncounterBots(Creature* source, float range = KAEL_ENCOUNTER_RANGE)
    {
        std::vector<Creature*> bots;

        for (Unit* unit : GatherKaelEncounterTargets(source, range))
            if (Creature* creature = unit->ToCreature())
                if (IsValidKaelEncounterBot(creature))
                    bots.push_back(creature);

        return bots;
    }

    bool HasKaelEncounterBots(Creature* source)
    {
        std::vector<Creature*> bots = GatherKaelEncounterBots(source);
        return !bots.empty();
    }

    Unit* SelectKaelRandomEncounterTarget(Creature* source, bool includeBots = true, float range = KAEL_ENCOUNTER_RANGE)
    {
        std::vector<Unit*> targets = GatherKaelEncounterTargets(source, range);
        if (!includeBots)
        {
            targets.erase(std::remove_if(targets.begin(), targets.end(), [](Unit const* unit)
            {
                return unit && unit->IsNPCBot();
            }), targets.end());
        }

        if (targets.empty())
            return nullptr;

        return targets[urand(0, uint32(targets.size() - 1))];
    }

    bool IsKaelFrostBlastTankTarget(Creature* source, Unit* unit)
    {
        if (!source || !unit)
            return true;

        if (source->GetVictim() == unit)
            return true;

        if (Creature* bot = unit->ToCreature())
        {
            bot_ai* ai = bot->IsNPCBot() ? bot->GetBotAI() : nullptr;
            return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF));
        }

        if (Player* player = unit->ToPlayer())
        {
            if (player->HasTankSpec())
                return true;

            if (Group* group = player->GetGroup())
            {
                Group::MemberSlotList const& slots = group->GetMemberSlots();
                for (Group::member_citerator itr = slots.begin(); itr != slots.end(); ++itr)
                    if (itr->guid == player->GetGUID())
                        return (itr->flags & (MEMBER_FLAG_MAINTANK | MEMBER_FLAG_MAINASSIST)) != 0;
            }
        }

        return false;
    }

    Unit* SelectKaelRandomFrostBlastTarget(Creature* source, bool includeBots = true, float range = KAEL_ENCOUNTER_RANGE)
    {
        std::vector<Unit*> targets = GatherKaelEncounterTargets(source, range);
        if (!includeBots)
        {
            targets.erase(std::remove_if(targets.begin(), targets.end(), [](Unit const* unit)
            {
                return unit && unit->IsNPCBot();
            }), targets.end());
        }

        targets.erase(std::remove_if(targets.begin(), targets.end(), [source](Unit* unit)
        {
            return IsKaelFrostBlastTankTarget(source, unit);
        }), targets.end());

        if (targets.empty())
            return nullptr;

        return targets[urand(0, uint32(targets.size() - 1))];
    }

    void MarkKaelAllAdvisorsRemoteToyActive(InstanceScript* instance, bool active)
    {
        if (!instance)
            return;

        if (Creature* telonicus = instance->GetCreature(DATA_TELONICUS))
        {
            if (active)
                KaelAllAdvisorsTelonicusGuids.insert(telonicus->GetGUID());
            else
                KaelAllAdvisorsTelonicusGuids.erase(telonicus->GetGUID());
        }
    }

    bool IsKaelAllAdvisorsRemoteToyActive(Unit* target)
    {
        if (!target)
            return false;

        Creature* telonicus = target->FindNearestCreature(NPC_TELONICUS, KAEL_ENCOUNTER_RANGE, true);
        return telonicus && KaelAllAdvisorsTelonicusGuids.count(telonicus->GetGUID()) != 0;
    }

    bool IsKaelCasterBotForRemoteToy(Creature* bot, Unit* target)
    {
        bot_ai* ai = IsValidKaelEncounterBot(bot) ? bot->GetBotAI() : nullptr;
        if (!ai || !target || bot == target)
            return false;

        if (bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
            return false;

        if (ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
            return false;

        if (ai->IsTank() || (!ai->HasRole(BOT_ROLE_RANGED) && !ai->HasRole(BOT_ROLE_HEAL)))
            return false;

        return bot->IsWithinDistInMap(target, KAEL_REMOTE_TOY_RESPONSE_RANGE) && bot->IsWithinLOSInMap(target);
    }

    Creature* SelectKaelRemoteToyResponder(Unit* target)
    {
        if (!target)
            return nullptr;

        Creature* source = target->FindNearestCreature(NPC_TELONICUS, KAEL_ENCOUNTER_RANGE, true);
        if (!source)
            source = target->FindNearestCreature(NPC_KAELTHAS, KAEL_ENCOUNTER_RANGE, true);

        if (!source)
            return nullptr;

        std::vector<Creature*> candidates;
        for (Creature* bot : GatherKaelEncounterBots(source, KAEL_REMOTE_TOY_RESPONSE_RANGE))
            if (IsKaelCasterBotForRemoteToy(bot, target))
                candidates.push_back(bot);

        if (candidates.empty())
            return nullptr;

        return candidates[urand(0, uint32(candidates.size() - 1))];
    }

    void TriggerKaelRemoteToyBotResponse(Unit* target)
    {
        if (!target || !target->IsAlive() || !target->HasAura(SPELL_REMOTE_TOY))
            return;

        if (!IsKaelAllAdvisorsRemoteToyActive(target))
            return;

        ObjectGuid const targetGuid = target->GetGUID();
        if (!KaelRemoteToyResponseLocks.insert(targetGuid).second)
            return;

        target->m_Events.AddEventAtOffset([targetGuid]
        {
            KaelRemoteToyResponseLocks.erase(targetGuid);
        }, Milliseconds(KAEL_REMOTE_TOY_RESPONSE_LOCK_MS));

        Creature* responder = SelectKaelRemoteToyResponder(target);
        if (!responder)
        {
            LOG_DEBUG("scripts", "Kael'thas NPCBot: no caster bot found to answer Remote Toy on {}", target->GetName());
            return;
        }

        responder->CastSpell(target, SPELL_REMOTE_TOY_RESPONSE, true);
        LOG_DEBUG("scripts", "Kael'thas NPCBot: caster bot {} answered Remote Toy on {}", responder->GetName(), target->GetName());

        if (Creature* targetBot = DBMFTABotCallouts::AsNPCBotCreature(target))
            DBMFTABotCallouts::AnnounceCustomForModule(targetBot, SPELL_REMOTE_TOY, "DBM-TheEye", "KaelThas", "Remote Toy on me!", DBMFTABotCallouts::GetCooldownMs());
    }

    void ApplySunKingsDominionDamage(Creature* kael, Unit* target)
    {
        if (!kael || !target || !target->IsAlive())
            return;

        if (Creature* targetBot = DBMFTABotCallouts::AsNPCBotCreature(target))
            DBMFTABotCallouts::AnnounceCustomForModule(targetBot, SPELL_MIND_CONTROL, "DBM-TheEye", "KaelThas", "Sun King's Dominion on me!", DBMFTABotCallouts::GetCooldownMs());

        ObjectGuid const casterGuid = kael->GetGUID();
        ObjectGuid const targetGuid = target->GetGUID();

        for (uint8 i = 1; i <= KAEL_DOMINION_TICK_COUNT; ++i)
        {
            target->m_Events.AddEventAtOffset([casterGuid, targetGuid, target]
            {
                Unit* currentTarget = ObjectAccessor::GetUnit(*target, targetGuid);
                Unit* caster = ObjectAccessor::GetUnit(*target, casterGuid);
                if (!currentTarget || !caster || !currentTarget->IsAlive() || !caster->IsInWorld())
                    return;

                Unit::DealDamage(caster, currentTarget, std::max<uint32>(1, currentTarget->CountPctFromMaxHealth(KAEL_DOMINION_DAMAGE_PCT)));
            }, Milliseconds(KAEL_DOMINION_TICK_MS * i));
        }
    }

    void CastSunKingsDominion(Creature* kael, uint32 maxTargets)
    {
        if (!kael || !maxTargets)
            return;

        std::vector<Unit*> targets = GatherKaelEncounterTargets(kael, KAEL_ENCOUNTER_RANGE);
        if (Unit* victim = kael->GetVictim())
        {
            targets.erase(std::remove(targets.begin(), targets.end(), victim), targets.end());
        }

        while (targets.size() > maxTargets)
            targets.erase(targets.begin() + urand(0, uint32(targets.size() - 1)));

        for (Unit* target : targets)
            ApplySunKingsDominionDamage(kael, target);
    }

    bool IsFilledThaladredKitePoint(Position const& position)
    {
        return std::abs(position.GetPositionX()) > 0.1f || std::abs(position.GetPositionY()) > 0.1f || std::abs(position.GetPositionZ()) > 0.1f;
    }

    bool SelectThaladredKitePoint(Creature* thaladred, Creature* bot, Position& destination)
    {
        if (!thaladred || !bot)
            return false;

        Position const* bestPoint = nullptr;
        float bestThaladredDistance = -1.0f;
        float bestBotDistance = std::numeric_limits<float>::max();

        for (Position const& point : KaelThaladredKitePoints)
        {
            if (!IsFilledThaladredKitePoint(point))
                continue;

            float const thaladredDistance = thaladred->GetExactDist2d(point.GetPositionX(), point.GetPositionY());
            float const botDistance = bot->GetExactDist2d(point.GetPositionX(), point.GetPositionY());

            if (!bestPoint || thaladredDistance > bestThaladredDistance ||
                (thaladredDistance == bestThaladredDistance && botDistance < bestBotDistance))
            {
                bestPoint = &point;
                bestThaladredDistance = thaladredDistance;
                bestBotDistance = botDistance;
            }
        }

        if (!bestPoint)
            return false;

        destination = *bestPoint;
        return true;
    }

    void MoveThaladredFixatedBotToKitePoint(Creature* thaladred, Unit* target)
    {
        Creature* bot = target ? target->ToCreature() : nullptr;
        bot_ai* ai = IsValidKaelEncounterBot(bot) ? bot->GetBotAI() : nullptr;
        if (!thaladred || !ai)
            return;

        if (KaelThaladredMoveLocks.count(bot->GetGUID()) != 0)
            return;

        Position destination;
        if (!SelectThaladredKitePoint(thaladred, bot, destination))
            return;

        KaelThaladredMoveLocks.insert(bot->GetGUID());
        ObjectGuid const botGuid = bot->GetGUID();
        bot->m_Events.AddEventAtOffset([botGuid]
        {
            KaelThaladredMoveLocks.erase(botGuid);
        }, Milliseconds(KAEL_THALADRED_MOVE_LOCK_MS));

        bot->m_Events.AddEventAtOffset([bot]
        {
            bot_ai* ai = IsValidKaelEncounterBot(bot) ? bot->GetBotAI() : nullptr;
            if (!ai)
                return;

            if (Creature* thaladred = bot->FindNearestCreature(NPC_THALADRED, KAEL_ENCOUNTER_RANGE, true))
                if (thaladred->GetVictim() == bot)
                    return;

            ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_COMBATRESET);
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        }, Milliseconds(KAEL_THALADRED_KITE_RELEASE_MS));

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW | BOT_COMMAND_ATTACK);
        ai->SetBotCommandState(BOT_COMMAND_STAY);
        ai->MoveToSendPosition(destination);

        DBMFTABotCallouts::AnnounceCustomForModule(bot, SPELL_PSYCHIC_BLOW, "DBM-TheEye", "KaelThas", "Thaladred fixated me!", DBMFTABotCallouts::GetCooldownMs());
        LOG_DEBUG("scripts", "Kael'thas NPCBot: moved Thaladred target {} to kite point {:.2f} {:.2f} {:.2f}",
            bot->GetName(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    }

    void MoveThaladredVictimIfTooClose(Creature* thaladred)
    {
        if (!thaladred)
            return;

        Unit* victim = thaladred->GetVictim();
        if (!victim || thaladred->GetExactDist2d(victim) > KAEL_THALADRED_DANGER_DISTANCE)
            return;

        MoveThaladredFixatedBotToKitePoint(thaladred, victim);
    }

    bool IsKaelBotCastingUninterruptible(Creature* bot)
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

    void MoveKaelBotToPosition(Creature* bot, Position const& destination)
    {
        bot_ai* ai = IsValidKaelEncounterBot(bot) ? bot->GetBotAI() : nullptr;
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

    Position GetKaelFireBombCenter(Creature const* kael)
    {
        Position center = kael ? kael->GetHomePosition() : Position();
        center.SetOrientation(0.0f);
        return center;
    }

    bool IsWithinKaelFireBombArea(Position const& center, Position const& position)
    {
        if (std::fabs(position.GetPositionX() - center.GetPositionX()) > KAEL_FIRE_BOMB_AREA_X * 0.5f - KAEL_FIRE_BOMB_SAFE_MARGIN)
            return false;

        if (std::fabs(position.GetPositionY() - center.GetPositionY()) > KAEL_FIRE_BOMB_AREA_Y * 0.5f - KAEL_FIRE_BOMB_SAFE_MARGIN)
            return false;

        return true;
    }

    bool IsSafeFromKaelFireBombs(Position const& center, Position const& position, std::vector<Creature*> const& bombs)
    {
        if (!IsWithinKaelFireBombArea(center, position))
            return false;

        for (Creature* bomb : bombs)
            if (bomb && bomb->IsInWorld() && bomb->GetExactDist2d(position.GetPositionX(), position.GetPositionY()) < KAEL_FIRE_BOMB_DANGER_RADIUS)
                return false;

        return true;
    }
}

struct boss_kaelthas : public BossAI
{
    boss_kaelthas(Creature* creature) : BossAI(creature, DATA_KAELTHAS) { }

    void PrepareAdvisors()
    {
        for (uint8 advisorData = DATA_THALADRED; advisorData <= DATA_TELONICUS; ++advisorData)
        {
            if (Creature* advisor = instance->GetCreature(advisorData))
            {
                advisor->Respawn(true);
                advisor->StopMovingOnCurrentPos();
            }
        }
    }

    void SetRoomState(GOState state)
    {
        //TODO: handle door closing
        if (GameObject* window = ObjectAccessor::GetGameObject(*me, instance->GetGuidData(GO_BRIDGE_WINDOW)))
            window->SetGoState(state);
        if (GameObject* window = ObjectAccessor::GetGameObject(*me, instance->GetGuidData(GO_KAEL_STATUE_RIGHT)))
            window->SetGoState(state);
        if (GameObject* window = ObjectAccessor::GetGameObject(*me, instance->GetGuidData(GO_KAEL_STATUE_LEFT)))
            window->SetGoState(state);
    }

    void Reset() override
    {
        BossAI::Reset();
        scheduler.Schedule(1s, [this](TaskContext)
        {
            PrepareAdvisors();
        });

        _phase = PHASE_NONE;
        _transitionSceneReached = false;
        _advisorsAlive = 4;
        MarkKaelAllAdvisorsRemoteToyActive(instance, false);
        StopKaelFireBombAvoidance();
        me->RemoveAurasDueToSpell(SPELL_FIRE_BOMB_CHANNEL);

        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_HOVER, true); // hover effect 36550 - Floating Drowned
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE);
        SetRoomState(GO_STATE_READY);
        me->SetDisableGravity(false);
        me->SetWalk(false);
    }

    void AttackStart(Unit* who) override
    {
        if (_phase == PHASE_FINAL)
            BossAI::AttackStart(who);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (_phase == PHASE_NONE && who->IsPlayer() && me->IsValidAttackTarget(who))
        {
            _phase = PHASE_SINGLE_ADVISOR;
            me->SetInCombatWithZone();
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_DISABLE_MOVE);
            Talk(SAY_INTRO);
            DoCastAOE(SPELL_REMOVE_ENCHANTED_WEAPONS, true);
            ScheduleUniqueTimedEvent(21s, [&]
            {
                IntroduceNewAdvisor(SAY_INTRO_THALADRED, ACTION_START_THALADRED);
            }, EVENT_PREFIGHT_PHASE1_01);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_NETHER_VAPOR)
            summon->GetMotionMaster()->MoveRandom(20.0f);
        if (summon->GetEntry() == NPC_FIRE_BOMB)
        {
            summon->SetReactState(REACT_PASSIVE);
            summon->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        }
        if (summon->GetEntry() >= NPC_NETHERSTRAND_LONGBOW && summon->GetEntry() <= NPC_STAFF_OF_DISINTEGRATION)
            summon->SetReactState(REACT_PASSIVE);
    }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (_phase == PHASE_SINGLE_ADVISOR && spell->Id == SPELL_KAEL_PHASE_TWO)
        {
            switch (caster->GetEntry())
            {
                case NPC_THALADRED:
                    IntroduceNewAdvisor(SAY_INTRO_SANGUINAR, ACTION_START_SANGUINAR);
                    break;
                case NPC_LORD_SANGUINAR:
                    IntroduceNewAdvisor(SAY_INTRO_CAPERNIAN, ACTION_START_CAPERNIAN);
                    break;
                case NPC_CAPERNIAN:
                    IntroduceNewAdvisor(SAY_INTRO_TELONICUS, ACTION_START_TELONICUS);
                    break;
                case NPC_TELONICUS:
                    PhaseEnchantedWeaponsExecute();
                    break;
                default:
                    break;
            }
        }
        else if (_phase == PHASE_ALL_ADVISORS && spell->Id == SPELL_KAEL_PHASE_TWO)
        {
            --_advisorsAlive;
            if (_advisorsAlive == 0)
            {
                PhaseKaelExecute();
            }
        }
    }

    void MovementInform(uint32 type, uint32 point) override
    {
        if (type != POINT_MOTION_TYPE && type != EFFECT_MOTION_TYPE)
            return;

        if (point == POINT_MIDDLE)
        {
            ExecuteMiddleEvent();
        }
        else if (point == POINT_AIR)
        {
            me->SetDisableGravity(true); // updating AnimationTier will break drowning animation later
        }
        else if (point == POINT_START_LAST_PHASE)
        {
            me->SetDisableGravity(false);
            me->SetWalk(false);
            me->RemoveAurasDueToSpell(SPELL_KAEL_FULL_POWER);
            me->SetReactState(REACT_AGGRESSIVE);
            me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
            //re-set validator
            scheduler.SetValidator([this]{
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
            ScheduleTimedEvent(0ms, [&]
            {
                DoCastVictim(SPELL_FIREBALL);
            }, 2400ms, 7500ms);
            ScheduleTimedEvent(10s, [&]
            {
                DoCastRandomTarget(SPELL_FLAME_STRIKE, 0, 100.0f);
            }, 30250ms, 50650ms);
            ScheduleTimedEvent(50s, [&]
            {
                Talk(SAY_SUMMON_PHOENIX);
                DoCastSelf(SPELL_PHOENIX);
            }, 61450ms, 96550ms);
            ScheduleTimedEvent(5s, [&]
            {
                StartKaelFireBombs();
            }, 70s);
            if (me->GetVictim())
            {
                me->SetTarget(me->GetVictim()->GetGUID());
                AttackStart(me->GetVictim());
            }
        }
    }

    void ExecuteMiddleEvent()
    {
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        me->RemoveAllAttackers();
        scheduler.ClearValidator();
        me->SetTarget();
        me->SetFacingTo(M_PI);
        me->SetWalk(true);
        Talk(SAY_PHASE5_NUTS);
        ScheduleUniqueTimedEvent(2500ms, [&]
        {
            me->SetTarget();
            DoCastSelf(SPELL_KAEL_EXPLODES1, true);
            DoCastSelf(SPELL_KAEL_GAINING_POWER);
        }, EVENT_SCENE_2);
        ScheduleUniqueTimedEvent(4s, [&]
        {
            me->SetTarget();
            for (uint8 i = 0; i < 2; ++i)
                if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, triggersPos[i], TEMPSUMMON_TIMED_DESPAWN, 60000))
                    trigger->CastSpell(me, SPELL_NETHERBEAM1 + i, false);
            me->SetDisableGravity(true);
            me->SendMovementFlagUpdate();
            me->GetMotionMaster()->MoveTakeoff(POINT_AIR, me->GetPositionX(), me->GetPositionY(), 75.0f, 2.99, true); // AnimType Movement::ToFly does not exist for Kael
            DoCastSelf(SPELL_GROW, true);
        }, EVENT_SCENE_3);
        ScheduleUniqueTimedEvent(7s, [&]
        {
            me->SetTarget();
            DoCastSelf(SPELL_GROW, true);
            DoCastSelf(SPELL_KAEL_EXPLODES2, true);
            DoCastSelf(SPELL_NETHERBEAM_AURA1, true);
            for (uint8 i = 0; i < 2; ++i)
                if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, triggersPos[i + 2], TEMPSUMMON_TIMED_DESPAWN, 60000))
                    trigger->CastSpell(me, SPELL_NETHERBEAM1 + i, false);
        }, EVENT_SCENE_4);
        ScheduleUniqueTimedEvent(10s, [&]
        {
            me->SetTarget();
            DoCastSelf(SPELL_GROW, true);
            DoCastSelf(SPELL_KAEL_EXPLODES3, true);
            DoCastSelf(SPELL_NETHERBEAM_AURA2, true);
            for (uint8 i = 0; i < 2; ++i)
                if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, triggersPos[i + 4], TEMPSUMMON_TIMED_DESPAWN, 60000))
                    trigger->CastSpell(me, SPELL_NETHERBEAM1 + i, false);
        }, EVENT_SCENE_5);
        ScheduleUniqueTimedEvent(14s, [&]
        {
            DoCastSelf(SPELL_GROW, true);
            DoCastSelf(SPELL_KAEL_EXPLODES4, true);
            DoCastSelf(SPELL_NETHERBEAM_AURA3, true);
        }, EVENT_SCENE_6);
        ScheduleUniqueTimedEvent(17500ms, [&]
        {
            SetRoomState(GO_STATE_ACTIVE);
        }, EVENT_SCENE_7);
        ScheduleUniqueTimedEvent(19s, [&]
        {
            summons.DespawnEntry(WORLD_TRIGGER);
            me->RemoveAurasDueToSpell(SPELL_NETHERBEAM_AURA1);
            me->RemoveAurasDueToSpell(SPELL_NETHERBEAM_AURA2);
            me->RemoveAurasDueToSpell(SPELL_NETHERBEAM_AURA3);
            DoCastSelf(SPELL_KAEL_EXPLODES5, true);
        }, EVENT_SCENE_8);
        ScheduleUniqueTimedEvent(22s, [&]
        {
            DoCastSelf(SPELL_DARK_BANISH_STATE, true);
            DoCastSelf(SPELL_ARCANE_EXPLOSION_VISUAL, true);
            me->SummonCreature(NPC_WORLD_TRIGGER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000);
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX() + 5, me->GetPositionY(), me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM1, true);
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX() - 5, me->GetPositionY(), me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM2, true);
        }, EVENT_SCENE_9);
        ScheduleUniqueTimedEvent(22800ms, [&]
        {
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX() - 5, me->GetPositionY() - 5, me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM3, true);
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX() + 5, me->GetPositionY() + 5, me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM1, true);
        }, EVENT_SCENE_10);
        ScheduleUniqueTimedEvent(23600ms, [&]
        {
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX(), me->GetPositionY() + 5, me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM2, true);
        }, EVENT_SCENE_11);
        ScheduleUniqueTimedEvent(24500ms, [&]
        {
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX(), me->GetPositionY() - 5, me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM3, true);
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX() + 5, me->GetPositionY() - 5, me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM1, true);
        }, EVENT_SCENE_12);
        ScheduleUniqueTimedEvent(24800ms, [&]
        {
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX() - 5, me->GetPositionY() + 5, me->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM2, true);
        }, EVENT_SCENE_13);
        ScheduleUniqueTimedEvent(25300ms, [&]
        {
            if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, me->GetPositionX()-5, me->GetPositionY()+5, me->GetPositionZ()+15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000))
                trigger->CastSpell(me, SPELL_PURE_NETHER_BEAM3, true);
        }, EVENT_SCENE_14);
        ScheduleUniqueTimedEvent(30500ms, [&]
        {
            me->SetFacingTo(M_PI);
            me->RemoveAurasDueToSpell(SPELL_FLOATING_DROWNED);
            me->CastStop();
        }, EVENT_SCENE_15);
        ScheduleUniqueTimedEvent(30700ms, [&]
        {
            me->CastStop();
            DoCastSelf(SPELL_KAEL_FULL_POWER);
        }, EVENT_SCENE_16);
        ScheduleUniqueTimedEvent(32s, [&]
        {
            DoCastSelf(SPELL_KAEL_PHASE_TWO, true);
            DoCastSelf(SPELL_PURE_NETHER_BEAM4, true);
            DoCastSelf(SPELL_PURE_NETHER_BEAM5, true);
            DoCastSelf(SPELL_PURE_NETHER_BEAM6, true);
        }, EVENT_SCENE_17);
        ScheduleUniqueTimedEvent(36s, [&]
        {
            summons.DespawnEntry(WORLD_TRIGGER);
            me->CastStop();
            me->GetMotionMaster()->Clear();
            me->RemoveAurasDueToSpell(SPELL_DARK_BANISH_STATE); // WRONG VISUAL
            me->SetDisableGravity(true);
            me->SendMovementFlagUpdate();
            me->GetMotionMaster()->MoveLand(POINT_START_LAST_PHASE, me->GetHomePosition(), 2.99f);
        }, EVENT_SCENE_18);
    }

    void IntroduceNewAdvisor(KTYells talkIntroduction, KTActions kaelAction)
    {
        std::chrono::milliseconds attackStartTimer = 0ms;
        uint32 dataIdx = DATA_THALADRED;
        scheduler.Schedule(2s, [this, talkIntroduction](TaskContext)
        {
            Talk(talkIntroduction);
        });
        //switch because talk times are different
        switch (kaelAction)
        {
            case ACTION_START_THALADRED:
                attackStartTimer = 7s;
                dataIdx = DATA_THALADRED;
                break;
            case ACTION_START_SANGUINAR:
                attackStartTimer = 14500ms;
                dataIdx = DATA_LORD_SANGUINAR;
                break;
            case ACTION_START_CAPERNIAN:
                attackStartTimer = 9s;
                dataIdx = DATA_CAPERNIAN;
                break;
            case ACTION_START_TELONICUS:
                attackStartTimer = 10400ms;
                dataIdx = DATA_TELONICUS;
                break;
            default:
                break;
        }
        scheduler.Schedule(attackStartTimer, [this, dataIdx](TaskContext)
        {
            if (Creature* advisor = instance->GetCreature(dataIdx))
            {
                advisor->SetReactState(REACT_AGGRESSIVE);
                advisor->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                if (Unit* target = SelectKaelRandomEncounterTarget(me))
                    advisor->AI()->AttackStart(target);
                advisor->SetInCombatWithZone();
                advisor->AI()->Talk(SAY_ADVISOR_AGGRO);
            }
        });
    }

    void PhaseEnchantedWeaponsExecute()
    {
        ScheduleUniqueTimedEvent(3s, [&]{
            Talk(SAY_PHASE2_WEAPON);
            DoCastSelf(SPELL_SUMMON_WEAPONS);
            _phase = PHASE_WEAPONS;
            scheduler.Schedule(95s, GROUP_PROGRESS_PHASE, [this](TaskContext)
            {
                PhaseAllAdvisorsExecute();
            });
        }, EVENT_PREFIGHT_PHASE5_01);
        ScheduleUniqueTimedEvent(9s, [&]{
            summons.DoForAllSummons([&](WorldObject* summon)
            {
                if (Creature* summonedCreature = summon->ToCreature())
                {
                    if (!summonedCreature->GetSpawnId())
                    {
                        summonedCreature->SetReactState(REACT_AGGRESSIVE);
                        summonedCreature->SetInCombatWithZone();
                        if (Unit* target = SelectKaelRandomEncounterTarget(me))
                        {
                            summonedCreature->AI()->AttackStart(target);
                        }
                    }
                }
            });
        }, EVENT_PREFIGHT_PHASE5_02);
    }

    void PhaseAllAdvisorsExecute()
    {
        _phase = PHASE_TRANSITION;
        scheduler.CancelGroup(GROUP_PROGRESS_PHASE);
        Talk(SAY_PHASE3_ADVANCE);
        ScheduleUniqueTimedEvent(6s, [&]{
            DoCastSelf(SPELL_RESURRECTION);
            _phase = PHASE_ALL_ADVISORS;
            MarkKaelAllAdvisorsRemoteToyActive(instance, true);
        }, EVENT_PREFIGHT_PHASE6_02);
        scheduler.Schedule(192s, GROUP_PROGRESS_PHASE, [this](TaskContext)
        {
            PhaseKaelExecute();
        });
    }

    void PhaseKaelExecute()
    {
        scheduler.CancelAll();
        Talk(SAY_PHASE4_INTRO2);
        MarkKaelAllAdvisorsRemoteToyActive(instance, false);
        _phase = PHASE_FINAL;
        DoResetThreatList();
        me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_DISABLE_MOVE);
        if (Unit* target = SelectKaelRandomEncounterTarget(me))
        {
            AttackStart(target);
        }
        ScheduleHealthCheckEvent(50, [&]{
            if (!_transitionSceneReached)
            {
                _transitionSceneReached = true;
                scheduler.CancelAll();
                me->AttackStop();
                me->CastStop();
                me->SetReactState(REACT_PASSIVE);
                me->GetMotionMaster()->MovePoint(POINT_MIDDLE, me->GetHomePosition(), FORCED_MOVEMENT_NONE, 0.f, true, true);
            }
        });
        ScheduleTimedEvent(1s, [&]
        {
            DoCastVictim(SPELL_FIREBALL);
        }, 2400ms, 7500ms);
        ScheduleTimedEvent(15s, [&]
        {
            DoCastRandomTarget(SPELL_FLAME_STRIKE, 0, 100.0f);
        }, 30250ms, 50650ms);
        ScheduleTimedEvent(50s, [&]
        {
            Talk(SAY_SUMMON_PHOENIX);
            DoCastSelf(SPELL_PHOENIX);
        }, 35450ms, 41550ms);
        ScheduleTimedEvent(20s, 23s, [&]
        {
            if (Unit* target = SelectKaelRandomFrostBlastTarget(me))
            {
                me->CastSpell(target, SPELL_FROST_BLAST_CUSTOM, false);

                if (Creature* targetBot = DBMFTABotCallouts::AsNPCBotCreature(target))
                    DBMFTABotCallouts::AnnounceCustomForModule(targetBot, SPELL_FROST_BLAST_CUSTOM, "DBM-TheEye", "KaelThas", "Frost Blast on me, heal me!", DBMFTABotCallouts::GetCooldownMs());
            }

            scheduler.Schedule(3s, [this](TaskContext)
            {
                DoCastSelf(SPELL_ARCANE_DISRUPTION);
            });
        }, 23s, 26s);
        ScheduleTimedEvent(60s, [&]
        {
            Talk(SAY_PYROBLAST);
            DoCastSelf(SPELL_SHOCK_BARRIER);
            scheduler.DelayAll(10s);
            scheduler.Schedule(0s, GROUP_PYROBLAST, [this](TaskContext context)
            {
                DoCastVictim(SPELL_PYROBLAST);
                context.Repeat(4s);
            }).Schedule(8500ms, GROUP_PYROBLAST, [this](TaskContext)
            {
                scheduler.CancelGroup(GROUP_PYROBLAST);
            });
        }, 50s);
    }

    std::vector<Creature*> GetActiveKaelFireBombs()
    {
        std::vector<Creature*> bombs;
        summons.DoForAllSummons([&](WorldObject* summon)
        {
            if (summon->GetEntry() != NPC_FIRE_BOMB)
                return;

            Creature* bomb = summon->ToCreature();
            if (bomb && bomb->IsInWorld())
                bombs.push_back(bomb);
        });

        return bombs;
    }

    void SpawnKaelFireBombs()
    {
        Position const center = GetKaelFireBombCenter(me);

        for (uint32 i = 0; i < KAEL_FIRE_BOMB_COUNT; ++i)
        {
            float x = center.GetPositionX() + float(irand(int32(-KAEL_FIRE_BOMB_AREA_X * 0.5f), int32(KAEL_FIRE_BOMB_AREA_X * 0.5f)));
            float y = center.GetPositionY() + float(irand(int32(-KAEL_FIRE_BOMB_AREA_Y * 0.5f), int32(KAEL_FIRE_BOMB_AREA_Y * 0.5f)));
            float z = center.GetPositionZ();
            me->UpdateAllowedPositionZ(x, y, z);
            me->SummonCreature(NPC_FIRE_BOMB, x, y, z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, KAEL_FIRE_BOMB_DESPAWN_MS);
        }
    }

    void ThrowKaelFireBombs()
    {
        std::chrono::milliseconds bombTimer = 100ms;

        summons.DoForAllSummons([this, &bombTimer](WorldObject* summon)
        {
            if (summon->GetEntry() != NPC_FIRE_BOMB)
                return;

            if (Creature* bomb = summon->ToCreature())
            {
                bomb->m_Events.AddEventAtOffset([this, bomb]
                {
                    bomb->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    DoCast(bomb, SPELL_FIRE_BOMB_THROW, true);
                    bomb->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                }, bombTimer);
            }

            bombTimer += 100ms;
        });
    }

    void DetonateKaelFireBombs()
    {
        summons.DoForAllSummons([&](WorldObject* summon)
        {
            if (summon->GetEntry() != NPC_FIRE_BOMB)
                return;

            if (Creature* bomb = summon->ToCreature())
            {
                bomb->AI()->DoCastSelf(SPELL_FIRE_BOMB_DAMAGE, true);
                bomb->RemoveAllAuras();
            }
        });
    }

    void StartKaelFireBombAvoidance()
    {
        _botFireBombDirectorActive = true;
        scheduler.CancelGroup(GROUP_BOT_FIRE_BOMB_DIRECTOR);
        scheduler.Schedule(500ms, GROUP_BOT_FIRE_BOMB_DIRECTOR, [this](TaskContext context)
        {
            MoveBotsToSafeKaelFireBombPositions();
            context.Repeat(500ms);
        });
    }

    void StopKaelFireBombAvoidance()
    {
        _botFireBombDirectorActive = false;
        scheduler.CancelGroup(GROUP_BOT_FIRE_BOMB_DIRECTOR);
        _botFireBombLocks.clear();
    }

    void MoveBotsToSafeKaelFireBombPositions()
    {
        if (!_botFireBombDirectorActive)
            return;

        std::vector<Creature*> bombs = GetActiveKaelFireBombs();
        if (bombs.empty())
            return;

        Position const center = GetKaelFireBombCenter(me);
        std::vector<Position> candidates;
        for (float dx = -KAEL_FIRE_BOMB_AREA_X * 0.5f + KAEL_FIRE_BOMB_SAFE_MARGIN; dx <= KAEL_FIRE_BOMB_AREA_X * 0.5f - KAEL_FIRE_BOMB_SAFE_MARGIN; dx += KAEL_FIRE_BOMB_CANDIDATE_STEP)
        {
            for (float dy = -KAEL_FIRE_BOMB_AREA_Y * 0.5f + KAEL_FIRE_BOMB_SAFE_MARGIN; dy <= KAEL_FIRE_BOMB_AREA_Y * 0.5f - KAEL_FIRE_BOMB_SAFE_MARGIN; dy += KAEL_FIRE_BOMB_CANDIDATE_STEP)
            {
                float x = center.GetPositionX() + dx;
                float y = center.GetPositionY() + dy;
                float z = center.GetPositionZ();
                me->UpdateAllowedPositionZ(x, y, z);

                Position candidate;
                candidate.Relocate(x, y, z, 0.0f);
                if (IsSafeFromKaelFireBombs(center, candidate, bombs))
                    candidates.push_back(candidate);
            }
        }

        if (candidates.empty())
            return;

        for (Creature* bot : GatherKaelEncounterBots(me, KAEL_FIRE_BOMB_BOT_MOVE_RANGE))
        {
            if (!bot || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            if (IsKaelBotCastingUninterruptible(bot))
                continue;

            Position current;
            current.Relocate(bot);
            if (IsSafeFromKaelFireBombs(center, current, bombs))
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
            MoveKaelBotToPosition(bot, *best);
            _botFireBombLocks.insert(bot->GetGUID());
            DBMFTABotCallouts::AnnounceCustomForModule(bot, SPELL_FIRE_BOMB_DAMAGE, "DBM-TheEye", "KaelThas", "Fire Bombs near me!", DBMFTABotCallouts::GetCooldownMs());
        }
    }

    void StartKaelFireBombs()
    {
        scheduler.CancelGroup(GROUP_FIRE_BOMB);
        scheduler.DelayAll(Milliseconds(KAEL_FIRE_BOMB_ACTIVE_MS + 2000));
        me->setAttackTimer(BASE_ATTACK, KAEL_FIRE_BOMB_ACTIVE_MS + 2000);
        me->AttackStop();
        me->CastStop();
        me->SetTarget();
        me->GetMotionMaster()->Clear();
        me->StopMoving();
        DoCastSelf(SPELL_FIRE_BOMB_CHANNEL);

        SpawnKaelFireBombs();
        StartKaelFireBombAvoidance();
        ThrowKaelFireBombs();

        scheduler.Schedule(Milliseconds(KAEL_FIRE_BOMB_ACTIVE_MS), GROUP_FIRE_BOMB, [this](TaskContext)
        {
            DetonateKaelFireBombs();
            StopKaelFireBombAvoidance();
            me->RemoveAurasDueToSpell(SPELL_FIRE_BOMB_CHANNEL);

            if (Unit* victim = me->GetVictim())
            {
                me->SetTarget(victim->GetGUID());
                me->GetMotionMaster()->MoveChase(victim);
            }
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

    bool CheckEvadeIfOutOfCombatArea() const override
    {
        return me->GetHomePosition().GetExactDist2d(me) > 165.0f || !SelectTargetFromPlayerList(165.0f);
    }
    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        MarkKaelAllAdvisorsRemoteToyActive(instance, false);
        StopKaelFireBombAvoidance();
        DoCastAOE(SPELL_REMOVE_ENCHANTED_WEAPONS, true);
    }
private:
    uint32 _phase;
    uint8 _advisorsAlive;
    bool _transitionSceneReached = false;
    bool _botFireBombDirectorActive = false;
    GuidSet _botFireBombLocks;
};

struct advisor_baseAI : public ScriptedAI

{
    advisor_baseAI(Creature* creature) : ScriptedAI(creature) {    }

    virtual void ScheduleEvents() {}

    void Reset() override
    {
        _preventDeath = true;
        _feigning = false;
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        scheduler.CancelAll();
    }

    void JustRespawned() override
    {
        ScriptedAI::JustRespawned();
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        me->SetReactState(REACT_PASSIVE);
    }

    void JustEngagedWith(Unit* /*who*/) override { ScheduleEvents(); }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageEffectType*/, SpellSchoolMask /*spellSchoolMask*/) override
    {
        if (!_preventDeath)
            return;
        if (damage >= me->GetHealth())
        {
            damage = me->GetHealth() - 1; // prevent death
            if (_feigning)
                return;
            scheduler.CancelAll();
            me->AttackStop();
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            DoCastAOE(SPELL_KAEL_PHASE_TWO, true);
            DoCastSelf(SPELL_PERMANENT_FEIGN_DEATH, true);
            _feigning = true;
        }
     }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_RESURRECTION && caster->GetEntry() == NPC_KAELTHAS)
        {
            me->RemoveAurasDueToSpell(SPELL_PERMANENT_FEIGN_DEATH);
            me->SetStandState(UNIT_STAND_STATE_STAND);
            me->SetFullHealth();
            scheduler.Schedule(6s, [&](TaskContext /*context*/)
            {
                _preventDeath = false;
                _feigning = false;
                me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                DoResetThreatList();
                me->SetInCombatWithZone();
                me->SetReactState(REACT_AGGRESSIVE);
                if (Unit* target = SelectKaelRandomEncounterTarget(me))
                {
                    AttackStart(target);
                }
                ScheduleEvents();
            });
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_ADVISOR_DEATH);
        scheduler.CancelAll();
        DoCastAOE(SPELL_KAEL_PHASE_TWO, true);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (_feigning)
            return;

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }
private:
    bool _preventDeath;
    bool _feigning;
};

struct npc_lord_sanguinar : public advisor_baseAI
{
    npc_lord_sanguinar(Creature* creature) : advisor_baseAI(creature) { };

    void ScheduleEvents() override
    {
        ScheduleTimedEvent(0s, 2s, [&]{
            DoCastSelf(SPELL_BELLOWING_ROAR);
        }, 30s, 40s);
    }
};

struct npc_capernian : public advisor_baseAI
{
    npc_capernian(Creature* creature) : advisor_baseAI(creature) { }

    void AttackStart(Unit* who) override
    {
        if (who && who->isTargetableForAttack() && me->GetReactState() != REACT_PASSIVE)
        {
            if (me->Attack(who, false))
            {
                me->GetMotionMaster()->MoveChase(who, 30.0f, 0);
                me->AddThreat(who, 0.0f);
            }
        }
    }

    void ScheduleEvents() override
    {
        ScheduleTimedEvent(0ms, [&]
        {
            if (!me->CanCastSpell(SPELL_CAPERNIAN_FIREBALL))
            {
                me->ResumeChasingVictim();
            }
            else
            {
                me->GetMotionMaster()->MoveChase(me->GetVictim(), 30.0f);
                DoCastVictim(SPELL_CAPERNIAN_FIREBALL);
            }
        }, 2500ms);
        ScheduleTimedEvent(7s, 10s, [&]{
            DoCastRandomTarget(SPELL_CONFLAGRATION, 0, 30.0f);
        }, 18500ms, 20500ms);
        ScheduleTimedEvent(3s, [&]{
            DoCastRandomTarget(SPELL_ARCANE_BURST, 0, 8.0f);
        }, 6s);
    }
};

struct npc_telonicus : public advisor_baseAI
{
    npc_telonicus(Creature* creature) : advisor_baseAI(creature) { }

    void ScheduleEvents() override
    {
        ScheduleTimedEvent(0ms, [&]{
            DoCastVictim(SPELL_BOMB);
        }, 3600ms, 7100ms);
        ScheduleTimedEvent(13250ms, [&]{
            DoCastRandomTarget(SPELL_REMOTE_TOY, 0, 100.0f);
        }, 15750ms);
    }
};

struct npc_thaladred : public advisor_baseAI
{
    npc_thaladred(Creature* creature) : advisor_baseAI(creature) { }

    void ScheduleEvents() override
    {
        ScheduleTimedEvent(100ms, [&]
        {
            DoResetThreatList();
            if (Unit* target = SelectKaelRandomEncounterTarget(me))
            {
                me->AddThreat(target, 10000000.0f);
                Talk(EMOTE_THALADRED_FIXATE, target);
                MoveThaladredFixatedBotToKitePoint(me, target);
            }
        }, 10s);
        ScheduleTimedEvent(4s, 19350ms, [&]
        {
            DoCastVictim(SPELL_PSYCHIC_BLOW);
        }, 15700ms, 48900ms);
        ScheduleTimedEvent(3s, 6050ms, [&]
        {
            DoCastVictim(SPELL_REND);
        }, 15700ms, 48900ms);
        ScheduleTimedEvent(3s, 6050ms, [&]
        {
            if (Unit* victim = me->GetVictim())
            {
                if (victim->IsNonMeleeSpellCast(false, false, true))
                {
                    DoCastSelf(SPELL_SILENCE);
                }
            }
        }, 3600ms, 15200ms);
    }

    void UpdateAI(uint32 diff) override
    {
        advisor_baseAI::UpdateAI(diff);
        MoveThaladredVictimIfTooClose(me);
    }
};

class spell_kaelthas_remote_toy : public AuraScript
{
    PrepareAuraScript(spell_kaelthas_remote_toy);

    void HandlePeriodic(AuraEffect const*  /*aurEff*/)
    {
        PreventDefaultAction();
        if (roll_chance_i(66))
            GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_REMOTE_TOY_STUN, true);

        TriggerKaelRemoteToyBotResponse(GetUnitOwner());
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kaelthas_remote_toy::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_kaelthas_summon_weapons : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_summon_weapons);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        for (uint32 i = SPELL_SUMMON_WEAPONA; i <= SPELL_SUMMON_WEAPONG; ++i)
            GetCaster()->CastSpell(GetCaster(), i, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_summon_weapons::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_kaelthas_mind_control : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_mind_control);

    void SelectTarget(std::list<WorldObject*>& targets)
    {
        if (Unit* victim = GetCaster()->GetVictim())
        {
            targets.remove_if(Acore::ObjectGUIDCheck(victim->GetGUID(), true));
        }

        targets.remove_if([&](WorldObject const* target) -> bool
        {
            if (!target->ToPlayer())
                return true;

            return (!GetCaster()->IsWithinLOSInMap(target));
        });
    }

    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        if (!GetCaster() || !GetHitPlayer())
            return;

        if (Player* player = GetHitPlayer())
            GetCaster()->GetThreatMgr().ResetThreat(player);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kaelthas_mind_control::SelectTarget, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_mind_control::HandleEffect, EFFECT_ALL, SPELL_AURA_ANY);
    }
};

class spell_kaelthas_custom_frost_blast : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_custom_frost_blast);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_FROST_BLAST_CUSTOM });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            Unit const* unit = target ? target->ToUnit() : nullptr;
            return !unit || unit->HasAura(SPELL_FROST_BLAST_CUSTOM);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kaelthas_custom_frost_blast::FilterTargets, EFFECT_ALL, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

class spell_kaelthas_custom_frost_blast_aura : public AuraScript
{
    PrepareAuraScript(spell_kaelthas_custom_frost_blast_aura);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_FROST_BLAST_CUSTOM, SPELL_FROST_BLAST_DAMAGE });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        Unit* target = GetTarget();
        Unit* caster = GetCaster();
        if (!target || !caster)
            return;

        int32 const damage = std::max<int32>(1, target->CountPctFromMaxHealth(KAEL_FROST_BLAST_DAMAGE_PCT));
        caster->CastCustomSpell(SPELL_FROST_BLAST_DAMAGE, SPELLVALUE_BASE_POINT0, damage, target, true, nullptr, aurEff);

        if (aurEff->GetTickNumber() == 1)
            caster->CastSpell(target, SPELL_FROST_BLAST_CUSTOM, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kaelthas_custom_frost_blast_aura::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_kaelthas_burn : public AuraScript
{
    PrepareAuraScript(spell_kaelthas_burn);

    void HandlePeriodic(AuraEffect const*  /*aurEff*/)
    {
        Unit::DealDamage(GetUnitOwner(), GetUnitOwner(), GetUnitOwner()->CountPctFromMaxHealth(5) + 1);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kaelthas_burn::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_kaelthas_flame_strike : public AuraScript
{
    PrepareAuraScript(spell_kaelthas_flame_strike);

    bool Load() override
    {
        return GetUnitOwner()->IsCreature();
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->RemoveAllAuras();
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_FLAME_STRIKE_DAMAGE, true);
        GetUnitOwner()->ToCreature()->DespawnOrUnsummon(2s);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_kaelthas_flame_strike::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class lapseTeleport : public BasicEvent
{
public:
    lapseTeleport(Player* owner) : _owner(owner)
    {
    }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        if (_owner->IsBeingTeleportedNear())
            _owner->m_Events.AddEventAtOffset(new lapseTeleport(_owner), 1ms);
        else if (!_owner->IsBeingTeleported())
        {
            _owner->CastSpell(_owner, SPELL_GRAVITY_LAPSE_KNOCKBACK, true);
            _owner->CastSpell(_owner, SPELL_GRAVITY_LAPSE_AURA, true);
        }
        return true;
    }

private:
    Player* _owner;
};

class spell_kaelthas_gravity_lapse : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_gravity_lapse);

    bool Load() override
    {
        _currentSpellId = SPELL_GRAVITY_LAPSE_TELEPORT1;
        return true;
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr)
        {
            // TODO: Replace this placeholder with bot-safe Gravity Lapse flight handling.
            if (HasKaelEncounterBots(caster))
                return;
        }

        if (_currentSpellId < SPELL_GRAVITY_LAPSE_TELEPORT1 + 25)
            if (Player* target = GetHitPlayer())
            {
                GetCaster()->CastSpell(target, _currentSpellId++, true);
                target->m_Events.AddEventAtOffset(new lapseTeleport(target), 1ms);
            }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_gravity_lapse::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }

private:
    uint32 _currentSpellId;
};

class spell_kaelthas_nether_beam : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_nether_beam);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);

        if (Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr)
        {
            if (HasKaelEncounterBots(caster))
            {
                // TODO: Replace this placeholder with bot-safe Nether Beam movement/targeting once phase-five movement is supported.
                std::vector<Unit*> targets = GatherKaelEncounterTargets(caster, KAEL_ENCOUNTER_RANGE);
                while (targets.size() > 5)
                    targets.erase(targets.begin() + urand(0, uint32(targets.size() - 1)));

                for (Unit* target : targets)
                    GetCaster()->CastSpell(target, SPELL_NETHER_BEAM_DAMAGE, true);

                return;
            }
        }

        std::list<Unit*> targetList;
        for (ThreatReference const* ref : GetCaster()->GetThreatMgr().GetUnsortedThreatList())
        {
            if (Unit* target = ref->GetVictim())
            {
                if (target->IsPlayer())
                    targetList.push_back(target);
            }
        }

        Acore::Containers::RandomResize(targetList, 5);
        for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
            GetCaster()->CastSpell(*itr, SPELL_NETHER_BEAM_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_nether_beam::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_kaelthas_summon_nether_vapor : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_summon_nether_vapor);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr)
        {
            // TODO: Add bot-safe Nether Vapor handling before enabling these summons in bot raids.
            if (HasKaelEncounterBots(caster))
                return;
        }

        for (uint32 i = 0; i < 5; ++i)
            GetCaster()->SummonCreature(NPC_NETHER_VAPOR, GetCaster()->GetPositionX() + 6 * cos(i / 5.0f * 2 * M_PI), GetCaster()->GetPositionY() + 6 * std::sin(i / 5.0f * 2 * M_PI), GetCaster()->GetPositionZ() + 7.0f + i, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 30000);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_summon_nether_vapor::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_kael_pyroblast : public SpellScript
{
    PrepareSpellScript(spell_kael_pyroblast);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (GetCaster()->GetVictim())
        {
            if (Unit* victim = GetCaster()->GetVictim())
                targets.remove_if(Acore::ObjectGUIDCheck(victim->GetGUID(), false));
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kael_pyroblast::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_kaelthas_remove_enchanted_weapons : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_remove_enchanted_weapons);

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target || !target->IsPlayer())
            return;
        TriggerCastFlags triggerFlags = TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_IGNORE_POWER_AND_REAGENT_COST);
        target->CastSpell((Unit*)nullptr, 39498, triggerFlags);
        target->CastSpell((Unit*)nullptr, 39499, triggerFlags);
        target->CastSpell((Unit*)nullptr, 39500, triggerFlags);
        target->CastSpell((Unit*)nullptr, 39501, triggerFlags);
        target->CastSpell((Unit*)nullptr, 39502, triggerFlags);
        target->CastSpell((Unit*)nullptr, 39503, triggerFlags);
        target->CastSpell((Unit*)nullptr, 39504, triggerFlags);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_remove_enchanted_weapons::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 36092 - Kael Explodes
class spell_kaelthas_kael_explodes : public SpellScript
{
    PrepareSpellScript(spell_kaelthas_kael_explodes);

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        caster->CastSpell((Unit*)nullptr, SPELL_FLOATING_DROWNED, true);
        // caster->CastSpell((Unit*)nullptr, SPELL_KAEL_STUNNED, true);
        caster->PlayDirectSound(3320);
        caster->PlayDirectSound(10845);
        caster->PlayDirectSound(6539);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kaelthas_kael_explodes::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_boss_kaelthas()
{
    RegisterTheEyeAI(boss_kaelthas);
    RegisterTheEyeAI(npc_lord_sanguinar);
    RegisterTheEyeAI(npc_capernian);
    RegisterTheEyeAI(npc_telonicus);
    RegisterTheEyeAI(npc_thaladred);
    RegisterSpellScript(spell_kaelthas_remote_toy);
    RegisterSpellScript(spell_kaelthas_summon_weapons);
    RegisterSpellScript(spell_kaelthas_mind_control);
    RegisterSpellScript(spell_kaelthas_custom_frost_blast);
    RegisterSpellScript(spell_kaelthas_custom_frost_blast_aura);
    RegisterSpellScript(spell_kaelthas_burn);
    RegisterSpellScript(spell_kaelthas_flame_strike);
    RegisterSpellScript(spell_kaelthas_gravity_lapse);
    RegisterSpellScript(spell_kaelthas_nether_beam);
    RegisterSpellScript(spell_kaelthas_summon_nether_vapor);
    RegisterSpellScript(spell_kael_pyroblast);
    RegisterSpellScript(spell_kaelthas_remove_enchanted_weapons);
    RegisterSpellScript(spell_kaelthas_kael_explodes);
}
