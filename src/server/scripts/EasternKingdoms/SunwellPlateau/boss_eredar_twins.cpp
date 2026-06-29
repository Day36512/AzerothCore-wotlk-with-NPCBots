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
#include "Containers.h"
#include "CreatureScript.h"
#include "GameObjectAI.h"
#include "GameObjectScript.h"
#include "Group.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "SpellAuraEffects.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "sunwell_plateau.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

enum Quotes
{
    YELL_INTRO_SAC              = 0,
    YELL_SAC_DEAD               = 4,
    EMOTE_SHADOW_NOVA           = 5,
    YELL_ENRAGE                 = 6,
    YELL_SISTER_ALYTHESS_DEAD   = 7,
    YELL_SAC_KILL               = 8,
    YELL_SHADOW_NOVA            = 9,

    YELL_INTRO_ALY              = 0,
    EMOTE_CONFLAGRATION         = 4,
    YELL_ALY_KILL               = 5,
    YELL_ALY_DEAD               = 6,
    YELL_SISTER_SACROLASH_DEAD  = 7,
    YELL_CANFLAGRATION          = 8,
    YELL_BERSERK                = 9
};

enum Spells
{
    //Shared spells
    SPELL_ENRAGE                = 46587,
    SPELL_EMPOWER               = 45366,
    SPELL_DARK_FLAME            = 45345,
    SPELL_FIREBLAST             = 45232,

    //Lady Sacrolash spells
    SPELL_SHADOWFORM            = 45455,
    SPELL_DARK_TOUCHED          = 45347,
    SPELL_SHADOW_BLADES         = 45248,
    SPELL_SHADOW_NOVA           = 45329,
    SPELL_CONFOUNDING_BLOW      = 45256,

    //Grand Warlock Alythess spells
    SPELL_FIREFORM              = 45457,
    SPELL_FLAME_TOUCHED         = 45348,
    SPELL_PYROGENICS            = 45230,
    SPELL_CONFLAGRATION         = 45342,
    SPELL_FLAME_SEAR            = 46771,
    SPELL_BLAZE                 = 45235,
    SPELL_BLAZE_SUMMON          = 45236,

    // NPCBot opening consumables
    SPELL_FLASK_OF_CHROMATIC_WONDER = 42735,
    SPELL_SCROLL_OF_STAMINA     = 48101,

    // NPCBot Conflagration responses
    SPELL_GNOMISH_CLOAKING_DEVICE = 4079,
    SPELL_SWIFTNESS_POTION      = 2379,
    SPELL_MAJOR_FIRE_PROTECTION_POTION = 28511,
    SPELL_FROZEN_RUNE           = 29432,
    SPELL_NIGHTMARE_SEED        = 28726,
    SPELL_FEL_BLOSSOM           = 28527,
    SPELL_ICE_BLOCK             = 45438,
    SPELL_DIVINE_SHIELD         = 642,
    SPELL_CLOAK_OF_SHADOWS      = 31224,
    SPELL_VANISH                = 1856
};

enum TwinPhases
{
    ACTION_SISTER_DIED          = 1,
    GROUP_SPECIAL_ABILITY       = 1,
    GROUP_PYROGENICS            = 2,
    GROUP_FLAME_SEAR            = 3
};

namespace
{
    constexpr float EREDAR_TWINS_BOT_CONSUMABLE_RANGE = 150.0f;

    bool IsOwnedEredarTwinsNpcBot(Unit const* unit)
    {
        Creature const* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }

    bool IsEredarTwinsConsumableBot(Unit const* unit)
    {
        return unit && unit->IsAlive() && unit->IsInWorld() && IsOwnedEredarTwinsNpcBot(unit);
    }

    bool HasEredarTwinsFlaskAura(Unit const* unit)
    {
        if (!unit)
            return false;

        for (Unit::AuraApplicationMap::value_type const& pair : unit->GetAppliedAuras())
        {
            Aura const* aura = pair.second ? pair.second->GetBase() : nullptr;
            if (!aura)
                continue;

            uint32 const spellId = aura->GetId();
            if (sSpellMgr->IsSpellMemberOfSpellGroup(spellId, SPELL_GROUP_ELIXIR_BATTLE) &&
                sSpellMgr->IsSpellMemberOfSpellGroup(spellId, SPELL_GROUP_ELIXIR_GUARDIAN))
                return true;
        }

        return false;
    }

    void CastEredarTwinsKnownSelfSpell(Creature* bot, uint32 spellId)
    {
        if (bot && sSpellMgr->GetSpellInfo(spellId))
            bot->CastSpell(bot, spellId, true);
    }

    void AddEredarTwinsConsumableBot(Creature* source, std::vector<Creature*>& bots, GuidSet& seen, Unit* unit)
    {
        if (!source || !IsEredarTwinsConsumableBot(unit))
            return;

        Creature* bot = unit->ToCreature();
        if (!bot || !bot->IsInMap(source) || !bot->InSamePhase(source))
            return;

        if (!source->IsWithinDistInMap(bot, EREDAR_TWINS_BOT_CONSUMABLE_RANGE))
            return;

        if (!seen.insert(bot->GetGUID()).second)
            return;

        bots.push_back(bot);
    }

    Player* GetEredarTwinsOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetEredarTwinsGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    void AddEredarTwinsOwnedBots(Creature* source, std::vector<Creature*>& bots, GuidSet& seen, Player* player)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddEredarTwinsConsumableBot(source, bots, seen, pair.second);
    }

    void AddEredarTwinsThreatBots(Creature* source, std::vector<Creature*>& bots, GuidSet& seen)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddEredarTwinsConsumableBot(source, bots, seen, ref->GetVictim());
        }
    }

    std::vector<Creature*> GatherEredarTwinsConsumableBots(Creature* source, Unit* seed)
    {
        std::vector<Creature*> bots;
        GuidSet seen;

        if (!source)
            return bots;

        if (!seed)
            seed = source->GetThreatMgr().GetCurrentVictim();

        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetEredarTwinsGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddEredarTwinsConsumableBot(source, bots, seen, member);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddEredarTwinsOwnedBots(source, bots, seen, player);
            }
        }
        else
        {
            AddEredarTwinsConsumableBot(source, bots, seen, seed);

            if (Player* player = GetEredarTwinsOwnerPlayer(seed))
            {
                AddEredarTwinsConsumableBot(source, bots, seen, player);
                AddEredarTwinsOwnedBots(source, bots, seen, player);
            }
        }

        AddEredarTwinsThreatBots(source, bots, seen);
        return bots;
    }

    void CastEredarTwinsOpeningBotConsumables(Creature* source, Unit* seed)
    {
        for (Creature* bot : GatherEredarTwinsConsumableBots(source, seed))
        {
            if (HasEredarTwinsFlaskAura(bot))
                continue;

            CastEredarTwinsKnownSelfSpell(bot, SPELL_FLASK_OF_CHROMATIC_WONDER);
            CastEredarTwinsKnownSelfSpell(bot, SPELL_SCROLL_OF_STAMINA);
        }
    }

    constexpr float EREDAR_TWINS_ABILITY_TARGET_RANGE = 120.0f;
    constexpr float EREDAR_TWINS_CONFLAGRATION_SAFE_DISTANCE = 22.0f;
    constexpr float EREDAR_TWINS_ALYTHESS_WARLOCK_THREAT = 3000000.0f;
    constexpr float EREDAR_TWINS_ALYTHESS_PRIMARY_WARLOCK_THREAT = 4000000.0f;

    struct EredarTwinsConflagrationBotState
    {
        ObjectGuid encounterGuid;
        uint32 instanceId = 0;
        bool cloakOfShadows = false;
        bool vanish = false;
        bool iceBlock = false;
        bool divineShield = false;
        bool cloakDevice = false;
    };

    std::unordered_map<ObjectGuid::LowType, EredarTwinsConflagrationBotState> EredarTwinsConflagrationStates;

    bool IsEredarTwinsWarlock(Unit const* unit)
    {
        if (!unit)
            return false;

        if (Player const* player = unit->ToPlayer())
            return player->getClass() == CLASS_WARLOCK;

        Creature const* creature = unit->ToCreature();
        bot_ai const* ai = creature && creature->IsNPCBot() ? creature->GetBotAI() : nullptr;
        return ai && ai->GetBotClass() == BOT_CLASS_WARLOCK;
    }

    bool IsNpcBotTank(Unit const* unit)
    {
        Creature const* creature = unit ? unit->ToCreature() : nullptr;
        bot_ai const* ai = creature && creature->IsNPCBot() ? creature->GetBotAI() : nullptr;
        return ai && (ai->IsTank(creature) || ai->IsOffTank(creature) || ai->HasRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF));
    }

    bool IsMarkedPlayerTank(Unit const* unit)
    {
        Player const* player = unit ? unit->ToPlayer() : nullptr;
        Group const* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (auto const& slot : group->GetMemberSlots())
            if (slot.guid == player->GetGUID())
                return (slot.flags & (MEMBER_FLAG_MAINTANK | MEMBER_FLAG_MAINASSIST)) != 0;

        return false;
    }

    bool IsCurrentOrRecentTankTarget(Creature const* source, Unit const* unit)
    {
        if (!source || !unit)
            return false;

        Creature* mutableSource = const_cast<Creature*>(source);
        return mutableSource->GetVictim() == unit ||
            mutableSource->GetThreatMgr().GetCurrentVictim() == unit ||
            mutableSource->GetThreatMgr().GetLastVictim() == unit;
    }

    bool IsEredarTwinsTankTarget(Creature const* source, Unit const* unit)
    {
        if (IsNpcBotTank(unit) || IsMarkedPlayerTank(unit) || IsEredarTwinsWarlock(unit) || IsCurrentOrRecentTankTarget(source, unit))
            return true;

        if (!source)
            return false;

        Creature* mutableSource = const_cast<Creature*>(source);
        if (InstanceScript* instance = mutableSource->GetInstanceScript())
        {
            if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
                if (IsCurrentOrRecentTankTarget(alythess, unit))
                    return true;

            if (Creature* sacrolash = instance->GetCreature(DATA_SACROLASH))
                if (IsCurrentOrRecentTankTarget(sacrolash, unit))
                    return true;
        }

        return false;
    }

    bool IsValidEredarTwinsAbilityTarget(Creature const* source, Unit const* unit)
    {
        if (!source || !unit || !unit->IsAlive() || !unit->IsInWorld() || unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        if (!unit->IsPlayer() && !IsOwnedEredarTwinsNpcBot(unit))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (!source->IsWithinDistInMap(unit, EREDAR_TWINS_ABILITY_TARGET_RANGE) || !source->IsWithinLOSInMap(unit))
            return false;

        return source->IsValidAttackTarget(unit);
    }

    void AddEredarTwinsAbilityTarget(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit)
    {
        if (!IsValidEredarTwinsAbilityTarget(source, unit))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    void AddEredarTwinsOwnedBotTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddEredarTwinsAbilityTarget(source, targets, seen, pair.second);
    }

    std::vector<Unit*> GatherEredarTwinsAbilityTargets(Creature* source, Unit* seed)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source)
            return targets;

        if (!seed)
            seed = source->GetThreatMgr().GetCurrentVictim();

        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetEredarTwinsGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddEredarTwinsAbilityTarget(source, targets, seen, member);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddEredarTwinsOwnedBotTargets(source, targets, seen, player);
            }
        }
        else
        {
            AddEredarTwinsAbilityTarget(source, targets, seen, seed);

            if (Player* player = GetEredarTwinsOwnerPlayer(seed))
            {
                AddEredarTwinsAbilityTarget(source, targets, seen, player);
                AddEredarTwinsOwnedBotTargets(source, targets, seen, player);
            }
        }

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            Unit* victim = ref->GetVictim();
            AddEredarTwinsAbilityTarget(source, targets, seen, victim);

            if (Player* player = GetEredarTwinsOwnerPlayer(victim))
            {
                AddEredarTwinsAbilityTarget(source, targets, seen, player);
                AddEredarTwinsOwnedBotTargets(source, targets, seen, player);
            }
        }

        return targets;
    }

    Unit* SelectEredarTwinsRandomTarget(Creature* source, Unit* seed, bool excludeTanks)
    {
        std::vector<Unit*> targets = GatherEredarTwinsAbilityTargets(source, seed);

        targets.erase(std::remove_if(targets.begin(), targets.end(), [source, excludeTanks](Unit* target)
        {
            return !target || (excludeTanks && IsEredarTwinsTankTarget(source, target));
        }), targets.end());

        if (targets.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(targets);
    }

    Unit* SelectEredarTwinsThreatTarget(Creature* source, Creature* threatSource, Unit* seed, uint8 topCount, bool excludeTanks)
    {
        if (!threatSource)
            threatSource = source;

        std::vector<Unit*> targets = GatherEredarTwinsAbilityTargets(source, seed);
        targets.erase(std::remove_if(targets.begin(), targets.end(), [source, excludeTanks](Unit* target)
        {
            return !target || (excludeTanks && IsEredarTwinsTankTarget(source, target));
        }), targets.end());

        if (targets.empty())
            return nullptr;

        std::sort(targets.begin(), targets.end(), [threatSource](Unit* left, Unit* right)
        {
            return threatSource->GetThreatMgr().GetThreat(left, true) > threatSource->GetThreatMgr().GetThreat(right, true);
        });

        if (topCount && targets.size() > topCount)
            targets.resize(topCount);

        return Acore::Containers::SelectRandomContainerElement(targets);
    }

    ObjectGuid GetEredarTwinsEncounterGuid(Creature* source)
    {
        if (source)
        {
            if (InstanceScript* instance = source->GetInstanceScript())
            {
                ObjectGuid guid = instance->GetGuidData(DATA_SACROLASH);
                if (guid != ObjectGuid::Empty)
                    return guid;

                guid = instance->GetGuidData(DATA_ALYTHESS);
                if (guid != ObjectGuid::Empty)
                    return guid;
            }

            return source->GetGUID();
        }

        return ObjectGuid::Empty;
    }

    EredarTwinsConflagrationBotState& GetEredarTwinsConflagrationState(Creature* source, Creature* bot)
    {
        EredarTwinsConflagrationBotState& state = EredarTwinsConflagrationStates[bot->GetGUID().GetCounter()];
        uint32 const instanceId = bot->GetInstanceId();
        ObjectGuid const encounterGuid = GetEredarTwinsEncounterGuid(source);

        if (state.instanceId != instanceId || state.encounterGuid != encounterGuid)
        {
            state = EredarTwinsConflagrationBotState();
            state.instanceId = instanceId;
            state.encounterGuid = encounterGuid;
        }

        return state;
    }

    bool TryCastEredarTwinsBotClassSpell(Creature* bot, bot_ai* ai, uint32 spellId)
    {
        if (!bot || !ai || !ai->HasSpell(spellId) || !ai->IsSpellReady(spellId, ai->GetLastDiff(), false) || !sSpellMgr->GetSpellInfo(spellId))
            return false;

        bot->InterruptNonMeleeSpells(false);
        bot->CastSpell(bot, spellId, true);
        return true;
    }

    float GetEredarTwinsConflagrationClearance(Position const& position, std::vector<Unit*> const& nearbyUnits, Unit const* ignored)
    {
        float clearance = std::numeric_limits<float>::max();

        for (Unit const* unit : nearbyUnits)
        {
            if (!unit || unit == ignored || !unit->IsAlive())
                continue;

            clearance = std::min(clearance, position.GetExactDist2d(unit->GetPositionX(), unit->GetPositionY()));
        }

        return clearance;
    }

    bool FindEredarTwinsConflagrationSafeSpot(Creature* source, Creature* bot, Position& destination)
    {
        if (!bot)
            return false;

        std::vector<Unit*> nearbyUnits = GatherEredarTwinsAbilityTargets(source, bot);
        float repelX = 0.0f;
        float repelY = 0.0f;

        for (Unit const* unit : nearbyUnits)
        {
            if (!unit || unit == bot || !unit->IsAlive())
                continue;

            float const dx = bot->GetPositionX() - unit->GetPositionX();
            float const dy = bot->GetPositionY() - unit->GetPositionY();
            float const dist = std::max(std::sqrt(dx * dx + dy * dy), 0.1f);
            float const weight = std::max(0.0f, EREDAR_TWINS_ABILITY_TARGET_RANGE - dist);
            repelX += dx / dist * weight;
            repelY += dy / dist * weight;
        }

        if (std::fabs(repelX) < 0.01f && std::fabs(repelY) < 0.01f && source)
        {
            repelX = bot->GetPositionX() - source->GetPositionX();
            repelY = bot->GetPositionY() - source->GetPositionY();
        }

        float const baseAngle = Position::NormalizeOrientation(std::atan2(repelY, repelX));
        std::array<float, 6> const distances = { 24.0f, 30.0f, 36.0f, 42.0f, 50.0f, 58.0f };
        std::array<float, 11> const offsets = { 0.0f, 0.35f, -0.35f, 0.7f, -0.7f, 1.1f, -1.1f, 1.57f, -1.57f, 2.35f, -2.35f };

        float bestClearance = 0.0f;
        Position bestPosition;

        for (float distance : distances)
        {
            for (float offset : offsets)
            {
                Position candidate = bot->GetFirstCollisionPosition(distance, Position::NormalizeOrientation(baseAngle + offset - bot->GetOrientation()));
                float x = candidate.GetPositionX();
                float y = candidate.GetPositionY();
                float z = candidate.GetPositionZ();
                bot->UpdateAllowedPositionZ(x, y, z);
                candidate.Relocate(x, y, z, bot->GetOrientation());

                if (!bot->IsWithinLOS(x, y, z))
                    continue;

                if (source && !source->IsWithinLOS(x, y, z))
                    continue;

                float const clearance = GetEredarTwinsConflagrationClearance(candidate, nearbyUnits, bot);
                if (clearance > bestClearance)
                {
                    bestClearance = clearance;
                    bestPosition = candidate;
                }

                if (clearance >= EREDAR_TWINS_CONFLAGRATION_SAFE_DISTANCE)
                {
                    destination = candidate;
                    return true;
                }
            }
        }

        if (bestClearance > 0.0f)
        {
            destination = bestPosition;
            return true;
        }

        return false;
    }

    void MoveEredarTwinsConflagrationBotToSafeSpot(Creature* source, Creature* bot, bot_ai* ai)
    {
        if (!bot || !ai)
            return;

        CastEredarTwinsKnownSelfSpell(bot, SPELL_SWIFTNESS_POTION);

        std::array<uint32, 4> const protectionSpells =
        {
            SPELL_FEL_BLOSSOM,
            SPELL_MAJOR_FIRE_PROTECTION_POTION,
            SPELL_NIGHTMARE_SEED,
            SPELL_FROZEN_RUNE
        };

        CastEredarTwinsKnownSelfSpell(bot, protectionSpells[urand(0, protectionSpells.size() - 1)]);

        Position destination;
        if (!FindEredarTwinsConflagrationSafeSpot(source, bot, destination))
            return;

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW | BOT_COMMAND_ATTACK);
        ai->SetBotCommandState(BOT_COMMAND_STAY, true);
        bot->GetMotionMaster()->MovePoint(bot->GetMapId(), destination, FORCED_MOVEMENT_RUN, 0.0f, false);

        bot->m_Events.AddEventAtOffset([bot]()
        {
            if (!bot || !bot->IsInWorld() || !bot->IsAlive() || !bot->IsNPCBot())
                return;

            if (bot_ai* ai = bot->GetBotAI())
            {
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
            }
        }, 16s);
    }

    void HandleEredarTwinsConflagrationTarget(Creature* source, Unit* target)
    {
        if (!source || !target || !IsOwnedEredarTwinsNpcBot(target))
            return;

        Creature* bot = target->ToCreature();
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!bot || !ai)
            return;

        EredarTwinsConflagrationBotState& state = GetEredarTwinsConflagrationState(source, bot);
        bool handled = false;

        switch (ai->GetBotClass())
        {
            case BOT_CLASS_ROGUE:
                if (!state.cloakOfShadows && TryCastEredarTwinsBotClassSpell(bot, ai, SPELL_CLOAK_OF_SHADOWS))
                {
                    state.cloakOfShadows = true;
                    handled = true;
                }
                else if (!state.vanish && TryCastEredarTwinsBotClassSpell(bot, ai, SPELL_VANISH))
                {
                    state.vanish = true;
                    handled = true;
                }
                break;
            case BOT_CLASS_MAGE:
                if (!state.iceBlock && TryCastEredarTwinsBotClassSpell(bot, ai, SPELL_ICE_BLOCK))
                {
                    state.iceBlock = true;
                    handled = true;
                }
                break;
            case BOT_CLASS_PALADIN:
                if (!state.divineShield && TryCastEredarTwinsBotClassSpell(bot, ai, SPELL_DIVINE_SHIELD))
                {
                    state.divineShield = true;
                    handled = true;
                }
                break;
            default:
                break;
        }

        if (handled)
            return;

        if (!state.cloakDevice)
        {
            state.cloakDevice = true;
            CastEredarTwinsKnownSelfSpell(bot, SPELL_GNOMISH_CLOAKING_DEVICE);
            return;
        }

        MoveEredarTwinsConflagrationBotToSafeSpot(source, bot, ai);
    }

    void ApplyEredarTwinsAlythessWarlockThreat(Creature* alythess, Unit* seed)
    {
        if (!alythess || !alythess->IsAlive() || !alythess->CanHaveThreatList())
            return;

        std::vector<Unit*> warlocks = GatherEredarTwinsAbilityTargets(alythess, seed);
        warlocks.erase(std::remove_if(warlocks.begin(), warlocks.end(), [](Unit* unit)
        {
            return !IsEredarTwinsWarlock(unit);
        }), warlocks.end());

        if (warlocks.empty())
            return;

        std::sort(warlocks.begin(), warlocks.end(), [](Unit* left, Unit* right)
        {
            if (IsMarkedPlayerTank(left) != IsMarkedPlayerTank(right))
                return IsMarkedPlayerTank(left);

            if (IsNpcBotTank(left) != IsNpcBotTank(right))
                return IsNpcBotTank(left);

            return left->GetGUID().GetCounter() < right->GetGUID().GetCounter();
        });

        for (std::size_t i = 0; i < warlocks.size(); ++i)
        {
            Unit* warlock = warlocks[i];
            float const desiredThreat = i == 0 ? EREDAR_TWINS_ALYTHESS_PRIMARY_WARLOCK_THREAT : EREDAR_TWINS_ALYTHESS_WARLOCK_THREAT;
            float const currentThreat = alythess->GetThreatMgr().GetThreat(warlock, true);

            alythess->SetInCombatWith(warlock);
            warlock->SetInCombatWith(alythess);

            if (currentThreat < desiredThreat)
                alythess->GetThreatMgr().AddThreat(warlock, desiredThreat - currentThreat, nullptr, true, true);
        }
    }
}

struct boss_sacrolash : public BossAI
{
    boss_sacrolash(Creature* creature) : BossAI(creature, DATA_EREDAR_TWINS), _isSisterDead(false) {}

    bool CheckInRoom() override
    {
        if (me->GetExactDist2d(me->GetHomePosition()) >= 50.f)
        {
            DoCastAOE(SPELL_FIREBLAST, true);

            if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
                alythess->AI()->DoCastAOE(SPELL_FIREBLAST, true);

            return false;
        }

        return true;
    }

    void Reset() override
    {
        DoCastSelf(SPELL_SHADOWFORM, true);
        _isSisterDead = false;
        BossAI::Reset();
        me->SetLootMode(0);

        if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
            if (!alythess->IsAlive())
                alythess->Respawn(true);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_SISTER_DIED)
        {
            me->ResetLootMode();
            _isSisterDead = true;
            Talk(YELL_SISTER_ALYTHESS_DEAD);
            me->CastSpell(me, SPELL_EMPOWER, true);

            scheduler.CancelGroup(GROUP_SPECIAL_ABILITY);
            ScheduleTimedEvent(20s, [&] {
                Unit* target = SelectEredarTwinsRandomTarget(me, me->GetVictim(), true);
                if (!target)
                    return;

                HandleEredarTwinsConflagrationTarget(me, target);
                DoCast(target, SPELL_CONFLAGRATION);

                if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
                    alythess->AI()->Talk(EMOTE_CONFLAGRATION, target);
            }, 30s, 35s);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        CastEredarTwinsOpeningBotConsumables(me, who);

        if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
        {
            if (alythess->IsAlive() && !alythess->IsInCombat())
                alythess->AI()->AttackStart(who);

            ApplyEredarTwinsAlythessWarlockThreat(alythess, who);
        }

        ScheduleEnrageTimer(SPELL_ENRAGE, 6min, YELL_BERSERK);

        ScheduleTimedEvent(3s, [&] {
            if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
                ApplyEredarTwinsAlythessWarlockThreat(alythess, me->GetVictim());
        }, 5s);

        ScheduleTimedEvent(10s, [&] {
            DoCastSelf(SPELL_SHADOW_BLADES);
        }, 10s);

        ScheduleTimedEvent(25s, [&] {
            DoCastVictim(SPELL_CONFOUNDING_BLOW);
        }, 20s, 25s);

        ScheduleTimedEvent(8s, 16s, [&] {
            me->SummonCreature(NPC_SHADOW_IMAGE, me->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 10000);
        }, 8s, 12s);

        scheduler.Schedule(36s, GROUP_SPECIAL_ABILITY, [this](TaskContext context) {
            Unit* target = nullptr;
            if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
                target = SelectEredarTwinsThreatTarget(me, alythess, me->GetVictim(), 6, false);

            if (!target)
                target = me->GetVictim();

            if (!target)
            {
                context.Repeat(30s, 35s);
                return;
            }

            Talk(EMOTE_SHADOW_NOVA, target);
            Talk(YELL_SHADOW_NOVA);
            DoCast(target, SPELL_SHADOW_NOVA);
            context.Repeat(30s, 35s);
        });
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && urand(0, 1))
            Talk(YELL_SAC_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        summons.DespawnAll();

        if (_isSisterDead)
        {
            Talk(YELL_SAC_DEAD);
            instance->SetBossState(DATA_EREDAR_TWINS, DONE);
        }
        else if (Creature* alythess = instance->GetCreature(DATA_ALYTHESS))
            alythess->AI()->DoAction(ACTION_SISTER_DIED);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (Unit* target = SelectEredarTwinsRandomTarget(summon, me->GetVictim(), false))
        {
            summon->AI()->AttackStart(target);
            summon->AddThreat(target, 10000000);
        }
    }

    private:
        bool _isSisterDead;
};

struct boss_alythess : public BossAI
{
    boss_alythess(Creature* creature) : BossAI(creature, DATA_EREDAR_TWINS), _isSisterDead(false) { }

    void Reset() override
    {
        DoCastSelf(SPELL_FIREFORM, true);
        _isSisterDead = false;
        BossAI::Reset();
        me->SetLootMode(0);

        if (Creature* sacrolash = instance->GetCreature(DATA_SACROLASH))
            if (!sacrolash->IsAlive())
                sacrolash->Respawn(true);
    }

    void AttackStart(Unit* who) override
    {
        if (who && who->isTargetableForAttack() && me->GetReactState() != REACT_PASSIVE)
            if (me->Attack(who, false))
                me->AddThreat(who, 0.0f);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_SISTER_DIED)
        {
            me->ResetLootMode();
            _isSisterDead = true;
            Talk(YELL_SISTER_SACROLASH_DEAD);
            me->CastSpell(me, SPELL_EMPOWER, true);

            scheduler.CancelAll();

            ScheduleTimedEvent(1s, [&] {
                DoCastVictim(SPELL_BLAZE);
            }, 3800ms);

            scheduler.Schedule(16s, GROUP_PYROGENICS, [this](TaskContext context) {
                DoCastSelf(SPELL_PYROGENICS);
                context.Repeat(16s, 28s);
            });

            ScheduleTimedEvent(8s, 10s, [&] {
                DoCast(SPELL_FLAME_SEAR);
            }, 8s, 10s);

            ScheduleTimedEvent(20s, 26s, [&] {
                Unit* target = SelectEredarTwinsRandomTarget(me, me->GetVictim(), false);
                if (!target)
                    target = me->GetVictim();

                if (!target)
                    return;

                DoCast(target, SPELL_SHADOW_NOVA);

                if (Creature* sacrolash = instance->GetCreature(DATA_SACROLASH))
                    sacrolash->AI()->Talk(EMOTE_SHADOW_NOVA, target);
            }, 20s, 26s);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        CastEredarTwinsOpeningBotConsumables(me, who);

        if (Creature* sacrolash = instance->GetCreature(DATA_SACROLASH))
            if (sacrolash->IsAlive() && !sacrolash->IsInCombat())
                sacrolash->AI()->AttackStart(who);

        ApplyEredarTwinsAlythessWarlockThreat(me, who);

        ScheduleEnrageTimer(SPELL_ENRAGE, 6min, YELL_BERSERK);

        ScheduleTimedEvent(3s, [&] {
            ApplyEredarTwinsAlythessWarlockThreat(me, me->GetVictim());
        }, 5s);

        ScheduleTimedEvent(1s, [&] {
            DoCastVictim(SPELL_BLAZE);
        }, 3800ms);

        // PYROGENICS Phase 1
        scheduler.Schedule(21s, GROUP_PYROGENICS, [this](TaskContext context) {
            DoCastSelf(SPELL_PYROGENICS);
            context.Repeat(21s, 34s);
        });

        // FLAME_SEAR Phase 1
        ScheduleTimedEvent(10s, 15s, [&] {
            DoCast(SPELL_FLAME_SEAR);
        }, 10s, 15s);

        scheduler.Schedule(20s, GROUP_SPECIAL_ABILITY, [this](TaskContext context) {
            Unit* target = nullptr;
            if (Creature* sacrolash = instance->GetCreature(DATA_SACROLASH))
                target = SelectEredarTwinsThreatTarget(me, sacrolash, me->GetVictim(), 6, true);

            if (!target)
                target = SelectEredarTwinsRandomTarget(me, me->GetVictim(), true);

            if (!target)
            {
                context.Repeat(30s, 35s);
                return;
            }

            Talk(EMOTE_CONFLAGRATION, target);
            Talk(YELL_CANFLAGRATION);
            HandleEredarTwinsConflagrationTarget(me, target);
            DoCast(target, SPELL_CONFLAGRATION);
            context.Repeat(30s, 35s);
        });
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && urand(0, 1))
            Talk(YELL_SAC_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        summons.DespawnAll();

        if (_isSisterDead)
        {
            Talk(YELL_SAC_DEAD);
            instance->SetBossState(DATA_EREDAR_TWINS, DONE);
        }
        else if (Creature* sacrolash = instance->GetCreature(DATA_SACROLASH))
            sacrolash->AI()->DoAction(ACTION_SISTER_DIED);
    }

    private:
        bool _isSisterDead;
};

class spell_eredar_twins_apply_touch : public SpellScript
{
    PrepareSpellScript(spell_eredar_twins_apply_touch);

public:
    spell_eredar_twins_apply_touch(uint32 touchSpell) : SpellScript(), _touchSpell(touchSpell) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _touchSpell });
    }

    void HandleApplyTouch()
    {
        Unit* target = GetHitUnit();
        if (target && (target->IsPlayer() || IsOwnedEredarTwinsNpcBot(target)))
            target->CastSpell(target, _touchSpell, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_eredar_twins_apply_touch::HandleApplyTouch);
    }

private:
    uint32 _touchSpell;
};

class spell_eredar_twins_handle_touch : public SpellScript
{
    PrepareSpellScript(spell_eredar_twins_handle_touch);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DARK_FLAME, SPELL_FLAME_TOUCHED, SPELL_DARK_TOUCHED });
    }

    SpellCastResult CheckCast()
    {
        if (GetCaster()->HasAura(SPELL_DARK_FLAME))
            return SPELL_FAILED_DONT_REPORT;

        if (GetSpellInfo()->Id == SPELL_DARK_TOUCHED)
        {
            if (GetCaster()->HasAura(SPELL_FLAME_TOUCHED))
            {
                GetCaster()->RemoveAurasDueToSpell(SPELL_FLAME_TOUCHED);
                GetCaster()->CastSpell(GetCaster(), SPELL_DARK_FLAME, true);
                return SPELL_FAILED_DONT_REPORT;
            }
        }
        else // if (m_spellInfo->Id == SPELL_FLAME_TOUCHED)
        {
            if (GetCaster()->HasAura(SPELL_DARK_TOUCHED))
            {
                GetCaster()->RemoveAurasDueToSpell(SPELL_DARK_TOUCHED);
                GetCaster()->CastSpell(GetCaster(), SPELL_DARK_FLAME, true);
                return SPELL_FAILED_DONT_REPORT;
            }
        }
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_eredar_twins_handle_touch::CheckCast);
    }
};

class spell_eredar_twins_flame_sear : public SpellScript
{
    PrepareSpellScript(spell_eredar_twins_flame_sear);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Acore::Containers::RandomResize(targets,5);
        targets.remove(GetCaster()->GetVictim());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_eredar_twins_flame_sear::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_eredar_twins_blaze : public SpellScript
{
    PrepareSpellScript(spell_eredar_twins_blaze);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BLAZE_SUMMON });
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_BLAZE_SUMMON, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_eredar_twins_blaze::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_eredar_twins_handle_touch_periodic : public AuraScript
{
    PrepareAuraScript(spell_eredar_twins_handle_touch_periodic);

public:
    spell_eredar_twins_handle_touch_periodic(uint32 touchSpell, uint8 effIndex, uint8 aura) : AuraScript(), _touchSpell(touchSpell), _effectIndex(effIndex), _aura(aura) {}

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _touchSpell });
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        if (aurEff->GetId() == SPELL_FLAME_SEAR)
        {
            uint32 tick = aurEff->GetTickNumber();
            if (tick % 2 != 0 || tick > 10)
                return;
        }

        if (Unit* owner = GetOwner()->ToUnit())
            owner->CastSpell(owner, _touchSpell, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_eredar_twins_handle_touch_periodic::OnPeriodic, _effectIndex, _aura);
    }

private:
    uint32 _touchSpell;
    uint8 _effectIndex;
    uint8 _aura;
};

class at_sunwell_eredar_twins : public OnlyOnceAreaTriggerScript
{
public:
    at_sunwell_eredar_twins() : OnlyOnceAreaTriggerScript("at_sunwell_eredar_twins") {}

    bool _OnTrigger(Player* player, AreaTrigger const* /*trigger*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
        {
            if (Creature* creature = instance->GetCreature(DATA_SACROLASH))
                creature->AI()->Talk(YELL_INTRO_SAC);
            if (Creature* creature = instance->GetCreature(DATA_ALYTHESS))
                creature->AI()->Talk(YELL_INTRO_ALY);
        }

        return true;
    }
};

struct go_eredar_twins_blaze : GameObjectAI
{
    explicit go_eredar_twins_blaze(GameObject *object) : GameObjectAI(object) { };

    void InitializeAI() override
    {
        // required for the trap to apply its startDelay
        if (InstanceScript* instance = me->GetInstanceScript())
            if (ObjectGuid creatureGUID = instance->GetGuidData(DATA_ALYTHESS))
            {
                me->SetOwnerGUID(creatureGUID);
                me->SetLootState(GO_NOT_READY);
            }
     }
};

void AddSC_boss_eredar_twins()
{
    RegisterSunwellPlateauCreatureAI(boss_sacrolash);
    RegisterSunwellPlateauCreatureAI(boss_alythess);
    RegisterSpellScriptWithArgs(spell_eredar_twins_apply_touch, "spell_eredar_twins_apply_dark_touched", SPELL_DARK_TOUCHED);
    RegisterSpellScriptWithArgs(spell_eredar_twins_apply_touch, "spell_eredar_twins_apply_flame_touched", SPELL_FLAME_TOUCHED);
    RegisterSpellScript(spell_eredar_twins_handle_touch);
    RegisterSpellScript(spell_eredar_twins_blaze);
    RegisterSpellScript(spell_eredar_twins_flame_sear);
    RegisterSpellScriptWithArgs(spell_eredar_twins_handle_touch_periodic, "spell_eredar_twins_handle_dark_touched_periodic", SPELL_DARK_TOUCHED, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE);
    RegisterSpellScriptWithArgs(spell_eredar_twins_handle_touch_periodic, "spell_eredar_twins_handle_flame_touched_periodic", SPELL_FLAME_TOUCHED, EFFECT_2, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    RegisterSpellScriptWithArgs(spell_eredar_twins_handle_touch_periodic, "spell_eredar_twins_handle_flame_touched_flame_sear", SPELL_FLAME_TOUCHED, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE);
    new at_sunwell_eredar_twins();
    RegisterGameObjectAI(go_eredar_twins_blaze);
}
