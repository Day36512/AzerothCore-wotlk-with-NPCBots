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

#include "CellImpl.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "hyjal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Texts
{
    SAY_AGGRO       = 1,
    SAY_DOOMFIRE    = 2,
    SAY_AIR_BURST   = 3,
    SAY_SLAY        = 4,
    SAY_ENRAGE      = 5,
    SAY_DEATH       = 6,
    SAY_SOUL_CHARGE = 7,
};

enum ArchiSpells
{
    SPELL_DENOUEMENT_WISP       = 32124,
    SPELL_ANCIENT_SPARK         = 39349,
    SPELL_PROTECTION_OF_ELUNE   = 38528,

    SPELL_DRAIN_WORLD_TREE      = 39140,
    SPELL_DRAIN_WORLD_TREE_2    = 39141,

    SPELL_FINGER_OF_DEATH       = 31984,
    SPELL_RED_SKY_EFFECT        = 32111,
    SPELL_HAND_OF_DEATH         = 35354,
    SPELL_AIR_BURST             = 32014,
    SPELL_GRIP_OF_THE_LEGION    = 31972,
    SPELL_DOOMFIRE_STRIKE       = 31903,    //summons two creatures
    SPELL_DOOMFIRE_SPAWN        = 32074,
    SPELL_DOOMFIRE              = 31945,
    SPELL_DOOMFIRE_DOT          = 31969,
    SPELL_SOUL_CHARGE_YELLOW    = 32045,
    SPELL_SOUL_CHARGE_GREEN     = 32051,
    SPELL_SOUL_CHARGE_RED       = 32052,
    SPELL_UNLEASH_SOUL_YELLOW   = 32054,
    SPELL_UNLEASH_SOUL_GREEN    = 32057,
    SPELL_UNLEASH_SOUL_RED      = 32053,
    SPELL_FEAR                  = 31970,
    SPELL_SLOW_FALL             = 130,
    SPELL_LEVITATE              = 1706,
};

enum Summons
{
    CREATURE_DOOMFIRE           = 18095,
    CREATURE_DOOMFIRE_SPIRIT    = 18104,
    CREATURE_ANCIENT_WISP       = 17946,
    CREATURE_CHANNEL_TARGET     = 22418,
    DISPLAY_ID_TRIGGER          = 11686
};

enum Events
{
    EVENT_ENRAGE = 0
};

enum SpellGroups
{
    GROUP_FEAR  = 0
};

uint32 const availableChargeAurasAndSpells[3][2] = {
    {SPELL_SOUL_CHARGE_RED,     SPELL_UNLEASH_SOUL_RED      },
    {SPELL_SOUL_CHARGE_YELLOW,  SPELL_UNLEASH_SOUL_YELLOW   },
    {SPELL_SOUL_CHARGE_GREEN,   SPELL_UNLEASH_SOUL_GREEN    }
};

Position const nordrassilPosition = { 5503.713f, -3523.436f, 1608.781f, 0.0f };

float const DOOMFIRE_OFFSET = 15.0f;
uint8 const WISP_OFFSET = 40;
uint8 NEAR_POINT = 0;

namespace
{
    char const* const DBM_HYJAL_MODULE_FOLDER = "DBM-Hyjal";
    char const* const DBM_ARCHIMONDE_MODULE_ID = "Archimonde";

    constexpr float ARCHIMONDE_PI = 3.14159265358979323846f;
    constexpr float AIR_BURST_SAFE_RADIUS = 15.0f;
    constexpr float AIR_BURST_SCAN_RADIUS = 25.0f;
    constexpr float AIR_BURST_MOVE_MIN_RADIUS = 17.0f;
    constexpr float AIR_BURST_MOVE_MAX_RADIUS = 31.0f;
    constexpr float AIR_BURST_MOVE_RADIUS_STEP = 3.5f;
    constexpr float AIR_BURST_MOVE_MAX_DIST = 45.0f;
    constexpr float AIR_BURST_FALL_PROTECTION_SCAN_RADIUS = 60.0f;

    bool IsValidArchimondeNPCBot(Creature const* creature)
    {
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI() && !creature->IsInEvadeMode();
    }

    bool IsArchimondePlayerOrBot(Unit const* unit)
    {
        if (!unit)
            return false;

        if (unit->IsPlayer())
            return true;

        return IsValidArchimondeNPCBot(unit->ToCreature());
    }

    bool IsValidArchimondeRandomTarget(Unit const* target, Creature const* source, float dist = 0.0f, bool withTank = true)
    {
        if (!target || !source || !target->IsInWorld() || !target->IsAlive() || !target->IsInCombat() || target->GetMap() != source->GetMap())
            return false;

        if (target->HasUnitState(UNIT_STATE_ISOLATED))
            return false;

        if (!withTank && target == source->GetThreatMgr().GetLastVictim())
            return false;

        if (dist > 0.0f && !source->IsWithinCombatRange(target, dist))
            return false;

        if (dist < 0.0f && source->IsWithinCombatRange(target, -dist))
            return false;

        if (!source->IsValidAttackTarget(target))
            return false;

        return IsArchimondePlayerOrBot(target);
    }

    uint8 GetArchimondeSoulChargeClass(Unit* unit)
    {
        if (!unit)
            return CLASS_NONE;

        if (Player* player = unit->ToPlayer())
            return player->getClass();

        Creature* bot = unit->ToCreature();
        if (!IsValidArchimondeNPCBot(bot))
            return CLASS_NONE;

        return bot->GetBotAI()->GetPlayerClass();
    }

    bool HasArchimondeFallProtection(Unit const* target)
    {
        return target && (target->HasAuraType(SPELL_AURA_FEATHER_FALL) || target->HasAuraType(SPELL_AURA_HOVER));
    }

    Creature* FindArchimondeFallProtectionCaster(Unit* target, uint8 playerClass)
    {
        if (!target)
            return nullptr;

        Creature* bestCaster = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        std::list<Unit*> units;
        Bcore::AnyUnitInObjectRangeCheck check(target, AIR_BURST_FALL_PROTECTION_SCAN_RADIUS);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(target, units, check);
        Cell::VisitObjects(target, searcher, AIR_BURST_FALL_PROTECTION_SCAN_RADIUS);

        for (Unit* unit : units)
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (!IsValidArchimondeNPCBot(bot) || !bot->IsAlive() || !bot->IsInCombat() || bot->GetMap() != target->GetMap())
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai || ai->GetPlayerClass() != playerClass)
                continue;

            if (!bot->IsWithinLOSInMap(target))
                continue;

            float const distance = bot->GetExactDist(target);
            if (distance < bestDistance)
            {
                bestCaster = bot;
                bestDistance = distance;
            }
        }

        return bestCaster;
    }

    void CastArchimondeAirBurstFallProtection(Unit* target)
    {
        if (!target || !target->IsInWorld() || !target->IsAlive() || !IsArchimondePlayerOrBot(target) || HasArchimondeFallProtection(target))
            return;

        if (Creature* mage = FindArchimondeFallProtectionCaster(target, CLASS_MAGE))
        {
            mage->CastSpell(target, SPELL_SLOW_FALL, true);
            return;
        }

        if (Creature* priest = FindArchimondeFallProtectionCaster(target, CLASS_PRIEST))
            priest->CastSpell(target, SPELL_LEVITATE, true);
    }

    bool IsWithinArchimondeBotAoe(bot_ai const* ai, Position const& position)
    {
        if (!ai)
            return false;

        for (auto const& spot : ai->GetAoeSpots())
        {
            float const dx = position.GetPositionX() - spot.first.GetPositionX();
            float const dy = position.GetPositionY() - spot.first.GetPositionY();
            if (dx * dx + dy * dy <= spot.second * spot.second)
                return true;
        }

        return false;
    }

    bool TryGetArchimondeAirBurstSafePosition(Creature* bot, Unit* burstTarget, Position& destination)
    {
        if (!bot || !burstTarget)
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return false;

        ai->RefreshAoeSpots();

        float bestScore = -std::numeric_limits<float>::max();
        Position bestPosition;
        bool found = false;

        float const baseAngle = std::atan2(bot->GetPositionY() - burstTarget->GetPositionY(), bot->GetPositionX() - burstTarget->GetPositionX());
        for (float radius = AIR_BURST_MOVE_MIN_RADIUS; radius <= AIR_BURST_MOVE_MAX_RADIUS; radius += AIR_BURST_MOVE_RADIUS_STEP)
        {
            for (uint8 step = 0; step < 16; ++step)
            {
                float const angle = baseAngle + (2.0f * ARCHIMONDE_PI * float(step) / 16.0f);
                float x = burstTarget->GetPositionX() + std::cos(angle) * radius;
                float y = burstTarget->GetPositionY() + std::sin(angle) * radius;
                float z = burstTarget->GetPositionZ();

                if (!bot->CanFly())
                    bot->UpdateAllowedPositionZ(x, y, z);

                if (!bot->IsWithinLOS(x, y, z))
                    continue;

                if (burstTarget->GetExactDist2d(x, y) < AIR_BURST_SAFE_RADIUS)
                    continue;

                Position candidate;
                candidate.Relocate(x, y, z, bot->GetAngle(burstTarget));

                if (bot->GetExactDist(candidate) > AIR_BURST_MOVE_MAX_DIST)
                    continue;

                if (IsWithinArchimondeBotAoe(ai, candidate))
                    continue;

                float score = burstTarget->GetExactDist2d(x, y);
                score -= bot->GetExactDist2d(x, y) * 0.08f;

                if (!found || score > bestScore)
                {
                    found = true;
                    bestScore = score;
                    bestPosition = candidate;
                }
            }
        }

        if (!found)
            return false;

        destination = bestPosition;
        return true;
    }

    void RestoreArchimondeAirBurstBotCommandState(Creature* bot, ObjectGuid botGuid, uint32 originalCommandState)
    {
        if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid)
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

    void MoveArchimondeAirBurstBot(Creature* bot, Unit* burstTarget)
    {
        if (!IsValidArchimondeNPCBot(bot) || !burstTarget || bot == burstTarget)
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree() || bot->GetExactDist2d(burstTarget) >= AIR_BURST_SAFE_RADIUS)
            return;

        Position destination;
        if (!TryGetArchimondeAirBurstSafePosition(bot, burstTarget, destination))
            return;

        uint32 const originalCommandState = ai->GetBotCommandState();
        ObjectGuid const botGuid = bot->GetGUID();

        if (!(originalCommandState & BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_INACTION);

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        ai->SetBotCommandState(BOT_COMMAND_STAY);
        ai->BotMovement(BOT_MOVE_POINT, &destination, nullptr, true);

        bot->m_Events.AddEventAtOffset([bot, botGuid, originalCommandState]()
        {
            RestoreArchimondeAirBurstBotCommandState(bot, botGuid, originalCommandState);
        }, 3500ms);
    }
}

struct npc_ancient_wisp : public ScriptedAI
{
    npc_ancient_wisp(Creature* creature) : ScriptedAI(creature)
    {
        _instance = creature->GetInstanceScript();
    }

    void Reset() override
    {
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        ScheduleTimedEvent(1s, [&]
        {
            if (Creature* archimonde = _instance->GetCreature(DATA_ARCHIMONDE))
            {
                if (archimonde->HealthBelowPct(2) || !archimonde->IsAlive())
                {
                    DoCastSelf(SPELL_DENOUEMENT_WISP);
                }
                else
                {
                    DoCast(archimonde, SPELL_ANCIENT_SPARK);
                }
            }
        }, 1s);
    }

    void JustEngagedWith(Unit* /*who*/) override { }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        damage = 0;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

    }
private:
    InstanceScript* _instance;
};

struct npc_doomfire_spirit : public ScriptedAI
{
    npc_doomfire_spirit(Creature* creature) : ScriptedAI(creature){ }

    float const turnConstant = 0.785402f; // 45 degree turns, verified with sniffs

    void Reset() override
    {
        ScheduleUniqueTimedEvent(10ms, [&] {
            TryTeleportInDirection(1.f, M_PI, 1.f, true); //turns around and teleports 1 unit on spawn, assuming same logic as later teleports applies

            ScheduleTimedEvent(10ms, [&] {
                float angle = irand(-1, 1) * turnConstant;
                TryTeleportInDirection(8.f, angle, 2.f, false);
            }, 1600ms);
        },1);
    }

    void TryTeleportInDirection(float dist, float angle, float step, bool alwaysturn)
    {
        Position pos;
        while (dist >= 0)
        {
            pos = me->WorldObject::GetFirstCollisionPosition(dist, angle);
            if (fabsf(dist - me->GetExactDist2d(pos)) < 0.001) // Account for small deviation
                break;
            dist -= step; // Distance drops with each unsuccessful attempt
        }

        if (dist || alwaysturn)
            me->NearTeleportTo(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), Position::NormalizeOrientation(me->GetOrientation() + angle));
        else // Orientation does not change if not moving, verified with sniffs
            me->NearTeleportTo(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), me->GetOrientation());
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

struct boss_archimonde : public BossAI
{
    boss_archimonde(Creature* creature) : BossAI(creature, DATA_ARCHIMONDE)
    {
    }

    void Reset() override
    {
        _Reset();
        _wispCount = 0;
        _isChanneling = false;
        _enraged = false;
        _availableAuras.clear();
        _availableSpells.clear();

        if (instance->GetBossState(DATA_AZGALOR) != DONE)
        {
            me->SetVisible(false);
            me->SetReactState(REACT_PASSIVE);
        }
        else
        {
            DoAction(ACTION_BECOME_ACTIVE_AND_CHANNEL);
        }

        ScheduleHealthCheckEvent(10, [&]{
            scheduler.CancelAll();
            me->SetReactState(REACT_PASSIVE);
            DoCastAOE(SPELL_PROTECTION_OF_ELUNE, true);
            Talk(SAY_DEATH);
            _enraged = true;
            me->GetMotionMaster()->Clear(false);
            me->GetMotionMaster()->MoveIdle();
            ScheduleTimedEvent(1s, [&]
            {
                if (_wispCount >= 30)
                {
                    me->KillSelf();
                }
                Position wispPosition = { me->GetPositionX() + float(rand() % WISP_OFFSET), me->GetPositionY() + float(rand() % WISP_OFFSET), me->GetPositionZ(), 0.0f };
                if (Creature* wisp = me->SummonCreature(CREATURE_ANCIENT_WISP, wispPosition))
                {
                    wisp->AI()->DoCast(me, SPELL_ANCIENT_SPARK);
                    ++_wispCount;
                }
            }, 1500ms);
            ScheduleTimedEvent(1500ms, [&]
            {
                DoCastVictim(SPELL_RED_SKY_EFFECT);
                DoCastVictim(SPELL_HAND_OF_DEATH);
            }, 3s);
        });
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_BECOME_ACTIVE_AND_CHANNEL:
                me->SetReactState(REACT_AGGRESSIVE);
                me->SetVisible(true);
                if (!_isChanneling)
                {
                    if (Creature* nordrassil = me->SummonCreature(CREATURE_CHANNEL_TARGET, nordrassilPosition, TEMPSUMMON_TIMED_DESPAWN, 1200000))
                    {
                        DoCast(nordrassil, SPELL_DRAIN_WORLD_TREE);
                        _isChanneling = true;
                        nordrassil->AI()->DoCast(me, SPELL_DRAIN_WORLD_TREE_2, true);
                    }
                }
                break;
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        me->InterruptNonMeleeSpells(false);
        Talk(SAY_AGGRO);
        ScheduleTimedEvent(25s, 35s, [&]
        {
            Unit* target = nullptr;
            if (DoCastRandomPlayerOrBotTarget(SPELL_AIR_BURST, 1, 0.0f, false, true, &target, false) == SPELL_CAST_OK)
            {
                AnnounceAirBurstTarget(target);
                MoveNearbyBotsAwayFromAirBurstTarget(target);
                scheduler.DelayGroup(GROUP_FEAR, 5s);
                Talk(SAY_AIR_BURST);
            }
        }, 25s, 40s);
        ScheduleTimedEvent(8s, [&]
        {
            DoCastDoomFire();
        }, 8s);
        ScheduleTimedEvent(25s, 35s, [&]
        {
            DoCastRandomPlayerOrBotTarget(SPELL_GRIP_OF_THE_LEGION);
        }, 5s, 25s);
        ScheduleTimedEvent(5s, [&]
        {
            if (me->GetExactDist2d(nordrassilPosition) < 75.0f)
            {
                if (!_enraged)
                {
                    _enraged = true;
                    Talk(SAY_ENRAGE);
                    ScheduleTimedEvent(1s, [&]
                    {
                        DoCastVictim(SPELL_RED_SKY_EFFECT);
                        DoCastVictim(SPELL_HAND_OF_DEATH);
                    }, 3s);
                }
            }
        }, 5s);
        ScheduleTimedEvent(5s, [&]
        {
            if (!HasPlayerOrBotInMeleeRange())
                DoCastRandomPlayerOrBotTarget(SPELL_FINGER_OF_DEATH);
        }, 3500ms);
        ScheduleTimedEvent(10min, [&]
        {
            DoCastVictim(SPELL_RED_SKY_EFFECT);
            DoCastVictim(SPELL_HAND_OF_DEATH);
        }, 3s);
        scheduler.Schedule(40s, GROUP_FEAR, [this](TaskContext context)
        {
            DoCastAOE(SPELL_FEAR);
            context.Repeat(42s);
        });
        instance->SetData(DATA_SPAWN_WAVES, 1);
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_SLAY);
    }

    void SetGUID(ObjectGuid const& guid, int32 type) override
    {
        if (type == GUID_GAIN_SOUL_CHARGE_PLAYER)
        {
            if (Unit* unit = ObjectAccessor::GetUnit(*me, guid))
            {
                uint32 spellId = 0;
                switch (GetArchimondeSoulChargeClass(unit))
                {
                    case CLASS_MAGE:
                    case CLASS_PRIEST:
                    case CLASS_WARLOCK:
                        spellId = SPELL_SOUL_CHARGE_RED;
                        break;
                    case CLASS_DEATH_KNIGHT:
                    case CLASS_PALADIN:
                    case CLASS_ROGUE:
                    case CLASS_WARRIOR:
                        spellId = SPELL_SOUL_CHARGE_YELLOW;
                        break;
                    case CLASS_DRUID:
                    case CLASS_HUNTER:
                    case CLASS_SHAMAN:
                        spellId = SPELL_SOUL_CHARGE_GREEN;
                        break;
                    case CLASS_NONE:
                    default:
                        break;
                }

                if (spellId)
                {
                    unit->CastSpell(me, spellId, true);

                    scheduler.Schedule(2s, 10s, [this](TaskContext)
                    {
                        UnleashSoulCharge();
                    });
                }
            }
        }
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        instance->SetData(DATA_RESET_NIGHT_ELF, 1);
        BossAI::EnterEvadeMode(why);
    }

    void JustSummoned(Creature* summoned) override
    {
        BossAI::JustSummoned(summoned);
        if (summoned->GetEntry() == CREATURE_ANCIENT_WISP)
        {
            summoned->AI()->AttackStart(me);
        }
        else if (summoned->GetEntry() == CREATURE_DOOMFIRE)
        {
            summoned->CastSpell(summoned, SPELL_DOOMFIRE_SPAWN);
            summoned->CastSpell(summoned, SPELL_DOOMFIRE, true, 0, 0, me->GetGUID());
        }
        else
        {
            summoned->SetFaction(me->GetFaction()); //remove?
            summoned->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            summoned->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        }
    }

    void DoCastDoomFire()
    {
        // hack because spell doesn't work?
        Talk(SAY_DOOMFIRE);
        float angle = 2 * M_PI * rand() / RAND_MAX;
        float x = me->GetPositionX() + DOOMFIRE_OFFSET * cos(angle);
        float y = me->GetPositionY() + DOOMFIRE_OFFSET * sin(angle);
        Position spiritPosition = Position(x, y, me->GetPositionZ(), Position::NormalizeOrientation(angle + M_PI)); //spirit faces archimonde on spawn
        Position doomfirePosition = Position(x, y, me->GetPositionZ());
        if (Creature* doomfireSpirit = me->SummonCreature(CREATURE_DOOMFIRE_SPIRIT, spiritPosition, TEMPSUMMON_TIMED_DESPAWN, 27000))
        {
            if (Creature* doomfire = me->SummonCreature(CREATURE_DOOMFIRE, doomfirePosition, TEMPSUMMON_TIMED_DESPAWN, 27000))
            {
                doomfireSpirit->SetWalk(false);
                doomfireSpirit->SetReactState(REACT_PASSIVE);
                doomfire->SetReactState(REACT_PASSIVE);
                doomfire->GetMotionMaster()->MoveFollow(doomfireSpirit, 0.0f, 0.0f);
            }
        }
    }

    void UnleashSoulCharge()
    {
        me->InterruptNonMeleeSpells(false);
        //add all auras to spells
        for (uint8 n = 0; n < 3; ++n)
        {
            if (me->HasAura(availableChargeAurasAndSpells[n][0]))
            {
                _availableAuras.push_back(availableChargeAurasAndSpells[n][0]);
                _availableSpells.push_back(availableChargeAurasAndSpells[n][1]);
            }
        }
        //only unleash when we found spells and auras
        if (!_availableAuras.empty() && !_availableSpells.empty())
        {
            //coin flip to swap front and back item
            if (urand(0, 1))
            {
                _availableAuras.insert(_availableAuras.begin(), _availableAuras.back());
                _availableAuras.pop_back();
                _availableSpells.insert(_availableSpells.begin(), _availableSpells.back());
                _availableSpells.pop_back();
            }
            //remove aura and cast spell
            me->RemoveAuraFromStack(_availableAuras.front());
            DoCastVictim(_availableSpells.front());
            //clear to clean vectors
            _availableAuras.clear();
            _availableSpells.clear();
        }
    }
private:
    Unit* SelectRandomPlayerOrBotTarget(uint32 threatTablePosition = 0, float dist = 0.0f, bool withTank = true)
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, threatTablePosition, [this, dist, withTank](Unit* target)
        {
            return IsValidArchimondeRandomTarget(target, me, dist, withTank);
        });

        return targets.empty() ? nullptr : targets.front();
    }

    SpellCastResult DoCastRandomPlayerOrBotTarget(uint32 spellId, uint32 threatTablePosition = 0, float dist = 0.0f, bool triggered = false, bool withTank = true, Unit** selectedTarget = nullptr, bool fallbackToVictim = true)
    {
        if (selectedTarget)
            *selectedTarget = nullptr;

        if (Unit* target = SelectRandomPlayerOrBotTarget(threatTablePosition, dist, withTank))
        {
            SpellCastResult result = DoCast(target, spellId, triggered);
            if (result == SPELL_CAST_OK && selectedTarget)
                *selectedTarget = target;

            return result;
        }

        SpellCastResult result = DoCastRandomTarget(spellId, threatTablePosition, dist, true, triggered, withTank);
        if (result != SPELL_FAILED_BAD_TARGETS)
            return result;

        if (!fallbackToVictim)
            return result;

        if (Unit* victim = me->GetVictim())
        {
            result = DoCast(victim, spellId, triggered);
            if (result == SPELL_CAST_OK && selectedTarget)
                *selectedTarget = victim;

            return result;
        }

        return result;
    }

    bool HasPlayerOrBotInMeleeRange() const
    {
        std::list<Unit*> units;
        Bcore::AnyUnitInObjectRangeCheck check(me, 30.0f);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(me, units, check);
        Cell::VisitObjects(me, searcher, 30.0f);

        for (Unit* unit : units)
        {
            if (IsValidArchimondeRandomTarget(unit, me) && me->IsWithinMeleeRange(unit))
                return true;
        }

        return false;
    }

    void AnnounceAirBurstTarget(Unit* target)
    {
        if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target))
            DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(bot, SPELL_AIR_BURST, DBM_HYJAL_MODULE_FOLDER, DBM_ARCHIMONDE_MODULE_ID, "Air Burst", DBMFTABotCallouts::GetCooldownMs());
    }

    void MoveNearbyBotsAwayFromAirBurstTarget(Unit* target)
    {
        if (!target || !target->IsInWorld())
            return;

        std::list<Unit*> units;
        Bcore::AnyUnitInObjectRangeCheck check(target, AIR_BURST_SCAN_RADIUS);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(target, units, check);
        Cell::VisitObjects(target, searcher, AIR_BURST_SCAN_RADIUS);

        for (Unit* unit : units)
            MoveArchimondeAirBurstBot(unit ? unit->ToCreature() : nullptr, target);
    }

    uint8 _wispCount;
    bool _isChanneling;
    bool _enraged;
    std::vector<uint32> _availableAuras;
    std::vector<uint32> _availableSpells;
};
class spell_red_sky_effect : public SpellScript
{
    PrepareSpellScript(spell_red_sky_effect);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (GetHitUnit())
            PreventHitDamage();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_red_sky_effect::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_archimonde_air_burst_fall_protection : public SpellScript
{
    PrepareSpellScript(spell_archimonde_air_burst_fall_protection);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_AIR_BURST, SPELL_SLOW_FALL, SPELL_LEVITATE });
    }

    void HandleAfterHit()
    {
        CastArchimondeAirBurstFallProtection(GetHitUnit());
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_archimonde_air_burst_fall_protection::HandleAfterHit);
    }
};

class spell_doomfire : public AuraScript
{
    PrepareAuraScript(spell_doomfire);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DOOMFIRE_DOT });
    }

    void PeriodicTick(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        int32 bp = GetSpellInfo()->Effects[EFFECT_1].CalcValue();
        float tickCoef = (static_cast<float>(aurEff->GetTickNumber() - 1) / aurEff->GetTotalTicks()); // Tick moved back to ensure proper damage on each tick
        int32 damage = bp - (bp*tickCoef);
        target->CastCustomSpell(target, SPELL_DOOMFIRE_DOT, &damage, &damage, &damage, true, nullptr, nullptr, target->GetGUID());
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_doomfire::PeriodicTick, EFFECT_ALL, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_boss_archimonde()
{
    RegisterSpellScript(spell_red_sky_effect);
    RegisterSpellScript(spell_archimonde_air_burst_fall_protection);
    RegisterSpellScript(spell_doomfire);
    RegisterHyjalAI(boss_archimonde);
    RegisterHyjalAI(npc_ancient_wisp);
    RegisterHyjalAI(npc_doomfire_spirit);
}
