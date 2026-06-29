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

#include "AreaTriggerScript.h"
#include "Config.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "Group.h"
#include "MapReference.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "ThreatManager.h"
#include "WorldSession.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "sunwell_plateau.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace BonusLootRolls
{
    void AddExplicitBossDeathOpportunities(Player* killer, Creature* killed);
}

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    void AnnounceMoveAwayFromMe(Creature* bot, uint32 spellId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Quotes
{
    YELL_INTRO                          = 0,
    YELL_INTRO_BREAK_ICE                = 1,
    YELL_INTRO_CHARGE                   = 2,
    YELL_INTRO_KILL_MADRIGOSA           = 3,
    YELL_INTRO_TAUNT                    = 4,

    YELL_AGGRO                          = 5,
    YELL_KILL                           = 6,
    YELL_LOVE                           = 7,
    YELL_BERSERK                        = 8,
    YELL_DEATH                          = 9,
};

enum Spells
{
    SPELL_METEOR_SLASH                  = 45150,
    SPELL_BURN_DAMAGE                   = 46394,
    SPELL_BURN                          = 45141,
    SPELL_BURN_SPREAD                   = 45151,
    SPELL_STOMP                         = 45185,
    SPELL_BERSERK                       = 26662,
    SPELL_DUAL_WIELD                    = 42459,
    SPELL_SUMMON_BRUTALLUS_DEATH_CLOUD  = 45884
};

enum Misc
{
    EVENT_SPELL_SLASH                   = 1,
    EVENT_SPELL_STOMP                   = 2,
    EVENT_SPELL_BURN                    = 3,
    EVENT_SPELL_BERSERK                 = 4,

    ACTION_START_EVENT                  = 1,
    ACTION_SPAWN_FELMYST                = 2
};

namespace
{
    constexpr float BRUTALLUS_BURN_TARGET_RANGE = 100.0f;
    constexpr float BRUTALLUS_BURN_SAFE_SCAN_RANGE = 60.0f;
    constexpr float BRUTALLUS_BURN_SAFE_DISTANCE = 11.0f;
    constexpr float BRUTALLUS_BURN_SAFE_MIN_BOSS_DISTANCE = 16.0f;
    constexpr float BRUTALLUS_BURN_SAFE_MAX_MOVE_DISTANCE = 90.0f;
    constexpr float BRUTALLUS_BURN_BOSS_RELOCATE_DISTANCE = 10.0f;
    constexpr float BRUTALLUS_BURN_DESTINATION_TOLERANCE = 2.0f;
    constexpr float BRUTALLUS_BURN_RUN_DISTANCE = 26.0f;
    constexpr float BRUTALLUS_BURN_RUN_ANGLE_OFFSET = 0.85f;
    constexpr float BRUTALLUS_TANK_PULL_THREAT = 750000.0f;
    constexpr float BRUTALLUS_TANK_ANCHOR_TOLERANCE = 2.0f;
    constexpr float BRUTALLUS_PI = 3.14159265358979323846f;
    constexpr char const* CONFIG_BRUTALLUS_METEOR_SLASH_DAMAGE_MULTIPLIER = "SunwellPlateau.Brutallus.MeteorSlashDamageMultiplier";
    constexpr char const* CONFIG_BRUTALLUS_BURN_DURATION_SECONDS = "SunwellPlateau.Brutallus.BurnDurationSeconds";
    constexpr char const* CONFIG_BRUTALLUS_MAX_ACTIVE_BURN_TARGETS = "SunwellPlateau.Brutallus.MaxActiveBurnTargets";

    float GetBrutallusMeteorSlashDamageMultiplier()
    {
        return std::clamp(sConfigMgr->GetOption<float>(CONFIG_BRUTALLUS_METEOR_SLASH_DAMAGE_MULTIPLIER, 1.0f), 0.0f, 5.0f);
    }

    int32 GetBrutallusBurnDurationMs()
    {
        int32 const seconds = std::clamp<int32>(sConfigMgr->GetOption<int32>(CONFIG_BRUTALLUS_BURN_DURATION_SECONDS, 60), 1, 300);
        return seconds * IN_MILLISECONDS;
    }

    uint32 GetBrutallusMaxActiveBurnTargets()
    {
        return std::clamp<int32>(sConfigMgr->GetOption<int32>(CONFIG_BRUTALLUS_MAX_ACTIVE_BURN_TARGETS, 3), 0, 25);
    }

    bool IsBrutallusNpcBotRangedOrHealer(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && !ai->IsTank() && !ai->IsOffTank() && !ai->HasRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF) && ai->HasRole(BOT_ROLE_RANGED | BOT_ROLE_HEAL);
    }

    bool IsBrutallusNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF));
    }

    bool IsBrutallusPlayerMarkedTank(Player* player)
    {
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (Group::member_citerator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & MEMBER_FLAG_MAINTANK;

        return false;
    }

    bool IsBrutallusTank(Unit* unit, Creature* source)
    {
        if (!unit)
            return false;

        if (source && (unit == source->GetThreatMgr().GetCurrentVictim() || unit == source->GetThreatMgr().GetLastVictim() || unit == source->GetVictim()))
            return true;

        if (IsBrutallusNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsBrutallusPlayerMarkedTank(player);

        return false;
    }

    bool IsValidBrutallusBurnTarget(Creature* source, Unit* unit, bool excludeTanks)
    {
        if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive())
            return false;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return false;

        if (unit->IsNPCBot() && !IsBrutallusNpcBotRangedOrHealer(unit))
            return false;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || unit->HasAura(SPELL_BURN_DAMAGE))
            return false;

        if (excludeTanks && IsBrutallusTank(unit, source))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (!source->IsWithinDistInMap(unit, BRUTALLUS_BURN_TARGET_RANGE))
            return false;

        if (!source->IsValidAttackTarget(unit) || !source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
            return false;

        return true;
    }

    void AddBrutallusBurnTarget(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, bool excludeTanks)
    {
        if (!IsValidBrutallusBurnTarget(source, unit, excludeTanks))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    Player* GetBrutallusOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetBrutallusGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    void AddBrutallusOwnedBots(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player, bool excludeTanks)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddBrutallusBurnTarget(source, targets, seen, pair.second, excludeTanks);
    }

    void AddBrutallusThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, bool excludeTanks)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddBrutallusBurnTarget(source, targets, seen, ref->GetVictim(), excludeTanks);
        }
    }

    std::vector<Unit*> GatherBrutallusBurnTargets(Creature* source, bool excludeTanks)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source)
            return targets;

        Unit* seed = source->GetThreatMgr().GetCurrentVictim();
        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetBrutallusGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddBrutallusBurnTarget(source, targets, seen, member, excludeTanks);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddBrutallusOwnedBots(source, targets, seen, player, excludeTanks);
            }
        }
        else
        {
            AddBrutallusBurnTarget(source, targets, seen, seed, excludeTanks);

            if (Player* player = GetBrutallusOwnerPlayer(seed))
            {
                AddBrutallusBurnTarget(source, targets, seen, player, excludeTanks);
                AddBrutallusOwnedBots(source, targets, seen, player, excludeTanks);
            }
        }

        AddBrutallusThreatTargets(source, targets, seen, excludeTanks);
        return targets;
    }

    Unit* SelectBrutallusBurnTarget(Creature* source)
    {
        std::vector<Unit*> targets = GatherBrutallusBurnTargets(source, true);
        if (targets.empty())
            targets = GatherBrutallusBurnTargets(source, false);

        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }

    bool IsBrutallusEncounterActive(Creature* brutallus)
    {
        return brutallus && brutallus->GetEntry() == NPC_BRUTALLUS && brutallus->IsAlive() && brutallus->IsInCombat();
    }

    Creature* GetBrutallusForUnit(Unit* unit)
    {
        if (!unit)
            return nullptr;

        InstanceScript* instance = unit->GetInstanceScript();
        return instance ? instance->GetCreature(DATA_BRUTALLUS) : nullptr;
    }

    void ForEachBrutallusPlayerAndBot(Creature* source, std::function<void(Unit*)> const& action)
    {
        if (!source || !source->GetMap())
            return;

        source->GetMap()->DoForAllPlayers([&](Player* player)
        {
            if (!player || !player->IsInMap(source) || !player->InSamePhase(source))
                return;

            action(player);

            if (!player->HaveBot())
                return;

            if (BotMgr* botMgr = player->GetBotMgr())
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    if (Creature* bot = pair.second)
                        if (bot->IsInMap(source) && bot->InSamePhase(source))
                            action(bot);
        });
    }

    void RemoveBrutallusBurnAuras(Unit* unit)
    {
        if (!unit)
            return;

        unit->RemoveAurasDueToSpell(SPELL_BURN_DAMAGE);
        unit->RemoveAurasDueToSpell(SPELL_BURN);
        unit->RemoveAurasDueToSpell(SPELL_BURN_SPREAD);
    }

    void RemoveAllBrutallusBurnAuras(Creature* source)
    {
        ForEachBrutallusPlayerAndBot(source, [](Unit* unit)
        {
            RemoveBrutallusBurnAuras(unit);
        });
    }

    bool TryGetBrutallusTankHomePosition(Creature* tank, Creature* brutallus, Position& destination)
    {
        if (!tank || !brutallus)
            return false;

        Position const& home = brutallus->GetHomePosition();
        float x = home.GetPositionX();
        float y = home.GetPositionY();
        float z = home.GetPositionZ();
        if (!tank->CanFly())
            tank->UpdateAllowedPositionZ(x, y, z);

        destination.Relocate(x, y, z, tank->GetAngle(brutallus));
        return destination.IsPositionValid() && tank->IsWithinLOS(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    }

    void MoveBrutallusTankBotToHome(Creature* tank, Creature* brutallus)
    {
        if (!tank || !tank->IsNPCBot() || !tank->IsAlive() || !brutallus)
            return;

        bot_ai* ai = tank->GetBotAI();
        if (!ai || ai->IAmFree())
            return;

        Position destination;
        if (!TryGetBrutallusTankHomePosition(tank, brutallus, destination))
            return;

        tank->InterruptNonMeleeSpells(false);
        tank->Attack(brutallus, true);

        if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
            ai->SetBotCommandState(BOT_COMMAND_STAY);

        if (tank->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY()) <= BRUTALLUS_TANK_ANCHOR_TOLERANCE)
        {
            tank->BotStopMovement();
            return;
        }

        ai->BotMovement(BOT_MOVE_POINT, &destination, nullptr, true);
    }

    void RestoreBrutallusTankBotCommandState(Creature* tank, uint32 originalCommandState)
    {
        if (!tank || !tank->IsNPCBot())
            return;

        bot_ai* ai = tank->GetBotAI();
        if (!ai)
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);

        if (!tank->IsAlive() || ai->IAmFree())
            return;

        if (originalCommandState & BOT_COMMAND_FULLSTOP)
            ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
        else if (originalCommandState & BOT_COMMAND_STAY)
            ai->SetBotCommandState(BOT_COMMAND_STAY);
        else if (originalCommandState & BOT_COMMAND_ATTACK)
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        else if (originalCommandState & BOT_COMMAND_COMBATRESET)
            ai->SetBotCommandState(BOT_COMMAND_COMBATRESET);
        else if (!(originalCommandState & BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
    }

    void PrepareBrutallusTankBots(Creature* brutallus, std::map<ObjectGuid, uint32>& originalStates)
    {
        if (!brutallus || !brutallus->CanHaveThreatList())
            return;

        ForEachBrutallusPlayerAndBot(brutallus, [brutallus, &originalStates](Unit* unit)
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (!bot || !bot->IsNPCBot())
                return;

            bool const isTank = IsBrutallusNpcBotTank(bot);
            if (!isTank && !IsBrutallusNpcBotRangedOrHealer(bot))
                return;

            if (bot_ai* ai = bot->GetBotAI())
                originalStates.try_emplace(bot->GetGUID(), ai->GetBotCommandState());

            if (!isTank)
                return;

            float const currentThreat = brutallus->GetThreatMgr().GetThreat(bot, true);
            if (currentThreat < BRUTALLUS_TANK_PULL_THREAT)
                brutallus->GetThreatMgr().AddThreat(bot, BRUTALLUS_TANK_PULL_THREAT - currentThreat, nullptr, true, true);

            MoveBrutallusTankBotToHome(bot, brutallus);
        });
    }

    void ReleaseBrutallusTankBots(Creature* brutallus, std::map<ObjectGuid, uint32>& originalStates)
    {
        if (!brutallus || originalStates.empty())
            return;

        ForEachBrutallusPlayerAndBot(brutallus, [&originalStates](Unit* unit)
        {
            Creature* tank = unit ? unit->ToCreature() : nullptr;
            if (!tank || !tank->IsNPCBot())
                return;

            auto itr = originalStates.find(tank->GetGUID());
            if (itr == originalStates.end())
                return;

            RestoreBrutallusTankBotCommandState(tank, itr->second);
        });

        originalStates.clear();
    }

    uint32 CountBrutallusBurnTargets(Creature* source)
    {
        uint32 count = 0;
        ForEachBrutallusPlayerAndBot(source, [&](Unit* unit)
        {
            if (unit && unit->IsAlive() && unit->HasAura(SPELL_BURN_DAMAGE))
                ++count;
        });

        return count;
    }

    bool CanApplyBrutallusBurn(Creature* brutallus, Unit* target)
    {
        if (!target || !target->IsAlive())
            return false;

        if (!IsBrutallusEncounterActive(brutallus))
            return true;

        if (IsBrutallusTank(target, brutallus))
            return false;

        uint32 const maxTargets = GetBrutallusMaxActiveBurnTargets();
        if (!maxTargets)
            return false;

        return target->HasAura(SPELL_BURN_DAMAGE) || CountBrutallusBurnTargets(brutallus) < maxTargets;
    }

    bool ShouldKeepBrutallusBurn(Creature* brutallus, Unit* target)
    {
        if (!target || !target->IsAlive())
            return false;

        if (!IsBrutallusEncounterActive(brutallus))
            return true;

        if (IsBrutallusTank(target, brutallus))
            return false;

        uint32 const maxTargets = GetBrutallusMaxActiveBurnTargets();
        if (!maxTargets)
            return false;

        return CountBrutallusBurnTargets(brutallus) <= maxTargets;
    }

    void AddBrutallusBurnObstacle(Creature* source, Unit* avoid, std::vector<Unit*>& obstacles, Unit* unit)
    {
        if (!source || !unit || unit == avoid || !unit->IsInWorld() || !unit->IsAlive())
            return;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return;

        // Existing Burn targets are already contaminated; do not make them chase
        // each other. Clean players/bots are the only spacing problem here.
        if (unit->HasAura(SPELL_BURN_DAMAGE))
            return;

        if (!source->IsWithinDistInMap(unit, BRUTALLUS_BURN_SAFE_SCAN_RANGE))
            return;

        obstacles.push_back(unit);
    }

    std::vector<Unit*> GatherBrutallusBurnObstacles(Creature* source, Unit* avoid)
    {
        std::vector<Unit*> obstacles;
        GuidSet seen;

        ForEachBrutallusPlayerAndBot(source, [&](Unit* unit)
        {
            if (!unit || !seen.insert(unit->GetGUID()).second)
                return;

            AddBrutallusBurnObstacle(source, avoid, obstacles, unit);
        });

        return obstacles;
    }

    float GetBrutallusBurnClearance(Position const& position, std::vector<Unit*> const& obstacles)
    {
        if (obstacles.empty())
            return BRUTALLUS_BURN_SAFE_DISTANCE * 2.0f;

        float clearance = std::numeric_limits<float>::max();
        for (Unit* unit : obstacles)
            if (unit)
                clearance = std::min(clearance, unit->GetExactDist2d(position.GetPositionX(), position.GetPositionY()));

        return clearance;
    }

    bool IsBrutallusBurnSafePosition(Creature* bot, Creature* brutallus, Position const& position)
    {
        if (!bot)
            return false;

        Creature* source = brutallus ? brutallus : bot;
        if (brutallus && brutallus->GetExactDist2d(position.GetPositionX(), position.GetPositionY()) < BRUTALLUS_BURN_SAFE_MIN_BOSS_DISTANCE)
            return false;

        if (!bot->IsWithinLOS(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()))
            return false;

        if (brutallus && !brutallus->IsWithinLOS(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()))
            return false;

        return GetBrutallusBurnClearance(position, GatherBrutallusBurnObstacles(source, bot)) >= BRUTALLUS_BURN_SAFE_DISTANCE;
    }

    Position GetBrutallusEncounterAnchor(Creature* brutallus)
    {
        if (!brutallus)
            return Position();

        Position const& home = brutallus->GetHomePosition();
        Position anchor = home;
        if (brutallus->GetExactDist2d(home.GetPositionX(), home.GetPositionY()) >= BRUTALLUS_BURN_BOSS_RELOCATE_DISTANCE)
            anchor.Relocate(brutallus->GetPositionX(), brutallus->GetPositionY(), brutallus->GetPositionZ(), brutallus->GetOrientation());

        return anchor;
    }

    bool TryGetBrutallusBurnSafePosition(Creature* bot, Creature* brutallus, Position& destination)
    {
        if (!bot || !brutallus)
            return false;

        Position anchor = GetBrutallusEncounterAnchor(brutallus);
        float const angle = Position::NormalizeOrientation(anchor.GetOrientation() + BRUTALLUS_PI + BRUTALLUS_BURN_RUN_ANGLE_OFFSET);
        float x = anchor.GetPositionX() + std::cos(angle) * BRUTALLUS_BURN_RUN_DISTANCE;
        float y = anchor.GetPositionY() + std::sin(angle) * BRUTALLUS_BURN_RUN_DISTANCE;
        float z = anchor.GetPositionZ();

        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        destination.Relocate(x, y, z, bot->GetAngle(brutallus));
        if (!destination.IsPositionValid())
            return false;

        if (bot->GetExactDist(destination) > BRUTALLUS_BURN_SAFE_MAX_MOVE_DISTANCE)
            return false;

        return bot->IsWithinLOS(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ()) &&
            brutallus->IsWithinLOS(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    }

    void RestoreBrutallusBurnBotCommandState(Creature* bot, uint32 originalCommandState)
    {
        if (!bot || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        if (!(originalCommandState & BOT_COMMAND_INACTION))
            ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

        if (!(originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY))
            ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);
        else
            ai->SetBotCommandState(originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY);

        ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);

        if (!bot->IsAlive() || ai->IAmFree())
            return;

        if (originalCommandState & BOT_COMMAND_FULLSTOP)
            ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
        else if (originalCommandState & BOT_COMMAND_STAY)
            ai->SetBotCommandState(BOT_COMMAND_STAY);
        else if (originalCommandState & BOT_COMMAND_ATTACK)
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        else if (originalCommandState & BOT_COMMAND_COMBATRESET)
            ai->SetBotCommandState(BOT_COMMAND_COMBATRESET);
        else if (!(originalCommandState & BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
    }

    bool HoldBrutallusBurnBot(Unit* target, uint32& originalCommandState)
    {
        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return false;

        if (!IsBrutallusNpcBotRangedOrHealer(bot))
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return false;

        originalCommandState = ai->GetBotCommandState();
        DBMFTABotCallouts::AnnounceMoveAwayFromMe(bot, SPELL_BURN_DAMAGE, "Burn", DBMFTABotCallouts::GetCooldownMs());

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();

        if (!(originalCommandState & BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_INACTION);

        ai->SetBotCommandState(BOT_COMMAND_STAY);
        return true;
    }

    void EnsureBrutallusBurnBotInaction(Creature* bot)
    {
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree() || ai->HasBotCommandState(BOT_COMMAND_INACTION))
            return;

        ai->SetBotCommandState(BOT_COMMAND_INACTION);
    }

    void StopBrutallusBurnBot(Creature* bot)
    {
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return;

        if (!ai->HasBotCommandState(BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_INACTION);

        bot->AttackStop();
        bot->BotStopMovement();
        if (!ai->HasBotCommandState(BOT_COMMAND_STAY) || bot->isMoving())
            ai->SetBotCommandState(BOT_COMMAND_STAY);
    }

    bool MoveBrutallusBurnBotTo(Creature* bot, Position const& destination)
    {
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return false;

        Position movePosition = destination;
        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        if (!ai->HasBotCommandState(BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_INACTION);
        if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
            ai->SetBotCommandState(BOT_COMMAND_STAY);
        ai->BotMovement(BOT_MOVE_POINT, &movePosition, nullptr, true);
        return true;
    }
}

struct boss_brutallus : public BossAI
{
    boss_brutallus(Creature* creature) : BossAI(creature, DATA_BRUTALLUS)
    {
        me->SetCorpseDelay(360);
    }

    void Reset() override
    {
        BossAI::Reset();
        DoCastSelf(SPELL_DUAL_WIELD, true);
        me->m_Events.KillAllEvents(false);
        ReleaseBrutallusTankBots(me, _tankAnchorOriginalStates);
        RemoveAllBrutallusBurnAuras(me);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (who->GetEntry() == NPC_MADRIGOSA)
            return;

        Talk(YELL_AGGRO);
        BossAI::JustEngagedWith(who);
        PrepareBrutallusTankBots(me, _tankAnchorOriginalStates);

        ScheduleEnrageTimer(SPELL_BERSERK, 6min, YELL_BERSERK);

        ScheduleTimedEvent(11s, [&] {
            DoCastVictim(SPELL_METEOR_SLASH);
        }, 12s);

        ScheduleTimedEvent(30s, [&] {
            DoCastVictim(SPELL_STOMP);
            Talk(YELL_LOVE);
        }, 30s);

        ScheduleTimedEvent(20s, [&] {
            if (Unit* target = SelectBrutallusBurnTarget(me))
                DoCast(target, SPELL_BURN);
        }, 20s);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && roll_chance_i(50))
            Talk(YELL_KILL);
    }

    void JustDied(Unit* killer) override
    {
        BonusLootRolls::AddExplicitBossDeathOpportunities(killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr, me);
        ReleaseBrutallusTankBots(me, _tankAnchorOriginalStates);
        RemoveAllBrutallusBurnAuras(me);

        BossAI::JustDied(killer);
        Talk(YELL_DEATH);

        DoCastAOE(SPELL_SUMMON_BRUTALLUS_DEATH_CLOUD, true);
        if (Creature* madrigosa = instance->GetCreature(DATA_MADRIGOSA))
            madrigosa->AI()->DoAction(ACTION_SPAWN_FELMYST);
    }

    void AttackStart(Unit* who) override
    {
        if (who->GetEntry() == NPC_MADRIGOSA)
            return;
        BossAI::AttackStart(who);
    }

private:
    std::map<ObjectGuid, uint32> _tankAnchorOriginalStates;
};

enum eMadrigosa
{
    EVENT_MAD_1                     = 1,
    EVENT_MAD_2                     = 2,
    EVENT_MAD_2_1                   = 200,
    EVENT_MAD_3                     = 3,
    EVENT_MAD_4                     = 4,
    EVENT_MAD_5                     = 5,
    EVENT_MAD_6                     = 6,
    EVENT_MAD_7                     = 7,
    EVENT_MAD_8                     = 8,
    EVENT_MAD_8_1                   = 800,
    EVENT_MAD_9                     = 9,
    EVENT_MAD_10                    = 10,
    EVENT_MAD_11                    = 11,
    EVENT_MAD_12                    = 12,
    EVENT_MAD_13                    = 13,
    EVENT_MAD_14                    = 14,
    EVENT_MAD_15                    = 15,
    EVENT_MAD_16                    = 16,
    EVENT_MAD_17                    = 17,
    EVENT_MAD_18                    = 18,
    EVENT_MAD_19                    = 19,
    EVENT_MAD_20                    = 20,
    EVENT_MAD_21                    = 21,
    EVENT_SPAWN_FELMYST             = 30,

    SAY_MAD_1                       = 0,
    SAY_MAD_2                       = 1,
    SAY_MAD_3                       = 2,
    SAY_MAD_4                       = 3,
    SAY_MAD_5                       = 4,

    SPELL_MADRIGOSA_FREEZE          = 46609,
    SPELL_MADRIGOSA_FROST_BREATH    = 45065,
    SPELL_MADRIGOSA_FROST_BLAST     = 44872,
    SPELL_MADRIGOSA_FROSTBOLT       = 44843,
    SPELL_MADRIGOSA_ENCAPSULATE     = 44883,

    SPELL_BRUTALLUS_CHARGE          = 44884,
    SPELL_BRUTALLUS_FEL_FIREBALL    = 44844,
    SPELL_BRUTALLUS_FLAME_RING      = 44873,
    SPELL_BRUTALLUS_BREAK_ICE       = 46637,
};

struct npc_madrigosa : public NullCreatureAI
{
    npc_madrigosa(Creature* creature) : NullCreatureAI(creature)
    {
        instance = creature->GetInstanceScript();
        creature->SetStandState(UNIT_STAND_STATE_DEAD);
        creature->SetDynamicFlag(UNIT_DYNFLAG_DEAD);

        if (instance->IsBossDone(DATA_BRUTALLUS))
            creature->SetVisible(false);
    }

    EventMap events;
    InstanceScript* instance;

    void DoAction(int32 param) override
    {
        if (param == ACTION_START_EVENT)
        {
            me->NearTeleportTo(1570.97f, 725.51f, 79.77f, 3.82f);
            me->SetDisableGravity(true);
            me->SetStandState(UNIT_STAND_STATE_STAND);
            me->RemoveDynamicFlag(UNIT_DYNFLAG_DEAD);
            me->SendMovementFlagUpdate();

            events.ScheduleEvent(EVENT_MAD_1, 2s);
        }
        else if (param == ACTION_SPAWN_FELMYST)
            events.ScheduleEvent(EVENT_SPAWN_FELMYST, 60s);
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);
        switch (events.ExecuteEvent())
        {
        case EVENT_MAD_1:
            me->SetVisible(true);
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                me->SetTarget(brutallus->GetGUID());
                brutallus->SetReactState(REACT_PASSIVE);
                brutallus->setActive(true);
            }
            me->GetMotionMaster()->MoveTakeoff(1, 1477.94f, 643.22f, 21.21f);
            me->AddUnitState(UNIT_STATE_NO_ENVIRONMENT_UPD);
            events.ScheduleEvent(EVENT_MAD_2, 4s);
            break;
        case EVENT_MAD_2:
            Talk(SAY_MAD_1);
            me->CastSpell(me, SPELL_MADRIGOSA_FREEZE, false);
            events.ScheduleEvent(EVENT_MAD_2_1, 1s);
            break;
        case EVENT_MAD_2_1:
            me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
            me->SetDisableGravity(false);
            me->CastSpell(me, SPELL_MADRIGOSA_FROST_BREATH, false);
            events.ScheduleEvent(EVENT_MAD_3, 7s);
            break;
        case EVENT_MAD_3:
            Talk(SAY_MAD_2);
            events.ScheduleEvent(EVENT_MAD_4, 7s);
            break;
        case EVENT_MAD_4:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
                brutallus->AI()->Talk(YELL_INTRO);
            events.ScheduleEvent(EVENT_MAD_5, 5s);
            break;
        case EVENT_MAD_5:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_ATTACK1H);
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_ATTACK1H);
            }
            events.ScheduleEvent(EVENT_MAD_6, 10s);
            break;
        case EVENT_MAD_6:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
            }
            me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
            me->SetDisableGravity(true);
            events.ScheduleEvent(EVENT_MAD_7, 4s);
            break;
        case EVENT_MAD_7:
            Talk(SAY_MAD_3);
            me->CastSpell(me, SPELL_MADRIGOSA_FROST_BLAST, false);
            events.ScheduleEvent(EVENT_MAD_8, 3s);
            events.ScheduleEvent(EVENT_MAD_8, 5s);
            events.ScheduleEvent(EVENT_MAD_8_1, 6s);
            events.ScheduleEvent(EVENT_MAD_8, 6500ms);
            events.ScheduleEvent(EVENT_MAD_8, 7500ms);
            events.ScheduleEvent(EVENT_MAD_8, 8500ms);
            events.ScheduleEvent(EVENT_MAD_8, 9500ms);
            events.ScheduleEvent(EVENT_MAD_9, 11s);
            events.ScheduleEvent(EVENT_MAD_8, 12s);
            events.ScheduleEvent(EVENT_MAD_8, 14s);
            break;
        case EVENT_MAD_8:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
                me->CastSpell(brutallus, SPELL_MADRIGOSA_FROSTBOLT, false);
            break;
        case EVENT_MAD_8_1:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
                brutallus->CastSpell(brutallus, SPELL_BRUTALLUS_FLAME_RING, false);
            break;
        case EVENT_MAD_9:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->RemoveAllAuras();
                brutallus->CastSpell(brutallus, SPELL_BRUTALLUS_FEL_FIREBALL, false);
                brutallus->AI()->Talk(YELL_INTRO_BREAK_ICE);
            }
            events.ScheduleEvent(EVENT_MAD_11, 6s);
            break;
            //case EVENT_MAD_10:
        case EVENT_MAD_11:
            me->SetDisableGravity(false);
            me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
            events.ScheduleEvent(EVENT_MAD_13, 2500ms);
            break;
        case EVENT_MAD_13:
            Talk(SAY_MAD_4);
            me->RemoveAllAuras();
            me->CastSpell(me, SPELL_MADRIGOSA_ENCAPSULATE, false);
            events.ScheduleEvent(EVENT_MAD_14, 2s);
            break;
        case EVENT_MAD_14:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->SetDisableGravity(true);
                brutallus->GetMotionMaster()->MovePoint(0, brutallus->GetPositionX(), brutallus->GetPositionY() - 30.0f, brutallus->GetPositionZ() + 15.0f, FORCED_MOVEMENT_NONE, 0.f, 0.f, false, true);
            }
            events.ScheduleEvent(EVENT_MAD_15, 10s);
            break;
        case EVENT_MAD_15:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->RemoveAllAuras();
                brutallus->SetDisableGravity(false);
                brutallus->GetMotionMaster()->MoveFall();
                brutallus->AI()->Talk(YELL_INTRO_CHARGE);
            }
            events.ScheduleEvent(EVENT_MAD_16, 1400ms);
            break;
        case EVENT_MAD_16:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
                brutallus->CastSpell(me, SPELL_BRUTALLUS_CHARGE, true);
            events.ScheduleEvent(EVENT_MAD_17, 1200ms);
            break;
        case EVENT_MAD_17:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
                brutallus->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
            events.ScheduleEvent(EVENT_MAD_18, 500ms);
            break;
        case EVENT_MAD_18:
            Talk(SAY_MAD_5);
            me->SetDynamicFlag(UNIT_DYNFLAG_DEAD);
            me->SetStandState(UNIT_STAND_STATE_DEAD);
            events.ScheduleEvent(EVENT_MAD_19, 6s);
            break;
        case EVENT_MAD_19:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
                brutallus->AI()->Talk(YELL_INTRO_KILL_MADRIGOSA);
            events.ScheduleEvent(EVENT_MAD_20, 7s);
            break;
        case EVENT_MAD_20:
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->SetFaction(FACTION_FRIENDLY);
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->AI()->Talk(YELL_INTRO_TAUNT);
                brutallus->CastSpell(brutallus, SPELL_BRUTALLUS_BREAK_ICE, false);
            }
            events.ScheduleEvent(EVENT_MAD_21, 4s);
            break;
        case EVENT_MAD_21:
            if (Creature* brutallus = instance->GetCreature(DATA_BRUTALLUS))
            {
                brutallus->SetReactState(REACT_AGGRESSIVE);
                brutallus->SetHealth(brutallus->GetMaxHealth());
                brutallus->AI()->EnterEvadeMode();
                brutallus->setActive(false);
            }
            break;
        case EVENT_SPAWN_FELMYST:
            DoCastAOE(SPELL_SUMMON_FELBLAZE, true);
            me->DespawnOrUnsummon(1ms);
            break;
        }
    }
};

class spell_madrigosa_activate_barrier : public SpellScript
{
    PrepareSpellScript(spell_madrigosa_activate_barrier);

    void HandleActivateObject(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (GameObject* go = GetHitGObj())
        {
            go->SetGoState(GO_STATE_READY);
            if (Map* map = go->GetMap())
                map->DoForAllPlayers([&](Player* player) {
                    UpdateData data;
                    WorldPacket pkt;
                    go->BuildValuesUpdateBlockForPlayer(&data, player);
                    data.BuildPacket(pkt);
                    player->SendDirectMessage(&pkt);
                });
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_madrigosa_activate_barrier::HandleActivateObject, EFFECT_0, SPELL_EFFECT_ACTIVATE_OBJECT);
    }
};

class spell_madrigosa_deactivate_barrier : public SpellScript
{
    PrepareSpellScript(spell_madrigosa_deactivate_barrier);

    void HandleActivateObject(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (GameObject* go = GetHitGObj())
        {
            go->SetGoState(GO_STATE_ACTIVE);
            if (Map* map = go->GetMap())
                map->DoForAllPlayers([&](Player* player) {
                    UpdateData data;
                    WorldPacket pkt;
                    go->BuildValuesUpdateBlockForPlayer(&data, player);
                    data.BuildPacket(pkt);
                    player->SendDirectMessage(&pkt);
                });
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_madrigosa_deactivate_barrier::HandleActivateObject, EFFECT_0, SPELL_EFFECT_ACTIVATE_OBJECT);
    }
};

class spell_brutallus_burn : public SpellScript
{
    PrepareSpellScript(spell_brutallus_burn);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BURN, SPELL_BURN_SPREAD, SPELL_BURN_DAMAGE });
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
        {
            Creature* brutallus = GetCaster() ? GetCaster()->ToCreature() : nullptr;
            if (!IsBrutallusEncounterActive(brutallus))
                brutallus = GetBrutallusForUnit(target);

            if (GetSpellInfo()->Id == SPELL_BURN_SPREAD && target->HasAura(SPELL_BURN_DAMAGE))
                return;

            if (!CanApplyBrutallusBurn(brutallus, target))
            {
                RemoveBrutallusBurnAuras(target);
                return;
            }

            if (!target->HasAura(SPELL_BURN_DAMAGE))
            {
                target->CastSpell(target, SPELL_BURN_DAMAGE, true);
                if (Aura* burn = target->GetAura(SPELL_BURN_DAMAGE))
                {
                    int32 const durationMs = GetBrutallusBurnDurationMs();
                    burn->SetMaxDuration(durationMs);
                    burn->SetDuration(durationMs);
                }

                if (!ShouldKeepBrutallusBurn(brutallus, target))
                {
                    RemoveBrutallusBurnAuras(target);
                    return;
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brutallus_burn::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_brutallus_burn_damage : public AuraScript
{
    PrepareAuraScript(spell_brutallus_burn_damage);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BURN_DAMAGE });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        Creature* brutallus = GetBrutallusForUnit(target);
        if (!ShouldKeepBrutallusBurn(brutallus, target))
        {
            RemoveBrutallusBurnAuras(target);
            return;
        }

        Creature* bot = target ? target->ToCreature() : nullptr;
        _handledBot = HoldBrutallusBurnBot(target, _originalCommandState);
        if (!_handledBot || !bot)
            return;

        UpdateBossAnchor(brutallus);
        SettleBurnBot(bot, brutallus, true);
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        Creature* brutallus = GetBrutallusForUnit(target);
        if (!ShouldKeepBrutallusBurn(brutallus, target))
        {
            RemoveBrutallusBurnAuras(target);
            return;
        }

        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!_handledBot || !bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        EnsureBrutallusBurnBotInaction(bot);

        if (HasBossMovedSignificantly(brutallus))
        {
            SettleBurnBot(bot, brutallus, true);
            return;
        }

        if (_movingToSafeSpot)
        {
            if (bot->GetExactDist2d(_safeSpot.GetPositionX(), _safeSpot.GetPositionY()) <= BRUTALLUS_BURN_DESTINATION_TOLERANCE)
            {
                StopBrutallusBurnBot(bot);
                _movingToSafeSpot = false;
                _settled = true;
            }
            else if (!bot->isMoving())
            {
                Position current;
                current.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
                if (!IsBrutallusBurnSafePosition(bot, brutallus, current) && !_retriedSafeMove)
                {
                    _retriedSafeMove = true;
                    Position destination;
                    if (TryGetBrutallusBurnSafePosition(bot, brutallus, destination))
                    {
                        _safeSpot = destination;
                        _hasSafeSpot = true;
                        _movingToSafeSpot = MoveBrutallusBurnBotTo(bot, _safeSpot);
                        return;
                    }
                }

                StopBrutallusBurnBot(bot);
                _movingToSafeSpot = false;
                _settled = true;
            }

            return;
        }

        if (aurEff->GetTickNumber() % 3 == 0)
        {
            if (!_settled)
            {
                SettleBurnBot(bot, brutallus, false);
                return;
            }

            Position current;
            current.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
            bool const atAssignedSpot = _hasSafeSpot &&
                bot->GetExactDist2d(_safeSpot.GetPositionX(), _safeSpot.GetPositionY()) <= BRUTALLUS_BURN_DESTINATION_TOLERANCE;

            if (!atAssignedSpot && !IsBrutallusBurnSafePosition(bot, brutallus, current))
            {
                SettleBurnBot(bot, brutallus, true);
                return;
            }
        }

        if (_settled && bot->isMoving())
            StopBrutallusBurnBot(bot);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!_handledBot)
            return;

        if (Creature* bot = GetTarget() ? GetTarget()->ToCreature() : nullptr)
            RestoreBrutallusBurnBotCommandState(bot, _originalCommandState);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_brutallus_burn_damage::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_brutallus_burn_damage::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
        AfterEffectRemove += AuraEffectRemoveFn(spell_brutallus_burn_damage::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }

private:
    void UpdateBossAnchor(Creature* brutallus)
    {
        if (!brutallus)
        {
            _hasBossAnchor = false;
            return;
        }

        _bossAnchor.Relocate(brutallus->GetPositionX(), brutallus->GetPositionY(), brutallus->GetPositionZ(), brutallus->GetOrientation());
        _hasBossAnchor = true;
    }

    bool HasBossMovedSignificantly(Creature* brutallus) const
    {
        if (!brutallus || !_hasBossAnchor)
            return false;

        return brutallus->GetExactDist2d(_bossAnchor.GetPositionX(), _bossAnchor.GetPositionY()) >= BRUTALLUS_BURN_BOSS_RELOCATE_DISTANCE;
    }

    void SettleBurnBot(Creature* bot, Creature* brutallus, bool forceRecheck)
    {
        if (!bot)
            return;

        Position current;
        current.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());

        if (!forceRecheck && IsBrutallusBurnSafePosition(bot, brutallus, current))
        {
            StopBrutallusBurnBot(bot);
            _hasSafeSpot = false;
            _movingToSafeSpot = false;
            _settled = true;
            UpdateBossAnchor(brutallus);
            return;
        }

        Position destination;
        if (TryGetBrutallusBurnSafePosition(bot, brutallus, destination))
        {
            _safeSpot = destination;
            _hasSafeSpot = true;
            _movingToSafeSpot = MoveBrutallusBurnBotTo(bot, _safeSpot);
            _settled = false;
            _retriedSafeMove = false;
            UpdateBossAnchor(brutallus);
            return;
        }

        StopBrutallusBurnBot(bot);
        _hasSafeSpot = false;
        _movingToSafeSpot = false;
        _settled = true;
        UpdateBossAnchor(brutallus);
    }

    bool _handledBot = false;
    bool _hasBossAnchor = false;
    bool _hasSafeSpot = false;
    bool _movingToSafeSpot = false;
    bool _settled = false;
    bool _retriedSafeMove = false;
    uint32 _originalCommandState = 0;
    Position _bossAnchor;
    Position _safeSpot;
};

class spell_brutallus_meteor_slash : public SpellScript
{
    PrepareSpellScript(spell_brutallus_meteor_slash);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_METEOR_SLASH });
    }

    void ScaleDamage()
    {
        int32 damage = GetHitDamage();
        if (damage <= 0)
            return;

        float const multiplier = GetBrutallusMeteorSlashDamageMultiplier();
        if (multiplier == 1.0f)
            return;

        double scaled = double(damage) * double(multiplier);
        scaled = std::clamp(scaled, double(std::numeric_limits<int32>::min()), double(std::numeric_limits<int32>::max()));
        SetHitDamage(int32(scaled));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_brutallus_meteor_slash::ScaleDamage);
    }
};

class at_sunwell_madrigosa : public OnlyOnceAreaTriggerScript
{
public:
    at_sunwell_madrigosa() : OnlyOnceAreaTriggerScript("at_sunwell_madrigosa") {}

    bool _OnTrigger(Player* player, AreaTrigger const* /*trigger*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
            if (!instance->IsBossDone(DATA_BRUTALLUS))
                if (Creature* creature = instance->GetCreature(DATA_MADRIGOSA))
                    creature->AI()->DoAction(ACTION_START_EVENT);

        return true;
    }
};

void AddSC_boss_brutallus()
{
    RegisterSunwellPlateauCreatureAI(boss_brutallus);
    RegisterSunwellPlateauCreatureAI(npc_madrigosa);
    RegisterSpellScript(spell_madrigosa_activate_barrier);
    RegisterSpellScript(spell_madrigosa_deactivate_barrier);
    RegisterSpellScript(spell_brutallus_burn);
    RegisterSpellScript(spell_brutallus_burn_damage);
    RegisterSpellScript(spell_brutallus_meteor_slash);
    new at_sunwell_madrigosa();
}
