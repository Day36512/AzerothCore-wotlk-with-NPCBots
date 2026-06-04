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
#include "GameObject.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"
#include "SpellMgr.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMe(Creature* bot, uint32 spellId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Texts
{
    SAY_AGGRO              = 0,
    SAY_FLAMEWREATH        = 1,
    SAY_BLIZZARD           = 2,
    SAY_EXPLOSION          = 3,
    SAY_DRINK              = 4,
    SAY_ELEMENTALS         = 5,
    SAY_KILL               = 6,
    SAY_TIMEOVER           = 7,
    SAY_DEATH              = 8,
    SAY_ATIESH             = 9,
    EMOTE_ARCANE_EXPLOSION = 10
};

enum Spells
{
    SPELL_FROSTBOLT              = 29954,
    SPELL_FIREBALL               = 29953,
    SPELL_ARCANE_MISSILE         = 29955,
    SPELL_CHAINSOFICE            = 29991,
    SPELL_DRAGONSBREATH          = 29964,
    SPELL_MASSSLOW               = 30035,
    SPELL_FLAME_WREATH           = 30004,
    SPELL_FLAME_WREATH_RING      = 29946,
    SPELL_FLAME_WREATH_RAN_THRU  = 29947, // You ran through the flames!
    SPELL_FLAME_WREATH_EXPLOSION = 29949,
    SPELL_AOE_CS                 = 29961,
    SPELL_PLAYERPULL             = 32265,
    SPELL_AEXPLOSION             = 29973,
    SPELL_MASS_POLY              = 29963,
    SPELL_BLINK_CENTER           = 29967,
    SPELL_CONJURE                = 29975,
    SPELL_DRINK                  = 30024,
    SPELL_POTION                 = 32453,
    SPELL_AOE_PYROBLAST          = 29978,

    SPELL_SUMMON_WELEMENTAL_1    = 29962,
    SPELL_SUMMON_WELEMENTAL_2    = 37051,
    SPELL_SUMMON_WELEMENTAL_3    = 37052,
    SPELL_SUMMON_WELEMENTAL_4    = 37053,

    SPELL_SUMMON_BLIZZARD        = 29969, // Activates the Blizzard NPC

    SPELL_SHADOW_PYRO            = 29978,

    SPELL_ATIESH_VISUAL          = 31796,

    SPELL_CURSE_OF_TONGUE_RANK1  = 1714,
    SPELL_CURSE_OF_TONGUE_RANK2  = 11719,
    SPELL_MIND_NUMBING_POISON    = 5760
};

enum Creatures
{
    NPC_ARAN_WATER_ELEMENTAL     = 17167,
    NPC_SHADOW_OF_ARAN           = 18254
};

enum SuperSpell
{
    SUPER_FLAME                  = 0,
    SUPER_BLIZZARD,
    SUPER_AE,
};

enum Groups
{
    GROUP_DRINKING               = 0,
    GROUP_BOT_DIRECTOR           = 1,
    GROUP_ARCANE_EXPLOSION_BOT_ESCAPE = 2,
    GROUP_FLAME_WREATH_BOT_LOCK  = 3
};

enum Misc
{
    ACTION_ATIESH_REACT          = 1,
    ACTION_ARCANE_EXPLOSION_FINISHED = 2
};

Position const roomCenter = {-11158.f, -1920.f};

static constexpr uint8 ARCANE_EXPLOSION_BOT_FAILSAFE_SECONDS = 12;
static constexpr uint8 ARCANE_EXPLOSION_BOT_START_NUDGES = 4;
static constexpr uint16 ARCANE_EXPLOSION_BOT_START_NUDGE_INTERVAL_MS = 250;
static constexpr uint8 FLAME_WREATH_CAST_SECONDS = 5;
static constexpr uint8 FLAME_WREATH_GROUND_SECONDS = 20;
static constexpr uint8 FLAME_WREATH_RELEASE_PAD_SECONDS = 1;
static constexpr uint8 FLAME_WREATH_BOT_HOLD_SECONDS = FLAME_WREATH_CAST_SECONDS + FLAME_WREATH_GROUND_SECONDS + FLAME_WREATH_RELEASE_PAD_SECONDS;

static std::array<Position, 7> const ArcaneExplosionBotSafeSpots =
{
    Position(-11138.65f, -1915.54f, 232.01f, 0.0f),
    Position(-11149.38f, -1933.10f, 232.01f, 0.0f),
    Position(-11168.77f, -1938.54f, 232.01f, 0.0f),
    Position(-11185.18f, -1927.98f, 232.01f, 0.0f),
    Position(-11190.73f, -1908.57f, 232.01f, 0.0f),
    Position(-11163.47f, -1886.92f, 232.01f, 0.0f),
    Position(-11144.53f, -1895.66f, 232.01f, 0.0f)
};

std::vector<uint32> immuneSpells = { SPELL_CURSE_OF_TONGUE_RANK1, SPELL_CURSE_OF_TONGUE_RANK2, SPELL_MIND_NUMBING_POISON };

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

static void ForceBotMoveTo(Creature* bot, Position const& dest)
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
    {
        ai->CancelAllActions();
        ai->SetBotCommandState(BOT_COMMAND_STAY);
        bot->BotStopMovement();
        bot->GetMotionMaster()->Clear();
        ai->BotMovement(BOT_MOVE_POINT, &p, nullptr, false);
    }
}

struct boss_shade_of_aran : public BossAI
{
    boss_shade_of_aran(Creature* creature) : BossAI(creature, DATA_ARAN), _atieshReaction(false) { }

    enum BotStateReason : uint8
    {
        BOT_STATE_POSITION = 0x01,
        BOT_STATE_ARCANE   = 0x02,
        BOT_STATE_DRINK    = 0x04,
        BOT_STATE_FLAME    = 0x08
    };

    struct ScriptedBotState
    {
        uint32 originalCommandState = 0;
        uint8 reasons = 0;
        bool staySet = false;
        bool inactionSet = false;
    };

    void Reset() override
    {
        ReleaseAllBotStates();
        BossAI::Reset();
        // Reset the mana of the boss fully before resetting drinking
        // If this was omitted, the boss would start drinking on reset if the mana was low on a wipe
        me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA));
        _drinkScheduler.CancelAll();

        _lastSuperSpell = 0;
        _currentNormalSpell = 0;

        _drinking = false;
        _hasDrunk = false;
        _arcaneExplosionActive = false;
        _arcaneExplosionBots.clear();
        _flameWreathActive = false;

        for (auto spell : immuneSpells)
            me->ApplySpellImmune(0, IMMUNITY_ID, spell, true);

        if (GameObject* libraryDoor = instance->instance->GetGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR)))
        {
            libraryDoor->SetGoState(GO_STATE_ACTIVE);
            libraryDoor->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
        }

        ScheduleHealthCheckEvent(40, [&]{
            Talk(SAY_ELEMENTALS);

            std::vector<uint32> elementalSpells = { SPELL_SUMMON_WELEMENTAL_1, SPELL_SUMMON_WELEMENTAL_2, SPELL_SUMMON_WELEMENTAL_3, SPELL_SUMMON_WELEMENTAL_4 };

            for (auto const& spell : elementalSpells)
            {
                DoCastAOE(spell, true);
            }

            scheduler.Schedule(2s, GROUP_BOT_DIRECTOR, [this](TaskContext)
            {
                FocusWaterElementals();
            });
        });
    }

    bool CheckAranInRoom()
    {
        return me->GetDistance2d(roomCenter.GetPositionX(), roomCenter.GetPositionY()) < 45.0f;
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == ACTION_ATIESH_REACT && !_atieshReaction)
        {
            Talk(SAY_ATIESH);
            _atieshReaction = true;
            if (Unit* atieshOwner = ObjectAccessor::GetUnit(*me, guid))
            {
                me->PauseMovement(3000);
                me->SetFacingToObject(atieshOwner);
            }
        }
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_ARCANE_EXPLOSION_FINISHED)
            FinishArcaneExplosionBotEscape();
    }

    void AttackStart(Unit* who) override
    {
        if (who && who->isTargetableForAttack() && me->GetReactState() != REACT_PASSIVE)
        {
            if (me->Attack(who, false))
            {
                me->GetMotionMaster()->MoveChase(who, 45.0f, 0);
                me->AddThreat(who, 0.0f);
            }
        }
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        ReleaseAllBotStates();
        Talk(SAY_DEATH);
        _JustDied();

        if (GameObject* libraryDoor = instance->instance->GetGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR)))
        {
            libraryDoor->SetGoState(GO_STATE_ACTIVE);
            libraryDoor->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
        }
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ReleaseAllBotStates();
        BossAI::EnterEvadeMode(why);
    }

    void DamageTaken(Unit* doneBy, uint32& damage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask) override
    {
        BossAI::DamageTaken(doneBy, damage, damagetype, damageSchoolMask);

        if ((damagetype == DIRECT_DAMAGE || damagetype == SPELL_DIRECT_DAMAGE) && _drinking && me->GetReactState() == REACT_PASSIVE)
        {
            me->RemoveAurasDueToSpell(SPELL_DRINK);
            me->SetStandState(UNIT_STAND_STATE_STAND);
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA) - 32000);
            _drinkScheduler.CancelGroup(GROUP_DRINKING);
            _drinkScheduler.Schedule(1s, [this](TaskContext)
            {
                DoCastSelf(SPELL_AOE_PYROBLAST, false);
                _drinking = false;
                ReleaseBotsAfterDrink();
            });
        }
    }

    void OnPowerUpdate(Powers /*power*/, int32 /*gain*/, int32 /*updateVal*/, uint32 currentPower) override
    {
        // Should drink at 10%, need 10% mana for mass polymorph
        if (!_hasDrunk && me->GetMaxPower(POWER_MANA) && (currentPower * 100 / me->GetMaxPower(POWER_MANA)) < 13.5)
        {
            _hasDrunk = true;
            me->SetReactState(REACT_PASSIVE);

            // Start drinking after conjuring drinks
            _drinkScheduler.Schedule(0s, GROUP_DRINKING, [this](TaskContext)
            {
                me->InterruptNonMeleeSpells(true);
                me->RemoveAurasDueToSpell(SPELL_ARCANE_MISSILE);
                Talk(SAY_DRINK);
                DoCastAOE(SPELL_MASS_POLY);
                // If we set drinking earlier it will break when someone attacks aran while casting poly
                _drinking = true;
                StopBotsDuringDrink();
            }).Schedule(3s, GROUP_DRINKING, [this](TaskContext)
            {
                DoCastSelf(SPELL_CONJURE);
            }).Schedule(6s, GROUP_DRINKING, [this](TaskContext)
            {
                me->SetStandState(UNIT_STAND_STATE_SIT);
                DoCastSelf(SPELL_DRINK);
            }).Schedule(12s, GROUP_DRINKING, [this](TaskContext)
            {
                me->SetStandState(UNIT_STAND_STATE_STAND);
                me->SetReactState(REACT_AGGRESSIVE);
                me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA) - 32000);
                DoCastSelf(SPELL_AOE_PYROBLAST);
                _drinkScheduler.CancelGroup(GROUP_DRINKING);
                _drinking = false;
                ReleaseBotsAfterDrink();
            });
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        //handle timed closing door
        scheduler.Schedule(15s, [this](TaskContext)
        {
            if (GameObject* libraryDoor = instance->instance->GetGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR)))
            {
                libraryDoor->SetGoState(GO_STATE_READY);
                libraryDoor->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
            }
        }).Schedule(1s, [this](TaskContext context)
        {
            if (!_drinking)
            {
                if (me->IsNonMeleeSpellCast(false))
                {
                    return;
                }

                std::list<uint32> normalSpells = { SPELL_ARCANE_MISSILE, SPELL_FIREBALL, SPELL_FROSTBOLT };
                normalSpells.remove_if([&](uint32 spell) -> bool { return !me->CanCastSpell(spell); });

                if (!normalSpells.empty())
                {
                    // If we are able to cast spells, cast them.
                    _currentNormalSpell = Acore::Containers::SelectRandomContainerElement(normalSpells);

                    if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                        DoCast(target, _currentNormalSpell);

                    if (me->GetVictim())
                    {
                        me->GetMotionMaster()->MoveChase(me->GetVictim(), 45.0f);
                    }
                }
                else
                {
                    // Otherwise, chase in melee range for auto attacks (and drink mana potion, if needed).
                    me->SetWalk(false);
                    me->ResumeChasingVictim();

                    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_currentNormalSpell))
                    {
                        if (int32(me->GetPower(POWER_MANA)) < spellInfo->CalcPowerCost(me, (SpellSchoolMask)spellInfo->SchoolMask))
                        {
                            DoCastSelf(SPELL_POTION);
                        }
                    }
                }
            }
            context.Repeat(2s);
        }).Schedule(5s, [this](TaskContext context)
        {
            if (!_drinking)
            {
                if (urand(0, 1))
                    DoCastSelf(SPELL_AOE_CS);
                else if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                    DoCast(target, SPELL_CHAINSOFICE);
            }
            context.Repeat(5s, 20s);
        }).Schedule(6s, [this](TaskContext context)
        {
            if (!_drinking)
            {
                me->ClearProhibitedSpellTimers();

                DoCastSelf(SPELL_BLINK_CENTER, true);

                std::vector<uint32> superSpells = { SPELL_SUMMON_BLIZZARD, SPELL_AEXPLOSION, SPELL_FLAME_WREATH };

                // Workaround for SelectRandomContainerElementIf
                std::vector<uint32> allowedSpells;
                std::copy_if(superSpells.begin(), superSpells.end(), std::back_inserter(allowedSpells), [&](uint32 superSpell) -> bool { return superSpell != _lastSuperSpell; });
                _lastSuperSpell = allowedSpells[urand(0, allowedSpells.size() - 1)];

                //  SelectRandomContainerElementIf produces unexpected output. Reintroduce when issue is resolved:
                //  Sample results:
                //       Selected Super Spell: 3722304989
                //       superSpells elements : 29969 29973 30004
                //  _lastSuperSpell = Acore::Containers::SelectRandomContainerElementIf(superSpells, [&](uint32 superSpell) -> bool { return superSpell != _lastSuperSpell; });

                me->InterruptNonMeleeSpells(true); // Super spell should have prio over normal spells

                switch (_lastSuperSpell)
                {
                    case SPELL_AEXPLOSION:
                        Talk(SAY_EXPLOSION);
                        Talk(EMOTE_ARCANE_EXPLOSION);
                        DoCastSelf(SPELL_PLAYERPULL, true);
                        DoCastSelf(SPELL_MASSSLOW, true);
                        if (DoCastAOE(SPELL_AEXPLOSION) == SPELL_CAST_OK)
                            ScheduleArcaneExplosionBotEscape();
                        else
                            FinishArcaneExplosionBotEscape();
                        break;
                    case SPELL_FLAME_WREATH:
                        Talk(SAY_FLAMEWREATH);
                        ScheduleFlameWreathBotLock();
                        if (DoCastAOE(SPELL_FLAME_WREATH) != SPELL_CAST_OK)
                            StopFlameWreathBotLock();
                        break;
                    case SPELL_SUMMON_BLIZZARD:
                        Talk(SAY_BLIZZARD);
                        DoCastAOE(SPELL_SUMMON_BLIZZARD);
                        break;
                }
            }
            context.Repeat(35s, 40s);
        }).Schedule(3s, GROUP_BOT_DIRECTOR, [this](TaskContext context)
        {
            if (!_drinking)
            {
                PositionBotsForAran();
                FocusWaterElementals();
            }

            context.Repeat(3s);
        }).Schedule(12min, [this](TaskContext context)
        {
            for (uint32 i = 0; i < 5; ++i)
            {
                if (Creature* unit = me->SummonCreature(NPC_SHADOW_OF_ARAN, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000))
                {
                    unit->Attack(me->GetVictim(), true);
                    unit->SetFaction(me->GetFaction());
                }
            }

            Talk(SAY_TIMEOVER);

            context.Repeat(1min);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        _drinkScheduler.Update(diff);

        if (!UpdateVictim())
            return;

        if (!CheckAranInRoom())
        {
            EnterEvadeMode(EVADE_REASON_OTHER);
            return;
        }

        if (!_drinking)
            DoMeleeAttackIfReady();
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

    std::vector<Creature*> GatherEncounterBots() const
    {
        std::vector<Creature*> bots;
        for (Unit* unit : GatherEncounterUnits())
        {
            if (Creature* bot = ToNPCBot(unit))
                bots.push_back(bot);
        }

        return bots;
    }

    Unit* SelectRandomEncounterTarget(float range, bool includeVictim)
    {
        std::vector<Unit*> candidates;
        Unit* victim = me->GetVictim();

        for (Unit* unit : GatherEncounterUnits())
        {
            if (!unit || !unit->IsAlive() || !me->IsWithinDistInMap(unit, range))
                continue;

            if (!includeVictim && unit == victim)
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

    bool IsManaUser(Unit* unit) const
    {
        return unit && unit->getPowerType() == POWER_MANA && unit->GetMaxPower(POWER_MANA) > 0;
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

    bool IsNpcBotRangedOrCaster(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        return ai && (ai->HasRole(BOT_ROLE_RANGED) || IsManaUser(bot));
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
        if (!bot || !IsEncounterParticipant(bot) || bot->HasAura(SPELL_FLAME_WREATH_RING))
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

    void TrackBotState(Creature* bot, uint8 reason, bool setInaction)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!ai)
            return;

        ScriptedBotState& state = _botStates[bot->GetGUID()];
        if (!state.reasons)
        {
            state.originalCommandState = ai->GetBotCommandState();
            state.staySet = !(state.originalCommandState & BOT_COMMAND_STAY);
            state.inactionSet = false;
        }

        state.reasons |= reason;

        if (setInaction && !(state.originalCommandState & BOT_COMMAND_INACTION))
        {
            ai->SetBotCommandState(BOT_COMMAND_INACTION);
            state.inactionSet = true;
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

                if (itr->second.inactionSet && !(itr->second.originalCommandState & BOT_COMMAND_INACTION))
                    ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

                if (resumeCombat && bot->IsAlive())
                {
                    if (!_drinking && me->IsAlive() && me->IsEngaged() && me->GetVictim() && bot->IsInMap(me))
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

    void SetEncounterBotsToFollow(bool interruptCasts = false)
    {
        for (Creature* bot : GatherEncounterBots())
        {
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (bot && bot->IsAlive() && ai && !ai->IAmFree())
            {
                if (interruptCasts)
                {
                    bot->AttackStop();
                    bot->InterruptNonMeleeSpells(true);
                    ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                    ai->RemoveBotCommandState(BOT_COMMAND_INACTION);
                    ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);
                }

                bot->BotStopMovement();
                bot->GetMotionMaster()->Clear();
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
            }
        }
    }

    void ReleaseArcaneExplosionBotStates()
    {
        for (ObjectGuid const& guid : _arcaneExplosionBots)
        {
            Creature* bot = ObjectAccessor::GetCreature(*me, guid);
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (!bot || !bot->IsAlive() || !ai || ai->IAmFree())
                continue;

            bot->AttackStop();
            bot->InterruptNonMeleeSpells(true);
            bot->BotStopMovement();
            bot->GetMotionMaster()->Clear();

            ai->RemoveBotCommandState(BOT_COMMAND_STAY);
            ai->RemoveBotCommandState(BOT_COMMAND_INACTION);
            ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }

        _arcaneExplosionBots.clear();

        // Belt and suspenders: catch any encounter bot that was moved by the Arcane Explosion helper
        // but was missed because of despawn/resummon timing or command-state weirdness.
        SetEncounterBotsToFollow(true);
    }

    void ReleaseFlameWreathBotStates()
    {
        std::vector<ObjectGuid> guids;
        guids.reserve(_botStates.size());

        for (std::map<ObjectGuid, ScriptedBotState>::value_type const& pair : _botStates)
            if (pair.second.reasons & BOT_STATE_FLAME)
                guids.push_back(pair.first);

        for (ObjectGuid const& guid : guids)
            ReleaseBotState(guid, BOT_STATE_FLAME, false);

        SetEncounterBotsToFollow();
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

                    if (itr->second.inactionSet && !(itr->second.originalCommandState & BOT_COMMAND_INACTION))
                        ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

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
        _arcaneExplosionBots.clear();
        scheduler.CancelGroup(GROUP_ARCANE_EXPLOSION_BOT_ESCAPE);
        _arcaneExplosionActive = false;
        _flameWreathActive = false;
    }

    void ReleaseBotsAfterDrink()
    {
        ReleaseBotStatesByReason(BOT_STATE_DRINK, true);
    }

    Position BuildRoomRingPosition(float radius, size_t index, size_t count, float angleOffset) const
    {
        static constexpr float TwoPi = 6.28318530f;
        float angle = angleOffset + (count ? TwoPi * float(index) / float(count) : 0.0f);
        float x = roomCenter.GetPositionX() + std::cos(angle) * radius;
        float y = roomCenter.GetPositionY() + std::sin(angle) * radius;
        float z = me->GetPositionZ();
        float orientation = Position::NormalizeOrientation(std::atan2(me->GetPositionY() - y, me->GetPositionX() - x));

        Position position;
        position.Relocate(x, y, z, orientation);
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
            scheduler.Schedule(2500ms, GROUP_BOT_DIRECTOR, [this, guid, reason](TaskContext)
            {
                ReleaseBotState(guid, reason, true);
            });
        }

        return true;
    }

    void PositionBotsForAran()
    {
        if (_drinking || _arcaneExplosionActive || _flameWreathActive)
            return;

        std::vector<Creature*> rangedBots;

        for (Creature* bot : GatherEncounterBots())
        {
            if (!bot || bot == me->GetVictim() || IsNpcBotTank(bot))
                continue;

            if (!IsNpcBotHealer(bot) && !IsNpcBotRangedOrCaster(bot))
                continue;

            if (!CanMoveBot(bot, false))
                continue;

            if (bot->GetDistance(me) >= 12.0f && bot->GetExactDist2d(roomCenter.GetPositionX(), roomCenter.GetPositionY()) <= 34.0f)
                continue;

            rangedBots.push_back(bot);
        }

        std::sort(rangedBots.begin(), rangedBots.end(), [](Creature* left, Creature* right)
        {
            return left->GetGUID() < right->GetGUID();
        });

        for (size_t i = 0; i < rangedBots.size(); ++i)
        {
            Position position = BuildRoomRingPosition(28.0f, i, rangedBots.size(), me->GetOrientation() + 0.85f);
            MoveBotIfNeeded(rangedBots[i], position, 2.5f, BOT_STATE_POSITION, false, true);
        }
    }

    Position GetArcaneExplosionEscapePosition(Creature* bot) const
    {
        size_t index = bot ? bot->GetGUID().GetCounter() % ArcaneExplosionBotSafeSpots.size() : 0;
        Position position = ArcaneExplosionBotSafeSpots[index];
        float x = position.GetPositionX();
        float y = position.GetPositionY();
        float orientation = Position::NormalizeOrientation(std::atan2(me->GetPositionY() - y, me->GetPositionX() - x));
        position.SetOrientation(orientation);
        return position;
    }

    bool IsArcaneExplosionBot(Creature* bot) const
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        return bot && bot->IsAlive() && IsEncounterParticipant(bot) && !bot->HasAura(SPELL_FLAME_WREATH_RING) && ai && !ai->IAmFree();
    }

    void TeleportBotsToArcaneExplosionCenter()
    {
        for (Creature* bot : GatherEncounterBots())
        {
            if (!IsArcaneExplosionBot(bot))
                continue;

            float x = roomCenter.GetPositionX();
            float y = roomCenter.GetPositionY();
            float z = me->GetPositionZ();

            if (!bot->CanFly())
                bot->UpdateAllowedPositionZ(x, y, z);

            if (bot_ai* ai = bot->GetBotAI())
                ai->CancelAllActions();

            bot->InterruptNonMeleeSpells(true);
            bot->AttackStop();
            bot->BotStopMovement();
            bot->GetMotionMaster()->Clear();
            bot->NearTeleportTo(x, y, z, me->GetOrientation());
        }
    }

    void MoveBotOutForArcaneExplosion(Creature* bot)
    {
        if (!IsArcaneExplosionBot(bot))
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        _arcaneExplosionBots.insert(bot->GetGUID());

        ai->CancelAllActions();
        bot->InterruptNonMeleeSpells(true);
        bot->AttackStop();
        bot->BotStopMovement();
        bot->GetMotionMaster()->Clear();
        ai->SetBotCommandState(BOT_COMMAND_STAY);
        ai->SetBotCommandState(BOT_COMMAND_INACTION);

        ForceBotMoveTo(bot, GetArcaneExplosionEscapePosition(bot));
    }

    void MoveBotsOutForArcaneExplosion()
    {
        if (!_arcaneExplosionActive)
            return;

        for (Creature* bot : GatherEncounterBots())
            MoveBotOutForArcaneExplosion(bot);
    }

    void ScheduleArcaneExplosionBotEscape()
    {
        scheduler.CancelGroup(GROUP_ARCANE_EXPLOSION_BOT_ESCAPE);
        _arcaneExplosionActive = true;
        _arcaneExplosionBots.clear();

        // Aran's pull hits first. NPCBots are forced to the center too, then immediately sent out
        // once the 10 second Arcane Explosion cast has actually started.
        TeleportBotsToArcaneExplosionCenter();
        MoveBotsOutForArcaneExplosion();

        // Four quick nudges keep the point movement from being eaten by cast recovery, slow aura timing,
        // or bot AI deciding to be clever at exactly the wrong moment.
        for (uint8 i = 1; i <= ARCANE_EXPLOSION_BOT_START_NUDGES; ++i)
        {
            scheduler.Schedule(std::chrono::milliseconds(i * ARCANE_EXPLOSION_BOT_START_NUDGE_INTERVAL_MS), GROUP_ARCANE_EXPLOSION_BOT_ESCAPE, [this](TaskContext)
            {
                MoveBotsOutForArcaneExplosion();
            });
        }

        // Backup release only. The real release is spell_shade_of_aran_arcane_explosion::HandleAfterCast(),
        // which fires when spell 29973 finishes its cast.
        scheduler.Schedule(std::chrono::seconds(ARCANE_EXPLOSION_BOT_FAILSAFE_SECONDS), GROUP_ARCANE_EXPLOSION_BOT_ESCAPE, [this](TaskContext)
        {
            FinishArcaneExplosionBotEscape();
        });
    }

    void FinishArcaneExplosionBotEscape()
    {
        if (!_arcaneExplosionActive)
            return;

        _arcaneExplosionActive = false;
        scheduler.CancelGroup(GROUP_ARCANE_EXPLOSION_BOT_ESCAPE);
        ReleaseArcaneExplosionBotStates();
    }

    void LockBotsForFlameWreath()
    {
        for (Creature* bot : GatherEncounterBots())
        {
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (!bot || !bot->IsAlive() || !ai || ai->IAmFree())
                continue;

            TrackBotState(bot, BOT_STATE_FLAME, false);
            bot->BotStopMovement();
            bot->GetMotionMaster()->Clear();

            if (!ai->HasBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP))
                ai->SetBotCommandState(BOT_COMMAND_STAY);
        }
    }

    void ScheduleFlameWreathBotLock()
    {
        _flameWreathActive = true;
        scheduler.CancelGroup(GROUP_FLAME_WREATH_BOT_LOCK);
        LockBotsForFlameWreath();

        scheduler.Schedule(std::chrono::seconds(FLAME_WREATH_BOT_HOLD_SECONDS), GROUP_FLAME_WREATH_BOT_LOCK, [this](TaskContext)
        {
            StopFlameWreathBotLock();
        });
    }

    void StopFlameWreathBotLock()
    {
        _flameWreathActive = false;
        ReleaseFlameWreathBotStates();
        scheduler.CancelGroup(GROUP_FLAME_WREATH_BOT_LOCK);
    }

    Creature* FindWaterElementalTarget() const
    {
        std::list<Creature*> elementalList;
        me->GetCreatureListWithEntryInGrid(elementalList, NPC_ARAN_WATER_ELEMENTAL, 90.0f);

        Creature* best = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        for (Creature* elemental : elementalList)
        {
            if (!elemental || !elemental->IsAlive() || elemental->GetMap() != me->GetMap())
                continue;

            float distance = elemental->GetDistance(me);
            if (distance < bestDistance)
            {
                best = elemental;
                bestDistance = distance;
            }
        }

        return best;
    }

    void FocusWaterElementals()
    {
        if (_drinking || _arcaneExplosionActive || _flameWreathActive)
            return;

        Creature* elemental = FindWaterElementalTarget();
        if (!elemental)
            return;

        for (Creature* bot : GatherEncounterBots())
        {
            if (!bot || bot->HasAura(SPELL_FLAME_WREATH_RING) || IsNpcBotHealer(bot))
                continue;

            if (!CanMoveBot(bot, false))
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai)
                continue;

            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(elemental);
        }
    }

    void StopBotsDuringDrink()
    {
        for (Creature* bot : GatherEncounterBots())
        {
            if (!bot || bot->HasAura(SPELL_FLAME_WREATH_RING))
                continue;

            if (IsNpcBotTank(bot) || IsNpcBotHealer(bot))
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai || ai->IAmFree())
                continue;

            TrackBotState(bot, BOT_STATE_DRINK, true);
            bot->AttackStop();
            bot->InterruptNonMeleeSpells(false);
            bot->BotStopMovement();
        }
    }

private:
    TaskScheduler _drinkScheduler;

    std::map<ObjectGuid, ScriptedBotState> _botStates;
    std::set<ObjectGuid> _arcaneExplosionBots;

    uint32 _currentNormalSpell;
    uint32 _lastSuperSpell;

    bool _drinking;
    bool _hasDrunk;
    bool _atieshReaction;
    bool _arcaneExplosionActive = false;
    bool _flameWreathActive = false;
};

// 29973 - Arcane Explosion
class spell_shade_of_aran_arcane_explosion : public SpellScript
{
    PrepareSpellScript(spell_shade_of_aran_arcane_explosion);

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        Creature* creature = caster ? caster->ToCreature() : nullptr;
        if (!creature || !creature->AI())
            return;

        creature->AI()->DoAction(ACTION_ARCANE_EXPLOSION_FINISHED);
    }

private:
    void Register() override
    {
        AfterCast += SpellCastFn(spell_shade_of_aran_arcane_explosion::HandleAfterCast);
    }
};

// 30004 - Flame Wreath
class spell_flamewreath : public SpellScript
{
    PrepareSpellScript(spell_flamewreath);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_FLAME_WREATH_RING });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        uint8 maxSize = 3;

        if (targets.size() > maxSize)
        {
            Acore::Containers::RandomResize(targets, maxSize);
        }

        _targets = targets;
    }

    void HandleFinish()
    {
        for (auto const& target : _targets)
        {
            if (Unit* targetUnit = target->ToUnit())
            {
                GetCaster()->CastSpell(targetUnit, SPELL_FLAME_WREATH_RING, true);
            }
        }
    }

private:
    std::list<WorldObject*> _targets;

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_flamewreath::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
        AfterCast += SpellCastFn(spell_flamewreath::HandleFinish);
    }
};

// 29946 - Flame Wreath (visual effect)
class spell_flamewreath_aura : public AuraScript
{
    PrepareAuraScript(spell_flamewreath_aura);

public:
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_FLAME_WREATH_RAN_THRU, SPELL_FLAME_WREATH_EXPLOSION });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(GetTarget()))
            DBMFTABotCallouts::AnnounceMoveAwayFromMe(bot, SPELL_FLAME_WREATH_RING, "Flame Wreath", DBMFTABotCallouts::GetCooldownMs());
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEFAULT && GetDuration())
        {
            if (target)
            {
                if (target->IsPlayer() || IsNPCBotUnit(target))
                {
                    target->CastSpell(target, SPELL_FLAME_WREATH_RAN_THRU, true);

                    target->m_Events.AddEventAtOffset([target] {
                        target->RemoveAurasDueToSpell(SPELL_FLAME_WREATH_RAN_THRU);
                        target->CastSpell(target, SPELL_FLAME_WREATH_EXPLOSION, true);
                    }, 1s);
                }
            }
        }
    }

private:
    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_flamewreath_aura::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_flamewreath_aura::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

class at_karazhan_atiesh_aran : public AreaTriggerScript
{
public:
    at_karazhan_atiesh_aran() : AreaTriggerScript("at_karazhan_atiesh_aran") { }

    bool OnTrigger(Player* player, AreaTrigger const* /*areaTrigger*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
        {
            if (player->HasAura(SPELL_ATIESH_VISUAL))
            {
                if (Creature* aran = instance->GetCreature(DATA_ARAN))
                {
                    aran->AI()->SetGUID(player->GetGUID(), ACTION_ATIESH_REACT);
                }
            }
        }

        return true;
    }
};

void AddSC_boss_shade_of_aran()
{
    RegisterKarazhanCreatureAI(boss_shade_of_aran);
    RegisterSpellScript(spell_shade_of_aran_arcane_explosion);
    RegisterSpellScript(spell_flamewreath);
    RegisterSpellScript(spell_flamewreath_aura);
    new at_karazhan_atiesh_aran();
}
