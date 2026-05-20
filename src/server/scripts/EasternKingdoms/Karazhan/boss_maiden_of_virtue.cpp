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
#include "Containers.h"
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
#include "karazhan.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

enum Text
{
    SAY_AGGRO           = 0,
    SAY_SLAY            = 1,
    SAY_REPENTANCE      = 2,
    SAY_DEATH           = 3
};

enum Spells
{
    SPELL_REPENTANCE    = 29511,
    SPELL_HOLY_FIRE     = 29522,
    SPELL_HOLY_WRATH    = 32445,
    SPELL_HOLY_GROUND   = 29523,
    SPELL_BERSERK       = 26662
};

enum EventGroups
{
    GROUP_BOT_POSITIONING = 1,
    GROUP_REPENTANCE_PREP = 2
};

static bool IsNPCBotUnit(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && creature->IsNPCBot() && creature->GetBotAI();
}

static void SafeBotMoveTo(Creature* bot, Position const& dest)
{
    if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
        return;

    float x = dest.GetPositionX();
    float y = dest.GetPositionY();
    float z = dest.GetPositionZ();

    if (!bot->CanFly())
        bot->UpdateAllowedPositionZ(x, y, z);

    Position p;
    p.Relocate(x, y, z, dest.GetOrientation());

    if (bot_ai* ai = bot->GetBotAI())
        ai->MoveToSendPosition(p);
}

struct boss_maiden_of_virtue : public BossAI
{
    boss_maiden_of_virtue(Creature* creature) : BossAI(creature, DATA_MAIDEN)
    {    }

    struct BotMoveState
    {
        uint32 originalCommandState = 0;
        bool staySet = false;
    };

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        DoCastAOE(SPELL_HOLY_GROUND, true);
        scheduler.Schedule(25s, [this](TaskContext context)
        {
            PrepareBotsForRepentance();

            scheduler.Schedule(2500ms, GROUP_REPENTANCE_PREP, [this](TaskContext)
            {
                DoCastAOE(SPELL_REPENTANCE);
                Talk(SAY_REPENTANCE);

                scheduler.Schedule(1500ms, GROUP_REPENTANCE_PREP, [this](TaskContext)
                {
                    RecoverBotsAfterRepentance();
                });
            });

            context.Repeat(25s, 35s);
        }).Schedule(8s, [this](TaskContext context)
        {
            if (Unit* target = SelectRandomEncounterTarget(50.0f, true, true))
                DoCast(target, SPELL_HOLY_FIRE);

            context.Repeat(8s, 18s);
        }).Schedule(15s, [this](TaskContext context)
        {
            if (Unit* target = SelectRandomEncounterTarget(80.0f, true, true))
                DoCast(target, SPELL_HOLY_WRATH);

            context.Repeat(20s, 25s);
        }).Schedule(3s, GROUP_BOT_POSITIONING, [this](TaskContext context)
        {
            PositionBotSpreadTick();
            context.Repeat(3s);
        }).Schedule(10min, [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_BERSERK, true);
        });
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() || IsNPCBotUnit(victim))
        {
            Talk(SAY_SLAY);
        }
    }

    void JustDied(Unit* killer) override
    {
        ReleaseAllBotMoveStates();
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ReleaseAllBotMoveStates();
        BossAI::EnterEvadeMode(why);
    }

    void Reset() override
    {
        ReleaseAllBotMoveStates();
        _repentanceBots.clear();
        _holyFireTargets.clear();
        BossAI::Reset();
    }

    void SpellHitTarget(Unit* target, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_HOLY_FIRE && target && (target->IsPlayer() || IsNPCBotUnit(target)))
            _holyFireTargets[target->GetGUID()] = getMSTime();
    }

    bool IsEncounterParticipant(Unit* unit) const
    {
        if (!unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != me->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(me, 120.0f))
            return false;

        if (unit->IsPlayer())
            return true;

        if (Creature* creature = unit->ToCreature())
            return creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();

        return false;
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

    Unit* SelectRandomEncounterTarget(float range, bool includeVictim, bool preferNonTank)
    {
        std::vector<Unit*> candidates;
        Unit* victim = me->GetVictim();

        for (Unit* unit : GatherEncounterUnits())
        {
            if (!unit || !unit->IsAlive())
                continue;

            if (!me->IsWithinDistInMap(unit, range))
                continue;

            if (!includeVictim && unit == victim)
                continue;

            if (preferNonTank && unit == victim)
                continue;

            candidates.push_back(unit);
        }

        if (!candidates.empty())
            return Acore::Containers::SelectRandomContainerElement(candidates);

        if (includeVictim && victim && victim->IsAlive() && me->IsWithinDistInMap(victim, range))
            return victim;

        return nullptr;
    }

    Creature* ToNPCBot(Unit* unit) const
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot() || creature->IsTempBot() || creature->IsFreeBot() || !creature->GetBotAI())
            return nullptr;

        return creature;
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

    bool IsNpcBotRanged(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        return ai && ai->HasRole(BOT_ROLE_RANGED);
    }

    bool IsNpcBotDispeller(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!ai)
            return false;

        if (ai->HasRole(BOT_ROLE_HEAL))
            return true;

        switch (ai->GetBotClass())
        {
            case BOT_CLASS_PALADIN:
            case BOT_CLASS_PRIEST:
            case BOT_CLASS_SHAMAN:
            case BOT_CLASS_DRUID:
            case BOT_CLASS_SPELLBREAKER:
                return true;
            default:
                return false;
        }
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

    bool CanMoveBot(Creature* bot, bool allowRepentancePrep) const
    {
        if (!bot || !IsEncounterParticipant(bot) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_ROOT | UNIT_STATE_ISOLATED))
            return false;

        if (bot == me->GetVictim())
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return false;

        if (!allowRepentancePrep && bot->HasAura(SPELL_REPENTANCE))
            return false;

        if (!_botMoveStates.count(bot->GetGUID()) &&
            ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY))
            return false;

        return !IsBotCastingUninterruptible(bot);
    }

    void TrackBotMoveState(Creature* bot)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!ai || _botMoveStates.count(bot->GetGUID()))
            return;

        BotMoveState state;
        state.originalCommandState = ai->GetBotCommandState();
        state.staySet = !(state.originalCommandState & BOT_COMMAND_STAY);
        _botMoveStates[bot->GetGUID()] = state;
    }

    void ReleaseBotMoveState(ObjectGuid const& guid, bool resumeCombat)
    {
        std::map<ObjectGuid, BotMoveState>::iterator itr = _botMoveStates.find(guid);
        if (itr == _botMoveStates.end())
            return;

        Creature* bot = ObjectAccessor::GetCreature(*me, guid);
        if (!bot)
        {
            _botMoveStates.erase(itr);
            return;
        }

        if (bot_ai* ai = bot->GetBotAI())
        {
            if (itr->second.staySet)
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);

            if (resumeCombat && bot->IsAlive())
            {
                if (me->IsAlive() && me->IsEngaged() && me->GetVictim() && bot->IsInMap(me))
                {
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->AttackStart(me);
                }
                else if (!ai->IAmFree())
                    ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
            }
        }

        _botMoveStates.erase(itr);
    }

    void ReleaseAllBotMoveStates()
    {
        std::vector<ObjectGuid> guids;
        guids.reserve(_botMoveStates.size());

        for (std::map<ObjectGuid, BotMoveState>::value_type const& pair : _botMoveStates)
            guids.push_back(pair.first);

        for (ObjectGuid const& guid : guids)
            ReleaseBotMoveState(guid, true);

        _botMoveStates.clear();
        _repentanceBots.clear();
    }

    Position BuildRingPosition(float radius, size_t index, size_t count, float angleOffset) const
    {
        static constexpr float TwoPi = 6.28318530f;
        float angle = angleOffset + (count ? TwoPi * float(index) / float(count) : 0.0f);
        float x = me->GetPositionX() + std::cos(angle) * radius;
        float y = me->GetPositionY() + std::sin(angle) * radius;
        float z = me->GetPositionZ();
        float orientation = Position::NormalizeOrientation(std::atan2(me->GetPositionY() - y, me->GetPositionX() - x));

        Position position;
        position.Relocate(x, y, z, orientation);
        return position;
    }

    bool MoveBotIfNeeded(Creature* bot, Position const& position, float arrivedDistance, bool releaseSoon, bool allowRepentancePrep)
    {
        if (!CanMoveBot(bot, allowRepentancePrep))
            return false;

        if (bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY()) <= arrivedDistance)
            return true;

        TrackBotMoveState(bot);
        SafeBotMoveTo(bot, position);

        if (releaseSoon)
        {
            ObjectGuid guid = bot->GetGUID();
            scheduler.Schedule(2500ms, GROUP_BOT_POSITIONING, [this, guid](TaskContext)
            {
                if (!_repentanceBots.count(guid))
                    ReleaseBotMoveState(guid, true);
            });
        }

        return true;
    }

    void PositionBotSpreadTick()
    {
        std::vector<Creature*> meleeBots;
        std::vector<Creature*> rangedBots;

        for (Unit* unit : GatherEncounterUnits())
        {
            Creature* bot = ToNPCBot(unit);
            if (!bot || bot == me->GetVictim() || IsNpcBotTank(bot) || _repentanceBots.count(bot->GetGUID()))
                continue;

            if (!CanMoveBot(bot, false))
                continue;

            if (IsNpcBotHealer(bot) || IsNpcBotRanged(bot))
                rangedBots.push_back(bot);
            else
                meleeBots.push_back(bot);
        }

        auto sortByGuid = [](Creature* left, Creature* right)
        {
            return left->GetGUID() < right->GetGUID();
        };

        std::sort(meleeBots.begin(), meleeBots.end(), sortByGuid);
        std::sort(rangedBots.begin(), rangedBots.end(), sortByGuid);

        for (size_t i = 0; i < meleeBots.size(); ++i)
        {
            Position position = BuildRingPosition(8.5f, i, meleeBots.size(), me->GetOrientation() + 0.35f);
            MoveBotIfNeeded(meleeBots[i], position, 2.5f, true, false);
        }

        for (size_t i = 0; i < rangedBots.size(); ++i)
        {
            Position position = BuildRingPosition(26.0f, i, rangedBots.size(), me->GetOrientation() + 1.1f);
            MoveBotIfNeeded(rangedBots[i], position, 2.5f, true, false);
        }
    }

    void PrepareBotsForRepentance()
    {
        _repentanceBots.clear();

        std::vector<Creature*> candidates;
        for (Unit* unit : GatherEncounterUnits())
        {
            Creature* bot = ToNPCBot(unit);
            if (!bot || bot == me->GetVictim() || !CanMoveBot(bot, true))
                continue;

            if (IsNpcBotHealer(bot) || IsNpcBotDispeller(bot))
                candidates.push_back(bot);
        }

        std::sort(candidates.begin(), candidates.end(), [](Creature* left, Creature* right)
        {
            bot_ai* leftAI = left->GetBotAI();
            bot_ai* rightAI = right->GetBotAI();
            bool leftHealer = leftAI && leftAI->HasRole(BOT_ROLE_HEAL);
            bool rightHealer = rightAI && rightAI->HasRole(BOT_ROLE_HEAL);

            if (leftHealer != rightHealer)
                return leftHealer > rightHealer;

            return left->GetGUID() < right->GetGUID();
        });

        size_t const count = std::min<size_t>(2, candidates.size());
        for (size_t i = 0; i < count; ++i)
        {
            Creature* bot = candidates[i];
            Position position = BuildRingPosition(10.5f, i, count, me->GetOrientation() + 2.35f);
            _repentanceBots.insert(bot->GetGUID());
            MoveBotIfNeeded(bot, position, 1.75f, false, true);
        }
    }

    void RecoverBotsAfterRepentance()
    {
        std::vector<ObjectGuid> guids(_repentanceBots.begin(), _repentanceBots.end());

        for (ObjectGuid const& guid : guids)
        {
            Creature* bot = ObjectAccessor::GetCreature(*me, guid);
            if (!bot || !bot->IsAlive())
            {
                _repentanceBots.erase(guid);
                ReleaseBotMoveState(guid, true);
                continue;
            }

            if (bot->HasAura(SPELL_REPENTANCE) && bot->GetExactDist2d(me) > 12.0f)
            {
                Position position = BuildRingPosition(10.5f, 0, 1, bot->GetAngle(me) + 3.14159265f);
                MoveBotIfNeeded(bot, position, 1.75f, false, true);
                continue;
            }

            if (!bot->HasAura(SPELL_REPENTANCE))
            {
                _repentanceBots.erase(guid);
                ReleaseBotMoveState(guid, true);
            }
        }

        scheduler.Schedule(4s, GROUP_REPENTANCE_PREP, [this](TaskContext)
        {
            std::vector<ObjectGuid> remaining(_repentanceBots.begin(), _repentanceBots.end());
            for (ObjectGuid const& guid : remaining)
            {
                _repentanceBots.erase(guid);
                ReleaseBotMoveState(guid, true);
            }
        });
    }

private:
    std::map<ObjectGuid, BotMoveState> _botMoveStates;
    std::map<ObjectGuid, uint32> _holyFireTargets;
    GuidSet _repentanceBots;
};

void AddSC_boss_maiden_of_virtue()
{
    RegisterKarazhanCreatureAI(boss_maiden_of_virtue);
}
