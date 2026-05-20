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
#include "Config.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

enum Emotes
{
    EMOTE_PHASE_BANISH      = 0,
    EMOTE_PHASE_PORTAL      = 1
};

enum Spells
{
    SPELL_NETHERBURN_AURA   = 30522,
    SPELL_VOIDZONE          = 37063,
    SPELL_NETHER_INFUSION   = 38688,
    SPELL_NETHERBREATH      = 38523,
    SPELL_BANISH_VISUAL     = 39833,
    SPELL_BANISH_ROOT       = 42716,
    SPELL_EMPOWERMENT       = 38549,
    SPELL_NETHERSPITE_ROAR  = 38684
};

enum Portals
{
    RED_PORTAL              = 0, // Perseverence
    GREEN_PORTAL            = 1, // Serenity
    BLUE_PORTAL             = 2  // Dominance
};

enum Groups
{
    PORTAL_PHASE            = 0,
    BANISH_PHASE            = 1,
    GROUP_BOT_DIRECTOR      = 2
};

struct TimedPosition
{
    Position pos;
    uint32 expireMs;
};

const float PortalCoord[3][3] =
{
    {-11195.353516f, -1613.237183f, 278.237258f}, // Left side
    {-11137.846680f, -1685.607422f, 278.239258f}, // Right side
    {-11094.493164f, -1591.969238f, 279.949188f}  // Back side
};

const uint32 PortalID[3] =     {17369, 17367, 17368};
const uint32 PortalVisual[3] = {30487, 30490, 30491};
const uint32 PortalBeam[3] =   {30465, 30464, 30463};
const uint32 PlayerBuff[3] =   {30421, 30422, 30423};
const uint32 NetherBuff[3] =   {30466, 30467, 30468};
const uint32 PlayerDebuff[3] = {38637, 38638, 38639};

struct boss_netherspite : public BossAI
{
    boss_netherspite(Creature* creature) : BossAI(creature, DATA_NETHERSPITE)
    {
        LoadBotDirectorConfig();
        ClearBeamAssignments();
    }

    bool IsBetween(WorldObject const* u1, WorldObject const* target, WorldObject const* u2) const // the in-line checker
    {
        if (!u1 || !u2 || !target)
            return false;

        float xn, yn, xp, yp, xh, yh;
        xn = u1->GetPositionX();
        yn = u1->GetPositionY();
        xp = u2->GetPositionX();
        yp = u2->GetPositionY();
        xh = target->GetPositionX();
        yh = target->GetPositionY();

        // check if target is between (not checking distance from the beam yet)
        if (dist(xn, yn, xh, yh) >= dist(xn, yn, xp, yp) || dist(xp, yp, xh, yh) >= dist(xn, yn, xp, yp))
            return false;
        // check  distance from the beam
        return (std::abs((xn - xp) * yh + (yp - yn) * xh - xn * yp + xp * yn) / dist(xn, yn, xp, yp) < 1.5f);
    }

    bool IsBetween(WorldObject const* u1, Position const& target, WorldObject const* u2) const
    {
        if (!u1 || !u2)
            return false;

        float xn = u1->GetPositionX();
        float yn = u1->GetPositionY();
        float xp = u2->GetPositionX();
        float yp = u2->GetPositionY();
        float xh = target.GetPositionX();
        float yh = target.GetPositionY();
        float beamLength = dist(xn, yn, xp, yp);

        if (beamLength <= 0.0f)
            return false;

        if (dist(xn, yn, xh, yh) >= beamLength || dist(xp, yp, xh, yh) >= beamLength)
            return false;

        return (std::abs((xn - xp) * yh + (yp - yn) * xh - xn * yp + xp * yn) / beamLength < 1.5f);
    }

    float dist(float xa, float ya, float xb, float yb) const // auxiliary method for distance
    {
        return std::sqrt((xa - xb) * (xa - xb) + (ya - yb) * (ya - yb));
    }

    void LoadBotDirectorConfig()
    {
        _npcBotsEnabled = sConfigMgr->GetOption<bool>("Netherspite.NPCBots.Enabled", true);
        _botDirectorTickMs = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Netherspite.NPCBots.DirectorTickMs", 750), 250, 3000);
        _voidZoneDangerRadius = std::clamp<float>(sConfigMgr->GetOption<float>("Netherspite.NPCBots.VoidZoneDangerRadius", 7.0f), 4.0f, 15.0f);
        _beamArrivedDistance = std::clamp<float>(sConfigMgr->GetOption<float>("Netherspite.NPCBots.BeamArrivedDistance", 1.25f), 0.75f, 3.0f);
        _beamMaxStacks[RED_PORTAL] = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Netherspite.NPCBots.RedBeamMaxStacks", 30), 5, 80));
        _beamMaxStacks[GREEN_PORTAL] = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Netherspite.NPCBots.GreenBeamMaxStacks", 25), 5, 80));
        _beamMaxStacks[BLUE_PORTAL] = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Netherspite.NPCBots.BlueBeamMaxStacks", 25), 5, 80));
        _redEmergencyUseNonTank = sConfigMgr->GetOption<bool>("Netherspite.NPCBots.RedEmergencyUseNonTank", true);
        _greenRequireManaUser = sConfigMgr->GetOption<bool>("Netherspite.NPCBots.GreenRequireManaUser", true);
        _banishMoveBotsToSafety = sConfigMgr->GetOption<bool>("Netherspite.NPCBots.BanishMoveBotsToSafety", true);
        _debugSay = sConfigMgr->GetOption<bool>("Netherspite.NPCBots.DebugSay", false);
    }

    static bool IsNPCBotUnit(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && creature->GetBotAI();
    }

    Creature* ToNPCBot(Unit* unit) const
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot() || creature->IsTempBot() || creature->IsFreeBot() || !creature->GetBotAI())
            return nullptr;

        return creature;
    }

    bool IsEncounterParticipant(Unit* unit) const
    {
        if (!unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != me->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(me, 160.0f))
            return false;

        if (unit->IsPlayer())
            return true;

        return ToNPCBot(unit) != nullptr;
    }

    std::vector<Unit*> GatherEncounterUnits() const
    {
        std::vector<Unit*> units;
        GuidSet seen;

        auto addUnit = [&](Unit* unit)
        {
            if (!IsEncounterParticipant(unit) || seen.count(unit->GetGUID()))
                return;

            seen.insert(unit->GetGUID());
            units.push_back(unit);
        };

        Map::PlayerList const& players = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            addUnit(player);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    addUnit(member);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
            {
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    addUnit(pair.second);
            }
        }

        return units;
    }

    bool IsNpcBotTank(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK));
    }

    bool IsNpcBotHealer(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        return ai && ai->HasRole(BOT_ROLE_HEAL);
    }

    bool IsManaUser(Unit* unit) const
    {
        return unit && unit->getPowerType() == POWER_MANA && unit->GetMaxPower(POWER_MANA) > 0;
    }

    bool IsNpcBotRangedOrCaster(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        return ai && (ai->HasRole(BOT_ROLE_RANGED) || IsManaUser(bot));
    }

    bool IsPlayerMarkedTank(Player* player) const
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

    bool IsBotCastingUninterruptible(Creature* bot) const
    {
        if (Spell* spell = bot ? bot->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr)
            if (SpellInfo const* spellInfo = spell->GetSpellInfo())
                if (!(spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT))
                    return true;

        if (Spell* spell = bot ? bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) : nullptr)
            if (SpellInfo const* spellInfo = spell->GetSpellInfo())
                if (!(spellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT))
                    return true;

        return false;
    }

    uint8 GetBeamStackCount(Unit* unit, uint8 beam) const
    {
        if (!unit)
            return 0;

        if (Aura* aura = unit->GetAura(PlayerBuff[beam]))
            return aura->GetStackAmount();

        return unit->HasAura(PlayerBuff[beam]) ? 1 : 0;
    }

    void ClearBeamAssignments()
    {
        for (uint8 i = 0; i < 3; ++i)
            _beamAssignments[i].Clear();
    }

    bool IsScriptControlledBot(Creature* bot) const
    {
        return bot && _scriptedBotCommandStates.count(bot->GetGUID());
    }

    bool IsAssignedToOtherBeam(Creature* bot, uint8 beam) const
    {
        if (!bot)
            return false;

        for (uint8 i = 0; i < 3; ++i)
            if (i != beam && _beamAssignments[i] == bot->GetGUID())
                return true;

        return false;
    }

    bool IsBeamMissing(uint8 beam) const
    {
        if (_beamAssignments[beam])
            return false;

        Creature* portal = ObjectAccessor::GetCreature(*me, PortalGUID[beam]);
        return portal && !IsBeamHandledByPlayer(beam, portal);
    }

    void TrackBotCommandState(Creature* bot)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!ai || _scriptedBotCommandStates.count(bot->GetGUID()))
            return;

        _scriptedBotCommandStates[bot->GetGUID()] = ai->GetBotCommandState();
    }

    void RestoreBotDirectorState(Creature* bot)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!bot || !ai)
            return;

        std::map<ObjectGuid, uint32>::iterator itr = _scriptedBotCommandStates.find(bot->GetGUID());
        if (itr == _scriptedBotCommandStates.end())
            return;

        uint32 originalState = itr->second;

        if (!(originalState & BOT_COMMAND_STAY))
            ai->RemoveBotCommandState(BOT_COMMAND_STAY);

        if (!(originalState & BOT_COMMAND_INACTION))
            ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

        if (!(originalState & BOT_COMMAND_MASK_NOCAST_ANY))
            ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);

        _scriptedBotCommandStates.erase(itr);

        if (!bot->IsAlive())
            return;

        if (me->IsEngaged() && me->GetVictim() && bot->IsInMap(me))
        {
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(me);
        }
        else if (!ai->IAmFree())
        {
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }
    }

    void RestoreBotDirectorStateIfUnassigned(ObjectGuid botGuid)
    {
        for (uint8 i = 0; i < 3; ++i)
            if (_beamAssignments[i] == botGuid)
                return;

        if (Creature* bot = ObjectAccessor::GetCreature(*me, botGuid))
            RestoreBotDirectorState(bot);
    }

    void ReleaseAllBotDirectorStates()
    {
        std::vector<ObjectGuid> controlledBots;
        controlledBots.reserve(_scriptedBotCommandStates.size());

        for (std::map<ObjectGuid, uint32>::value_type const& pair : _scriptedBotCommandStates)
            controlledBots.push_back(pair.first);

        for (ObjectGuid const& guid : controlledBots)
            if (Creature* bot = ObjectAccessor::GetCreature(*me, guid))
                RestoreBotDirectorState(bot);

        _scriptedBotCommandStates.clear();
    }

    void ReleaseBeamAssignment(uint8 beam, bool moveAway, char const* debugText = nullptr)
    {
        ObjectGuid botGuid = _beamAssignments[beam];
        _beamAssignments[beam].Clear();

        Creature* bot = ObjectAccessor::GetCreature(*me, botGuid);
        if (!bot)
            return;

        if (_debugSay && debugText)
            bot->Say(debugText, LANG_UNIVERSAL);

        if (moveAway)
        {
            if (Creature* portal = ObjectAccessor::GetCreature(*me, PortalGUID[beam]))
                MoveBotOffBeam(bot, portal);

            ObjectGuid guid = bot->GetGUID();
            scheduler.Schedule(2500ms, GROUP_BOT_DIRECTOR, [this, guid](TaskContext /*context*/)
            {
                RestoreBotDirectorStateIfUnassigned(guid);
            });
            return;
        }

        RestoreBotDirectorState(bot);
    }

    void DebugBeamSay(Creature* bot, uint8 beam, char const* prefix)
    {
        if (!_debugSay || !bot || !prefix)
            return;

        char const* beamName = beam == RED_PORTAL ? "red beam." : beam == GREEN_PORTAL ? "green beam." : "blue beam.";
        bot->Say(std::string(prefix) + beamName, LANG_UNIVERSAL);
    }

    bool IsBotEligibleForDirectorMovement(Creature* bot, bool emergencyMovement) const
    {
        if (!IsEncounterParticipant(bot))
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree() || bot->IsTempBot() || bot->IsFreeBot())
            return false;

        if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_ROOT | UNIT_STATE_ISOLATED))
            return false;

        if (!emergencyMovement && IsBotCastingUninterruptible(bot))
            return false;

        if (!IsScriptControlledBot(bot) && ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY))
            return false;

        return true;
    }

    bool IsBotEligibleForBeam(Creature* bot, uint8 beam, bool allowVoidZone) const
    {
        if (!IsBotEligibleForDirectorMovement(bot, false))
            return false;

        if (IsAssignedToOtherBeam(bot, beam))
            return false;

        if (bot->HasAura(PlayerDebuff[beam]))
            return false;

        if (GetBeamStackCount(bot, beam) >= _beamMaxStacks[beam])
            return false;

        if (!allowVoidZone && IsInTrackedVoidZone(bot->GetPosition()))
            return false;

        return true;
    }

    std::vector<Creature*> GetEligibleBeamBots(uint8 beam, bool allowVoidZone) const
    {
        std::vector<Creature*> bots;

        for (Unit* unit : GatherEncounterUnits())
        {
            Creature* bot = ToNPCBot(unit);
            if (bot && IsBotEligibleForBeam(bot, beam, allowVoidZone))
                bots.push_back(bot);
        }

        return bots;
    }

    float BotCommonScore(Creature* bot, uint8 beam) const
    {
        float score = bot->GetHealthPct() * 10.0f;
        score += float(bot->GetMaxHealth()) / 250.0f;
        score -= bot->GetDistance(me);
        score -= float(GetBeamStackCount(bot, beam)) * 20.0f;

        if (IsInTrackedVoidZone(bot->GetPosition()))
            score -= 10000.0f;

        return score;
    }

    Creature* SelectRedBeamBot() const
    {
        if (Creature* victimBot = ToNPCBot(me->GetVictim()))
            if (IsNpcBotTank(victimBot) && IsBotEligibleForBeam(victimBot, RED_PORTAL, true))
                return victimBot;

        std::vector<Creature*> candidates = GetEligibleBeamBots(RED_PORTAL, false);
        if (candidates.empty())
            candidates = GetEligibleBeamBots(RED_PORTAL, true);

        Creature* bestMainTank = nullptr;
        Creature* bestOffTank = nullptr;
        Creature* bestAnyTank = nullptr;
        float bestMainScore = -std::numeric_limits<float>::max();
        float bestOffScore = -std::numeric_limits<float>::max();
        float bestAnyTankScore = -std::numeric_limits<float>::max();

        for (Creature* bot : candidates)
        {
            bot_ai* ai = bot->GetBotAI();
            if (!ai || !IsNpcBotTank(bot))
                continue;

            float score = BotCommonScore(bot, RED_PORTAL);
            if (ai->IsTank() && !ai->IsOffTank() && score > bestMainScore)
            {
                bestMainTank = bot;
                bestMainScore = score;
            }
            else if (ai->IsOffTank() && score > bestOffScore)
            {
                bestOffTank = bot;
                bestOffScore = score;
            }

            if (score > bestAnyTankScore)
            {
                bestAnyTank = bot;
                bestAnyTankScore = score;
            }
        }

        if (bestMainTank)
            return bestMainTank;

        if (bestOffTank)
            return bestOffTank;

        if (bestAnyTank)
            return bestAnyTank;

        if (!_redEmergencyUseNonTank)
            return nullptr;

        Creature* bestEmergency = nullptr;
        float bestScore = -std::numeric_limits<float>::max();
        for (Creature* bot : candidates)
        {
            if (IsNpcBotTank(bot))
                continue;

            bot_ai* ai = bot->GetBotAI();
            float score = BotCommonScore(bot, RED_PORTAL);
            if (ai && ai->HasRole(BOT_ROLE_DPS) && !IsNpcBotRangedOrCaster(bot))
                score += 250.0f;

            if (score > bestScore)
            {
                bestEmergency = bot;
                bestScore = score;
            }
        }

        return bestEmergency;
    }

    Creature* SelectGreenBeamBot() const
    {
        std::vector<Creature*> candidates = GetEligibleBeamBots(GREEN_PORTAL, false);
        if (candidates.empty())
            candidates = GetEligibleBeamBots(GREEN_PORTAL, true);

        Creature* best = nullptr;
        float bestScore = -std::numeric_limits<float>::max();
        bool foundManaUser = false;

        for (Creature* bot : candidates)
        {
            if (!IsManaUser(bot))
                continue;

            foundManaUser = true;
            float manaPct = bot->GetMaxPower(POWER_MANA) ? 100.0f * float(bot->GetPower(POWER_MANA)) / float(bot->GetMaxPower(POWER_MANA)) : 0.0f;
            float score = BotCommonScore(bot, GREEN_PORTAL) + manaPct * 4.0f;

            if (IsNpcBotHealer(bot))
                score += 1000.0f;

            if (score > bestScore)
            {
                best = bot;
                bestScore = score;
            }
        }

        if (best || (_greenRequireManaUser && foundManaUser))
            return best;

        if (_greenRequireManaUser && !candidates.empty())
        {
            // No mana user exists in the eligible bot pool, so take the least-bad fallback rather than feeding Netherspite the beam.
            for (Creature* bot : candidates)
            {
                float score = BotCommonScore(bot, GREEN_PORTAL);
                if (score > bestScore)
                {
                    best = bot;
                    bestScore = score;
                }
            }

            return best;
        }

        for (Creature* bot : candidates)
        {
            float score = BotCommonScore(bot, GREEN_PORTAL);
            if (IsNpcBotHealer(bot))
                score += 500.0f;

            if (score > bestScore)
            {
                best = bot;
                bestScore = score;
            }
        }

        return best;
    }

    Creature* SelectBlueBeamBot() const
    {
        std::vector<Creature*> candidates = GetEligibleBeamBots(BLUE_PORTAL, false);
        if (candidates.empty())
            candidates = GetEligibleBeamBots(BLUE_PORTAL, true);

        bool const redNeedsTank = IsBeamMissing(RED_PORTAL);
        bool const greenNeedsHealer = IsBeamMissing(GREEN_PORTAL);

        Creature* best = nullptr;
        float bestScore = -std::numeric_limits<float>::max();

        auto choose = [&](bool allowHealer) -> Creature*
        {
            Creature* localBest = nullptr;
            float localBestScore = -std::numeric_limits<float>::max();

            for (Creature* bot : candidates)
            {
                bot_ai* ai = bot->GetBotAI();
                if (!ai)
                    continue;

                if (redNeedsTank && IsNpcBotTank(bot))
                    continue;

                if (!allowHealer && greenNeedsHealer && IsNpcBotHealer(bot))
                    continue;

                float score = BotCommonScore(bot, BLUE_PORTAL);

                if (ai->HasRole(BOT_ROLE_DPS))
                    score += 600.0f;

                if (IsNpcBotRangedOrCaster(bot))
                    score += 350.0f;

                if (IsNpcBotTank(bot))
                    score -= 700.0f;

                if (IsNpcBotHealer(bot))
                    score -= 500.0f;

                if (score > localBestScore)
                {
                    localBest = bot;
                    localBestScore = score;
                }
            }

            if (localBest && localBestScore > bestScore)
            {
                best = localBest;
                bestScore = localBestScore;
            }

            return localBest;
        };

        if (choose(false))
            return best;

        if (choose(true))
            return best;

        return best;
    }

    Creature* SelectBotForBeam(uint8 beam) const
    {
        switch (beam)
        {
            case RED_PORTAL:
                return SelectRedBeamBot();
            case GREEN_PORTAL:
                return SelectGreenBeamBot();
            case BLUE_PORTAL:
                return SelectBlueBeamBot();
            default:
                return nullptr;
        }
    }

    void StartBeamAssignment(Creature* bot, uint8 beam)
    {
        if (!bot)
            return;

        _beamAssignments[beam] = bot->GetGUID();
        TrackBotCommandState(bot);
        DebugBeamSay(bot, beam, "Taking ");
    }

    bool IsEligibleBeamTarget(Unit* unit, uint8 beam, Creature* portal) const
    {
        return unit &&
            IsEncounterParticipant(unit) &&
            !unit->HasAura(PlayerDebuff[beam]) &&
            (unit->IsPlayer() || IsNPCBotUnit(unit)) &&
            IsBetween(me, unit, portal);
    }

    bool IsBeamHandledByPlayer(uint8 beam, Creature* portal) const
    {
        for (Unit* unit : GatherEncounterUnits())
            if (Player* player = unit->ToPlayer())
                if (IsEligibleBeamTarget(player, beam, portal))
                    return true;

        return false;
    }

    bool IsInTrackedVoidZone(Position const& position) const
    {
        for (TimedPosition const& voidZone : _voidZones)
            if (dist(position.GetPositionX(), position.GetPositionY(), voidZone.pos.GetPositionX(), voidZone.pos.GetPositionY()) <= _voidZoneDangerRadius)
                return true;

        return false;
    }

    TimedPosition const* GetNearestVoidZone(Position const& position) const
    {
        TimedPosition const* nearest = nullptr;
        float nearestDist = std::numeric_limits<float>::max();

        for (TimedPosition const& voidZone : _voidZones)
        {
            float distance = dist(position.GetPositionX(), position.GetPositionY(), voidZone.pos.GetPositionX(), voidZone.pos.GetPositionY());
            if (distance < nearestDist)
            {
                nearest = &voidZone;
                nearestDist = distance;
            }
        }

        return nearest;
    }

    void TrackVoidZone(Position const& position, uint32 durationMs)
    {
        TimedPosition tracked;
        tracked.pos = position;
        tracked.expireMs = durationMs;
        _voidZones.push_back(tracked);
    }

    void PruneVoidZones(uint32 diff)
    {
        for (TimedPosition& voidZone : _voidZones)
        {
            if (voidZone.expireMs > diff)
                voidZone.expireMs -= diff;
            else
                voidZone.expireMs = 0;
        }

        _voidZones.erase(std::remove_if(_voidZones.begin(), _voidZones.end(), [](TimedPosition const& voidZone)
        {
            return voidZone.expireMs == 0;
        }), _voidZones.end());
    }

    Position GetBeamSoakPosition(Creature* portal, uint8 beam, float forcedT = -1.0f) const
    {
        static constexpr float DefaultBeamT[3] = { 0.30f, 0.60f, 0.70f };

        float t = forcedT >= 0.0f ? forcedT : DefaultBeamT[beam];
        float x = me->GetPositionX() + (portal->GetPositionX() - me->GetPositionX()) * t;
        float y = me->GetPositionY() + (portal->GetPositionY() - me->GetPositionY()) * t;
        float z = me->GetPositionZ() + (portal->GetPositionZ() - me->GetPositionZ()) * t;
        float orientation = Position::NormalizeOrientation(std::atan2(me->GetPositionY() - y, me->GetPositionX() - x));

        Position position;
        position.Relocate(x, y, z, orientation);
        return position;
    }

    bool FindSafeBeamSoakPosition(Creature* portal, uint8 beam, Creature* bot, Position& out) const
    {
        Position defaultPosition = GetBeamSoakPosition(portal, beam);
        if (!IsInTrackedVoidZone(defaultPosition) && IsBetween(me, defaultPosition, portal))
        {
            out = defaultPosition;
            return true;
        }

        float minT = beam == RED_PORTAL ? 0.20f : 0.45f;
        float maxT = beam == RED_PORTAL ? 0.45f : beam == GREEN_PORTAL ? 0.80f : 0.85f;
        Creature const* distanceAnchor = bot ? bot : me;
        float bestDistance = std::numeric_limits<float>::max();
        bool found = false;

        for (uint8 i = 0; i <= 8; ++i)
        {
            float t = minT + (maxT - minT) * float(i) / 8.0f;
            Position candidate = GetBeamSoakPosition(portal, beam, t);
            if (IsInTrackedVoidZone(candidate) || !IsBetween(me, candidate, portal))
                continue;

            float distance = distanceAnchor->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
            if (distance < bestDistance)
            {
                out = candidate;
                bestDistance = distance;
                found = true;
            }
        }

        return found;
    }

    bool SafeBotMoveTo(Creature* bot, Position const& destination)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!bot || !ai || !bot->IsAlive())
            return false;

        float x = destination.GetPositionX();
        float y = destination.GetPositionY();
        float z = destination.GetPositionZ();

        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        if (!bot->IsWithinLOS(x, y, z))
            return false;

        Position finalPosition;
        finalPosition.Relocate(x, y, z, destination.GetOrientation());

        TrackBotCommandState(bot);
        ai->MoveToSendPosition(finalPosition);
        return true;
    }

    bool MoveBotToPosition(Creature* bot, Position const& destination, bool emergencyMovement)
    {
        if (!IsBotEligibleForDirectorMovement(bot, emergencyMovement))
            return false;

        if (!emergencyMovement && bot->isMoving() && bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY()) > _beamArrivedDistance)
            return true;

        if (bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY()) <= _beamArrivedDistance && !IsInTrackedVoidZone(bot->GetPosition()))
            return true;

        return SafeBotMoveTo(bot, destination);
    }

    void MoveBotOffBeam(Creature* bot, Creature* portal)
    {
        if (!bot || !portal)
            return;

        float dx = portal->GetPositionX() - me->GetPositionX();
        float dy = portal->GetPositionY() - me->GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.1f)
            return;

        float px = -dy / len;
        float py = dx / len;

        Position position;
        position.Relocate(bot->GetPositionX() + px * 6.0f, bot->GetPositionY() + py * 6.0f, bot->GetPositionZ(), bot->GetOrientation());

        if (IsInTrackedVoidZone(position))
            position.Relocate(bot->GetPositionX() - px * 6.0f, bot->GetPositionY() - py * 6.0f, bot->GetPositionZ(), bot->GetOrientation());

        MoveBotToPosition(bot, position, true);
    }

    void MoveBotOutOfVoidZone(Creature* bot)
    {
        if (!bot)
            return;

        TimedPosition const* nearestVoid = GetNearestVoidZone(bot->GetPosition());
        if (!nearestVoid)
            return;

        float dx = bot->GetPositionX() - nearestVoid->pos.GetPositionX();
        float dy = bot->GetPositionY() - nearestVoid->pos.GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        if (len <= 0.1f)
        {
            dx = std::cos(me->GetOrientation() + 1.57079637f);
            dy = std::sin(me->GetOrientation() + 1.57079637f);
            len = 1.0f;
        }

        float distance = _voidZoneDangerRadius + 2.0f;
        Position safe;
        safe.Relocate(nearestVoid->pos.GetPositionX() + dx / len * distance,
            nearestVoid->pos.GetPositionY() + dy / len * distance,
            bot->GetPositionZ(),
            bot->GetOrientation());

        if (_debugSay)
            bot->Say("Void zone, shifting.", LANG_UNIVERSAL);

        MoveBotToPosition(bot, safe, true);

        ObjectGuid guid = bot->GetGUID();
        scheduler.Schedule(2500ms, GROUP_BOT_DIRECTOR, [this, guid](TaskContext /*context*/)
        {
            RestoreBotDirectorStateIfUnassigned(guid);
        });
    }

    Position GetBanishSafetyPosition(uint32 index) const
    {
        float const pi = 3.14159265f;
        float angle = Position::NormalizeOrientation(me->GetOrientation() + pi + (float(index % 5) - 2.0f) * 0.22f);
        float distance = 26.0f + float(index / 5) * 3.0f;

        Position position;
        position.Relocate(me->GetPositionX() + std::cos(angle) * distance,
            me->GetPositionY() + std::sin(angle) * distance,
            me->GetPositionZ(),
            Position::NormalizeOrientation(angle + pi));
        return position;
    }

    void MaintainBeamAssignment(uint8 beam)
    {
        Creature* portal = ObjectAccessor::GetCreature(*me, PortalGUID[beam]);
        if (!portal || !_beamAssignments[beam])
            return;

        Creature* bot = ObjectAccessor::GetCreature(*me, _beamAssignments[beam]);
        if (!bot || !IsBotEligibleForDirectorMovement(bot, true) || bot->HasAura(PlayerDebuff[beam]))
        {
            ReleaseBeamAssignment(beam, false);
            return;
        }

        if (GetBeamStackCount(bot, beam) >= _beamMaxStacks[beam])
        {
            DebugBeamSay(bot, beam, "Rotating off ");
            ReleaseBeamAssignment(beam, true);
            return;
        }

        Position soakPosition;
        if (!FindSafeBeamSoakPosition(portal, beam, bot, soakPosition))
        {
            ReleaseBeamAssignment(beam, false);
            MoveBotOutOfVoidZone(bot);
            return;
        }

        if (IsInTrackedVoidZone(bot->GetPosition()))
        {
            if (IsBetween(me, soakPosition, portal))
            {
                MoveBotToPosition(bot, soakPosition, true);
                return;
            }

            ReleaseBeamAssignment(beam, false);
            MoveBotOutOfVoidZone(bot);
            return;
        }

        if (IsEligibleBeamTarget(bot, beam, portal) && bot->GetExactDist2d(soakPosition.GetPositionX(), soakPosition.GetPositionY()) <= _beamArrivedDistance)
        {
            if (beam == RED_PORTAL)
            {
                if (bot_ai* ai = bot->GetBotAI())
                {
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->AttackStart(me);
                }
            }

            return;
        }

        MoveBotToPosition(bot, soakPosition, false);
    }

    void DirectorBeamTick(uint32 diff)
    {
        if (!_npcBotsEnabled)
            return;

        PruneVoidZones(diff);

        bool playerHandled[3] = {};
        for (uint8 beam = 0; beam < 3; ++beam)
        {
            Creature* portal = ObjectAccessor::GetCreature(*me, PortalGUID[beam]);
            if (!portal)
            {
                if (_beamAssignments[beam])
                    ReleaseBeamAssignment(beam, false);

                continue;
            }

            playerHandled[beam] = IsBeamHandledByPlayer(beam, portal);
            if (playerHandled[beam] && _beamAssignments[beam])
                ReleaseBeamAssignment(beam, false);
        }

        for (uint8 beam = 0; beam < 3; ++beam)
        {
            if (playerHandled[beam] || _beamAssignments[beam])
                continue;

            if (!ObjectAccessor::GetCreature(*me, PortalGUID[beam]))
                continue;

            if (Creature* bot = SelectBotForBeam(beam))
                StartBeamAssignment(bot, beam);
        }

        for (uint8 beam = 0; beam < 3; ++beam)
            if (_beamAssignments[beam])
                MaintainBeamAssignment(beam);
    }

    void ScheduleBotDirector()
    {
        if (!_npcBotsEnabled)
            return;

        scheduler.Schedule(std::chrono::milliseconds(_botDirectorTickMs), GROUP_BOT_DIRECTOR, [this](TaskContext context)
        {
            DirectorBeamTick(_botDirectorTickMs);
            context.Repeat(std::chrono::milliseconds(_botDirectorTickMs));
        });
    }

    void MoveBotsToBanishSafety()
    {
        if (!_npcBotsEnabled || !_banishMoveBotsToSafety)
            return;

        uint32 index = 0;
        for (Unit* unit : GatherEncounterUnits())
        {
            Creature* bot = ToNPCBot(unit);
            if (!bot || IsNpcBotTank(bot) || !IsBotEligibleForDirectorMovement(bot, false))
                continue;

            MoveBotToPosition(bot, GetBanishSafetyPosition(index++), false);
        }
    }

    void CleanupBotDirector(bool restoreStates)
    {
        scheduler.CancelGroup(GROUP_BOT_DIRECTOR);
        ClearBeamAssignments();

        if (restoreStates)
            ReleaseAllBotDirectorStates();

        _voidZones.clear();
    }

    void Reset() override
    {
        CleanupBotDirector(true);
        LoadBotDirectorConfig();
        BossAI::Reset();
        berserk = false;
        HandleDoors(true);
        DestroyPortals();
    }

    void SummonPortals()
    {
        uint8 r = rand() % 4;
        uint8 pos[3];
        pos[RED_PORTAL] = ((r % 2) ? (r > 1 ? 2 : 1) : 0);
        pos[GREEN_PORTAL] = ((r % 2) ? 0 : (r > 1 ? 2 : 1));
        pos[BLUE_PORTAL] = (r > 1 ? 1 : 2); // Blue Portal not on the left side (0)
        for (int i = 0; i < 3; ++i)
        {
            if (Creature* portal = me->SummonCreature(PortalID[i], PortalCoord[pos[i]][0], PortalCoord[pos[i]][1], PortalCoord[pos[i]][2], 0, TEMPSUMMON_TIMED_DESPAWN, 60000))
            {
                PortalGUID[i] = portal->GetGUID();
                portal->AddAura(PortalVisual[i], portal);
            }
        }
    }

    void UpdatePortals() // Here we handle the beams' behavior
    {
        std::vector<Unit*> encounterUnits = GatherEncounterUnits();

        for (int j = 0; j < 3; ++j) // j = color
        {
            if (Creature* portal = ObjectAccessor::GetCreature(*me, PortalGUID[j]))
            {
                // the one who's been cast upon before
                Unit* current = ObjectAccessor::GetUnit(*portal, BeamTarget[j]);
                // temporary store for the best suitable beam reciever
                Unit* target = me;
                float bestDistance = std::numeric_limits<float>::max();
                bool currentEligible = IsEligibleBeamTarget(current, j, portal);
                bool playerTargetFound = false;

                if (currentEligible && current->IsPlayer())
                {
                    target = current;
                    bestDistance = current->GetDistance2d(portal);
                    playerTargetFound = true;
                }

                for (Unit* unit : encounterUnits)
                {
                    if (!unit->IsPlayer() || !IsEligibleBeamTarget(unit, j, portal))
                        continue;

                    float distance = unit->GetDistance2d(portal);
                    if (target == me || (!playerTargetFound && distance < bestDistance) || (playerTargetFound && distance + 2.0f < bestDistance))
                    {
                        target = unit;
                        bestDistance = distance;
                        playerTargetFound = true;
                    }
                }

                if (target == me && currentEligible)
                {
                    target = current;
                    bestDistance = current->GetDistance2d(portal);
                }

                // get the best suitable target
                if (!playerTargetFound)
                {
                    for (Unit* unit : encounterUnits)
                    {
                        if (!IsEligibleBeamTarget(unit, j, portal))
                            continue;

                        float distance = unit->GetDistance2d(portal);
                        if (target == me || (!currentEligible && distance < bestDistance) || (currentEligible && distance + 2.0f < bestDistance))
                        {
                            target = unit;
                            bestDistance = distance;
                        }
                    }
                }

                // buff the target
                if (target->IsPlayer() || IsNPCBotUnit(target))
                {
                    target->AddAura(PlayerBuff[j], target);
                }
                else
                {
                    target->AddAura(NetherBuff[j], target);
                }
                // cast visual beam on the chosen target if switched
                // simple target switching isn't working -> using BeamerGUID to cast (workaround)
                if (!current || target != current)
                {
                    BeamTarget[j] = target->GetGUID();
                    // remove currently beaming portal
                    if (Creature* beamer = ObjectAccessor::GetCreature(*portal, BeamerGUID[j]))
                    {
                        beamer->CastSpell(target, PortalBeam[j], false);
                        beamer->DisappearAndDie();
                        BeamerGUID[j].Clear();
                    }
                    // create new one and start beaming on the target
                    if (Creature* beamer = portal->SummonCreature(PortalID[j], portal->GetPositionX(), portal->GetPositionY(), portal->GetPositionZ(), portal->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 60000))
                    {
                        beamer->CastSpell(target, PortalBeam[j], false);
                        BeamerGUID[j] = beamer->GetGUID();
                    }
                }
                // aggro target if Red Beam
                if (j == RED_PORTAL && me->GetVictim() != target && (target->IsPlayer() || IsNPCBotUnit(target)))
                {
                    float threat = 100000.0f;
                    if (Unit* victim = me->GetVictim())
                        threat += DoGetThreat(victim);

                    me->GetThreatMgr().AddThreat(target, threat);
                }
            }
        }
    }

    void SwitchToPortalPhase(bool aggro = false)
    {
        if (!aggro)
        {
            Talk(EMOTE_PHASE_PORTAL);
        }

        ReleaseAllBotDirectorStates();
        ClearBeamAssignments();
        _voidZones.clear();
        scheduler.CancelGroup(BANISH_PHASE);
        me->RemoveAurasDueToSpell(SPELL_BANISH_ROOT);
        me->RemoveAurasDueToSpell(SPELL_BANISH_VISUAL);
        SummonPortals();
        ScheduleBotDirector();
        scheduler.Schedule(60s, [this](TaskContext /*context*/)
        {
            if (!me->IsNonMeleeSpellCast(false))
            {
                SwitchToBanishPhase();
                return;
            }
        }).Schedule(10s, PORTAL_PHASE, [this](TaskContext context)
        {
            UpdatePortals();
            context.Repeat(1s);
        }).Schedule(10s, PORTAL_PHASE, [this](TaskContext context)
        {
            DoCastSelf(SPELL_EMPOWERMENT);
            me->AddAura(SPELL_NETHERBURN_AURA, me);
            context.Repeat(90s);
        }).Schedule(15s, PORTAL_PHASE, [this](TaskContext context)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 45.0f, true))
            {
                TrackVoidZone(target->GetPosition(), 25 * IN_MILLISECONDS);
                DoCast(target, SPELL_VOIDZONE);
            }
            context.Repeat(15s);
        });
    }

    void DestroyPortals()
    {
        for (int i = 0; i < 3; ++i)
        {
            if (Creature* portal = ObjectAccessor::GetCreature(*me, PortalGUID[i]))
            {
                portal->DisappearAndDie();
            }
            if (Creature* portal = ObjectAccessor::GetCreature(*me, BeamerGUID[i]))
            {
                portal->DisappearAndDie();
            }

            PortalGUID[i].Clear();
            BeamerGUID[i].Clear();
            BeamTarget[i].Clear();
        }
    }

    void SwitchToBanishPhase()
    {
        Talk(EMOTE_PHASE_BANISH);
        scheduler.CancelGroup(PORTAL_PHASE);
        CleanupBotDirector(true);
        me->RemoveAurasDueToSpell(SPELL_EMPOWERMENT);
        me->RemoveAurasDueToSpell(SPELL_NETHERBURN_AURA);
        DoCastSelf(SPELL_BANISH_VISUAL, true);
        DoCastSelf(SPELL_BANISH_ROOT, true);

        DestroyPortals();
        MoveBotsToBanishSafety();

        scheduler.Schedule(30s, [this](TaskContext)
        {
            SwitchToPortalPhase();
            DoResetThreatList();
            return;
        }).Schedule(10s, BANISH_PHASE, [this](TaskContext context)
        {
            DoCastRandomTarget(SPELL_NETHERBREATH, 0, 40.0f, true);
            context.Repeat(5s, 7s);
        });

        for (uint8 i = 0; i < 3; ++i)
        {
            me->RemoveAurasDueToSpell(NetherBuff[i]);
        }
    }

    void HandleDoors(bool open)
    {
        if (GameObject* door = ObjectAccessor::GetGameObject(*me, instance->GetGuidData(DATA_GO_MASSIVE_DOOR)))
        {
            door->SetGoState(open ? GO_STATE_ACTIVE : GO_STATE_READY);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        HandleDoors(false);
        SwitchToPortalPhase(true);
        DoZoneInCombat();
        scheduler.Schedule(9min, [this](TaskContext /*context*/)
        {
            if (!berserk)
            {
                DoCastSelf(SPELL_NETHER_INFUSION);
                DoCastAOE(SPELL_NETHERSPITE_ROAR);
                berserk = true;
            }
        });
    }

    void JustDied(Unit* killer) override
    {
        CleanupBotDirector(true);
        BossAI::JustDied(killer);
        HandleDoors(true);
        DestroyPortals();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        CleanupBotDirector(true);
        _EnterEvadeMode(why);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool berserk;
    ObjectGuid PortalGUID[3]; // guid's of portals
    ObjectGuid BeamerGUID[3]; // guid's of auxiliary beaming portals
    ObjectGuid BeamTarget[3]; // guid's of portals' current targets
    ObjectGuid _beamAssignments[3];
    std::map<ObjectGuid, uint32> _scriptedBotCommandStates;
    std::vector<TimedPosition> _voidZones;
    uint32 _botDirectorTickMs;
    uint8 _beamMaxStacks[3];
    float _voidZoneDangerRadius;
    float _beamArrivedDistance;
    bool _npcBotsEnabled;
    bool _redEmergencyUseNonTank;
    bool _greenRequireManaUser;
    bool _banishMoveBotsToSafety;
    bool _debugSay;
};

class spell_nether_portal_perseverence : public AuraScript
{
    PrepareAuraScript(spell_nether_portal_perseverence);

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        const_cast<AuraEffect*>(aurEff)->SetAmount(aurEff->GetAmount() + 30000);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_nether_portal_perseverence::HandleApply, EFFECT_2, SPELL_AURA_MOD_INCREASE_HEALTH, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

void AddSC_boss_netherspite()
{
    RegisterKarazhanCreatureAI(boss_netherspite);
    RegisterSpellScript(spell_nether_portal_perseverence);
}
