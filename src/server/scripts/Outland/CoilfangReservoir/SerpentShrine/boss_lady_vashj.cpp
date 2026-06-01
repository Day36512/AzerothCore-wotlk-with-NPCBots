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

#include "Config.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScriptLoader.h"
#include "WorldSession.h"
#include "serpent_shrine.h"
#include "SpellScript.h"
#include "ThreatManager.h"
#include "bot_ai.h"
#include "botmgr.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

enum Says
{
    SAY_INTRO                       = 0,
    SAY_AGGRO                       = 1,
    SAY_PHASE1                      = 2,
    SAY_PHASE2                      = 3,
    SAY_PHASE3                      = 4,
    SAY_BOWSHOT                     = 5,
    SAY_SLAY                        = 6,
    SAY_DEATH                       = 7
};

enum Spells
{
    SPELL_SHOOT                     = 37770,
    SPELL_MULTI_SHOT                = 38310,
    SPELL_SHOCK_BLAST               = 38509,
    SPELL_STATIC_CHARGE             = 38280,
    SPELL_ENTANGLE                  = 38316,
    SPELL_MAGIC_BARRIER             = 38112,
    SPELL_FORKED_LIGHTNING          = 38145,

    SPELL_SUMMON_ENCHANTED_ELEMENTAL = 38017,
    SPELL_SUMMON_COILFANG_ELITE     = 38248,
    SPELL_SUMMON_COILFANG_STRIDER   = 38241,
    SPELL_SUMMON_TAINTED_ELEMENTAL  = 38140,
    SPELL_SURGE                     = 38044,

    SPELL_REMOVE_TAINTED_CORES      = 39495,
    SPELL_SUMMON_TOXIC_SPOREBAT     = 38494,
    SPELL_SUMMON_SPOREBAT1          = 38489,
    SPELL_SUMMON_SPOREBAT2          = 38490,
    SPELL_SUMMON_SPOREBAT3          = 38492,
    SPELL_SUMMON_SPOREBAT4          = 38493,
    SPELL_TOXIC_SPORES              = 38574
};

enum Misc
{
    ITEM_TAINTED_CORE               = 31088,
    POINT_HOME                      = 1,
    NPC_TRIGGER                     = 15384
};

namespace
{
    constexpr char const* CONFIG_TAINTED_ELEMENTAL_USE_OVERRIDE_SPAWNS = "SerpentShrine.LadyVashj.TaintedElemental.UseOverrideSpawnPositions";
    constexpr char const* CONFIG_TAINTED_ELEMENTAL_OVERRIDE_SPAWNS = "SerpentShrine.LadyVashj.TaintedElemental.OverrideSpawnPositions";
    constexpr char const* DEFAULT_TAINTED_ELEMENTAL_OVERRIDE_SPAWNS =
        "3.56 -966.54 41.17; "
        "29.70 -972.68 41.19; "
        "52.66 -966.17 41.20; "
        "73.83 -950.03 41.15; "
        "79.93 -929.85 41.13; "
        "67.11 -890.00 41.15; "
        "28.62 -871.18 41.14; "
        "3.59 -880.78 41.18; "
        "-14.78 -901.12 41.17; "
        "-21.61 -922.90 41.16; "
        "-14.49 -947.85 41.17";

    constexpr float VASHJ_STATIC_CHARGE_SPREAD_MIN_RADIUS = 22.0f;
    constexpr float VASHJ_STATIC_CHARGE_SPREAD_MAX_RADIUS = 36.0f;
    constexpr float VASHJ_STATIC_CHARGE_SPREAD_STEP = 4.0f;
    constexpr float VASHJ_STATIC_CHARGE_SAFE_SCAN_RANGE = 95.0f;
    constexpr float VASHJ_STATIC_CHARGE_SAFE_MOVE_MAX_DIST = 80.0f;
    constexpr float VASHJ_PI = 3.14159265358979323846f;

    std::vector<Position> ParseVashjTaintedElementalOverrideSpawns(std::string spawns)
    {
        for (char& ch : spawns)
            if (ch == ',' || ch == ';' || ch == '|')
                ch = ' ';

        std::vector<Position> positions;
        std::stringstream stream(spawns);
        float x;
        float y;
        float z;

        while (stream >> x >> y >> z)
            positions.emplace_back(x, y, z, 0.0f);

        return positions;
    }

    std::vector<Position> GetVashjTaintedElementalOverrideSpawns()
    {
        return ParseVashjTaintedElementalOverrideSpawns(sConfigMgr->GetOption<std::string>(
            CONFIG_TAINTED_ELEMENTAL_OVERRIDE_SPAWNS, DEFAULT_TAINTED_ELEMENTAL_OVERRIDE_SPAWNS));
    }

    bool TrySummonVashjTaintedElementalFromOverride(Creature* vashj)
    {
        if (!vashj || !sConfigMgr->GetOption<bool>(CONFIG_TAINTED_ELEMENTAL_USE_OVERRIDE_SPAWNS, false))
            return false;

        std::vector<Position> const positions = GetVashjTaintedElementalOverrideSpawns();
        if (positions.empty())
        {
            LOG_WARN("scripts", "Lady Vashj Tainted Elemental override spawns are enabled, but {} has no valid coordinate triples. Falling back to trigger-based spawns.", CONFIG_TAINTED_ELEMENTAL_OVERRIDE_SPAWNS);
            return false;
        }

        Position const& position = Acore::Containers::SelectRandomContainerElement(positions);
        return vashj->SummonCreature(NPC_TAINTED_ELEMENTAL, position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(), position.GetAngle(vashj), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30 * IN_MILLISECONDS) != nullptr;
    }

    bool IsVashjNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsVashjPlayerMarkedTank(Player* player)
    {
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group)
            return false;

        for (Group::member_citerator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
            if (itr->guid == player->GetGUID())
                return itr->flags & MEMBER_FLAG_MAINTANK;

        return false;
    }

    bool IsVashjTank(Unit* unit, Creature* source)
    {
        if (!unit)
            return false;

        if (source && (unit == source->GetThreatMgr().GetCurrentVictim() || unit == source->GetThreatMgr().GetLastVictim() || unit == source->GetVictim()))
            return true;

        if (IsVashjNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsVashjPlayerMarkedTank(player);

        return false;
    }

    float GetVashjSpellRange(uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        return spellInfo ? spellInfo->GetMaxRange(false) : 0.0f;
    }

    bool IsValidVashjRandomTarget(Creature* source, Unit* unit, float range, bool excludeTanks)
    {
        if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive())
            return false;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return false;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        if (excludeTanks && IsVashjTank(unit, source))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (range > 0.0f && !source->IsWithinDistInMap(unit, range))
            return false;

        if (!source->IsValidAttackTarget(unit) || !source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
            return false;

        return true;
    }

    void AddVashjRandomTarget(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, float range, bool excludeTanks)
    {
        if (!IsValidVashjRandomTarget(source, unit, range, excludeTanks))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        targets.push_back(unit);
    }

    Player* GetVashjOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetVashjGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    void AddVashjOwnedBots(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player, float range, bool excludeTanks)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddVashjRandomTarget(source, targets, seen, pair.second, range, excludeTanks);
    }

    void AddVashjThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, float range, bool excludeTanks)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddVashjRandomTarget(source, targets, seen, ref->GetVictim(), range, excludeTanks);
        }
    }

    std::vector<Unit*> GatherVashjRandomTargets(Creature* source, uint32 spellId, bool excludeTanks)
    {
        std::vector<Unit*> targets;
        GuidSet seen;

        if (!source)
            return targets;

        float const range = GetVashjSpellRange(spellId);
        Unit* seed = source->GetThreatMgr().GetCurrentVictim();
        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetVashjGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddVashjRandomTarget(source, targets, seen, member, range, excludeTanks);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddVashjOwnedBots(source, targets, seen, player, range, excludeTanks);
            }
        }
        else
        {
            AddVashjRandomTarget(source, targets, seen, seed, range, excludeTanks);

            if (Player* player = GetVashjOwnerPlayer(seed))
            {
                AddVashjRandomTarget(source, targets, seen, player, range, excludeTanks);
                AddVashjOwnedBots(source, targets, seen, player, range, excludeTanks);
            }
        }

        AddVashjThreatTargets(source, targets, seen, range, excludeTanks);
        return targets;
    }

    Unit* SelectVashjRandomTarget(Creature* source, uint32 spellId, bool excludeTanks)
    {
        std::vector<Unit*> targets = GatherVashjRandomTargets(source, spellId, excludeTanks);
        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }

    Creature* GetVashjForUnit(Unit* unit)
    {
        if (!unit)
            return nullptr;

        InstanceScript* instance = unit->GetInstanceScript();
        return instance ? instance->GetCreature(DATA_LADY_VASHJ) : nullptr;
    }

    bool IsValidVashjStaticChargeObstacle(Creature* bot, Creature* vashj, Unit* unit)
    {
        if (!bot || !unit || unit == bot || !unit->IsInWorld() || !unit->IsAlive())
            return false;

        if (!unit->IsPlayer() && !unit->IsNPCBot())
            return false;

        if (!unit->IsInMap(bot) || !unit->InSamePhase(bot))
            return false;

        Position const& anchor = vashj ? vashj->GetHomePosition() : bot->GetPosition();
        return unit->GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY()) <= VASHJ_STATIC_CHARGE_SAFE_SCAN_RANGE;
    }

    void AddVashjStaticChargeObstacle(Creature* bot, Creature* vashj, std::vector<Unit*>& units, GuidSet& seen, Unit* unit)
    {
        if (!IsValidVashjStaticChargeObstacle(bot, vashj, unit))
            return;

        if (!seen.insert(unit->GetGUID()).second)
            return;

        units.push_back(unit);
    }

    void AddVashjOwnedStaticChargeBotObstacles(Creature* bot, Creature* vashj, std::vector<Unit*>& units, GuidSet& seen, Player* player)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddVashjStaticChargeObstacle(bot, vashj, units, seen, pair.second);
    }

    std::vector<Unit*> GatherVashjStaticChargeObstacles(Creature* bot)
    {
        std::vector<Unit*> units;
        GuidSet seen;

        Creature* vashj = GetVashjForUnit(bot);
        Group* group = GetVashjGroup(bot);
        if (group)
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddVashjStaticChargeObstacle(bot, vashj, units, seen, member);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddVashjOwnedStaticChargeBotObstacles(bot, vashj, units, seen, player);
            }
        }

        Player* owner = GetVashjOwnerPlayer(bot);
        AddVashjStaticChargeObstacle(bot, vashj, units, seen, owner);
        AddVashjOwnedStaticChargeBotObstacles(bot, vashj, units, seen, owner);

        return units;
    }

    float GetVashjStaticChargePositionScore(Position const& candidate, std::vector<Unit*> const& obstacles)
    {
        if (obstacles.empty())
            return 1000.0f;

        float score = std::numeric_limits<float>::max();
        for (Unit* unit : obstacles)
        {
            if (!unit)
                continue;

            score = std::min(score, unit->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY()));
        }

        return score;
    }

    bool TryGetVashjStaticChargeSafePosition(Creature* bot, Position& destination)
    {
        if (!bot)
            return false;

        Creature* vashj = GetVashjForUnit(bot);
        Position const& anchor = vashj ? vashj->GetHomePosition() : bot->GetPosition();
        std::vector<Unit*> obstacles = GatherVashjStaticChargeObstacles(bot);

        float bestScore = -1.0f;
        Position bestPosition;
        bool found = false;

        float const baseAngle = std::atan2(bot->GetPositionY() - anchor.GetPositionY(), bot->GetPositionX() - anchor.GetPositionX());
        for (float radius = VASHJ_STATIC_CHARGE_SPREAD_MIN_RADIUS; radius <= VASHJ_STATIC_CHARGE_SPREAD_MAX_RADIUS; radius += VASHJ_STATIC_CHARGE_SPREAD_STEP)
        {
            for (uint8 step = 0; step < 16; ++step)
            {
                float const angle = baseAngle + (2.0f * VASHJ_PI * float(step) / 16.0f);
                float x = anchor.GetPositionX() + std::cos(angle) * radius;
                float y = anchor.GetPositionY() + std::sin(angle) * radius;
                float z = anchor.GetPositionZ();

                if (!bot->CanFly())
                    bot->UpdateAllowedPositionZ(x, y, z);

                if (!bot->IsWithinLOS(x, y, z))
                    continue;

                Position candidate;
                candidate.Relocate(x, y, z, vashj ? bot->GetAngle(vashj) : bot->GetOrientation());

                if (bot->GetExactDist(candidate) > VASHJ_STATIC_CHARGE_SAFE_MOVE_MAX_DIST)
                    continue;

                float score = GetVashjStaticChargePositionScore(candidate, obstacles);
                score -= bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY()) * 0.04f;

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

    void MoveVashjStaticChargeBot(Creature* bot)
    {
        if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return;

        Position destination;
        if (!TryGetVashjStaticChargeSafePosition(bot, destination))
            return;

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        ai->SetBotCommandState(BOT_COMMAND_STAY);
        ai->BotMovement(BOT_MOVE_POINT, &destination, nullptr, true);
    }
}

struct boss_lady_vashj : public BossAI
{
    boss_lady_vashj(Creature* creature) : BossAI(creature, DATA_LADY_VASHJ)
    {
        _intro = false;
    }

    void Reset() override
    {
        _count = 0;
        _recentlySpoken = false;
        _batTimer = 20s;
        _playerAngle = 0.0f;
        BossAI::Reset();

        ScheduleHealthCheckEvent(70, [&]{
            Talk(SAY_PHASE2);
            scheduler.CancelAll();
            me->CastStop();
            me->SetReactState(REACT_PASSIVE);
            me->GetMotionMaster()->MovePoint(POINT_HOME, me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY(), me->GetHomePosition().GetPositionZ(), FORCED_MOVEMENT_NONE, 0.f, true, true);
        });
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (!_recentlySpoken)
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;
        }
        scheduler.Schedule(6s, [this](TaskContext)
        {
            _recentlySpoken = false;
        });
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        DoCastSelf(SPELL_REMOVE_TAINTED_CORES, true);
        ScheduleSpells();
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        switch (summon->GetEntry())
        {
            case WORLD_TRIGGER:
                summon->CastSpell(summon, SPELL_MAGIC_BARRIER);
                break;
            case NPC_ENCHANTED_ELEMENTAL:
                summon->GetMotionMaster()->MoveFollow(me, 0.0f, 0.0f, MOTION_SLOT_ACTIVE, false, false);
                summon->SetWalk(true);
                summon->SetReactState(REACT_PASSIVE);
                break;
            case NPC_TAINTED_ELEMENTAL:
                break;
            case NPC_TOXIC_SPOREBAT:
                summon->GetMotionMaster()->MoveRandom(30.0f);
                break;
              default:
                summon->GetMotionMaster()->MovePoint(POINT_HOME, me->GetPosition());
        }
    }

    void ScheduleSpells()
    {
        scheduler.Schedule(14550ms, [this](TaskContext context)
        {
            DoCastVictim(SPELL_SHOCK_BLAST);
            context.Repeat(10850ms, 25100ms);
        }).Schedule(18150ms, [this](TaskContext context)
        {
            if (Unit* target = SelectVashjRandomTarget(me, SPELL_STATIC_CHARGE, true))
                DoCast(target, SPELL_STATIC_CHARGE);

            context.Repeat(7250ms, 27050ms);
        }).Schedule(25450ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_ENTANGLE);
            context.Repeat(18200ms, 51500ms);
        });
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!_intro && who->IsPlayer())
        {
            _intro = true;
            Talk(SAY_INTRO);
        }

        BossAI::MoveInLineOfSight(who);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_HOME)
            return;

        me->AddUnitState(UNIT_STATE_ROOT);
        me->SetFacingTo(me->GetHomePosition().GetOrientation());
        instance->SetData(DATA_ACTIVATE_SHIELD, 0);
        scheduler.Schedule(2400ms, [this](TaskContext context)
        {
            if (Unit* target = SelectVashjRandomTarget(me, SPELL_FORKED_LIGHTNING, false))
            {
                _playerAngle = me->GetAngle(target);
                me->SetOrientation(_playerAngle);
                DoCast(target, SPELL_FORKED_LIGHTNING);
            }
            context.Repeat(2400ms, 12450ms);
        }).Schedule(0s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SUMMON_ENCHANTED_ELEMENTAL, true);
            context.Repeat(2500ms);
        }).Schedule(45s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SUMMON_COILFANG_ELITE, true);
            context.Repeat(45s);
        }).Schedule(60s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SUMMON_COILFANG_STRIDER, true);
            context.Repeat(60s);
        }).Schedule(50s, [this](TaskContext context)
        {
            if (!TrySummonVashjTaintedElementalFromOverride(me))
                DoCastSelf(SPELL_SUMMON_TAINTED_ELEMENTAL, true);

            context.Repeat(50s);
        }).Schedule(1s, [this](TaskContext context)
        {
            if (!me->HasAura(SPELL_MAGIC_BARRIER))
            {
                Talk(SAY_PHASE3);
                me->ClearUnitState(UNIT_STATE_ROOT);
                me->SetReactState(REACT_AGGRESSIVE);
                me->GetMotionMaster()->MoveChase(me->GetVictim());
                scheduler.CancelAll();

                ScheduleSpells();
                scheduler.Schedule(5s, [this](TaskContext context)
                {
                    DoCastSelf(SPELL_SUMMON_TOXIC_SPOREBAT, true);
                    _batTimer = 20s - static_cast<std::chrono::seconds>(std::min(_count++, 16));
                    context.Repeat(_batTimer);
                });
            }
            else
                context.Repeat(1s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (me->GetReactState() != REACT_AGGRESSIVE || !me->isAttackReady())
            return;

        if (!me->IsWithinMeleeRange(me->GetVictim()))
        {
            me->resetAttackTimer();
            me->SetSheath(SHEATH_STATE_RANGED);
            DoCastVictim(roll_chance_i(33) ? SPELL_MULTI_SHOT : SPELL_SHOOT);
            if (roll_chance_i(15))
                Talk(SAY_BOWSHOT);
        }
        else
        {
            me->SetSheath(SHEATH_STATE_MELEE);
            DoMeleeAttackIfReady();
        }
    }

    bool CheckEvadeIfOutOfCombatArea() const override
    {
        return me->GetHomePosition().GetExactDist2d(me) > 80.0f || !SelectTargetFromPlayerList(100.0f);
    }

private:
    float _playerAngle;
    bool _recentlySpoken;
    bool _intro;
    int32 _count;
    std::chrono::seconds _batTimer;
};

class spell_lady_vashj_magic_barrier : public AuraScript
{
    PrepareAuraScript(spell_lady_vashj_magic_barrier);

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit::DealDamage(GetTarget(), GetTarget(), GetTarget()->CountPctFromMaxHealth(5));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_lady_vashj_magic_barrier::HandleEffectRemove, EFFECT_0, SPELL_AURA_SCHOOL_IMMUNITY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_lady_vashj_remove_tainted_cores : public SpellScript
{
    PrepareSpellScript(spell_lady_vashj_remove_tainted_cores);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Player* target = GetHitPlayer())
            target->DestroyItemCount(ITEM_TAINTED_CORE, -1, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lady_vashj_remove_tainted_cores::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_lady_vashj_summon_sporebat : public SpellScript
{
    PrepareSpellScript(spell_lady_vashj_summon_sporebat);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetCaster()->CastSpell(GetCaster(), RAND(SPELL_SUMMON_SPOREBAT1, SPELL_SUMMON_SPOREBAT2, SPELL_SUMMON_SPOREBAT3, SPELL_SUMMON_SPOREBAT4), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lady_vashj_summon_sporebat::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_lady_vashj_spore_drop_effect : public SpellScript
{
    PrepareSpellScript(spell_lady_vashj_spore_drop_effect);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_TOXIC_SPORES, true, nullptr, nullptr, GetCaster()->GetGUID());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lady_vashj_spore_drop_effect::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_lady_vashj_summons : public SpellScript
{
    PrepareSpellScript(spell_lady_vashj_summons);

    enum SpellIds : uint32
    {
        SPELL_SUMMON_WAVE_A_MOB = 38019,
        SPELL_SUMMON_WAVE_B_MOB = 38247,
        SPELL_SUMMON_WAVE_C_MOB = 38242,
        SPELL_SUMMON_WAVE_D_MOB = 38244
    };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_WAVE_A_MOB, SPELL_SUMMON_WAVE_B_MOB, SPELL_SUMMON_WAVE_C_MOB, SPELL_SUMMON_WAVE_D_MOB });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        // Filter targets by distance depending on the spell
        // Coilfang Elites/Striders spawns on top of the stairs. The others at the foot of the stairs.
        bool top = GetSpellInfo()->Id == SPELL_SUMMON_COILFANG_ELITE || GetSpellInfo()->Id == SPELL_SUMMON_COILFANG_STRIDER;
        float minDist = top ? 25.f : 60.f;
        float maxDist = top ? 60.f : 100.f;

        Unit* caster = GetCaster();
        targets.remove(caster);
        targets.remove_if([caster, minDist, maxDist](WorldObject const* target) -> bool
        {
            float dist = caster->GetExactDist2d(target);
            return target->GetEntry() != NPC_TRIGGER || (dist < minDist || dist > maxDist);
        });

        Acore::Containers::RandomResize(targets, 1);
    }

    void HandleHit()
    {
        if (Unit* target = GetHitUnit())
        {
            switch (GetSpellInfo()->Id)
            {
                case SPELL_SUMMON_ENCHANTED_ELEMENTAL:
                    target->CastSpell(target, SPELL_SUMMON_WAVE_A_MOB, true);
                    break;
                case SPELL_SUMMON_COILFANG_ELITE:
                    target->CastSpell(target, SPELL_SUMMON_WAVE_B_MOB, true);
                    break;
                case SPELL_SUMMON_COILFANG_STRIDER:
                    target->CastSpell(target, SPELL_SUMMON_WAVE_C_MOB, true);
                    break;
                case SPELL_SUMMON_TAINTED_ELEMENTAL:
                    target->CastSpell(target, SPELL_SUMMON_WAVE_D_MOB, true);
                    break;
                default:
                    break;
            }
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_lady_vashj_summons::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENTRY);
        OnHit += SpellHitFn(spell_lady_vashj_summons::HandleHit);
    }
};

class spell_lady_vashj_static_charge_bot_movement : public AuraScript
{
    PrepareAuraScript(spell_lady_vashj_static_charge_bot_movement);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_STATIC_CHARGE });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        _handledBot = false;
        _originalCommandState = 0;

        Creature* bot = GetTarget() ? GetTarget()->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot() || !bot->IsAlive())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return;

        _handledBot = true;
        _originalCommandState = ai->GetBotCommandState();

        if (!(_originalCommandState & BOT_COMMAND_INACTION))
            ai->SetBotCommandState(BOT_COMMAND_INACTION);

        MoveVashjStaticChargeBot(bot);

        ObjectGuid const botGuid = bot->GetGUID();
        uint32 const originalCommandState = _originalCommandState;
        for (uint32 i = 1; i <= 4; ++i)
        {
            bot->m_Events.AddEventAtOffset([botGuid, originalCommandState, bot]()
            {
                if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid || !bot->IsAlive() || !bot->HasAura(SPELL_STATIC_CHARGE))
                    return;

                MoveVashjStaticChargeBot(bot);

                if (!(originalCommandState & BOT_COMMAND_INACTION))
                    if (bot_ai* ai = bot->GetBotAI())
                        ai->RemoveBotCommandState(BOT_COMMAND_INACTION);
            }, Milliseconds(i * 500));
        }
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!_handledBot)
            return;

        Creature* bot = GetTarget() ? GetTarget()->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        if (!(_originalCommandState & BOT_COMMAND_INACTION))
            ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

        if (!(_originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY))
            ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);
        else
            ai->SetBotCommandState(_originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY);

        ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);

        if (bot->IsAlive() && !ai->IAmFree())
            ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_lady_vashj_static_charge_bot_movement::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_lady_vashj_static_charge_bot_movement::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }

private:
    bool _handledBot = false;
    uint32 _originalCommandState = 0;
};

// Spell 38132 - Paralyze (applied to player when looting Tainted Core item 31088)
class spell_lady_vashj_tainted_core_paralyze : public AuraScript
{
    PrepareAuraScript(spell_lady_vashj_tainted_core_paralyze);

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
            return;

        if (Player* player = GetTarget()->ToPlayer())
            player->DestroyItemCount(ITEM_TAINTED_CORE, -1, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_lady_vashj_tainted_core_paralyze::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_lady_vashj()
{
    RegisterSerpentShrineAI(boss_lady_vashj);
    RegisterSpellScript(spell_lady_vashj_magic_barrier);
    RegisterSpellScript(spell_lady_vashj_remove_tainted_cores);
    RegisterSpellScript(spell_lady_vashj_summon_sporebat);
    RegisterSpellScript(spell_lady_vashj_spore_drop_effect);
    RegisterSpellScript(spell_lady_vashj_summons);
    RegisterSpellScript(spell_lady_vashj_static_charge_bot_movement);
    RegisterSpellScript(spell_lady_vashj_tainted_core_paralyze);
}
