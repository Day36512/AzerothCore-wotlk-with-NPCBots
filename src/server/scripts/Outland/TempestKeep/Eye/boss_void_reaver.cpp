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
#include "CreatureScript.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "the_eye.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum voidReaver
{
    SAY_AGGRO                   = 0,
    SAY_SLAY                    = 1,
    SAY_DEATH                   = 2,
    SAY_POUNDING                = 3,

    SPELL_POUNDING              = 34162,
    SPELL_ARCANE_ORB            = 34172,
    SPELL_KNOCK_AWAY            = 25778,
    SPELL_BERSERK               = 26662
};

enum Groups
{
    GROUP_ARCANE_ORB            = 1,
    GROUP_BOT_RECENTER          = 2,
    GROUP_BOT_SPREAD            = 3,
    GROUP_BOT_ORB               = 4
};

namespace
{
constexpr float VOID_REAVER_ENCOUNTER_RANGE = 150.0f;
constexpr float VOID_REAVER_ARCANE_ORB_MIN_RANGE = 20.0f;
constexpr float VOID_REAVER_ARCANE_ORB_MELEE_FALLBACK_RANGE = 18.0f;
constexpr float VOID_REAVER_ARCANE_ORB_DANGER_RADIUS = 18.0f;
constexpr float VOID_REAVER_ORB_DODGE_CLEARANCE = 8.0f;
constexpr float VOID_REAVER_RANGED_RING_RADIUS = 32.0f;
constexpr float VOID_REAVER_RANGED_RING_MIN_RADIUS = 24.0f;
constexpr float VOID_REAVER_RANGED_RING_MAX_RADIUS = 42.0f;
constexpr float VOID_REAVER_RANGED_SPREAD_MIN_DISTANCE = 8.0f;
constexpr float VOID_REAVER_HOME_OK_RADIUS = 18.0f;
constexpr float VOID_REAVER_HOME_RESET_RADIUS = 26.0f;
constexpr float VOID_REAVER_RECENTER_TANK_OFFSET = 6.0f;
constexpr float VOID_REAVER_RECENTER_THREAT = 75000.0f;
constexpr float VOID_REAVER_OFFTANK_KNOCK_THREAT = 15000.0f;
constexpr uint32 VOID_REAVER_ORB_IMPACT_MEMORY_MS = 6500;
constexpr uint32 VOID_REAVER_BOT_DODGE_RELEASE_MS = 5500;
constexpr uint32 VOID_REAVER_BOT_SPREAD_RELEASE_MS = 2500;
constexpr uint32 VOID_REAVER_RECENTER_THREAT_INTERVAL_MS = 6000;
constexpr float VOID_REAVER_PI = 3.14159265358979323846f;

bool IsValidVoidReaverEncounterBot(Creature const* creature)
{
    return creature && creature->GetBotAI() && creature->IsAlive() && creature->IsInWorld() && !creature->IsTempBot() && !creature->IsFreeBot();
}

bool IsValidVoidReaverEncounterUnit(Unit const* unit)
{
    if (!unit || !unit->IsAlive() || !unit->IsInWorld())
        return false;

    if (unit->IsPlayer())
        return true;

    Creature const* creature = unit->ToCreature();
    return IsValidVoidReaverEncounterBot(creature);
}

bool IsVoidReaverEncounterParticipantFor(Creature const* boss, Unit const* unit, float range = VOID_REAVER_ENCOUNTER_RANGE)
{
    if (!boss || !IsValidVoidReaverEncounterUnit(unit) || unit == boss)
        return false;

    if (boss->GetMap() != unit->GetMap() || !boss->IsWithinDistInMap(unit, range))
        return false;

    if (boss->GetVictim() == unit)
        return true;

    if (unit->IsInCombatWith(boss))
        return true;

    return boss->GetThreatMgr().GetThreat(unit, true) > 0.0f;
}

bool IsVoidReaverTankBot(Creature const* bot)
{
    bot_ai const* ai = bot ? bot->GetBotAI() : nullptr;
    return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK) || ai->HasRole(BOT_ROLE_TANK_OFF));
}

bool IsVoidReaverRangedOrHealerBot(Creature const* bot)
{
    bot_ai const* ai = bot ? bot->GetBotAI() : nullptr;
    return ai && (ai->HasRole(BOT_ROLE_RANGED) || ai->HasRole(BOT_ROLE_HEAL));
}

bool IsVoidReaverRangedOrHealerUnit(Unit const* unit)
{
    if (!unit)
        return false;

    if (unit->IsPlayer())
        return true;

    Creature const* bot = unit->ToCreature();
    return IsVoidReaverRangedOrHealerBot(bot);
}

bool IsVoidReaverMeleeUnit(Creature const* boss, Unit const* unit)
{
    if (!boss || !unit)
        return false;

    if (unit == boss->GetVictim())
        return true;

    float meleeZone = std::max(VOID_REAVER_ARCANE_ORB_MELEE_FALLBACK_RANGE, boss->GetCombatReach() + unit->GetCombatReach() + 6.0f);
    return boss->GetExactDist2d(unit) <= meleeZone;
}

bool IsBotCastingUninterruptible(Creature const* bot)
{
    if (!bot)
        return false;

    for (uint8 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        if (Spell* spell = bot->GetCurrentSpell(CurrentSpellTypes(i)))
            if (spell->GetSpellInfo() && (spell->GetSpellInfo()->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) == 0)
                return true;

    return false;
}

std::vector<Unit*> GatherVoidReaverEncounterUnits(Creature const* boss, float range = VOID_REAVER_ENCOUNTER_RANGE)
{
    std::vector<Unit*> units;

    if (!boss || !boss->GetMap())
        return units;

    Map::PlayerList const& players = boss->GetMap()->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* player = itr->GetSource();
        if (!player)
            continue;

        if (IsVoidReaverEncounterParticipantFor(boss, player, range))
            units.push_back(player);

        if (Group const* group = player->GetGroup())
        {
            for (GroupReference const* ref = group->GetFirstMember(); ref != nullptr; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->GetBotMgr())
                    continue;

                if (BotMap const* botMap = member->GetBotMgr()->GetBotMap())
                    for (BotMap::value_type const& pair : *botMap)
                        if (IsVoidReaverEncounterParticipantFor(boss, pair.second, range))
                            units.push_back(pair.second);
            }
        }
        else if (BotMgr* botMgr = player->GetBotMgr())
        {
            if (BotMap const* botMap = botMgr->GetBotMap())
                for (BotMap::value_type const& pair : *botMap)
                    if (IsVoidReaverEncounterParticipantFor(boss, pair.second, range))
                        units.push_back(pair.second);
        }
    }

    std::sort(units.begin(), units.end(), [](Unit const* left, Unit const* right)
    {
        return left->GetGUID() < right->GetGUID();
    });

    units.erase(std::unique(units.begin(), units.end()), units.end());
    return units;
}

std::vector<Creature*> GatherVoidReaverEncounterBots(Creature const* boss, float range = VOID_REAVER_ENCOUNTER_RANGE)
{
    std::vector<Creature*> bots;

    for (Unit* unit : GatherVoidReaverEncounterUnits(boss, range))
        if (Creature* bot = unit->ToCreature())
            if (IsValidVoidReaverEncounterBot(bot))
                bots.push_back(bot);

    return bots;
}

Position GetHomePositionFor(Creature const* creature)
{
    Position position;
    position.Relocate(creature->GetHomePosition());

    if (position.GetPositionX() == 0.0f && position.GetPositionY() == 0.0f)
        position.Relocate(creature);

    return position;
}

float GetDistance2d(Position const& first, Position const& second)
{
    float dx = first.GetPositionX() - second.GetPositionX();
    float dy = first.GetPositionY() - second.GetPositionY();
    return std::sqrt(dx * dx + dy * dy);
}

float GetDistance2d(Position const& position, WorldObject const* object)
{
    float dx = position.GetPositionX() - object->GetPositionX();
    float dy = position.GetPositionY() - object->GetPositionY();
    return std::sqrt(dx * dx + dy * dy);
}

Position MakePosition(float x, float y, float z, float orientation)
{
    Position position;
    position.Relocate(x, y, z, Position::NormalizeOrientation(orientation));
    return position;
}

void UpdateAllowedBotPositionZ(Creature* bot, Position& position)
{
    float x = position.GetPositionX();
    float y = position.GetPositionY();
    float z = position.GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    position.Relocate(x, y, z, position.GetOrientation());
}

void SafeBotMoveToPosition(Creature* bot, Position const& destination)
{
    if (!bot)
        return;

    if (bot_ai* ai = bot->GetBotAI())
        ai->MoveToSendPosition(destination);
    else
        bot->GetMotionMaster()->MovePoint(0, destination);
}

bool HasBlockingBotCommandState(Creature const* bot)
{
    bot_ai const* ai = bot ? bot->GetBotAI() : nullptr;
    if (!ai)
        return true;

    uint32 commandState = ai->GetBotCommandState();
    return (commandState & (BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION)) != 0;
}
}

struct boss_void_reaver : public BossAI
{
    boss_void_reaver(Creature* creature) : BossAI(creature, DATA_REAVER)
    {
        callForHelpRange = 105.0f;
        me->ApplySpellImmune(0, IMMUNITY_DISPEL, DISPEL_POISON, true);
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_HEALTH_LEECH, true);
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_POWER_DRAIN, true);
        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_PERIODIC_LEECH, true);
        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_PERIODIC_MANA_LEECH, true);
    }

    void Reset() override
    {
        BossAI::Reset();
        _recentlySpoken = false;
        _activeOrbImpacts.clear();
        _temporaryBotMoveStates.clear();
        _nextOrbId = 0;
        _recenteringTankGuid.Clear();
        _lastRecenterThreatMs = 0;
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (!_recentlySpoken)
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;
            scheduler.Schedule(5s, [this](TaskContext)
            {
                _recentlySpoken = false;
            });
        }
    }

    void JustDied(Unit* killer) override
    {
        ReleaseAllTemporaryBotMoves();
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);

        scheduler.Schedule(10min, [this](TaskContext)
        {
            DoCastSelf(SPELL_BERSERK);
        }).Schedule(8300ms, [this](TaskContext context)
        {
            Talk(SAY_POUNDING);
            DoCastSelf(SPELL_POUNDING);
            scheduler.DelayGroup(GROUP_ARCANE_ORB, 3s);
            context.Repeat(12100ms, 15800ms);
        }).Schedule(3450ms, GROUP_ARCANE_ORB, [this](TaskContext context)
        {
            CastArcaneOrb();
            context.Repeat(2400ms, 6300ms);
        }).Schedule(14350ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_KNOCK_AWAY);
            context.Repeat(20550ms, 22550ms);
        }).Schedule(3s, GROUP_BOT_RECENTER, [this](TaskContext context)
        {
            MaintainBotTankRecenter();
            context.Repeat(3s);
        }).Schedule(5s, GROUP_BOT_SPREAD, [this](TaskContext context)
        {
            MaintainRangedBotSpread();
            context.Repeat(6s);
        });
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ReleaseAllTemporaryBotMoves();
        BossAI::EnterEvadeMode(why);
    }

    void SpellHitTarget(Unit* target, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo)
            return;

        if (spellInfo->Id == SPELL_KNOCK_AWAY)
            SupportOffTankBotThreat(target);
    }

private:
    Unit* SelectArcaneOrbTarget()
    {
        std::vector<Unit*> preferred;
        std::vector<Unit*> fallback;
        std::vector<Unit*> lastResort;

        Unit* victim = me->GetVictim();
        for (Unit* unit : GatherVoidReaverEncounterUnits(me))
        {
            if (!unit || unit == me)
                continue;

            bool isVictim = unit == victim;
            float distance = me->GetExactDist2d(unit);
            bool outsideOrbRange = distance >= VOID_REAVER_ARCANE_ORB_MIN_RANGE && !IsVoidReaverMeleeUnit(me, unit);

            if (!isVictim && outsideOrbRange && IsVoidReaverRangedOrHealerUnit(unit))
            {
                preferred.push_back(unit);
                continue;
            }

            if (!isVictim && outsideOrbRange)
            {
                fallback.push_back(unit);
                continue;
            }

            if (!isVictim && distance > VOID_REAVER_ARCANE_ORB_MELEE_FALLBACK_RANGE)
                lastResort.push_back(unit);
        }

        if (!preferred.empty())
            return preferred[urand(0, uint32(preferred.size() - 1))];

        if (!fallback.empty())
            return fallback[urand(0, uint32(fallback.size() - 1))];

        if (!lastResort.empty())
            return lastResort[urand(0, uint32(lastResort.size() - 1))];

        return nullptr;
    }

    void CastArcaneOrb()
    {
        if (Unit* target = SelectArcaneOrbTarget())
        {
            Position impactPosition;
            impactPosition.Relocate(target);

            if (DoCast(target, SPELL_ARCANE_ORB) == SPELL_CAST_OK)
            {
                RegisterArcaneOrbImpact(impactPosition);
                HandleArcaneOrbTarget(target, impactPosition);
                return;
            }
        }

        if (DoCastRandomTarget(SPELL_ARCANE_ORB, 0, -VOID_REAVER_ARCANE_ORB_MIN_RANGE) == SPELL_CAST_OK)
            return;

        DoCastRandomTarget(SPELL_ARCANE_ORB, 0, VOID_REAVER_ARCANE_ORB_MELEE_FALLBACK_RANGE);
    }

    void RegisterArcaneOrbImpact(Position const& impactPosition)
    {
        uint32 orbId = ++_nextOrbId;
        _activeOrbImpacts[orbId] = impactPosition;

        scheduler.Schedule(Milliseconds(VOID_REAVER_ORB_IMPACT_MEMORY_MS), GROUP_BOT_ORB, [this, orbId](TaskContext)
        {
            _activeOrbImpacts.erase(orbId);
        });
    }

    void HandleArcaneOrbTarget(Unit* target, Position const& impactPosition)
    {
        if (Creature* targetBot = DBMFTABotCallouts::AsNPCBotCreature(target))
        {
            DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(targetBot, SPELL_ARCANE_ORB, "DBM-TheEye", "VoidReaver", "Arcane Orb", DBMFTABotCallouts::GetCooldownMs());
            MoveBotAwayFromArcaneOrb(targetBot, impactPosition, true);
        }

        for (Creature* bot : GatherVoidReaverEncounterBots(me))
        {
            if (!bot || bot == target)
                continue;

            if (IsInArcaneOrbDanger(bot, impactPosition))
                MoveBotAwayFromArcaneOrb(bot, impactPosition, false);
        }
    }

    bool IsInArcaneOrbDanger(Unit const* unit, Position const& impactPosition) const
    {
        if (!unit)
            return false;

        float dangerRadius = VOID_REAVER_ARCANE_ORB_DANGER_RADIUS + unit->GetCombatReach();
        return GetDistance2d(impactPosition, unit) <= dangerRadius;
    }

    bool IsSafeArcaneOrbDodgePosition(Position const& position, Creature const* bot) const
    {
        if (GetDistance2d(position, me) <= me->GetCombatReach() + bot->GetCombatReach() + 6.0f)
            return false;

        for (auto const& activeOrb : _activeOrbImpacts)
            if (GetDistance2d(activeOrb.second, position) <= VOID_REAVER_ARCANE_ORB_DANGER_RADIUS + bot->GetCombatReach())
                return false;

        Position home = GetHomePositionFor(me);
        float homeDistance = GetDistance2d(home, position);
        return homeDistance <= VOID_REAVER_RANGED_RING_MAX_RADIUS + 8.0f;
    }

    Position GetArcaneOrbDodgePosition(Creature* bot, Position const& impactPosition) const
    {
        float dx = bot->GetPositionX() - impactPosition.GetPositionX();
        float dy = bot->GetPositionY() - impactPosition.GetPositionY();
        float length = std::sqrt(dx * dx + dy * dy);

        Position home = GetHomePositionFor(me);
        if (length < 0.1f)
        {
            dx = bot->GetPositionX() - home.GetPositionX();
            dy = bot->GetPositionY() - home.GetPositionY();
            length = std::sqrt(dx * dx + dy * dy);
        }

        if (length < 0.1f)
        {
            float angle = bot->GetOrientation();
            dx = std::cos(angle);
            dy = std::sin(angle);
            length = 1.0f;
        }

        dx /= length;
        dy /= length;

        float clearance = VOID_REAVER_ARCANE_ORB_DANGER_RADIUS + bot->GetCombatReach() + VOID_REAVER_ORB_DODGE_CLEARANCE;
        Position candidate = MakePosition(
            impactPosition.GetPositionX() + dx * clearance,
            impactPosition.GetPositionY() + dy * clearance,
            bot->GetPositionZ(),
            bot->GetAngle(me));

        ClampToVoidReaverRangedRing(candidate, bot);
        UpdateAllowedBotPositionZ(bot, candidate);

        if (IsSafeArcaneOrbDodgePosition(candidate, bot))
            return candidate;

        float baseAngle = std::atan2(bot->GetPositionY() - home.GetPositionY(), bot->GetPositionX() - home.GetPositionX());
        for (float offset : { VOID_REAVER_PI / 3.0f, -VOID_REAVER_PI / 3.0f, VOID_REAVER_PI / 2.0f, -VOID_REAVER_PI / 2.0f })
        {
            float angle = baseAngle + offset;
            Position alternate = MakePosition(
                home.GetPositionX() + std::cos(angle) * VOID_REAVER_RANGED_RING_RADIUS,
                home.GetPositionY() + std::sin(angle) * VOID_REAVER_RANGED_RING_RADIUS,
                bot->GetPositionZ(),
                bot->GetAngle(me));
            UpdateAllowedBotPositionZ(bot, alternate);

            if (IsSafeArcaneOrbDodgePosition(alternate, bot))
                return alternate;
        }

        return candidate;
    }

    void ClampToVoidReaverRangedRing(Position& position, Creature const* bot) const
    {
        Position home = GetHomePositionFor(me);
        float dx = position.GetPositionX() - home.GetPositionX();
        float dy = position.GetPositionY() - home.GetPositionY();
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 0.1f)
        {
            float angle = bot->GetOrientation();
            dx = std::cos(angle);
            dy = std::sin(angle);
            distance = 1.0f;
        }

        float clampedDistance = std::min(std::max(distance, VOID_REAVER_RANGED_RING_MIN_RADIUS), VOID_REAVER_RANGED_RING_MAX_RADIUS);
        position.Relocate(
            home.GetPositionX() + dx / distance * clampedDistance,
            home.GetPositionY() + dy / distance * clampedDistance,
            position.GetPositionZ(),
            position.GetOrientation());
    }

    void MoveBotAwayFromArcaneOrb(Creature* bot, Position const& impactPosition, bool isTarget)
    {
        if (!IsValidVoidReaverEncounterBot(bot))
            return;

        if (!isTarget && IsVoidReaverTankBot(bot))
            return;

        if (!isTarget && !IsInArcaneOrbDanger(bot, impactPosition))
            return;

        if (bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
            return;

        if (!isTarget && IsBotCastingUninterruptible(bot))
            return;

        TrackTemporaryBotMove(bot);
        bot->InterruptNonMeleeSpells(false);
        bot->BotStopMovement();

        Position destination = GetArcaneOrbDodgePosition(bot, impactPosition);
        SafeBotMoveToPosition(bot, destination);

        ObjectGuid guid = bot->GetGUID();
        scheduler.Schedule(Milliseconds(VOID_REAVER_BOT_DODGE_RELEASE_MS), GROUP_BOT_ORB, [this, guid](TaskContext)
        {
            ReleaseTemporaryBotMove(guid);
        });

        LOG_DEBUG("scripts", "Void Reaver moved NPCBot {} away from Arcane Orb{}.", bot->GetName(), isTarget ? " target" : " splash danger");
    }

    void MaintainRangedBotSpread()
    {
        std::vector<Creature*> bots = GatherVoidReaverEncounterBots(me);
        std::vector<Creature*> rangedBots;

        for (Creature* bot : bots)
        {
            if (!IsValidVoidReaverEncounterBot(bot) || IsVoidReaverTankBot(bot) || !IsVoidReaverRangedOrHealerBot(bot))
                continue;

            if (IsTemporaryBotMoveActive(bot) || HasBlockingBotCommandState(bot) || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            rangedBots.push_back(bot);
        }

        if (rangedBots.size() < 2)
            return;

        std::sort(rangedBots.begin(), rangedBots.end(), [](Creature const* left, Creature const* right)
        {
            return left->GetGUID() < right->GetGUID();
        });

        Position home = GetHomePositionFor(me);
        for (std::size_t i = 0; i < rangedBots.size(); ++i)
        {
            Creature* bot = rangedBots[i];

            if (IsBotCastingUninterruptible(bot))
                continue;

            bool needsMove = IsVoidReaverMeleeUnit(me, bot) || IsTooCloseToOtherRangedBot(bot, rangedBots);
            Position destination = GetRangedBotSpreadPosition(bot, home, i, rangedBots.size());

            if (!needsMove && GetDistance2d(destination, bot) <= 8.0f)
                continue;

            TrackTemporaryBotMove(bot);
            SafeBotMoveToPosition(bot, destination);

            ObjectGuid guid = bot->GetGUID();
            scheduler.Schedule(Milliseconds(VOID_REAVER_BOT_SPREAD_RELEASE_MS), GROUP_BOT_SPREAD, [this, guid](TaskContext)
            {
                ReleaseTemporaryBotMove(guid);
            });

            LOG_DEBUG("scripts", "Void Reaver spread NPCBot {} to ranged ring.", bot->GetName());
        }
    }

    bool IsTooCloseToOtherRangedBot(Creature const* bot, std::vector<Creature*> const& rangedBots) const
    {
        for (Creature const* other : rangedBots)
        {
            if (!other || other == bot)
                continue;

            if (bot->GetExactDist2d(other) < VOID_REAVER_RANGED_SPREAD_MIN_DISTANCE)
                return true;
        }

        return false;
    }

    Position GetRangedBotSpreadPosition(Creature* bot, Position const& home, std::size_t index, std::size_t count) const
    {
        float angle = count ? (2.0f * VOID_REAVER_PI * float(index) / float(count)) : bot->GetOrientation();
        Position destination = MakePosition(
            home.GetPositionX() + std::cos(angle) * VOID_REAVER_RANGED_RING_RADIUS,
            home.GetPositionY() + std::sin(angle) * VOID_REAVER_RANGED_RING_RADIUS,
            bot->GetPositionZ(),
            bot->GetAngle(me));
        UpdateAllowedBotPositionZ(bot, destination);
        return destination;
    }

    void MaintainBotTankRecenter()
    {
        if (!me->IsInCombat())
            return;

        Position home = GetHomePositionFor(me);
        float bossHomeDistance = GetDistance2d(home, me);

        if (bossHomeDistance <= VOID_REAVER_HOME_OK_RADIUS)
        {
            if (!_recenteringTankGuid.IsEmpty())
            {
                ReleaseTemporaryBotMove(_recenteringTankGuid);
                LOG_DEBUG("scripts", "Void Reaver stopped NPCBot recenter support because boss returned home.");
                _recenteringTankGuid.Clear();
            }
            return;
        }

        if (bossHomeDistance < VOID_REAVER_HOME_RESET_RADIUS && _recenteringTankGuid.IsEmpty())
            return;

        Unit* victim = me->GetVictim();
        if (victim && victim->IsPlayer() && victim->IsAlive())
        {
            if (!_recenteringTankGuid.IsEmpty())
            {
                ReleaseTemporaryBotMove(_recenteringTankGuid);
                LOG_DEBUG("scripts", "Void Reaver released NPCBot recenter support because a player is tanking.");
                _recenteringTankGuid.Clear();
            }

            return;
        }

        Creature* tankBot = SelectRecenterTankBot();
        if (!tankBot)
            return;

        Position destination = GetTankRecenterPosition(tankBot, home);
        TrackTemporaryBotMove(tankBot);
        tankBot->BotStopMovement();
        SafeBotMoveToPosition(tankBot, destination);

        bool newTank = _recenteringTankGuid != tankBot->GetGUID();
        uint32 now = uint32(GameTime::GetGameTimeMS().count());
        if (newTank || now >= _lastRecenterThreatMs + VOID_REAVER_RECENTER_THREAT_INTERVAL_MS)
        {
            AddThreatToTankBot(tankBot, VOID_REAVER_RECENTER_THREAT);
            _lastRecenterThreatMs = now;
        }

        _recenteringTankGuid = tankBot->GetGUID();
        LOG_DEBUG("scripts", "Void Reaver recenter support moved NPCBot tank {} toward home.", tankBot->GetName());
    }

    Creature* SelectRecenterTankBot()
    {
        if (Unit* victim = me->GetVictim())
            if (Creature* victimBot = victim->ToCreature())
                if (IsValidVoidReaverEncounterBot(victimBot) && IsVoidReaverTankBot(victimBot))
                    return victimBot;

        Creature* bestBot = nullptr;
        float bestThreat = -1.0f;

        for (Creature* bot : GatherVoidReaverEncounterBots(me))
        {
            if (!IsValidVoidReaverEncounterBot(bot) || !IsVoidReaverTankBot(bot))
                continue;

            if (bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            float threat = me->GetThreatMgr().GetThreat(bot, true);
            if (threat > bestThreat)
            {
                bestThreat = threat;
                bestBot = bot;
            }
        }

        return bestBot;
    }

    Position GetTankRecenterPosition(Creature* tankBot, Position const& home) const
    {
        float dx = home.GetPositionX() - me->GetPositionX();
        float dy = home.GetPositionY() - me->GetPositionY();
        float length = std::sqrt(dx * dx + dy * dy);

        if (length < 0.1f)
        {
            dx = std::cos(me->GetOrientation());
            dy = std::sin(me->GetOrientation());
            length = 1.0f;
        }

        dx /= length;
        dy /= length;

        Position destination = MakePosition(
            home.GetPositionX() + dx * VOID_REAVER_RECENTER_TANK_OFFSET,
            home.GetPositionY() + dy * VOID_REAVER_RECENTER_TANK_OFFSET,
            tankBot->GetPositionZ(),
            tankBot->GetAngle(me));
        UpdateAllowedBotPositionZ(tankBot, destination);
        return destination;
    }

    void SupportOffTankBotThreat(Unit* knockedTarget)
    {
        if (!knockedTarget || knockedTarget->IsPlayer())
            return;

        Creature* knockedBot = knockedTarget->ToCreature();
        if (!IsValidVoidReaverEncounterBot(knockedBot) || !IsVoidReaverTankBot(knockedBot))
            return;

        for (Creature* bot : GatherVoidReaverEncounterBots(me))
        {
            if (!bot || bot == knockedBot || !IsVoidReaverTankBot(bot))
                continue;

            AddThreatToTankBot(bot, VOID_REAVER_OFFTANK_KNOCK_THREAT);
            LOG_DEBUG("scripts", "Void Reaver added NPCBot off-tank threat to {} after Knock Away.", bot->GetName());
        }
    }

    void AddThreatToTankBot(Creature* tankBot, float amount)
    {
        if (!IsValidVoidReaverEncounterBot(tankBot))
            return;

        me->SetInCombatWith(tankBot);
        tankBot->SetInCombatWith(me);
        me->GetThreatMgr().AddThreat(tankBot, amount, nullptr, true, true);

        if (bot_ai* ai = tankBot->GetBotAI())
        {
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(me);
        }
    }

    void TrackTemporaryBotMove(Creature* bot)
    {
        if (!bot)
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        _temporaryBotMoveStates.emplace(bot->GetGUID(), ai->GetBotCommandState());
    }

    bool IsTemporaryBotMoveActive(Creature const* bot) const
    {
        return bot && _temporaryBotMoveStates.find(bot->GetGUID()) != _temporaryBotMoveStates.end();
    }

    void ReleaseTemporaryBotMove(ObjectGuid guid)
    {
        auto itr = _temporaryBotMoveStates.find(guid);
        if (itr == _temporaryBotMoveStates.end())
            return;

        uint32 originalState = itr->second;
        _temporaryBotMoveStates.erase(itr);

        Creature* bot = ObjectAccessor::GetCreature(*me, guid);
        if (!IsValidVoidReaverEncounterBot(bot))
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        if ((originalState & BOT_COMMAND_STAY) == 0)
            ai->RemoveBotCommandState(BOT_COMMAND_STAY);

        if (me->IsAlive() && me->IsInCombat() && (originalState & (BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION)) == 0)
        {
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(me);
        }
    }

    void ReleaseAllTemporaryBotMoves()
    {
        std::vector<ObjectGuid> guids;
        guids.reserve(_temporaryBotMoveStates.size());

        for (auto const& state : _temporaryBotMoveStates)
            guids.push_back(state.first);

        for (ObjectGuid const& guid : guids)
            ReleaseTemporaryBotMove(guid);

        _activeOrbImpacts.clear();
        _recenteringTankGuid.Clear();
    }

    bool _recentlySpoken = false;
    std::map<ObjectGuid, uint32> _temporaryBotMoveStates;
    std::map<uint32, Position> _activeOrbImpacts;
    ObjectGuid _recenteringTankGuid;
    uint32 _nextOrbId = 0;
    uint32 _lastRecenterThreatMs = 0;
};

void AddSC_boss_void_reaver()
{
    RegisterTheEyeAI(boss_void_reaver);
}
