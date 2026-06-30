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

#include "Containers.h"
#include "Config.h"
#include "CreatureScript.h"
#include "CreatureTextMgr.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "sunwell_plateau.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <unordered_set>
#include <vector>

namespace BonusLootRolls
{
    void AddExplicitBossDeathOpportunities(Player* killer, Creature* killed);
}

enum Yells
{
    SAY_KJ_OFFCOMBAT                            = 0,

    SAY_KALECGOS_ENCOURAGE                      = 0,
    SAY_KALECGOS_READY1                         = 1,
    SAY_KALECGOS_READY2                         = 2,
    SAY_KALECGOS_READY_ALL                      = 3,
    SAY_KALECGOS_AWAKEN                         = 5,
    SAY_KALECGOS_LETGO                          = 6,
    SAY_KALECGOS_FOCUS                          = 7,
    SAY_KALECGOS_FATE                           = 8,
    SAY_KALECGOS_GOODBYE                        = 9,
    SAY_KALECGOS_JOIN                           = 10,

    SAY_KJ_DEATH                                = 0,
    SAY_KJ_SLAY                                 = 1,
    SAY_KJ_REFLECTION                           = 2,
    SAY_KJ_EMERGE                               = 3,
    SAY_KJ_DARKNESS                             = 4,
    SAY_KJ_PHASE3                               = 5,
    SAY_KJ_PHASE4                               = 6,
    SAY_KJ_PHASE5                               = 7,
    EMOTE_KJ_DARKNESS                           = 8,

    SAY_ANVEENA_IMPRISONED                      = 0,
    SAY_ANVEENA_LOST                            = 1,
    SAY_ANVEENA_KALEC                           = 2,
    SAY_ANVEENA_GOODBYE                         = 3
};

enum Spells
{
    // Kil'jaeden spells
    SPELL_REBIRTH                               = 44200,
    SPELL_SOUL_FLAY                             = 45442,
    SPELL_LEGION_LIGHTNING                      = 45664,
    SPELL_FIRE_BLOOM                            = 45641,
    SPELL_SHADOW_SPIKE                          = 46680,
    SPELL_SINISTER_REFLECTION                   = 45892,
    SPELL_FLAME_DART                            = 45737,
    SPELL_FLAME_DART_EXPLOSION                  = 45746,
    SPELL_DARKNESS_OF_A_THOUSAND_SOULS          = 46605,
    SPELL_DARKNESS_OF_A_THOUSAND_SOULS_DAMAGE   = 45657,
    SPELL_ARMAGEDDON_PERIODIC                   = 45921,
    SPELL_ARMAGEDDON_VISUAL                     = 45911,
    SPELL_ARMAGEDDON_MISSILE                    = 45909,
    SPELL_CUSTOM_08_STATE                       = 45800,
    SPELL_DESTROY_ALL_DRAKES                    = 46707,

    // Sinister Reflections
    SPELL_SINISTER_REFLECTION_SUMMON            = 45891,
    SPELL_SINISTER_REFLECTION_CLASS             = 45893,
    SPELL_SINISTER_REFLECTION_CLONE             = 45785,
    // TODO
    // These should be applied to target of SPELL_SINISTER_REFLECTION but not implemented
    //SPELL_SINISTER_COPY_WEAPON                  = 41054,
    //SPELL_SINISTER_COPY_OFFHAND_WEAPON          = 45205,

    // Misc
    SPELL_ANVEENA_ENERGY_DRAIN                  = 46410,
    SPELL_RING_OF_BLUE_FLAMES                   = 45825,
    SPELL_SUMMON_BLUE_DRAKE                     = 45836,
    SPELL_VENGEANCE_OF_THE_BLUE_FLIGHT          = 45839,
    SPELL_POSSESS_DRAKE_IMMUNITY                = 45838,
    SPELL_SHIELD_OF_THE_BLUE                    = 45848,
    SPELL_KILJAEDEN_ANTI_MAGIC_ZONE             = 600446,
    SPELL_POTION_OF_SWIFTNESS                   = 2379,
    SPELL_SACRIFICE_OF_ANVEENA                  = 46474,

    // NPCBot opening consumables
    SPELL_FLASK_OF_CHROMATIC_WONDER             = 42735,
    SPELL_SCROLL_OF_STAMINA                     = 48101,
};

namespace
{
    constexpr float KILJAEDEN_MECHANIC_TARGET_RANGE = 65.0f;
    constexpr float KILJAEDEN_SHIELD_STACK_RADIUS = 18.0f;
    constexpr float KILJAEDEN_ANTI_MAGIC_ZONE_STACK_RADIUS = 8.0f;
    constexpr float KILJAEDEN_DARKNESS_STACK_HOLD_TOLERANCE = 2.5f;
    constexpr float KILJAEDEN_DARKNESS_STACK_WIDTH = 6.0f;
    constexpr char CONFIG_KILJAEDEN_DARKNESS_ANTI_MAGIC_ZONE[] = "SunwellPlateau.Kiljaeden.DarknessAntiMagicZone.Enable";

    std::unordered_set<ObjectGuid::LowType> KiljaedenDarknessStackedBots;
    ObjectGuid::LowType KiljaedenDarknessAntiMagicZoneAnchor = 0;

    bool KiljaedenDarknessAntiMagicZoneEnabled()
    {
        return sConfigMgr->GetOption<bool>(CONFIG_KILJAEDEN_DARKNESS_ANTI_MAGIC_ZONE, true);
    }

    bool IsOwnedKiljaedenNpcBot(Unit const* unit)
    {
        Creature const* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }

    bool IsKiljaedenPlayerOrOwnedBot(Unit const* unit)
    {
        if (!unit || !unit->IsAlive() || !unit->IsInWorld())
            return false;

        if (Player const* player = unit->ToPlayer())
            return !player->IsGameMaster();

        return IsOwnedKiljaedenNpcBot(unit);
    }

    bool IsKiljaedenConsumableBot(Unit const* unit)
    {
        return unit && unit->IsAlive() && unit->IsInWorld() && IsOwnedKiljaedenNpcBot(unit);
    }

    bool IsKiljaedenTankBot(Creature const* bot)
    {
        bot_ai* ai = bot && bot->IsNPCBot() ? bot->GetBotAI() : nullptr;
        return ai && (ai->IsTank(bot) || ai->IsOffTank(bot) || ai->HasRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF));
    }

    void AddKiljaedenUnit(std::vector<Unit*>& units, Unit* unit, WorldObject const* source, float range)
    {
        if (!unit || !source || !IsKiljaedenPlayerOrOwnedBot(unit) || unit->GetMap() != source->GetMap())
            return;

        if (!unit->InSamePhase(source) || source->GetDistance(unit) > range)
            return;

        if (std::find(units.begin(), units.end(), unit) == units.end())
            units.push_back(unit);
    }

    std::vector<Unit*> GatherKiljaedenParticipants(WorldObject const* source, float range)
    {
        std::vector<Unit*> units;
        if (!source || !source->GetMap())
            return units;

        Map::PlayerList const& playerList = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            AddKiljaedenUnit(units, player, source, range);
            if (!player || !player->HaveBot())
                continue;

            if (BotMgr* botMgr = player->GetBotMgr())
                for (auto const& [_, bot] : *botMgr->GetBotMap())
                    AddKiljaedenUnit(units, bot, source, range);
        }

        return units;
    }

    bool HasKiljaedenFlaskAura(Unit const* unit)
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

    void CastKiljaedenKnownSelfSpell(Creature* bot, uint32 spellId)
    {
        if (bot && sSpellMgr->GetSpellInfo(spellId))
            bot->CastSpell(bot, spellId, true);
    }

    void CastKiljaedenOpeningBotConsumables(Creature* source)
    {
        for (Unit* unit : GatherKiljaedenParticipants(source, 150.0f))
        {
            if (!IsKiljaedenConsumableBot(unit))
                continue;

            Creature* bot = unit->ToCreature();
            if (HasKiljaedenFlaskAura(bot))
                continue;

            CastKiljaedenKnownSelfSpell(bot, SPELL_FLASK_OF_CHROMATIC_WONDER);
            CastKiljaedenKnownSelfSpell(bot, SPELL_SCROLL_OF_STAMINA);
        }
    }

    bool IsKiljaedenMechanicTarget(WorldObject const* target, WorldObject const* source, float range)
    {
        Unit const* unit = target ? target->ToUnit() : nullptr;
        return unit && IsKiljaedenPlayerOrOwnedBot(unit) && source && unit->GetMap() == source->GetMap() &&
            unit->InSamePhase(source) && source->GetDistance(unit) <= range &&
            !unit->HasAura(SPELL_VENGEANCE_OF_THE_BLUE_FLIGHT);
    }

    void AddKiljaedenMechanicTargets(WorldObject const* source, std::list<WorldObject*>& targets, float range)
    {
        for (Unit* unit : GatherKiljaedenParticipants(source, range))
            if (std::find(targets.begin(), targets.end(), unit) == targets.end())
                targets.push_back(unit);
    }

    Unit* SelectKiljaedenMechanicTarget(Unit* caster, float range)
    {
        std::vector<Unit*> targets = GatherKiljaedenParticipants(caster, range);
        std::erase_if(targets, [caster, range](Unit const* unit)
        {
            return !IsKiljaedenMechanicTarget(unit, caster, range);
        });

        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }

    bool TryMoveNpcBotAwayFrom(Creature* bot, WorldObject const* hazard, float moveDistance)
    {
        if (!bot || !hazard || !bot->IsAlive() || bot->GetMap() != hazard->GetMap())
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree() || ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION) || bot_ai::CCed(bot, true))
            return false;

        float const baseAngle = Position::NormalizeOrientation(std::atan2(bot->GetPositionY() - hazard->GetPositionY(), bot->GetPositionX() - hazard->GetPositionX()));
        static constexpr std::array<float, 9> offsets =
        {
            0.0f, 0.35f, -0.35f, 0.70f, -0.70f, 1.05f, -1.05f, 1.45f, -1.45f
        };

        for (float offset : offsets)
        {
            Position candidate = bot->GetFirstCollisionPosition(moveDistance, Position::NormalizeOrientation(baseAngle + offset - bot->GetOrientation()));
            if (!candidate.IsPositionValid() || !bot->IsWithinLOS(candidate.GetPositionX(), candidate.GetPositionY(), candidate.GetPositionZ()))
                continue;

            bot->InterruptNonMeleeSpells(false);
            ai->BotMovement(BOT_MOVE_POINT, &candidate, nullptr, true);
            return true;
        }

        return false;
    }

    void MoveNearbyNpcBotsAwayFrom(WorldObject const* hazard, float dangerRadius, float moveDistance)
    {
        if (!hazard)
            return;

        for (Unit* unit : GatherKiljaedenParticipants(hazard, 80.0f))
            if (Creature* bot = unit->ToCreature())
                if (bot->IsNPCBot() && bot->GetExactDist2d(hazard) <= dangerRadius)
                    TryMoveNpcBotAwayFrom(bot, hazard, moveDistance);
    }

    Unit* SelectKiljaedenDarknessStackTank(Creature* kiljaeden)
    {
        if (!kiljaeden)
            return nullptr;

        if (Unit* victim = kiljaeden->GetVictim())
            if (IsKiljaedenPlayerOrOwnedBot(victim))
                return victim;

        if (Unit* victim = kiljaeden->GetThreatMgr().GetCurrentVictim())
            if (IsKiljaedenPlayerOrOwnedBot(victim))
                return victim;

        if (Unit* victim = kiljaeden->GetThreatMgr().GetLastVictim())
            if (IsKiljaedenPlayerOrOwnedBot(victim))
                return victim;

        Creature* bestTank = nullptr;
        float bestDistance = std::numeric_limits<float>::max();
        for (Unit* unit : GatherKiljaedenParticipants(kiljaeden, 120.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (!bot || !IsKiljaedenTankBot(bot))
                continue;

            float const distance = kiljaeden->GetDistance(bot);
            if (!bestTank || distance < bestDistance)
            {
                bestTank = bot;
                bestDistance = distance;
            }
        }

        return bestTank;
    }

    Creature* GetKiljaedenDarknessAntiMagicZoneAnchor(WorldObject const* source)
    {
        if (!source || !KiljaedenDarknessAntiMagicZoneAnchor)
            return nullptr;

        for (Unit* unit : GatherKiljaedenParticipants(source, 180.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (bot && bot->GetGUID().GetCounter() == KiljaedenDarknessAntiMagicZoneAnchor && IsOwnedKiljaedenNpcBot(bot))
                return bot;
        }

        return nullptr;
    }

    Creature* SelectKiljaedenDarknessAntiMagicZoneCaster(Creature* kiljaeden)
    {
        if (!kiljaeden)
            return nullptr;

        Unit* stackTank = SelectKiljaedenDarknessStackTank(kiljaeden);
        Creature* bestBot = nullptr;
        float bestScore = std::numeric_limits<float>::max();

        for (Unit* unit : GatherKiljaedenParticipants(kiljaeden, 140.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            bot_ai* ai = bot && IsOwnedKiljaedenNpcBot(bot) ? bot->GetBotAI() : nullptr;
            if (!bot || !ai || !IsKiljaedenTankBot(bot) || ai->IAmFree() || ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
                continue;

            float score = stackTank ? bot->GetExactDist2d(stackTank) : kiljaeden->GetExactDist2d(bot);
            if (unit == stackTank)
                score = -1.0f;

            if (!bestBot || score < bestScore)
            {
                bestBot = bot;
                bestScore = score;
            }
        }

        return bestBot;
    }

    uint8 GetKiljaedenBotSlot(Creature const* bot)
    {
        bot_ai* ai = bot && bot->IsNPCBot() ? bot->GetBotAI() : nullptr;
        Player const* owner = ai ? ai->GetBotOwner() : nullptr;
        if (owner && owner->GetBotMgr())
            return owner->GetBotMgr()->GetNpcBotSlot(bot);

        return bot ? uint8(bot->GetGUID().GetCounter() % 24) : 0;
    }

    bool TryGetKiljaedenDarknessStackPosition(Creature* kiljaeden, Creature* bot, Unit* stackTank, Position& destination)
    {
        if (!kiljaeden || !bot || !stackTank)
            return false;

        if (IsKiljaedenTankBot(bot))
        {
            destination.Relocate(bot);
            return destination.IsPositionValid();
        }

        float const tankToBossAngle = stackTank->GetAngle(kiljaeden);
        float const behindTankAngle = Position::NormalizeOrientation(tankToBossAngle + float(M_PI));
        float const sideAngle = Position::NormalizeOrientation(behindTankAngle + float(M_PI) * 0.5f);
        uint8 const slot = GetKiljaedenBotSlot(bot);
        uint8 const row = slot / 7;
        int8 const sideSlot = int8(slot % 7) - 3;
        float const backOffset = 2.5f + float(row % 3) * 1.8f;
        float const sideOffset = std::clamp(float(sideSlot) * 1.35f, -KILJAEDEN_DARKNESS_STACK_WIDTH, KILJAEDEN_DARKNESS_STACK_WIDTH);

        float x = stackTank->GetPositionX() + std::cos(behindTankAngle) * backOffset + std::cos(sideAngle) * sideOffset;
        float y = stackTank->GetPositionY() + std::sin(behindTankAngle) * backOffset + std::sin(sideAngle) * sideOffset;
        float z = stackTank->GetPositionZ();
        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        destination.Relocate(x, y, z, bot->GetAngle(kiljaeden));
        return destination.IsPositionValid() && bot->IsWithinLOS(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    }

    void PrepareKiljaedenDarknessNpcBotStack(Creature* kiljaeden)
    {
        if (!kiljaeden || !kiljaeden->IsAlive() || !kiljaeden->IsInCombat())
            return;

        Unit* stackTank = SelectKiljaedenDarknessStackTank(kiljaeden);
        if (!stackTank)
            return;

        for (Unit* unit : GatherKiljaedenParticipants(kiljaeden, 140.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            bot_ai* ai = bot && IsOwnedKiljaedenNpcBot(bot) ? bot->GetBotAI() : nullptr;
            if (!bot || !ai || ai->IAmFree() || ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
                continue;

            if (bot->HasAura(SPELL_FIRE_BLOOM))
                continue;

            Position destination;
            if (!TryGetKiljaedenDarknessStackPosition(kiljaeden, bot, stackTank, destination))
                continue;

            KiljaedenDarknessStackedBots.insert(bot->GetGUID().GetCounter());

            if (!bot->GetVictim())
                bot->Attack(kiljaeden, true);

            if (bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY()) <= KILJAEDEN_DARKNESS_STACK_HOLD_TOLERANCE)
            {
                if (bot->isMoving())
                    bot->BotStopMovement();

                if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                    ai->SetBotCommandState(BOT_COMMAND_STAY);

                continue;
            }

            bot->InterruptNonMeleeSpells(false);
            if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                ai->SetBotCommandState(BOT_COMMAND_STAY);

            ai->BotMovement(BOT_MOVE_POINT, &destination, nullptr, true);
        }
    }

    void ReleaseKiljaedenDarknessNpcBotStack(WorldObject const* source)
    {
        KiljaedenDarknessAntiMagicZoneAnchor = 0;

        if (KiljaedenDarknessStackedBots.empty())
            return;

        for (Unit* unit : GatherKiljaedenParticipants(source, 160.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            bot_ai* ai = bot && IsOwnedKiljaedenNpcBot(bot) ? bot->GetBotAI() : nullptr;
            if (!bot || !ai)
                continue;

            if (!KiljaedenDarknessStackedBots.contains(bot->GetGUID().GetCounter()))
                continue;

            if (!ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }

        KiljaedenDarknessStackedBots.clear();
    }

    Creature* FindActiveKiljaedenDarknessCaster(Unit* source)
    {
        if (!source || !source->GetMap())
            return nullptr;

        if (Creature* kiljaeden = source->FindNearestCreature(NPC_KILJAEDEN, 180.0f, true))
            if (kiljaeden->IsInCombat() && kiljaeden->HasAura(SPELL_DARKNESS_OF_A_THOUSAND_SOULS))
                return kiljaeden;

        return nullptr;
    }

    bool TryGetKiljaedenShieldStackPosition(Unit* shieldCaster, Creature* bot, Position& destination)
    {
        if (!shieldCaster || !bot)
            return false;

        uint8 const slot = GetKiljaedenBotSlot(bot);
        float const angle = Position::NormalizeOrientation(shieldCaster->GetOrientation() + float(slot % 8) * (2.0f * float(M_PI) / 8.0f));
        float const radius = 2.0f + float(slot / 8) * 1.5f;

        float x = shieldCaster->GetPositionX() + std::cos(angle) * radius;
        float y = shieldCaster->GetPositionY() + std::sin(angle) * radius;
        float z = shieldCaster->GetPositionZ();
        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        destination.Relocate(x, y, z, bot->GetOrientation());
        if (destination.IsPositionValid() && bot->IsWithinLOS(x, y, z))
            return true;

        x = shieldCaster->GetPositionX();
        y = shieldCaster->GetPositionY();
        z = shieldCaster->GetPositionZ();
        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        destination.Relocate(x, y, z, bot->GetOrientation());
        return destination.IsPositionValid();
    }

    void TryUseKiljaedenSwiftnessPotion(Creature* bot)
    {
        if (!bot || bot->HasAura(SPELL_POTION_OF_SWIFTNESS) || !sSpellMgr->GetSpellInfo(SPELL_POTION_OF_SWIFTNESS))
            return;

        bot->CastSpell(bot, SPELL_POTION_OF_SWIFTNESS, true);
    }

    void MoveKiljaedenNpcBotsToAntiMagicZone(Creature* anchor, Creature* kiljaeden)
    {
        if (!anchor || !kiljaeden)
            return;

        for (Unit* unit : GatherKiljaedenParticipants(anchor, 160.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            bot_ai* ai = bot && IsOwnedKiljaedenNpcBot(bot) ? bot->GetBotAI() : nullptr;
            if (!bot || !ai || ai->IAmFree() || ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
                continue;

            KiljaedenDarknessStackedBots.insert(bot->GetGUID().GetCounter());

            if (!bot->GetVictim())
                bot->Attack(kiljaeden, true);

            bot->InterruptNonMeleeSpells(false);
            if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                ai->SetBotCommandState(BOT_COMMAND_STAY);

            if (bot->GetExactDist2d(anchor) <= KILJAEDEN_ANTI_MAGIC_ZONE_STACK_RADIUS - 1.0f)
            {
                if (bot->isMoving())
                    bot->BotStopMovement();

                continue;
            }

            Position destination;
            if (!TryGetKiljaedenShieldStackPosition(anchor, bot, destination))
                continue;

            TryUseKiljaedenSwiftnessPotion(bot);
            ai->BotMovement(BOT_MOVE_POINT, &destination, nullptr, true);
        }
    }

    void CastKiljaedenDarknessAntiMagicZone(Creature* kiljaeden)
    {
        KiljaedenDarknessAntiMagicZoneAnchor = 0;

        if (!kiljaeden || !KiljaedenDarknessAntiMagicZoneEnabled() || !sSpellMgr->GetSpellInfo(SPELL_KILJAEDEN_ANTI_MAGIC_ZONE))
            return;

        Creature* tankBot = SelectKiljaedenDarknessAntiMagicZoneCaster(kiljaeden);
        bot_ai* ai = tankBot ? tankBot->GetBotAI() : nullptr;
        if (!tankBot || !ai)
            return;

        KiljaedenDarknessAntiMagicZoneAnchor = tankBot->GetGUID().GetCounter();

        if (!tankBot->GetVictim())
            tankBot->Attack(kiljaeden, true);

        tankBot->InterruptNonMeleeSpells(false);
        if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
            ai->SetBotCommandState(BOT_COMMAND_STAY);

        tankBot->CastSpell(tankBot, SPELL_KILJAEDEN_ANTI_MAGIC_ZONE, true);
        MoveKiljaedenNpcBotsToAntiMagicZone(tankBot, kiljaeden);
    }

    void MoveKiljaedenNpcBotsToShield(Unit* shieldCaster)
    {
        Creature* kiljaeden = FindActiveKiljaedenDarknessCaster(shieldCaster);
        if (!kiljaeden)
            return;

        if (KiljaedenDarknessAntiMagicZoneEnabled())
        {
            if (Creature* anchor = GetKiljaedenDarknessAntiMagicZoneAnchor(shieldCaster))
            {
                MoveKiljaedenNpcBotsToAntiMagicZone(anchor, kiljaeden);
                return;
            }
        }

        for (Unit* unit : GatherKiljaedenParticipants(shieldCaster, 160.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            bot_ai* ai = bot && IsOwnedKiljaedenNpcBot(bot) ? bot->GetBotAI() : nullptr;
            if (!bot || !ai || ai->IAmFree() || ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
                continue;

            KiljaedenDarknessStackedBots.insert(bot->GetGUID().GetCounter());

            if (!bot->GetVictim())
                bot->Attack(kiljaeden, true);

            bot->InterruptNonMeleeSpells(false);
            if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                ai->SetBotCommandState(BOT_COMMAND_STAY);

            if (bot->GetExactDist2d(shieldCaster) <= KILJAEDEN_SHIELD_STACK_RADIUS - 1.0f)
            {
                if (bot->isMoving())
                    bot->BotStopMovement();

                continue;
            }

            Position destination;
            if (!TryGetKiljaedenShieldStackPosition(shieldCaster, bot, destination))
                continue;

            TryUseKiljaedenSwiftnessPotion(bot);
            bot->BotStopMovement();
            bot->NearTeleportTo(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(), destination.GetOrientation(), true);
            bot->BotStopMovement();
        }
    }

    void EnsureKiljaedenTankBotThreat(Creature* target, float desiredThreat)
    {
        if (!target || !target->CanHaveThreatList())
            return;

        for (Unit* unit : GatherKiljaedenParticipants(target, 80.0f))
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            bot_ai* ai = bot && bot->IsNPCBot() ? bot->GetBotAI() : nullptr;
            if (!bot || !ai || !(ai->IsTank(bot) || ai->IsOffTank(bot) || ai->HasRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF)))
                continue;

            target->SetInCombatWith(bot);
            bot->SetInCombatWith(target);

            float const currentThreat = target->GetThreatMgr().GetThreat(bot, true);
            if (currentThreat < desiredThreat)
                target->GetThreatMgr().AddThreat(bot, desiredThreat - currentThreat, nullptr, true, true);
        }
    }
}

enum Misc
{
    PHASE_DECEIVERS                 = 1,
    PHASE_NORMAL                    = 2,
    PHASE_DARKNESS                  = 3,
    PHASE_ARMAGEDDON                = 4,
    PHASE_SACRIFICE                 = 5,

    ACTION_START_POST_EVENT         = 1,
    ACTION_NO_KILL_TALK             = 2
};

class CastArmageddon : public BasicEvent
{
public:
    CastArmageddon(Creature* caster) : _caster(caster) { }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        _caster->CastSpell(_caster, SPELL_ARMAGEDDON_MISSILE, true);
        _caster->SetPosition(_caster->GetPositionX(), _caster->GetPositionY(), _caster->GetPositionZ() - 20.0f, 0.0f);
        return true;
    }

private:
    Creature* _caster;
};

struct npc_kiljaeden_controller : public NullCreatureAI
{
    npc_kiljaeden_controller(Creature* creature) : NullCreatureAI(creature), summons(me)
    {
        instance = creature->GetInstanceScript();
    }

    void ResetOrbs()
    {
        for (uint8 i = DATA_ORB_OF_THE_BLUE_DRAGONFLIGHT_1; i < DATA_ORB_OF_THE_BLUE_DRAGONFLIGHT_4 + 1; ++i)
            if (GameObject* orb = instance->GetGameObject(i))
                orb->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
    }

    void Reset() override
    {
        scheduler.CancelAll();
        instance->SetBossState(DATA_KILJAEDEN, NOT_STARTED);
        summons.DespawnAll();
        ResetOrbs();

        me->SummonCreature(NPC_HAND_OF_THE_DECEIVER, 1702.62f, 611.19f, 27.66f, 1.81f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000);
        me->SummonCreature(NPC_HAND_OF_THE_DECEIVER, 1684.099f, 618.848f, 27.67f, 0.589f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000);
        me->SummonCreature(NPC_HAND_OF_THE_DECEIVER, 1688.38f, 641.10f, 27.50f, 5.43f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000);
        me->SummonCreature(NPC_ANVEENA, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ() + 40.0f, 0.0f);

        DoCastAOE(SPELL_DESTROY_ALL_DRAKES, true);
        DoCastSelf(SPELL_ANVEENA_ENERGY_DRAIN, true);

        scheduler.Schedule(1min, [this](TaskContext context) {
            if (instance->GetBossState(DATA_KILJAEDEN) == NOT_STARTED)
                Talk(SAY_KJ_OFFCOMBAT);

            context.Repeat(90s, 3min);
        });
    }

    void JustDied(Unit*) override
    {
        EntryCheckPredicate kilCheck(NPC_KILJAEDEN);
        EntryCheckPredicate kalCheck(NPC_KALECGOS_KJ);
        summons.DespawnIf(kilCheck);
        summons.DoAction(ACTION_START_POST_EVENT, kalCheck);
        summons.DespawnIf(kalCheck);

        DoCastAOE(SPELL_DESTROY_ALL_DRAKES, true);
        summons.DespawnAll();
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_SINISTER_REFLECTION)
        {
            EnsureKiljaedenTankBotThreat(summon, 650000.0f);
            summon->m_Events.AddEventAtOffset([summon] {
                if (summon && summon->IsAlive() && !summon->IsInCombat())
                    summon->SetInCombatWithZone();
            }, 5s);
        }
        else if (summon->GetEntry() == NPC_HAND_OF_THE_DECEIVER)
        {
            summon->m_Events.AddEventAtOffset([summon] {
                if (summon && summon->IsAlive())
                {
                    summon->SetInCombatWithZone();
                    EnsureKiljaedenTankBotThreat(summon, 850000.0f);
                }
            }, 500ms);
        }
        else if (summon->GetEntry() == NPC_KALECGOS_KJ)
            summon->setActive(true);
    }

    void SummonedCreatureDies(Creature* summon, Unit*) override
    {
        summons.Despawn(summon);

        if (summon->GetEntry() == NPC_HAND_OF_THE_DECEIVER)
        {
            instance->SetBossState(DATA_KILJAEDEN, IN_PROGRESS);

            scheduler.Schedule(1s, [this](TaskContext context) {
                auto const& playerList = me->GetMap()->GetPlayers();
                for (auto const& playerRef : playerList)
                    if (Player* player = playerRef.GetSource())
                        if (!player->IsGameMaster() && me->GetDistance2d(player) < 60.0f && player->IsAlive())
                        {
                            context.Repeat();
                            return;
                        }

                CreatureAI::EnterEvadeMode();
            });

            if (!summons.HasEntry(NPC_HAND_OF_THE_DECEIVER))
            {
                me->RemoveAurasDueToSpell(SPELL_ANVEENA_ENERGY_DRAIN);
                me->SummonCreature(NPC_KILJAEDEN, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ() + 1.5f, 4.3f, TEMPSUMMON_MANUAL_DESPAWN);
                me->SummonCreature(NPC_KALECGOS_KJ, 1726.80f, 661.43f, 138.65f, 3.95f, TEMPSUMMON_MANUAL_DESPAWN);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    InstanceScript* instance;
    SummonList summons;
};

struct boss_kiljaeden : public BossAI
{
    boss_kiljaeden(Creature* creature) : BossAI(creature, DATA_KILJAEDEN)
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        me->SetVisible(false);

        me->m_Events.AddEventAtOffset([&] {
            me->SetVisible(true);
            DoCastSelf(SPELL_REBIRTH);
        }, 1s);

        me->m_Events.AddEventAtOffset([&] {
            me->SetReactState(REACT_AGGRESSIVE);
            me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            me->SetInCombatWithZone();
        }, 11s);
    }

    void Reset() override
    {
        ReleaseKiljaedenDarknessNpcBotStack(me);
        _phase = PHASE_NORMAL;

        ScheduleHealthCheckEvent(85, [&]{
            _phase = PHASE_DARKNESS;
            if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
                kalec->AI()->Talk(SAY_KALECGOS_AWAKEN, 21s);

            if (Creature* anveena = instance->GetCreature(DATA_ANVEENA))
                anveena->AI()->Talk(SAY_ANVEENA_IMPRISONED, 26s);

            Talk(SAY_KJ_PHASE3, 32s);

            scheduler.CancelAll();

            ScheduleBasicAbilities();

            me->m_Events.AddEventAtOffset([&] {
                if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
                    kalec->AI()->Talk(SAY_KALECGOS_READY1);
                EmpowerOrb(false);
            }, 38s);

            me->m_Events.AddEventAtOffset([&] {
                Talk(SAY_KJ_REFLECTION);
                me->CastCustomSpell(SPELL_SINISTER_REFLECTION, SPELLVALUE_MAX_TARGETS, 1, me, TRIGGERED_NONE);
            }, 1s);

            scheduler.Schedule(1s+200ms, [this](TaskContext)
            {
                DoCastSelf(SPELL_SHADOW_SPIKE);
            });

            ScheduleTimedEvent(31s, [&] {
                DoCastSelf(SPELL_FLAME_DART);
            }, 20s);

            ScheduleTimedEvent(50s, [&] {
                PrepareKiljaedenDarknessNpcBotStack(me);
            }, 45s);

            ScheduleTimedEvent(53s, [&] {
                PrepareKiljaedenDarknessNpcBotStack(me);
            }, 45s);

            ScheduleTimedEvent(55s, [&] {
                Talk(EMOTE_KJ_DARKNESS);
                CastKiljaedenDarknessAntiMagicZone(me);
                DoCastAOE(SPELL_DARKNESS_OF_A_THOUSAND_SOULS);
            }, 45s);
        });

        ScheduleHealthCheckEvent(55, [&] {
            _phase = PHASE_ARMAGEDDON;
            if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
                kalec->AI()->Talk(SAY_KALECGOS_LETGO, 18s);

            if (Creature* anveena = instance->GetCreature(DATA_ANVEENA))
                anveena->AI()->Talk(SAY_ANVEENA_LOST, 25s);

            Talk(SAY_KJ_PHASE4, 32s);

            scheduler.CancelAll();

            me->m_Events.AddEventAtOffset([&] {
                if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
                    kalec->AI()->Talk(SAY_KALECGOS_READY2);
                EmpowerOrb(false);
            }, 38s);

            scheduler.Schedule(1s, [this](TaskContext)
            {
                Talk(SAY_KJ_REFLECTION);
                me->CastCustomSpell(SPELL_SINISTER_REFLECTION, SPELLVALUE_MAX_TARGETS, 1, me, TRIGGERED_NONE);
            });

            scheduler.Schedule(1s + 200ms, [this](TaskContext)
            {
                DoCastSelf(SPELL_SHADOW_SPIKE);
                ScheduleBasicAbilities();
            });

            ScheduleTimedEvent(28s, [&] {
                DoCastSelf(SPELL_FLAME_DART);
            }, 20s);

            ScheduleTimedEvent(59s, [&] {
                PrepareKiljaedenDarknessNpcBotStack(me);
            }, 45s);

            ScheduleTimedEvent(62s, [&] {
                PrepareKiljaedenDarknessNpcBotStack(me);
            }, 45s);

            ScheduleTimedEvent(64s, [&] {
                me->RemoveAurasDueToSpell(SPELL_ARMAGEDDON_PERIODIC);
                Talk(EMOTE_KJ_DARKNESS);
                CastKiljaedenDarknessAntiMagicZone(me);
                DoCastAOE(SPELL_DARKNESS_OF_A_THOUSAND_SOULS);

                me->m_Events.AddEventAtOffset([this]() {
                    if (me->IsAlive() && me->IsInCombat())
                        DoCastSelf(SPELL_ARMAGEDDON_PERIODIC, true);
                }, 9s);
            }, 45s);

            ScheduleTimedEvent(10s, [&] {
                DoCastSelf(SPELL_ARMAGEDDON_PERIODIC, true);
            }, 40s);
        });

        ScheduleHealthCheckEvent(25, [&] {
            _phase = PHASE_SACRIFICE;

            scheduler.CancelAll();

            scheduler.Schedule(1s, [this](TaskContext)
            {
                scheduler.Schedule(1s, [this](TaskContext)
                {
                    Talk(SAY_KJ_REFLECTION);
                    me->CastCustomSpell(SPELL_SINISTER_REFLECTION, SPELLVALUE_MAX_TARGETS, 1, me, TRIGGERED_NONE);
                });

                scheduler.Schedule(2s, [this](TaskContext)
                {
                    DoCastSelf(SPELL_SHADOW_SPIKE);
                });

                if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
                {
                    kalec->AI()->Talk(SAY_KALECGOS_FOCUS, 9s);
                    kalec->AI()->Talk(SAY_KALECGOS_FATE, 22s + 200ms);
                }

                if (Creature* anveena = instance->GetCreature(DATA_ANVEENA))
                {
                    anveena->AI()->Talk(SAY_ANVEENA_KALEC, 20s);
                    anveena->AI()->Talk(SAY_ANVEENA_GOODBYE, 29s);
                }

                me->m_Events.AddEventAtOffset([&] {
                    if (Creature* anveena = instance->GetCreature(DATA_ANVEENA))
                    {
                        anveena->RemoveAllAuras();
                        anveena->CastSpell(anveena, SPELL_SACRIFICE_OF_ANVEENA, true);
                        anveena->DespawnOrUnsummon(1500ms);
                        DoCastSelf(SPELL_CUSTOM_08_STATE, true);
                        me->SetUnitFlag(UNIT_FLAG_PACIFIED);
                        scheduler.CancelAll();

                        me->m_Events.AddEventAtOffset([&] {
                            me->RemoveAurasDueToSpell(SPELL_CUSTOM_08_STATE);
                            me->RemoveUnitFlag(UNIT_FLAG_PACIFIED);

                            ScheduleBasicAbilities();

                            ScheduleTimedEvent(16s, [&] {
                                DoCastSelf(SPELL_FLAME_DART);
                            }, 20s);

                            ScheduleTimedEvent(12s, [&] {
                                PrepareKiljaedenDarknessNpcBotStack(me);
                            }, 25s);

                            ScheduleTimedEvent(14s, [&] {
                                PrepareKiljaedenDarknessNpcBotStack(me);
                            }, 25s);

                            ScheduleTimedEvent(15s, [&] {
                                me->RemoveAurasDueToSpell(SPELL_ARMAGEDDON_PERIODIC);
                                Talk(EMOTE_KJ_DARKNESS);
                                CastKiljaedenDarknessAntiMagicZone(me);
                                DoCastAOE(SPELL_DARKNESS_OF_A_THOUSAND_SOULS);

                                me->m_Events.AddEventAtOffset([this]() {
                                    if (me->IsAlive() && me->IsInCombat())
                                        DoCastSelf(SPELL_ARMAGEDDON_PERIODIC, true);
                                }, 9s);
                            }, 25s);

                            ScheduleTimedEvent(1500ms, [&] {
                                DoCastSelf(SPELL_ARMAGEDDON_PERIODIC, true);
                            }, 20s);
                        }, 7s);
                    }
                    Talk(SAY_KJ_PHASE5);
                }, 36s);

                me->m_Events.AddEventAtOffset([&] {
                    if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
                        kalec->AI()->Talk(SAY_KALECGOS_READY_ALL);
                    EmpowerOrb(true);
                }, 48s);
            });
        });
    }

    void ScheduleBasicAbilities()
    {
        ScheduleTimedEvent(1s, [&] {
            DoCastVictim(SPELL_SOUL_FLAY);
        }, 4s, 5s);

        ScheduleTimedEvent(7s, [&] {
            if (Unit* target = SelectKiljaedenMechanicTarget(me, 40.0f))
                me->CastSpell(target, SPELL_LEGION_LIGHTNING, false);
            else
                DoCastRandomTarget(SPELL_LEGION_LIGHTNING, 0, 40.0f);
        }, _phase == PHASE_SACRIFICE ? 15s : 30s);

        ScheduleTimedEvent(9s, [&] {
            me->CastCustomSpell(SPELL_FIRE_BLOOM, SPELLVALUE_MAX_TARGETS, 5, me, TRIGGERED_NONE);
            me->SetTarget(me->GetVictim()->GetGUID());
        }, _phase == PHASE_SACRIFICE ? 20s : 40s);

        if (_phase != PHASE_SACRIFICE)
        {
            ScheduleTimedEvent(10s, [&] {
                for (uint8 i = 1; i < _phase; ++i)
                {
                    float x = me->GetPositionX() + 18.0f * cos((i * 2.0f - 1.0f) * M_PI / 3.0f);
                    float y = me->GetPositionY() + 18.0f * std::sin((i * 2.0f - 1.0f) * M_PI / 3.0f);
                    if (Creature* orb = me->SummonCreature(NPC_SHIELD_ORB, x, y, 40.0f, 0, TEMPSUMMON_CORPSE_DESPAWN))
                    {
                        Movement::PointsArray movementArray;
                        movementArray.push_back(G3D::Vector3(x, y, 40.0f));

                        // generate movement array
                        for (uint8 j = 1; j < 20; ++j)
                        {
                            x = me->GetPositionX() + 18.0f * cos(((i * 2.0f - 1.0f) * M_PI / 3.0f) + (j / 20.0f * 2 * M_PI));
                            y = me->GetPositionY() + 18.0f * std::sin(((i * 2.0f - 1.0f) * M_PI / 3.0f) + (j / 20.0f * 2 * M_PI));
                            movementArray.push_back(G3D::Vector3(x, y, 40.0f));
                        }

                        Movement::MoveSplineInit init(orb);
                        init.MovebyPath(movementArray);
                        init.SetCyclic();
                        init.Launch();
                    }
                }
            }, 40s);
        }
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (me->GetReactState() == REACT_PASSIVE)
            return;

        ReleaseKiljaedenDarknessNpcBotStack(me);
        ScriptedAI::EnterEvadeMode(why);
        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* controller = instance->GetCreature(DATA_KJ_CONTROLLER))
                if (controller->IsAIEnabled)
                    controller->AI()->Reset();
    }

    void AttackStart(Unit* who) override
    {
        if (me->GetReactState() == REACT_PASSIVE)
            return;
        ScriptedAI::AttackStart(who);
    }

    void DamageTaken(Unit* unit, uint32& damage, DamageEffectType damageType, SpellSchoolMask schoolMask) override
    {
        BossAI::DamageTaken(unit, damage, damageType, schoolMask);

        if (damage >= me->GetHealth())
        {
            me->SetTarget();
            me->SetReactState(REACT_PASSIVE);
            me->RemoveAllAuras();
            me->GetThreatMgr().ClearAllThreat();
            me->SetRegeneratingHealth(false);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->HandleEmoteCommand(EMOTE_ONESHOT_DROWN);
            me->resetAttackTimer();
            events.Reset();
            damage = 0;
            me->m_Events.AddEventAtOffset([&] {
                me->KillSelf();
            }, 1s);
        }
    }

    void JustDied(Unit* killer) override
    {
        ReleaseKiljaedenDarknessNpcBotStack(me);
        BonusLootRolls::AddExplicitBossDeathOpportunities(killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr, me);
        Talk(SAY_KJ_DEATH);
        instance->SetBossState(DATA_KILJAEDEN, DONE);
        if (Creature* controller = instance->GetCreature(DATA_KJ_CONTROLLER))
            controller->KillSelf();
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_NO_KILL_TALK)
            Talk(SAY_KJ_DARKNESS);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_KJ_SLAY);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        CastKiljaedenOpeningBotConsumables(me);

        if (Creature* kalec = instance->GetCreature(DATA_KALECGOS_KJ))
            kalec->AI()->Talk(SAY_KALECGOS_JOIN, 26s);

        Talk(SAY_KJ_EMERGE);
        ScheduleBasicAbilities();
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);
        if (summon->GetEntry() == NPC_ARMAGEDDON_TARGET)
        {
            summon->SetCanFly(true);
            summon->SetDisableGravity(true);
            summon->CastSpell(summon, SPELL_ARMAGEDDON_VISUAL, true);
            summon->SetPosition(summon->GetPositionX(), summon->GetPositionY(), summon->GetPositionZ() + 20.0f, 0.0f);
            summon->m_Events.AddEventAtOffset(new CastArmageddon(summon), 6s);
            summon->DespawnOrUnsummon(randtime(8s, 10s));
        }
        else if (summon->GetEntry() == NPC_SHIELD_ORB)
            summon->SetInCombatWithZone();
        else if (summon->GetEntry() == NPC_SINISTER_REFLECTION)
            EnsureKiljaedenTankBotThreat(summon, 650000.0f);
    }

    void UpdateAI(uint32 diff) override
    {
        if (me->GetReactState() != REACT_AGGRESSIVE)
            return;

        if (!UpdateVictim())
            return;

        scheduler.Update(diff,
            std::bind(&BossAI::DoMeleeAttackIfReady, this));
    }

    void EmpowerOrb(bool empowerAll)
    {
        for (uint8 i = DATA_ORB_OF_THE_BLUE_DRAGONFLIGHT_1; i < DATA_ORB_OF_THE_BLUE_DRAGONFLIGHT_4 + 1; ++i)
        {
            if (GameObject* orb = instance->GetGameObject(i))
            {
                if (orb->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
                {
                    orb->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
                    if (Creature* trigger = me->SummonTrigger(orb->GetPositionX(), orb->GetPositionY(), orb->GetPositionZ(), 0, 10 * MINUTE * IN_MILLISECONDS))
                    {
                        trigger->CastSpell(trigger, SPELL_RING_OF_BLUE_FLAMES, true, nullptr, nullptr, trigger->GetGUID());
                        if (Creature* controller = instance->GetCreature(DATA_KJ_CONTROLLER))
                            controller->AI()->JustSummoned(trigger);
                    }

                    if (!empowerAll)
                        break;
                }
            }
        }
    }

    private:
        uint8 _phase;
};

enum postEvent
{
    SAY_VELEN_01                        = 0,
    SAY_VELEN_02                        = 1,
    SAY_VELEN_03                        = 2,
    SAY_VELEN_04                        = 3,
    SAY_VELEN_05                        = 4,
    SAY_VELEN_06                        = 5,
    SAY_VELEN_07                        = 6,
    SAY_VELEN_08                        = 7,
    SAY_VELEN_09                        = 8,
    SAY_LIADRIN_01                      = 0,
    SAY_LIADRIN_02                      = 1,
    SAY_LIADRIN_03                      = 2,

    NPC_SHATTERED_SUN_RIFTWAKER         = 26289,
    NPC_SHATTRATH_PORTAL_DUMMY          = 26251,
    NPC_INERT_PORTAL                    = 26254,
    NPC_SHATTERED_SUN_SOLDIER           = 26259,
    NPC_LADY_LIADRIN                    = 26247,
    NPC_PROPHET_VELEN                   = 26246,
    NPC_THE_CORE_OF_ENTROPIUS           = 26262,

    SPELL_TELEPORT_AND_TRANSFORM        = 46473,
    SPELL_OPEN_PORTAL_FROM_SHATTRATH    = 46801,
    SPELL_TELEPORT_VISUAL               = 35517,
    SPELL_BOSS_ARCANE_PORTAL_STATE      = 42047,
    SPELL_CALL_ENTROPIUS                = 46818,
    SPELL_BLAZE_TO_LIGHT                = 46821,
    SPELL_SUNWELL_IGNITION              = 46822,

    EVENT_SCENE_01                      = 1,
    EVENT_SCENE_02,
    EVENT_SCENE_03,
    EVENT_SCENE_04,
    EVENT_SCENE_05,
    EVENT_SCENE_06,
    EVENT_SCENE_07,
    EVENT_SCENE_08,
    EVENT_SCENE_09,
    EVENT_SCENE_10,
    EVENT_SCENE_11,
    EVENT_SCENE_12,
    EVENT_SCENE_13,
    EVENT_SCENE_14,
    EVENT_SCENE_15,
    EVENT_SCENE_16,
    EVENT_SCENE_17,
    EVENT_SCENE_18,
    EVENT_SCENE_19,
    EVENT_SCENE_20,
    EVENT_SCENE_21,
    EVENT_SCENE_22,
    EVENT_SCENE_23,
    EVENT_SCENE_24,
    EVENT_SCENE_25,
    EVENT_SCENE_26,
    EVENT_SCENE_27
};

class MoveDelayed : public BasicEvent
{
public:
    MoveDelayed(Creature* owner, float x, float y, float z, float o) : _owner(owner), _x(x), _y(y), _z(z), _o(o) { }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        Movement::MoveSplineInit init(_owner);
        init.MoveTo(_x, _y, _z, false, true);
        init.SetFacing(_o);
        init.Launch();
        return true;
    }

private:
    Creature* _owner;
    float _x, _y, _z, _o;
};

class FixOrientation : public BasicEvent
{
public:
    FixOrientation(Creature* owner) : _owner(owner) { }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        std::list<Creature*> cList;
        _owner->GetCreaturesWithEntryInRange(cList, 20.0f, NPC_SHATTERED_SUN_SOLDIER);
        for (std::list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); ++itr)
            (*itr)->SetFacingTo(_owner->GetOrientation());
        return true;
    }

private:
    Creature* _owner;
};

struct npc_kalecgos_kj : public NullCreatureAI
{
    npc_kalecgos_kj(Creature* creature) : NullCreatureAI(creature), summons(me)
    {
        instance = creature->GetInstanceScript();
    }

    EventMap events;
    InstanceScript* instance;
    SummonList summons;

    void Reset() override
    {
        events.Reset();
        summons.DespawnAll();
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_START_POST_EVENT)
        {
            me->SetCanFly(false);
            me->SetDisableGravity(false);
            me->CastSpell(me, SPELL_TELEPORT_AND_TRANSFORM, true);
            events.ScheduleEvent(EVENT_SCENE_01, 35s);
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_SHATTERED_SUN_RIFTWAKER)
        {
            summon->CastSpell(summon, SPELL_TELEPORT_VISUAL, true);

            if (summons.size() == 1)
                summon->GetMotionMaster()->MovePoint(0, 1727.08f, 656.82f, 28.37f, FORCED_MOVEMENT_NONE, 0.f, 5.14f, false, true);
            else
                summon->GetMotionMaster()->MovePoint(0, 1738.84f, 627.32f, 28.26f, FORCED_MOVEMENT_NONE, 0.f, 2.0f, false, true);
        }
        else if (summon->GetEntry() == NPC_SHATTRATH_PORTAL_DUMMY)
        {
            if (Creature* riftwaker = summon->FindNearestCreature(NPC_SHATTERED_SUN_RIFTWAKER, 10.0f))
                riftwaker->CastSpell(summon, SPELL_OPEN_PORTAL_FROM_SHATTRATH, false);
            summon->SetWalk(true);
            summon->GetMotionMaster()->MovePoint(0, summon->GetPositionX(), summon->GetPositionY(), summon->GetPositionZ() + 30.0f, FORCED_MOVEMENT_NONE, 0.f, 0.f, false, true);
        }
        else if (summon->GetEntry() == NPC_INERT_PORTAL)
            summon->CastSpell(summon, SPELL_BOSS_ARCANE_PORTAL_STATE, true);
        else if (summon->GetEntry() == NPC_SHATTERED_SUN_SOLDIER)
            summon->CastSpell(summon, SPELL_TELEPORT_VISUAL, true);
        else if (summon->GetEntry() == NPC_LADY_LIADRIN)
        {
            summon->CastSpell(summon, SPELL_TELEPORT_VISUAL, true);
            summon->SetWalk(true);
        }
        else if (summon->GetEntry() == NPC_PROPHET_VELEN)
        {
            summon->CastSpell(summon, SPELL_TELEPORT_VISUAL, true);
            summon->SetWalk(true);
            summon->GetMotionMaster()->MovePoint(0, 1710.15f, 639.23f, 27.311f, FORCED_MOVEMENT_NONE, 0.f, 0.f, false, true);
        }
        else if (summon->GetEntry() == NPC_THE_CORE_OF_ENTROPIUS)
            summon->GetMotionMaster()->MovePoint(0, summon->GetPositionX(), summon->GetPositionY(), 30.0f);
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);
        switch (uint32 eventId = events.ExecuteEvent())
        {
        case EVENT_SCENE_01:
            Talk(SAY_KALECGOS_GOODBYE);
            events.ScheduleEvent(eventId + 1, 15s);
            break;
        case EVENT_SCENE_02:
            me->SummonCreature(NPC_SHATTERED_SUN_RIFTWAKER, 1688.42f, 641.82f, 27.60f, 0.67f);
            me->SummonCreature(NPC_SHATTERED_SUN_RIFTWAKER, 1712.58f, 616.29f, 27.78f, 0.76f);
            events.ScheduleEvent(eventId + 1, 6s);
            break;
        case EVENT_SCENE_03:
            me->SummonCreature(NPC_SHATTRATH_PORTAL_DUMMY, 1727.08f + cos(5.14f), 656.82f + std::sin(5.14f), 28.37f + 2.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 10000);
            me->SummonCreature(NPC_SHATTRATH_PORTAL_DUMMY, 1738.84f + cos(2.0f), 627.32f + std::sin(2.0f), 28.26f + 2.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 10000);
            events.ScheduleEvent(eventId + 1, 11s);
            break;
        case EVENT_SCENE_04:
            me->SummonCreature(NPC_INERT_PORTAL, 1734.96f, 642.43f, 28.06f, 3.49f);
            events.ScheduleEvent(eventId + 1, 4s);
            break;
        case EVENT_SCENE_05:
            if (Creature* first = me->SummonCreature(NPC_SHATTERED_SUN_SOLDIER, 1729.48f, 640.49f, 28.06f, 3.49f))
            {
                first->m_Events.AddEventAtOffset(new MoveDelayed(first, 1718.70f, 607.78f, 28.06f, 2.323f), 5s);
                first->m_Events.AddEventAtOffset(new FixOrientation(first), 12s);
                for (uint8 i = 0; i < 9; ++i)
                    if (Creature* follower = me->SummonCreature(NPC_SHATTERED_SUN_SOLDIER, 1729.48f + 5 * cos(i * 2.0f * M_PI / 9), 640.49f + 5 * std::sin(i * 2.0f * M_PI / 9), 28.06f, 3.49f))
                        follower->GetMotionMaster()->MoveFollow(first, 3.0f, follower->GetAngle(first));
            }
            events.ScheduleEvent(eventId + 1, 10s);
            break;
        case EVENT_SCENE_06:
            if (Creature* first = me->SummonCreature(NPC_SHATTERED_SUN_SOLDIER, 1729.48f, 640.49f, 28.06f, 3.49f))
            {
                first->m_Events.AddEventAtOffset(new MoveDelayed(first, 1678.69f, 649.27f, 28.06f, 5.46f), 5s);
                first->m_Events.AddEventAtOffset(new FixOrientation(first), 14500ms);
                for (uint8 i = 0; i < 9; ++i)
                    if (Creature* follower = me->SummonCreature(NPC_SHATTERED_SUN_SOLDIER, 1729.48f + 5 * cos(i * 2.0f * M_PI / 9), 640.49f + 5 * std::sin(i * 2.0f * M_PI / 9), 28.06f, 3.49f))
                        follower->GetMotionMaster()->MoveFollow(first, 3.0f, follower->GetAngle(first));
            }
            events.ScheduleEvent(eventId + 1, 12s);
            break;
        case EVENT_SCENE_07:
            me->SummonCreature(NPC_LADY_LIADRIN, 1719.87f, 644.265f, 28.06f, 3.83f);
            me->SummonCreature(NPC_PROPHET_VELEN, 1717.97f, 646.44f, 28.06f, 3.94f);
            events.ScheduleEvent(eventId + 1, 7s);
            break;
        case EVENT_SCENE_08:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_01);
            events.ScheduleEvent(eventId + 1, 25s);
            break;
        case EVENT_SCENE_09:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_02);
            events.ScheduleEvent(eventId + 1, 14500ms);
            break;
        case EVENT_SCENE_10:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_03);
            events.ScheduleEvent(eventId + 1, 12500ms);
            break;
        case EVENT_SCENE_11:
            me->SummonCreature(NPC_THE_CORE_OF_ENTROPIUS, 1698.86f, 628.73f, 92.83f, 0.0f);
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->CastSpell(velen, SPELL_CALL_ENTROPIUS, false);
            events.ScheduleEvent(eventId + 1, 8s);
            break;
        case EVENT_SCENE_12:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
            {
                velen->InterruptNonMeleeSpells(false);
                velen->AI()->Talk(SAY_VELEN_04);
            }
            events.ScheduleEvent(eventId + 1, 20s);
            break;
        case EVENT_SCENE_13:
            if (Creature* liadrin = summons.GetCreatureWithEntry(NPC_LADY_LIADRIN))
                liadrin->GetMotionMaster()->MovePoint(0, 1711.28f, 637.29f, 27.29f);
            events.ScheduleEvent(eventId + 1, 6s);
            break;
        case EVENT_SCENE_14:
            if (Creature* liadrin = summons.GetCreatureWithEntry(NPC_LADY_LIADRIN))
                liadrin->AI()->Talk(SAY_LIADRIN_01);
            events.ScheduleEvent(eventId + 1, 10s);
            break;
        case EVENT_SCENE_15:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_05);
            events.ScheduleEvent(eventId + 1, 14s);
            break;
        case EVENT_SCENE_16:
            if (Creature* liadrin = summons.GetCreatureWithEntry(NPC_LADY_LIADRIN))
                liadrin->AI()->Talk(SAY_LIADRIN_02);
            events.ScheduleEvent(eventId + 1, 2s);
            break;
        case EVENT_SCENE_17:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_06);
            events.ScheduleEvent(eventId + 1, 3s);
            break;
        case EVENT_SCENE_18:
            if (Creature* core = summons.GetCreatureWithEntry(NPC_THE_CORE_OF_ENTROPIUS))
            {
                core->RemoveAllAuras();
                core->CastSpell(core, SPELL_BLAZE_TO_LIGHT, true);
            }
            events.ScheduleEvent(eventId + 1, 8s);
            break;
        case EVENT_SCENE_19:
            if (Creature* core = summons.GetCreatureWithEntry(NPC_THE_CORE_OF_ENTROPIUS))
            {
                core->SetObjectScale(0.75f);
                core->GetMotionMaster()->MovePoint(0, core->GetPositionX(), core->GetPositionY(), 28.0f);
            }
            events.ScheduleEvent(eventId + 1, 2s);
            break;
        case EVENT_SCENE_20:
            if (Creature* core = summons.GetCreatureWithEntry(NPC_THE_CORE_OF_ENTROPIUS))
                core->CastSpell(core, SPELL_SUNWELL_IGNITION, true);
            events.ScheduleEvent(eventId + 1, 3s);
            break;
        case EVENT_SCENE_21:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_07);
            events.ScheduleEvent(eventId + 1, 15s);
            break;
        case EVENT_SCENE_22:
            if (Creature* liadrin = summons.GetCreatureWithEntry(NPC_LADY_LIADRIN))
                liadrin->AI()->Talk(SAY_LIADRIN_03);
            events.ScheduleEvent(eventId + 1, 20s);
            break;
        case EVENT_SCENE_23:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_08);
            if (Creature* liadrin = summons.GetCreatureWithEntry(NPC_LADY_LIADRIN))
                liadrin->SetStandState(UNIT_STAND_STATE_KNEEL);
            events.ScheduleEvent(eventId + 1, 8s);
            break;
        case EVENT_SCENE_24:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
                velen->AI()->Talk(SAY_VELEN_09);
            events.ScheduleEvent(eventId + 1, 5s);
            break;
        case EVENT_SCENE_25:
            if (Creature* velen = summons.GetCreatureWithEntry(NPC_PROPHET_VELEN))
            {
                velen->GetMotionMaster()->MovePoint(0, 1739.38f, 643.79f, 28.06f);
                velen->DespawnOrUnsummon(5s);
            }
            events.ScheduleEvent(eventId + 1, 3s);
            break;
        case EVENT_SCENE_26:
            for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
                if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                    if (summon->GetEntry() == NPC_SHATTERED_SUN_SOLDIER)
                    {
                        summon->GetMotionMaster()->MovePoint(0, 1739.38f, 643.79f, 28.06f);
                        summon->DespawnOrUnsummon(Milliseconds(uint32(summon->GetExactDist2d(1734.96f, 642.43f) * 100)));
                    }
            events.ScheduleEvent(eventId + 1, 7s);
            break;
        case EVENT_SCENE_27:
            me->setActive(false);
            summons.DespawnEntry(NPC_INERT_PORTAL);
            summons.DespawnEntry(NPC_SHATTERED_SUN_RIFTWAKER);
            break;
        }
    }
};

class spell_kiljaeden_shadow_spike_aura : public AuraScript
{
    PrepareAuraScript(spell_kiljaeden_shadow_spike_aura);

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        Unit* target = SelectKiljaedenMechanicTarget(GetUnitOwner(), 60.0f);
        if (!target)
            target = GetUnitOwner()->GetAI()->SelectTarget(SelectTargetMethod::Random, 0, 60.0f, true);

        if (target)
        {
            GetUnitOwner()->CastSpell(target, GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell, true);
            MoveNearbyNpcBotsAwayFrom(target, 13.0f, 18.0f);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kiljaeden_shadow_spike_aura::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_kiljaeden_sinister_reflection : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_sinister_reflection);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SINISTER_REFLECTION_SUMMON, SPELL_SINISTER_REFLECTION_CLONE });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        AddKiljaedenMechanicTargets(GetCaster(), targets, KILJAEDEN_MECHANIC_TARGET_RANGE);
        targets.remove_if(Acore::UnitAuraCheck(true, SPELL_VENGEANCE_OF_THE_BLUE_FLIGHT));
        targets.remove_if([this](WorldObject const* target)
        {
            return !IsKiljaedenMechanicTarget(target, GetCaster(), KILJAEDEN_MECHANIC_TARGET_RANGE);
        });
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
        {
            for (uint8 i = 0; i < 4; ++i)
            {
                target->CastSpell(target, SPELL_SINISTER_REFLECTION_SUMMON, true);
            }
            // TODO implement these auras
            //target->AddAura(SPELL_SINISTER_COPY_WEAPON, target);
            //target->AddAura(SPELL_SINISTER_COPY_OFFHAND_WEAPON, target);
            target->CastSpell(target, SPELL_SINISTER_REFLECTION_CLONE, true);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kiljaeden_sinister_reflection::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_kiljaeden_sinister_reflection::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_kiljaeden_fire_bloom : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_fire_bloom);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        AddKiljaedenMechanicTargets(GetCaster(), targets, KILJAEDEN_MECHANIC_TARGET_RANGE);
        targets.remove_if([this](WorldObject const* target)
        {
            return !IsKiljaedenMechanicTarget(target, GetCaster(), KILJAEDEN_MECHANIC_TARGET_RANGE);
        });

        if (targets.size() > 5)
            Acore::Containers::RandomResize(targets, 5);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kiljaeden_fire_bloom::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_kiljaeden_sinister_reflection_clone : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_sinister_reflection_clone);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        targets.sort(Acore::ObjectDistanceOrderPred(GetCaster()));
        WorldObject* target = targets.front();

        targets.clear();
        if (target && target->IsCreature())
        {
            target->ToCreature()->AI()->SetData(1, GetCaster()->getClass());
            targets.push_back(target);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kiljaeden_sinister_reflection_clone::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_kiljaeden_flame_dart : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_flame_dart);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FLAME_DART_EXPLOSION });
    }

    void HandleSchoolDamage(SpellEffIndex  /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_FLAME_DART_EXPLOSION, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kiljaeden_flame_dart::HandleSchoolDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_kiljaeden_darkness_aura : public AuraScript
{
    PrepareAuraScript(spell_kiljaeden_darkness_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DARKNESS_OF_A_THOUSAND_SOULS_DAMAGE });
    }

    void HandleRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (GetUnitOwner()->IsCreature())
            GetUnitOwner()->ToCreature()->AI()->DoAction(ACTION_NO_KILL_TALK);

        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_DARKNESS_OF_A_THOUSAND_SOULS_DAMAGE, true);
        ReleaseKiljaedenDarknessNpcBotStack(GetUnitOwner());
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_kiljaeden_darkness_aura::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_kiljaeden_power_of_the_blue_flight : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_power_of_the_blue_flight);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_BLUE_DRAKE, SPELL_VENGEANCE_OF_THE_BLUE_FLIGHT });
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Player* player = GetHitPlayer())
        {
            player->CastSpell(player, SPELL_SUMMON_BLUE_DRAKE, true);
            player->CastSpell(player, SPELL_VENGEANCE_OF_THE_BLUE_FLIGHT, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kiljaeden_power_of_the_blue_flight::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_kiljaeden_vengeance_of_the_blue_flight_aura : public AuraScript
{
    PrepareAuraScript(spell_kiljaeden_vengeance_of_the_blue_flight_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_POSSESS_DRAKE_IMMUNITY });
    }

    void HandleApply(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_POSSESS_DRAKE_IMMUNITY, true);
    }

    void HandleRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->RemoveAurasDueToSpell(SPELL_POSSESS_DRAKE_IMMUNITY);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_kiljaeden_vengeance_of_the_blue_flight_aura::HandleApply, EFFECT_0, SPELL_AURA_MOD_POSSESS, AURA_EFFECT_HANDLE_REAL);
        OnEffectApply += AuraEffectApplyFn(spell_kiljaeden_vengeance_of_the_blue_flight_aura::HandleApply, EFFECT_2, SPELL_AURA_MOD_PACIFY_SILENCE, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_kiljaeden_vengeance_of_the_blue_flight_aura::HandleRemove, EFFECT_0, SPELL_AURA_MOD_POSSESS, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_kiljaeden_vengeance_of_the_blue_flight_aura::HandleRemove, EFFECT_2, SPELL_AURA_MOD_PACIFY_SILENCE, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_kiljaeden_armageddon_periodic_aura : public AuraScript
{
    PrepareAuraScript(spell_kiljaeden_armageddon_periodic_aura);

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        Unit* caster = GetUnitOwner();

        std::list<Creature*> armageddons;
        caster->GetCreatureListWithEntryInGrid(armageddons, NPC_ARMAGEDDON_TARGET, 100.0f);
        if (armageddons.size() >= 3)
            return;

        Unit* target = SelectKiljaedenMechanicTarget(caster, 60.0f);
        if (!target)
            target = caster->GetAI()->SelectTarget(SelectTargetMethod::Random, 0, 60.0f, true);

        if (target)
        {
            caster->CastSpell(target, GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell, true);
            MoveNearbyNpcBotsAwayFrom(target, 18.0f, 24.0f);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kiljaeden_armageddon_periodic_aura::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_kiljaeden_armageddon_missile : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_armageddon_missile);

    void SetDest(SpellDestination& dest)
    {
        Position const offset = { 0.0f, 0.0f, -20.0f, 0.0f };
        dest.RelocateOffset(offset);
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_kiljaeden_armageddon_missile::SetDest, EFFECT_0, TARGET_DEST_CASTER);
    }
};

// 45856 - Breath: Haste
// 45860 - Breath: Revitalize
class spell_kiljaeden_dragon_breath : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_dragon_breath);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if(Acore::UnitAuraCheck(true, SPELL_VENGEANCE_OF_THE_BLUE_FLIGHT));
    }

    void HandleHit(SpellEffIndex /*effindex*/)
    {
        if (Unit* target = GetHitUnit())
            target->RemoveMovementImpairingAuras(false);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kiljaeden_dragon_breath::FilterTargets, EFFECT_ALL, TARGET_UNIT_CONE_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_kiljaeden_dragon_breath::HandleHit, EFFECT_ALL, SPELL_EFFECT_ANY);
    }
};

// 45848 - Shield of the Blue
class spell_kiljaeden_shield_of_the_blue : public SpellScript
{
    PrepareSpellScript(spell_kiljaeden_shield_of_the_blue);

    void HandleBeforeCast()
    {
        MoveKiljaedenNpcBotsToShield(GetCaster());
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_kiljaeden_shield_of_the_blue::HandleBeforeCast);
    }
};

class spell_kiljaeden_shield_of_the_blue_aura : public AuraScript
{
    PrepareAuraScript(spell_kiljaeden_shield_of_the_blue_aura);

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* target = GetTarget())
        {
            target->RemoveAurasDueToSpell(SPELL_FIRE_BLOOM);
            target->RemoveMovementImpairingAuras(false);
        }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_kiljaeden_shield_of_the_blue_aura::HandleEffectApply, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_kiljaeden()
{
    RegisterSunwellPlateauCreatureAI(npc_kiljaeden_controller);
    RegisterSunwellPlateauCreatureAI(boss_kiljaeden);
    RegisterSunwellPlateauCreatureAI(npc_kalecgos_kj);
    RegisterSpellScript(spell_kiljaeden_shadow_spike_aura);
    RegisterSpellScript(spell_kiljaeden_sinister_reflection);
    RegisterSpellScript(spell_kiljaeden_sinister_reflection_clone);
    RegisterSpellScript(spell_kiljaeden_fire_bloom);
    RegisterSpellScript(spell_kiljaeden_flame_dart);
    RegisterSpellScript(spell_kiljaeden_darkness_aura);
    RegisterSpellScript(spell_kiljaeden_power_of_the_blue_flight);
    RegisterSpellScript(spell_kiljaeden_vengeance_of_the_blue_flight_aura);
    RegisterSpellScript(spell_kiljaeden_armageddon_periodic_aura);
    RegisterSpellScript(spell_kiljaeden_armageddon_missile);
    RegisterSpellScript(spell_kiljaeden_dragon_breath);
    RegisterSpellAndAuraScriptPair(spell_kiljaeden_shield_of_the_blue, spell_kiljaeden_shield_of_the_blue_aura);
}
