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
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "UnitAI.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <vector>

enum Text
{
    SAY_APPROACH = 0,
    SAY_AGGRO    = 1,
    SAY_SUMMON   = 2
};

enum Spells
{
    SPELL_BLOOD_MIRROR0                  = 50844,
    SPELL_BLOOD_MIRROR1                  = 50845,
    SPELL_BLOOD_MIRROR_TARGET_PICKER     = 50883,
    SPELL_BLOOD_MIRROR_TRANSITION_VISUAL = 50910,
    SPELL_BLOOD_MIRROR_DAMAGE            = 50846,

    SPELL_BLOOD_TAP = 51135,

    SPELL_BLOOD_SWOOP                             = 50922,
    SPELL_DASH_GASH_PRE_SPELL                     = 50923,
    SPELL_DASH_GASH_RETURN_TO_TANK                = 50924,
    // SPELL_DASH_GASH_RETURN_TO_TANK_PRE_SPELL      = 50925,
    // SPELL_DASH_GASH_RETURN_TO_TANK_PRE_SPELL_ROOT = 50932,

    SPELL_DESPAWN_SANGUINE_SPIRIT_VISUAL             = 51214,
    SPELL_DESPAWN_SANGUINE_SPIRITS                   = 51212,
    SPELL_SANGUINE_SPIRIT_AURA                       = 50993,
    SPELL_SANGUINE_SPIRIT_PRE_AURA                   = 51282,
    SPELL_SANGUINE_SPIRIT_PRE_AURA2                  = 51283,
    SPELL_SUMMON_SANGUINE_SPIRIT0                    = 50996,
    SPELL_SUMMON_SANGUINE_SPIRIT1                    = 50998,
    // SPELL_SUMMON_SANGUINE_SPIRIT2                 = 51204,
    SPELL_SUMMON_SANGUINE_SPIRIT_MISSILE_BURST       = 51208,
    SPELL_SUMMON_SANGUINE_SPIRIT_SHORT_MISSILE_BURST = 51280,
    SPELL_SUMMON_SANGUINE_SPIRIT_ON_KILL             = 51205,
    SPELL_EXSANGUINATE                               = 51013,
    SPELL_DUMMY_NUKE_RANGE_SELF                      = 51106,
};

enum Events
{
    EVENT_SAY  = 1,
    EVENT_FLAG = 2
};

enum Groups
{
    GROUP_BOT_DIRECTOR = 1,
    GROUP_BLOOD_MIRROR_BOT_CONTROL = 2,
    GROUP_SPIRIT_AVOIDANCE = 3
};

enum Creatures
{
    NPC_SANGUINE_SPIRIT = 28232
};

static bool IsNPCBotUnit(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && creature->IsNPCBot() && creature->GetBotAI();
}

static Creature* ToValidNPCBot(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    if (!creature || !creature->IsNPCBot() || creature->IsTempBot() || creature->IsFreeBot() || !creature->GetBotAI())
        return nullptr;

    return creature;
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

static bool IsMirkbloodParticipant(Creature const* source, Unit* unit, float radius)
{
    if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
        return false;

    if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(source, radius))
        return false;

    if (unit->IsPlayer())
        return true;

    return ToValidNPCBot(unit) != nullptr;
}

static std::vector<Unit*> GatherMirkbloodEncounterUnits(Creature* source, float radius)
{
    std::vector<Unit*> units;
    if (!source || !source->GetMap())
        return units;

    GuidSet seen;

    auto addUnit = [&](Unit* unit)
    {
        if (!IsMirkbloodParticipant(source, unit, radius) || seen.count(unit->GetGUID()))
            return;

        seen.insert(unit->GetGUID());
        units.push_back(unit);
    };

    Map::PlayerList const& players = source->GetMap()->GetPlayers();
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

static bool IsMirkbloodTankish(Unit* unit, Creature* boss)
{
    if (!unit)
        return false;

    if (boss && unit == boss->GetVictim())
        return true;

    if (Player* player = unit->ToPlayer())
    {
        Group* group = player->GetGroup();
        if (!group)
            return false;

        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::member_citerator itr = slots.begin(); itr != slots.end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & (MEMBER_FLAG_MAINTANK | MEMBER_FLAG_MAINASSIST);

        return false;
    }

    if (Creature* bot = ToValidNPCBot(unit))
    {
        bot_ai* ai = bot->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK));
    }

    return false;
}

static Unit* SelectMirkbloodEncounterTarget(Creature* source, float radius, bool preferNonTank, bool excludeBloodMirror = true)
{
    std::vector<Unit*> candidates = GatherMirkbloodEncounterUnits(source, radius);
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [source, excludeBloodMirror](Unit* unit)
    {
        return !unit || unit == source || (excludeBloodMirror && unit->HasAnyAuras(SPELL_BLOOD_MIRROR0, SPELL_BLOOD_MIRROR1));
    }), candidates.end());

    if (preferNonTank && candidates.size() > 1)
    {
        std::vector<Unit*> nonTanks;
        for (Unit* unit : candidates)
            if (!IsMirkbloodTankish(unit, source))
                nonTanks.push_back(unit);

        if (!nonTanks.empty())
            candidates = nonTanks;
    }

    if (!candidates.empty())
        return Acore::Containers::SelectRandomContainerElement(candidates);

    return source ? source->GetVictim() : nullptr;
}

struct boss_tenris_mirkblood : public BossAI
{
    boss_tenris_mirkblood(Creature* creature) : BossAI(creature, DATA_MIRKBLOOD)
    {    }

    enum BotStateReason : uint8
    {
        BOT_STATE_MIRROR = 0x01,
        BOT_STATE_SPIRIT = 0x02,
        BOT_STATE_TANK_KITE = 0x04
    };

    struct ScriptedBotState
    {
        uint32 originalCommandState = 0;
        uint8 reasons = 0;
        bool followSet = false;
        bool staySet = false;
    };

    void Reset() override
    {
        ReleaseAllBotStates();
        _mirrorTargetGuid.Clear();
        _lastTankKiteMs = 0;
        _Reset();

        me->SetImmuneToPC(true);

        ScheduleHealthCheckEvent(50, [&] {
            Talk(SAY_SUMMON);
            DoCast(SPELL_SUMMON_SANGUINE_SPIRIT_MISSILE_BURST);
            });

        ScheduleHealthCheckEvent(45, [&] {
            ScheduleTimedEvent(10s, 15s, [&] {
                DoCast(SPELL_BLOOD_TAP);
                }, 15s, 40s);
            });
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();

        ScheduleTimedEvent(1s, 5s, [&] {
            // Blood Mirror
            DoCast(SPELL_BLOOD_MIRROR_TARGET_PICKER);
            }, 20s, 50s);
        ScheduleTimedEvent(30s, [&] {
            // Blood Swoop
            DoCast(SPELL_DASH_GASH_PRE_SPELL);
            }, 15s, 40s);
        ScheduleTimedEvent(6s, 15s, [&] {
            // Sanguine Spirit
            DoCast(SPELL_SUMMON_SANGUINE_SPIRIT_SHORT_MISSILE_BURST);
            }, 6s, 15s);

        scheduler.Schedule(1s, GROUP_BOT_DIRECTOR, [this](TaskContext context)
        {
            HandleBloodMirrorBotControl();
            HandleSanguineSpiritAvoidance();
            context.Repeat(1s);
        });
    }

    void KilledUnit(Unit* victim) override
    {
        if (!victim)
            return;

        DoCast(victim, SPELL_SUMMON_SANGUINE_SPIRIT_ON_KILL);

        if (victim->GetGUID() == _mirrorTargetGuid)
            ClearBloodMirror();
    }

    void JustDied(Unit* killer) override
    {
        ReleaseAllBotStates();
        _mirrorTargetGuid.Clear();
        BossAI::JustDied(killer);
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellSchoolMask damageSchoolMask) override
    {
        BossAI::DamageTaken(attacker, damage, damageType, damageSchoolMask);

        if (!me->HasAura(SPELL_BLOOD_MIRROR0))
        {
            ReleaseBloodMirrorBotHolds();
            return;
        }

        Unit* mirrorTarget = GetMirrorTarget();
        if (!mirrorTarget || !mirrorTarget->IsAlive() || !mirrorTarget->IsInMap(me))
        {
            ClearBloodMirror();
            return;
        }

        int32 damageTaken = damage;

        me->CastCustomSpell(mirrorTarget, SPELL_BLOOD_MIRROR_DAMAGE, &damageTaken, &damageTaken, &damageTaken, true, nullptr, nullptr, me->GetGUID());
    }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_BLOOD_MIRROR0 && caster && caster != me)
            _mirrorTargetGuid = caster->GetGUID();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ReleaseAllBotStates();
        _mirrorTargetGuid.Clear();
        _EnterEvadeMode(why);
        me->SetImmuneToPC(false);
    }

private:
    Unit* GetMirrorTarget() const
    {
        return _mirrorTargetGuid ? ObjectAccessor::GetUnit(*me, _mirrorTargetGuid) : nullptr;
    }

    bool IsEncounterParticipant(Unit* unit) const
    {
        return IsMirkbloodParticipant(me, unit, 150.0f);
    }

    std::vector<Unit*> GatherEncounterUnits() const
    {
        return GatherMirkbloodEncounterUnits(me, 150.0f);
    }

    std::vector<Creature*> GatherEncounterBots() const
    {
        std::vector<Creature*> bots;
        for (Unit* unit : GatherEncounterUnits())
        {
            if (Creature* bot = ToValidNPCBot(unit))
                bots.push_back(bot);
        }

        return bots;
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

    bool IsScriptControlledBot(Creature* bot) const
    {
        return bot && _botStates.count(bot->GetGUID());
    }

    bool CanMoveBot(Creature* bot, bool emergencyMovement) const
    {
        if (!bot || !IsEncounterParticipant(bot))
            return false;

        if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_ROOT | UNIT_STATE_ISOLATED))
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return false;

        if (!IsScriptControlledBot(bot) && ai->HasBotCommandState(BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY))
            return false;

        if (!emergencyMovement && IsBotCastingUninterruptible(bot))
            return false;

        return true;
    }

    void TrackBotState(Creature* bot, uint8 reason, bool setFollow)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!ai)
            return;

        ScriptedBotState& state = _botStates[bot->GetGUID()];
        if (!state.reasons)
        {
            state.originalCommandState = ai->GetBotCommandState();
            state.followSet = false;
            state.staySet = !(state.originalCommandState & BOT_COMMAND_STAY);
        }

        state.reasons |= reason;

        if (setFollow && !(state.originalCommandState & BOT_COMMAND_FOLLOW))
        {
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW);
            state.followSet = true;
        }
    }

    void ReleaseBotState(ObjectGuid const& guid, uint8 reason, bool resumeCombat)
    {
        std::map<ObjectGuid, ScriptedBotState>::iterator itr = _botStates.find(guid);
        if (itr == _botStates.end())
            return;

        itr->second.reasons &= ~reason;
        if (itr->second.reasons)
            return;

        Creature* bot = ObjectAccessor::GetCreature(*me, guid);
        if (bot)
        {
            if (bot_ai* ai = bot->GetBotAI())
            {
                if (itr->second.staySet)
                    ai->RemoveBotCommandState(BOT_COMMAND_STAY);

                if (itr->second.followSet && !(itr->second.originalCommandState & BOT_COMMAND_FOLLOW))
                    ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW);

                if (resumeCombat && bot->IsAlive())
                {
                    if (me->IsAlive() && me->IsEngaged() && me->GetVictim() && bot->IsInMap(me))
                    {
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                        ai->AttackStart(me);
                    }
                    else if (!ai->IAmFree())
                    {
                        ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
                    }
                }
            }
        }

        _botStates.erase(itr);
    }

    void ReleaseBotStatesByReason(uint8 reason, bool resumeCombat)
    {
        std::vector<ObjectGuid> guids;
        guids.reserve(_botStates.size());

        for (std::map<ObjectGuid, ScriptedBotState>::value_type const& pair : _botStates)
            if (pair.second.reasons & reason)
                guids.push_back(pair.first);

        for (ObjectGuid const& guid : guids)
            ReleaseBotState(guid, reason, resumeCombat);
    }

    void ReleaseAllBotStates()
    {
        std::vector<ObjectGuid> guids;
        guids.reserve(_botStates.size());

        for (std::map<ObjectGuid, ScriptedBotState>::value_type const& pair : _botStates)
            guids.push_back(pair.first);

        for (ObjectGuid const& guid : guids)
        {
            std::map<ObjectGuid, ScriptedBotState>::iterator itr = _botStates.find(guid);
            if (itr == _botStates.end())
                continue;

            Creature* bot = ObjectAccessor::GetCreature(*me, guid);
            if (bot)
            {
                if (bot_ai* ai = bot->GetBotAI())
                {
                    if (itr->second.staySet)
                        ai->RemoveBotCommandState(BOT_COMMAND_STAY);

                    if (itr->second.followSet && !(itr->second.originalCommandState & BOT_COMMAND_FOLLOW))
                        ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW);

                    if (bot->IsAlive())
                    {
                        if (me->IsAlive() && me->IsEngaged() && me->GetVictim() && bot->IsInMap(me))
                        {
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->AttackStart(me);
                        }
                        else if (!ai->IAmFree())
                        {
                            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
                        }
                    }
                }
            }
        }

        _botStates.clear();
    }

    void ClearBloodMirror()
    {
        if (Unit* mirrorTarget = GetMirrorTarget())
        {
            mirrorTarget->RemoveAurasDueToSpell(SPELL_BLOOD_MIRROR0);
            mirrorTarget->RemoveAurasDueToSpell(SPELL_BLOOD_MIRROR1);
        }

        me->RemoveAurasDueToSpell(SPELL_BLOOD_MIRROR0);
        me->RemoveAurasDueToSpell(SPELL_BLOOD_MIRROR1);
        _mirrorTargetGuid.Clear();
        ReleaseBloodMirrorBotHolds();
    }

    void ReleaseBloodMirrorBotHolds()
    {
        ReleaseBotStatesByReason(BOT_STATE_MIRROR, true);
    }

    void HandleBloodMirrorBotControl()
    {
        Unit* mirrorTarget = GetMirrorTarget();
        bool mirrorActive = mirrorTarget && mirrorTarget->IsAlive() && me->HasAura(SPELL_BLOOD_MIRROR0);

        if (!mirrorActive)
        {
            ReleaseBloodMirrorBotHolds();
            return;
        }

        for (Creature* bot : GatherEncounterBots())
        {
            if (!bot || !bot->IsAlive())
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai || ai->IAmFree())
                continue;

            if (IsNpcBotTank(bot) || IsNpcBotHealer(bot))
                continue;

            TrackBotState(bot, BOT_STATE_MIRROR, true);
            bot->AttackStop();
        }
    }

    std::vector<Creature*> GetActiveSanguineSpirits(float radius) const
    {
        std::list<Creature*> spiritList;
        me->GetCreatureListWithEntryInGrid(spiritList, NPC_SANGUINE_SPIRIT, radius);

        std::vector<Creature*> spirits;
        spirits.reserve(spiritList.size());

        for (Creature* spirit : spiritList)
        {
            if (!spirit || !spirit->IsAlive() || spirit->GetMap() != me->GetMap())
                continue;

            spirits.push_back(spirit);
        }

        return spirits;
    }

    bool IsNearSanguineSpirit(Unit* unit, float dangerRadius) const
    {
        if (!unit)
            return false;

        for (Creature* spirit : GetActiveSanguineSpirits(80.0f))
            if (unit->GetDistance(spirit) <= dangerRadius)
                return true;

        return false;
    }

    Position GetSpiritEscapePosition(Creature* bot, float desiredMoveDistance) const
    {
        float awayX = 0.0f;
        float awayY = 0.0f;

        for (Creature* spirit : GetActiveSanguineSpirits(80.0f))
        {
            float distance = std::max(bot->GetDistance(spirit), 1.0f);
            if (distance > 16.0f)
                continue;

            awayX += (bot->GetPositionX() - spirit->GetPositionX()) / distance;
            awayY += (bot->GetPositionY() - spirit->GetPositionY()) / distance;
        }

        float length = std::sqrt(awayX * awayX + awayY * awayY);
        if (length < 0.1f)
        {
            awayX = bot->GetPositionX() - me->GetPositionX();
            awayY = bot->GetPositionY() - me->GetPositionY();
            length = std::sqrt(awayX * awayX + awayY * awayY);
        }

        if (length < 0.1f)
        {
            awayX = std::cos(me->GetOrientation());
            awayY = std::sin(me->GetOrientation());
            length = 1.0f;
        }

        awayX /= length;
        awayY /= length;

        float x = bot->GetPositionX() + awayX * desiredMoveDistance;
        float y = bot->GetPositionY() + awayY * desiredMoveDistance;
        float distFromBoss = std::sqrt(std::pow(x - me->GetPositionX(), 2.0f) + std::pow(y - me->GetPositionY(), 2.0f));

        if (distFromBoss > 45.0f)
        {
            float toBossX = me->GetPositionX() - bot->GetPositionX();
            float toBossY = me->GetPositionY() - bot->GetPositionY();
            float toBossLength = std::max(0.1f, std::sqrt(toBossX * toBossX + toBossY * toBossY));
            x = bot->GetPositionX() + toBossX / toBossLength * desiredMoveDistance;
            y = bot->GetPositionY() + toBossY / toBossLength * desiredMoveDistance;
        }

        float orientation = Position::NormalizeOrientation(std::atan2(me->GetPositionY() - y, me->GetPositionX() - x));

        Position position;
        position.Relocate(x, y, bot->GetPositionZ(), orientation);
        return position;
    }

    bool MoveBotIfNeeded(Creature* bot, Position const& position, float arrivedDistance, uint8 reason, bool emergencyMovement, bool releaseSoon)
    {
        if (!CanMoveBot(bot, emergencyMovement))
            return false;

        if (bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY()) <= arrivedDistance)
            return true;

        TrackBotState(bot, reason, false);
        SafeBotMoveTo(bot, position);

        if (releaseSoon)
        {
            ObjectGuid guid = bot->GetGUID();
            scheduler.Schedule(2500ms, GROUP_SPIRIT_AVOIDANCE, [this, guid, reason](TaskContext)
            {
                ReleaseBotState(guid, reason, true);
            });
        }

        return true;
    }

    void HandleSanguineSpiritAvoidance()
    {
        std::vector<Creature*> spirits = GetActiveSanguineSpirits(80.0f);
        if (spirits.empty())
            return;

        for (Creature* bot : GatherEncounterBots())
        {
            if (!bot || !bot->IsAlive())
                continue;

            bool activeTank = bot == me->GetVictim() && IsNpcBotTank(bot);
            float dangerRadius = activeTank ? 12.0f : 9.0f;

            if (!IsNearSanguineSpirit(bot, dangerRadius) && !(activeTank && IsNearSanguineSpirit(me, 12.0f)))
                continue;

            if (!CanMoveBot(bot, true))
                continue;

            uint8 reason = activeTank ? BOT_STATE_TANK_KITE : BOT_STATE_SPIRIT;
            if (activeTank)
            {
                uint32 now = getMSTime();
                if (_lastTankKiteMs && now - _lastTankKiteMs < 2000)
                    continue;

                _lastTankKiteMs = now;
            }

            bot->InterruptNonMeleeSpells(false);
            MoveBotIfNeeded(bot, GetSpiritEscapePosition(bot, activeTank ? 10.0f : 12.0f), 2.5f, reason, true, true);
        }
    }

    std::map<ObjectGuid, ScriptedBotState> _botStates;
    ObjectGuid _mirrorTargetGuid;
    uint32 _lastTankKiteMs = 0;
};

struct npc_sanguine_spirit : public ScriptedAI
{
    npc_sanguine_spirit(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        scheduler.CancelAll();
        me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_ALL, true);

        me->SetReactState(REACT_PASSIVE);

        DoCastSelf(SPELL_SANGUINE_SPIRIT_PRE_AURA);

        scheduler.Schedule(5s, [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_SANGUINE_SPIRIT_PRE_AURA2);
        }).Schedule(3s, [this](TaskContext /*context*/)
        {
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetInCombatWithZone();
            DoCastSelf(SPELL_SANGUINE_SPIRIT_AURA);

            if (Unit* target = SelectMirkbloodEncounterTarget(me, 100.0f, false, false))
                AttackStart(target);
        }).Schedule(30s, [this](TaskContext /*context*/)
        {
            me->DespawnOrUnsummon();
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }
};

class spell_mirkblood_blood_mirror_target_picker : public SpellScript
{
    PrepareSpellScript(spell_mirkblood_blood_mirror_target_picker)

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_BLOOD_MIRROR0, SPELL_BLOOD_MIRROR1, SPELL_BLOOD_MIRROR_TRANSITION_VISUAL });
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();

        if (!caster->ToCreature())
            return;

        Creature* casterCreature = caster->ToCreature();
        Unit* target = SelectMirkbloodEncounterTarget(casterCreature, 150.0f, true);

        if (!target) // Only Blood Mirror the tank if they're the only one around
            target = caster->GetVictim();

        if (!target)
            return;

        caster->CastSpell(caster, SPELL_BLOOD_MIRROR_TRANSITION_VISUAL, TRIGGERED_FULL_MASK);
        caster->CastSpell(target, SPELL_BLOOD_MIRROR_TRANSITION_VISUAL, TRIGGERED_FULL_MASK);

        caster->AddAura(SPELL_BLOOD_MIRROR1, caster); // Should be a cast, but channeled spell results in either Mirkblood or player being unactionable
        caster->AddAura(SPELL_BLOOD_MIRROR1, target); // Adding aura manually causes visual to not appear properly, but better than breaking gameplay

        target->CastSpell(caster, SPELL_BLOOD_MIRROR0); // Clone player
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_mirkblood_blood_mirror_target_picker::HandleHit);
    }
};

class spell_mirkblood_dash_gash_return_to_tank_pre_spell : public SpellScript
{
    PrepareSpellScript(spell_mirkblood_dash_gash_return_to_tank_pre_spell)

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DASH_GASH_RETURN_TO_TANK });
    }

    void HandleCast()
    {
        if (!GetCaster() || !GetCaster()->GetThreatMgr().GetCurrentVictim())
            return;
        // Probably wrong, maybe don't charge if would charge the same target?
        if (GetCaster()->GetDistance2d(GetCaster()->GetThreatMgr().GetCurrentVictim()) < 5.0f)
            return;

        GetCaster()->CastSpell(GetCaster()->GetVictim(), SPELL_DASH_GASH_RETURN_TO_TANK);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_mirkblood_dash_gash_return_to_tank_pre_spell::HandleCast);
    }
};

class spell_mirkblood_exsanguinate : public SpellScript
{
    PrepareSpellScript(spell_mirkblood_exsanguinate)

    void CalculateDamage()
    {
        if (!GetHitUnit())
            return;

        SetHitDamage(std::max((GetHitUnit()->GetHealth() * 0.66f), 2000.0f));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_mirkblood_exsanguinate::CalculateDamage);
    }
};

class at_karazhan_mirkblood_approach : public AreaTriggerScript
{
public:
    at_karazhan_mirkblood_approach() : AreaTriggerScript("at_karazhan_mirkblood_approach") {}

    bool OnTrigger(Player* player, AreaTrigger const* /*trigger*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
            if (Creature* mirkblood = instance->GetCreature(DATA_MIRKBLOOD))
                if (mirkblood->IsAlive() && !mirkblood->IsInCombat())
                    mirkblood->AI()->Talk(SAY_APPROACH, player);

        return false;
    }
};

class at_karazhan_mirkblood_entrance : public AreaTriggerScript
{
public:
    at_karazhan_mirkblood_entrance() : AreaTriggerScript("at_karazhan_mirkblood_entrance") {}

    bool OnTrigger(Player* player, AreaTrigger const* /*trigger*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
            if (Creature* mirkblood = instance->GetCreature(DATA_MIRKBLOOD))
                if (mirkblood->IsAlive() && mirkblood->IsImmuneToPC())
                    mirkblood->SetImmuneToPC(false);

        return false;
    }
};

class go_blood_drenched_door : public GameObjectScript
{
public:
    go_blood_drenched_door() : GameObjectScript("go_blood_drenched_door") {}

    struct go_blood_drenched_doorAI : public GameObjectAI
    {
        go_blood_drenched_doorAI(GameObject* go) : GameObjectAI(go) {}

        EventMap events;
        Creature* mirkblood;
        Player* opener;

        bool GossipHello(Player* player, bool /*reportUse*/) override
        {
            events.Reset();

            if (InstanceScript* instance = player->GetInstanceScript())
            {
                if (instance->GetBossState(DATA_MIRKBLOOD) != DONE)
                {
                    opener = player;
                    mirkblood = instance->GetCreature(DATA_MIRKBLOOD);

                    events.ScheduleEvent(EVENT_SAY, 1s);
                    events.ScheduleEvent(EVENT_FLAG, 5s);
                }
            }

            me->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);

            return true;
        }

        void UpdateAI(uint32 diff) override
        {
            if (events.Empty())
                return;

            events.Update(diff);
            switch (events.ExecuteEvent())
            {
            case EVENT_SAY:
                if (!mirkblood || !mirkblood->IsAlive())
                    return;
                mirkblood->AI()->Talk(SAY_AGGRO, opener);
                break;
            case EVENT_FLAG:
                if (mirkblood)
                    mirkblood->SetImmuneToPC(false);
                me->Delete();
                break;
            }
        }
    };

    GameObjectAI* GetAI(GameObject* go) const override
    {
        return new go_blood_drenched_doorAI(go);
    }
};

void AddSC_boss_tenris_mirkblood()
{
    RegisterKarazhanCreatureAI(boss_tenris_mirkblood);
    RegisterKarazhanCreatureAI(npc_sanguine_spirit);
    RegisterSpellScript(spell_mirkblood_blood_mirror_target_picker);
    RegisterSpellScript(spell_mirkblood_dash_gash_return_to_tank_pre_spell);
    RegisterSpellScript(spell_mirkblood_exsanguinate);
    new at_karazhan_mirkblood_approach();
    new at_karazhan_mirkblood_entrance();
    new go_blood_drenched_door();
}
