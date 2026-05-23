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
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Weather.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "zulaman.h"
#include <cmath>
#include <map>
#include <vector>

enum Spells
{
    SPELL_STATIC_DISRUPTION     = 43622,
    SPELL_STATIC_VISUAL         = 45265,
    SPELL_CALL_LIGHTNING        = 43661, // Missing timer
    SPELL_GUST_OF_WIND          = 43621,
    SPELL_ELECTRICAL_STORM      = 43648,
    SPELL_ELECTRICAL_STORM_AREA = 44007, // Safe within the eye of the storm
    SPELL_BERSERK               = 45078,
    SPELL_ELECTRICAL_OVERLOAD   = 43658,
    SPELL_EAGLE_SWOOP           = 44732,
    SPELL_ZAP                   = 43137,
    SPELL_SAND_STORM            = 25160,

    // Bot defensive consumables used while collapsing for Electrical Storm.
    SPELL_BOT_HEALTHSTONE       = 27235,
    SPELL_BOT_HEALING_POTION    = 28495,
    SPELL_BOT_NIGHTMARE_SEED    = 28726
};

enum Says
{
    SAY_AGGRO                   = 0,
    SAY_SUMMON                  = 1,
    SAY_INTRO                   = 2, // Not used in script
    SAY_ENRAGE                  = 3,
    SAY_KILL                    = 4,
    SAY_DEATH                   = 5
};

enum Misc
{
    ACTION_STORM_EXPIRE         = 1,
    GROUP_ELECTRICAL_STORM      = 1,
    GROUP_STATIC_DISRUPTION     = 2,
    GROUP_BOT_STORM_DIRECTOR    = 10,
    GROUP_BOT_SPREAD_DIRECTOR   = 11,
    ELECTRICAL_STORM_PREP_MS     = 1500,
    STORM_COLLAPSE_NUDGE_COUNT   = 16,
    SAND_STORM_DURATION_MS       = 8850,
    STORM_COLLAPSE_FALLBACK_MS   = ELECTRICAL_STORM_PREP_MS + SAND_STORM_DURATION_MS + 250
};

constexpr auto NPC_SOARING_EAGLE = 24858;

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

static void SafeBotMoveToStormAnchor(Creature* bot, Position const& destination)
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

struct boss_akilzon : public BossAI
{
    boss_akilzon(Creature* creature) : BossAI(creature, DATA_AKILZON), _stormActive(false), _hasStormGroundAnchor(false), _isRaining(false)
    {
        _stormGroundAnchor.Relocate(0.0f, 0.0f, 0.0f, 0.0f);
    }

    void Reset() override
    {
        StopStormBotCollapse(false);
        _stormGroundAnchor.Relocate(0.0f, 0.0f, 0.0f, 0.0f);
        _hasStormGroundAnchor = false;
        _Reset();

        _targetGUID.Clear();
        _cycloneGUID.Clear();
        _isRaining = false;

        SetWeather(WEATHER_STATE_FINE, 0.0f);

        me->m_Events.KillAllEvents(false);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler.Schedule(10s, 20s, GROUP_STATIC_DISRUPTION, [this](TaskContext context)
        {
            Unit* target = SelectRandomEncounterTarget(100.0f, false);
            if (!target)
                target = me->GetVictim();
            if (target)
            {
                _targetGUID = target->GetGUID();
                DoCast(target, SPELL_STATIC_DISRUPTION, false);
                me->SetInFront(me->GetVictim());
            }

            context.Repeat(10s, 18s);
        });

        ScheduleTimedEvent(20s, 30s, [&] {
            if (scheduler.GetNextGroupOccurrence(GROUP_ELECTRICAL_STORM) > 5s)
                if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                    DoCast(target, SPELL_GUST_OF_WIND);
        }, 20s, 30s);

        ScheduleTimedEvent(10s, 20s, [&] {
            DoCastVictim(SPELL_CALL_LIGHTNING);
        }, 12s, 17s);

        scheduler.Schedule(58500ms, GROUP_ELECTRICAL_STORM, [this](TaskContext context)
        {
            PrepareElectricalStorm();
            context.Repeat(1min);
        });

        ScheduleTimedEvent(47s, 52s, [&] {
            if (!_isRaining)
            {
                SetWeather(WEATHER_STATE_HEAVY_RAIN, 0.9999f);
                _isRaining = true;
            }
        }, 47s, 52s);

        me->m_Events.AddEventAtOffset([&] {
            Talk(SAY_ENRAGE);
            DoCastSelf(SPELL_BERSERK, true);
        }, 10min);

        Talk(SAY_AGGRO);
    }

    void JustDied(Unit* /*killer*/) override
    {
        StopStormBotCollapse(false);
        Talk(SAY_DEATH);
        _JustDied();
        me->m_Events.KillAllEvents(false);
    }

    void KilledUnit(Unit* who) override
    {
        if (who->IsPlayer() || IsNPCBotUnit(who))
            Talk(SAY_KILL);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        StopStormBotCollapse(false);
        BossAI::EnterEvadeMode(why);
    }

    void SetWeather(uint32 weather, float grade)
    {
        me->GetMap()->SetZoneWeather(me->GetZoneId(), WeatherState(weather), grade);
    }

    void HandleStormSequence()
    {
        me->m_Events.AddEventAtOffset([&] {
            HandleStormSequence();
        }, 1s);
    }

    void DoAction(int32 actionId) override
    {
        if (actionId == ACTION_STORM_EXPIRE)
        {
            StopStormBotCollapse();
            scheduler.DelayGroup(GROUP_STATIC_DISRUPTION, 3s);
            me->m_Events.AddEventAtOffset([&] {
                SummonEagles();
            }, 5s);

            me->InterruptNonMeleeSpells(false);

            SetWeather(WEATHER_STATE_FINE, 0.0f);
            _isRaining = false;
        }
    }

    void SummonEagles()
    {
        Talk(SAY_SUMMON);

        float x, y, z;
        me->GetPosition(x, y, z);

        for (uint8 i = 0; i < 8; ++i)
        {
            Unit* bird = ObjectAccessor::GetUnit(*me, _birdGUIDs[i]);
            if (!bird) //they despawned on die
            {
                Unit* target = SelectRandomEncounterTarget(100.0f, true);
                if (target)
                {
                    x = target->GetPositionX() + irand(-10, 10);
                    y = target->GetPositionY() + irand(-10, 10);
                    z = target->GetPositionZ() + urand(16, 20);
                    if (z > 95)
                        z = 95.0f - urand(0, 5);
                }
                ;
                if (Creature* creature = me->SummonCreature(NPC_SOARING_EAGLE, x, y, z, 0, TEMPSUMMON_CORPSE_DESPAWN, 0))
                {
                    Unit* attackTarget = target ? target : me->GetVictim();
                    if (attackTarget)
                    {
                        creature->AddThreat(attackTarget, 1.0f);
                        creature->AI()->AttackStart(attackTarget);
                    }
                    _birdGUIDs[i] = creature->GetGUID();
                }
            }
        }
    }

    Unit* SelectRandomEncounterTarget(float range, bool includeVictim = true) const
    {
        return SelectRandomEncounterTargetForCreature(me, range, includeVictim);
    }

    void PrepareElectricalStorm()
    {
        Unit* target = SelectRandomEncounterTarget(50.0f, true);
        if (!target)
        {
            EnterEvadeMode();
            return;
        }

        bool const targetIsBot = IsNPCBotUnit(target);
        ObjectGuid targetGuid = target->GetGUID();
        _stormGroundAnchor = GetElectricalStormGroundAnchor(target);
        _hasStormGroundAnchor = true;

        if (targetIsBot)
        {
            PrepareStormTargetBot(target);
            TeleportStormTargetBotToGroundAnchor(target);
        }

        StartStormBotCollapse(targetGuid);

        scheduler.Schedule(std::chrono::milliseconds(ELECTRICAL_STORM_PREP_MS), GROUP_ELECTRICAL_STORM, [this, targetGuid](TaskContext)
        {
            CastPreparedElectricalStorm(targetGuid);
        });
    }

    void CastPreparedElectricalStorm(ObjectGuid targetGuid)
    {
        if (!_stormActive || _stormTargetGUID != targetGuid)
            return;

        Unit* target = ObjectAccessor::GetUnit(*me, targetGuid);
        if (!target || !target->IsAlive() || !target->IsInWorld())
        {
            StopStormBotCollapse(false);
            return;
        }

        bool const targetIsBot = IsNPCBotUnit(target);
        if (targetIsBot)
        {
            PrepareStormTargetBot(target);
            TeleportStormTargetBotToGroundAnchor(target);
        }
        else
        {
            _stormGroundAnchor.Relocate(target);
            _hasStormGroundAnchor = true;
            MoveBotsUnderStormTarget(targetGuid);
        }

        DoCast(target, SPELL_ELECTRICAL_STORM); // storm cyclon + visual
        target->CastSpell(target, SPELL_ELECTRICAL_STORM_AREA, true); // cloud visual
        SetStormEffectDurations(target);

        if (targetIsBot)
        {
            TeleportStormTargetBotToAirAnchor(target);
            scheduler.Schedule(250ms, GROUP_BOT_STORM_DIRECTOR, [this, targetGuid](TaskContext)
            {
                ReassertStormTargetBotAnchor(targetGuid);
            });
        }
        else
            target->GetMotionMaster()->MoveJump(_stormGroundAnchor.GetPositionX(), _stormGroundAnchor.GetPositionY(), _stormGroundAnchor.GetPositionZ() + 16.0f, 1.0f, 1.0f);

        me->m_Events.AddEventAtOffset([&] {
            HandleStormSequence();
        }, 3s);

        me->m_Events.AddEventAtOffset([&] {
            if (!_isRaining)
            {
                SetWeather(WEATHER_STATE_HEAVY_RAIN, 0.9999f);
                _isRaining = true;
            }
        }, Seconds(urand(47, 52)));
    }

    void SetStormEffectDurations(Unit* target)
    {
        if (!target)
            return;

        if (Aura* aura = target->GetAura(SPELL_ELECTRICAL_STORM))
        {
            aura->SetDuration(SAND_STORM_DURATION_MS);
            aura->SetMaxDuration(SAND_STORM_DURATION_MS);
        }

        if (Aura* aura = target->GetAura(SPELL_SAND_STORM))
        {
            aura->SetDuration(SAND_STORM_DURATION_MS);
            aura->SetMaxDuration(SAND_STORM_DURATION_MS);
        }

        if (DynamicObject* dynObj = target->GetDynObject(SPELL_ELECTRICAL_STORM_AREA))
            dynObj->SetDuration(SAND_STORM_DURATION_MS);

        if (DynamicObject* dynObj = target->GetDynObject(SPELL_SAND_STORM))
            dynObj->SetDuration(SAND_STORM_DURATION_MS);
    }

    void StartStormBotCollapse(ObjectGuid targetGuid)
    {
        _stormActive = true;
        _stormTargetGUID = targetGuid;

        scheduler.CancelGroup(GROUP_BOT_STORM_DIRECTOR);
        scheduler.Schedule(250ms, GROUP_BOT_STORM_DIRECTOR, [this, targetGuid](TaskContext context)
        {
            MoveBotsUnderStormTarget(targetGuid);
            context.Repeat(500ms);
        });
        ScheduleStormCollapseNudgeBurst(targetGuid);

        scheduler.Schedule(std::chrono::milliseconds(STORM_COLLAPSE_FALLBACK_MS), GROUP_BOT_STORM_DIRECTOR, [this, targetGuid](TaskContext)
        {
            if (_stormActive && _stormTargetGUID == targetGuid)
                StopStormBotCollapse();
        });
    }

    void ScheduleStormCollapseNudgeBurst(ObjectGuid targetGuid)
    {
        uint32 const intervalMs = ELECTRICAL_STORM_PREP_MS / (STORM_COLLAPSE_NUDGE_COUNT - 1);

        for (uint32 i = 0; i < STORM_COLLAPSE_NUDGE_COUNT; ++i)
        {
            uint32 const delayMs = i ? i * intervalMs : 1;
            scheduler.Schedule(std::chrono::milliseconds(delayMs), GROUP_BOT_STORM_DIRECTOR, [this, targetGuid](TaskContext)
            {
                MoveBotsUnderStormTarget(targetGuid);
            });
        }
    }

    void StopStormBotCollapse(bool resumeAttack = true)
    {
        Position stormGroundAnchor = _stormGroundAnchor;
        bool hadStormGroundAnchor = _hasStormGroundAnchor;
        ObjectGuid stormTargetGuid = _stormTargetGUID;

        _stormActive = false;
        _stormTargetGUID.Clear();
        scheduler.CancelGroup(GROUP_BOT_STORM_DIRECTOR);

        for (ObjectGuid const& guid : _botStormLocks)
        {
            Creature* bot = ObjectAccessor::GetCreature(*me, guid);
            if (!bot || !bot->IsNPCBot())
                continue;

            if (bot_ai* ai = bot->GetBotAI())
            {
                uint32 originalState = 0;
                if (std::map<ObjectGuid, uint32>::const_iterator itr = _botStormOriginalStates.find(guid); itr != _botStormOriginalStates.end())
                    originalState = itr->second;

                if (guid == stormTargetGuid && hadStormGroundAnchor)
                {
                    if (bot->IsAlive() && bot->GetPositionZ() > stormGroundAnchor.GetPositionZ() + 3.0f)
                        bot->NearTeleportTo(stormGroundAnchor.GetPositionX(), stormGroundAnchor.GetPositionY(), stormGroundAnchor.GetPositionZ(), bot->GetOrientation(), true);

                    if (_stormTargetBotGravityDisabled && !_stormTargetBotWasLevitating)
                        bot->SetDisableGravity(false);
                }

                if (!(originalState & BOT_COMMAND_INACTION))
                    ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

                if (!(originalState & BOT_COMMAND_STAY))
                    ai->RemoveBotCommandState(BOT_COMMAND_STAY);

                if (bot->IsAlive() && resumeAttack && me->IsAlive() && me->IsEngaged())
                {
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->AttackStart(me);
                }
                else if (bot->IsAlive() && !ai->IAmFree())
                    ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
            }
        }

        _botStormLocks.clear();
        _botStormOriginalStates.clear();
        _botStormConsumablesUsed.clear();
        _stormTargetBotGUID.Clear();
        _stormTargetBotGravityDisabled = false;
        _stormTargetBotWasLevitating = false;
        _hasStormGroundAnchor = false;
        _stormGroundAnchor.Relocate(0.0f, 0.0f, 0.0f, 0.0f);
    }

    Position GetElectricalStormGroundAnchor(Unit* target) const
    {
        if (IsNPCBotUnit(target))
        {
            Position anchor = me->GetHomePosition();
            if (anchor.GetPositionX() != 0.0f || anchor.GetPositionY() != 0.0f || anchor.GetPositionZ() != 0.0f)
                return anchor;

            return me->GetPosition();
        }

        Position anchor;
        anchor.Relocate(target);
        return anchor;
    }

    void TrackStormBot(Creature* bot)
    {
        if (!bot || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        ObjectGuid guid = bot->GetGUID();
        _botStormLocks.insert(guid);
        if (!_botStormOriginalStates.count(guid))
            _botStormOriginalStates[guid] = ai->GetBotCommandState();
    }

    void PrepareStormTargetBot(Unit* target)
    {
        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->GetBotAI())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        TrackStormBot(bot);
        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        bot->GetMotionMaster()->Clear();

        ai->SetBotCommandState(BOT_COMMAND_INACTION);
        _stormTargetBotGUID = bot->GetGUID();
        _stormTargetBotWasLevitating = bot->IsLevitating();
        _stormTargetBotGravityDisabled = !_stormTargetBotWasLevitating;
        if (_stormTargetBotGravityDisabled)
            bot->SetDisableGravity(true);
    }

    void TeleportStormTargetBotToGroundAnchor(Unit* target)
    {
        if (!_hasStormGroundAnchor)
            return;

        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        bot->BotStopMovement();
        bot->GetMotionMaster()->Clear();
        bot->NearTeleportTo(
            _stormGroundAnchor.GetPositionX(),
            _stormGroundAnchor.GetPositionY(),
            _stormGroundAnchor.GetPositionZ(),
            bot->GetOrientation(),
            true
        );
    }

    void TeleportStormTargetBotToAirAnchor(Unit* target)
    {
        if (!_hasStormGroundAnchor)
            return;

        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        bot->BotStopMovement();
        bot->GetMotionMaster()->Clear();
        bot->NearTeleportTo(
            _stormGroundAnchor.GetPositionX(),
            _stormGroundAnchor.GetPositionY(),
            _stormGroundAnchor.GetPositionZ() + 16.0f,
            bot->GetOrientation(),
            true
        );
    }

    void ReassertStormTargetBotAnchor(ObjectGuid targetGuid)
    {
        if (!_stormActive || !_hasStormGroundAnchor)
            return;

        Unit* target = ObjectAccessor::GetUnit(*me, targetGuid);
        Creature* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        if (bot->GetExactDist2d(_stormGroundAnchor.GetPositionX(), _stormGroundAnchor.GetPositionY()) > 1.0f ||
            bot->GetPositionZ() < _stormGroundAnchor.GetPositionZ() + 12.0f)
        {
            PrepareStormTargetBot(bot);
            TeleportStormTargetBotToAirAnchor(bot);
        }
    }

    void CastRandomStormConsumable(Creature* bot)
    {
        if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
            return;

        if (_botStormConsumablesUsed.contains(bot->GetGUID()))
            return;

        _botStormConsumablesUsed.insert(bot->GetGUID());

        uint32 spellId = SPELL_BOT_HEALTHSTONE;
        switch (urand(0, 2))
        {
            case 0:
                spellId = SPELL_BOT_HEALTHSTONE;
                break;
            case 1:
                spellId = SPELL_BOT_HEALING_POTION;
                break;
            default:
                spellId = SPELL_BOT_NIGHTMARE_SEED;
                break;
        }

        bot->CastSpell(bot, spellId, true);
    }

    void MoveBotsUnderStormTarget(ObjectGuid targetGuid)
    {
        if (!_stormActive || !_hasStormGroundAnchor)
            return;

        Unit* target = ObjectAccessor::GetUnit(*me, targetGuid);
        if (!target || !target->IsAlive() || !target->IsInWorld())
            return;

        float centerX = _stormGroundAnchor.GetPositionX();
        float centerY = _stormGroundAnchor.GetPositionY();

        std::vector<Creature*> bots = GatherEncounterBotsForCreature(me, 150.0f);
        uint32 movableCount = 0;
        for (Creature* bot : bots)
        {
            if (!bot || bot->GetGUID() == targetGuid)
                continue;

            if (bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            ++movableCount;
        }

        uint32 index = 0;
        for (Creature* bot : bots)
        {
            if (!bot || bot->GetGUID() == targetGuid)
                continue;

            if (bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            if (IsBotCastingUninterruptible(bot))
                continue;

            float angle = movableCount ? (2.0f * float(M_PI) * float(index)) / float(movableCount) : 0.0f;
            float radius = 1.25f + float(index % 4) * 0.55f;
            float x = centerX + std::cos(angle) * radius;
            float y = centerY + std::sin(angle) * radius;

            TrackStormBot(bot);
            bot->InterruptNonMeleeSpells(false);
            bot->AttackStop();

            if (bot_ai* ai = bot->GetBotAI())
                ai->SetBotCommandState(BOT_COMMAND_INACTION);

            Position destination;
            destination.Relocate(x, y, _stormGroundAnchor.GetPositionZ(), bot->GetOrientation());

            bool const needsMove = bot->GetExactDist2d(x, y) > 1.25f || std::fabs(bot->GetPositionZ() - _stormGroundAnchor.GetPositionZ()) > 3.0f;
            if (needsMove)
            {
                CastRandomStormConsumable(bot);
                SafeBotMoveToStormAnchor(bot, destination);
            }
            else if (bot->isMoving())
                bot->BotStopMovement();

            ++index;
        }
    }

private:
    ObjectGuid _birdGUIDs[8];
    ObjectGuid _targetGUID;
    ObjectGuid _cycloneGUID;
    ObjectGuid _stormTargetGUID;
    ObjectGuid _stormTargetBotGUID;
    GuidSet _botStormLocks;
    GuidSet _botStormConsumablesUsed;
    std::map<ObjectGuid, uint32> _botStormOriginalStates;
    Position _stormGroundAnchor;
    bool _stormActive;
    bool _hasStormGroundAnchor;
    bool _stormTargetBotGravityDisabled = false;
    bool _stormTargetBotWasLevitating = false;
    bool   _isRaining;
};

struct npc_akilzon_eagle : public ScriptedAI
{
    npc_akilzon_eagle(Creature* creature) : ScriptedAI(creature) { }

    uint32 EagleSwoop_Timer;
    bool arrived;
    ObjectGuid TargetGUID;

    void Reset() override
    {
        EagleSwoop_Timer = urand(5000, 10000);
        arrived = true;
        TargetGUID.Clear();
        me->SetDisableGravity(true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void MoveInLineOfSight(Unit* /*who*/) override { }

    void MovementInform(uint32, uint32) override
    {
        arrived = true;
        if (TargetGUID)
        {
            if (Unit* target = ObjectAccessor::GetUnit(*me, TargetGUID))
                DoCast(target, SPELL_EAGLE_SWOOP, true);
            TargetGUID.Clear();
            me->SetSpeed(MOVE_RUN, 1.2f);
            EagleSwoop_Timer = urand(5000, 10000);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (EagleSwoop_Timer <= diff)
            EagleSwoop_Timer = 0;
        else
            EagleSwoop_Timer -= diff;

        if (arrived)
        {
            if (Unit* target = SelectRandomEncounterTargetForCreature(me, 100.0f, true))
            {
                float x, y, z;
                if (EagleSwoop_Timer)
                {
                    x = target->GetPositionX() + irand(-10, 10);
                    y = target->GetPositionY() + irand(-10, 10);
                    z = target->GetPositionZ() + urand(10, 15);
                    if (z > 95)
                        z = 95.0f - urand(0, 5);
                }
                else
                {
                    target->GetContactPoint(me, x, y, z);
                    z += 2;
                    me->SetSpeed(MOVE_RUN, 5.0f);
                    TargetGUID = target->GetGUID();
                }
                me->GetMotionMaster()->MovePoint(0, x, y, z);
                arrived = false;
            }
        }
    }
};

// 43648 - Electrical Storm
class spell_electrial_storm : public AuraScript
{
    PrepareAuraScript(spell_electrial_storm);

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsCreature();
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            GetCaster()->ToCreature()->AI()->DoAction(ACTION_STORM_EXPIRE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_electrial_storm::OnRemove, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

// 43657 - Electrical Storm
class spell_electrical_storm_proc : public SpellScript
{
    PrepareSpellScript(spell_electrical_storm_proc);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if(Acore::UnitAuraCheck(true, SPELL_ELECTRICAL_STORM_AREA));
    }

    void HandleDamageCalc(SpellEffIndex /*effIndex*/)
    {
        if (Aura* aura = GetCaster()->GetAura(SPELL_ELECTRICAL_STORM))
        {
            uint8 multiplier = aura->GetEffect(EFFECT_1)->GetTickNumber();
            SetHitDamage(GetHitDamage() * multiplier);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_electrical_storm_proc::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_electrical_storm_proc::HandleDamageCalc, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

void AddSC_boss_akilzon()
{
    RegisterZulAmanCreatureAI(boss_akilzon);
    RegisterZulAmanCreatureAI(npc_akilzon_eagle);
    RegisterSpellScript(spell_electrial_storm);
    RegisterSpellScript(spell_electrical_storm_proc);
}
