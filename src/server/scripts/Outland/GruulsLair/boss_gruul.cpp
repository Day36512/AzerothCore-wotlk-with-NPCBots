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
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "gruuls_lair.h"
#include "SpellAuraEffects.h"
#include "bot_ai.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

enum Yells
{
    SAY_AGGRO                   = 0,
    SAY_SLAM                    = 1,
    SAY_SHATTER                 = 2,
    SAY_SLAY                    = 3,
    SAY_DEATH                   = 4,

    EMOTE_GROW                  = 5
};

enum Spells
{
    SPELL_GROWTH                        = 36300,
    SPELL_CAVE_IN                       = 36240,
    SPELL_GROUND_SLAM                   = 33525,
    SPELL_REVERBERATION                 = 36297,
    SPELL_HURTFUL_STRIKE                = 33813,
    SPELL_SHATTER                       = 33654,
    SPELL_LOOK_AROUND                   = 33965,

    // Ground Slam spells
    SPELL_SUMMON_TRACTOR_BEAM_CREATOR   = 33496,
    SPELL_TRACTOR_BEAM_PULL             = 33497,
    SPELL_SUMMON_TRACTOR_BEAM_1         = 33495,
    SPELL_SUMMON_TRACTOR_BEAM_2         = 33514,
    SPELL_SUMMON_TRACTOR_BEAM_3         = 33515,
    SPELL_SUMMON_TRACTOR_BEAM_4         = 33516,
    SPELL_SUMMON_TRACTOR_BEAM_5         = 33517,
    SPELL_SUMMON_TRACTOR_BEAM_6         = 33518,
    SPELL_SUMMON_TRACTOR_BEAM_7         = 33519,
    SPELL_SUMMON_TRACTOR_BEAM_8         = 33520,

    SPELL_SHATTER_EFFECT                = 33671,
    SPELL_STONED                        = 33652,

    // Bot support
    SPELL_SWIFTNESS_POTION              = 2379,
};

namespace GruulBotDirector
{
    // Gruul's Ground Slam applies a heavy slow, so bots cannot wait until the slam lands.
    // They need to begin spreading before the cast and they need more padding than the
    // bare minimum Shatter radius, especially when many bots are present.
    constexpr float SHATTER_MIN_DIST = 18.0f;
    constexpr float RANGED_SPREAD_RADIUS = 42.0f;
    constexpr float MELEE_SPREAD_RADIUS = 31.0f;
    constexpr float TANK_SPREAD_RADIUS = 23.0f;
    constexpr float ARRIVED_DISTANCE_2D = 2.25f;
    constexpr float MAX_DIRECTED_MOVE_DISTANCE = 95.0f;
    constexpr uint32 SPREAD_TICK_INTERVAL_MS = 500;
    constexpr uint32 PRE_SLAM_SPREAD_LEAD_MS = 5000;
    constexpr uint32 GROUND_SLAM_TO_SHATTER_MS = 9700;
    constexpr float TWO_PI = 6.28318530717958647692f;

    struct BotSpreadRecord
    {
        ObjectGuid Guid;
        Position Destination;
        uint32 OriginalCommandState = 0;
        bool IsHealer = false;
        bool NoCastApplied = false;
    };

    struct BotRoleGroups
    {
        std::vector<Creature*> MainTanks;
        std::vector<Creature*> OffTanks;
        std::vector<Creature*> Melee;
        std::vector<Creature*> Ranged;
        std::vector<Creature*> Healers;
    };

    bool IsEncounterNPCBot(Creature* boss, Unit* unit)
    {
        if (!boss || !unit || !unit->IsAlive() || !unit->IsInWorld() || unit->GetMap() != boss->GetMap())
            return false;

        if (!unit->IsNPCBot())
            return false;

        Creature* bot = unit->ToCreature();
        if (!bot || !bot->GetBotAI())
            return false;

        return boss->IsWithinDistInMap(bot, 120.0f);
    }

    void SortByGuid(std::vector<Creature*>& bots)
    {
        std::sort(bots.begin(), bots.end(), [](Creature const* left, Creature const* right)
        {
            return left->GetGUID() < right->GetGUID();
        });
    }

    void SafeBotMoveTo(Creature* bot, Position const& dest)
    {
        if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        float x = dest.GetPositionX();
        float y = dest.GetPositionY();
        float z = dest.GetPositionZ();

        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        if (bot->GetExactDist(x, y, z) > MAX_DIRECTED_MOVE_DISTANCE)
            return;

        if (!bot->IsWithinLOS(x, y, z))
            return;

        Position finalPos;
        finalPos.Relocate(x, y, z, dest.GetOrientation());
        ai->MoveToSendPosition(finalPos);
    }

    void CastBotSwiftnessPotion(Creature* bot)
    {
        if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
            return;

        // Forced encounter helper. This does not require inventory consumption and is only
        // used during Gruul's pre-slam spread window so bots can beat the Ground Slam slow.
        bot->CastSpell(bot, SPELL_SWIFTNESS_POTION, true);
    }

    float GetRingRadius(float baseRadius, float maxRadius, size_t count)
    {
        if (count <= 1)
            return baseRadius;

        float const neededRadius = SHATTER_MIN_DIST / (2.0f * std::sin((TWO_PI * 0.5f) / float(count)));
        return std::min(maxRadius, std::max(baseRadius, neededRadius));
    }

    void BuildCircleNodes(Position const& center, float radius, size_t count, std::vector<Position>& out, float startAngle)
    {
        out.clear();
        if (!count)
            return;

        float angle = Position::NormalizeOrientation(startAngle);
        float const step = TWO_PI / float(count);

        for (size_t i = 0; i < count; ++i)
        {
            float const x = center.GetPositionX() + radius * std::cos(angle);
            float const y = center.GetPositionY() + radius * std::sin(angle);
            float const orientation = Position::NormalizeOrientation(std::atan2(center.GetPositionY() - y, center.GetPositionX() - x));

            Position node;
            node.Relocate(x, y, center.GetPositionZ(), orientation);
            out.push_back(node);

            angle = Position::NormalizeOrientation(angle + step);
        }
    }

    BotRoleGroups CollectNearbyNPCBots(Creature* boss)
    {
        BotRoleGroups groups;
        if (!boss)
            return groups;

        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck check(boss, 120.0f);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(boss, nearby, check);
        Cell::VisitObjects(boss, searcher, 120.0f);

        for (Unit* unit : nearby)
        {
            if (!IsEncounterNPCBot(boss, unit))
                continue;

            Creature* bot = unit->ToCreature();
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (!ai)
                continue;

            bool const isOffTank = ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK_OFF);
            bool const isTank = isOffTank || ai->IsTank() || ai->HasRole(BOT_ROLE_TANK);

            if (isTank)
            {
                if (isOffTank)
                    groups.OffTanks.push_back(bot);
                else
                    groups.MainTanks.push_back(bot);
            }
            else if (ai->HasRole(BOT_ROLE_HEAL))
                groups.Healers.push_back(bot);
            else if (ai->HasRole(BOT_ROLE_RANGED))
                groups.Ranged.push_back(bot);
            else
                groups.Melee.push_back(bot);
        }

        SortByGuid(groups.MainTanks);
        SortByGuid(groups.OffTanks);
        SortByGuid(groups.Melee);
        SortByGuid(groups.Ranged);
        SortByGuid(groups.Healers);

        return groups;
    }

    class ShatterSpreadDirector
    {
    public:
        void Start(Creature* boss, Position const& center)
        {
            StopAndRestore(boss);

            _center = center;
            _active = true;
            _tickTimer = 0;
            BuildAssignments(boss);
            SpreadTick(boss);
        }

        void Update(Creature* boss, uint32 diff)
        {
            if (!_active)
                return;

            if (!boss || !boss->IsAlive() || !boss->IsInCombat())
            {
                StopAndRestore(boss);
                return;
            }

            if (_tickTimer > diff)
            {
                _tickTimer -= diff;
                return;
            }

            _tickTimer = SPREAD_TICK_INTERVAL_MS;
            SpreadTick(boss);
        }

        void StopAndRestore(Creature* boss)
        {
            if (!_active && _assignments.empty())
                return;

            for (BotSpreadRecord const& record : _assignments)
            {
                Creature* bot = boss ? ObjectAccessor::GetCreature(*boss, record.Guid) : nullptr;
                if (!bot || !bot->IsNPCBot())
                    continue;

                bot_ai* ai = bot->GetBotAI();
                if (!ai)
                    continue;

                if (!(record.OriginalCommandState & BOT_COMMAND_STAY))
                    ai->RemoveBotCommandState(BOT_COMMAND_STAY);

                if (!(record.OriginalCommandState & BOT_COMMAND_INACTION))
                    ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

                if (record.NoCastApplied && !(record.OriginalCommandState & BOT_COMMAND_MASK_NOCAST_ANY))
                    ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);

                if (!bot->IsAlive())
                    continue;

                if (boss && boss->IsAlive() && boss->IsEngaged() && boss->GetVictim() && bot->IsInMap(boss))
                {
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->AttackStart(boss);
                }
                else if (!ai->IAmFree())
                    ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
            }

            _assignments.clear();
            _active = false;
            _tickTimer = 0;
        }

    private:
        void AddAssignments(std::vector<Creature*> const& bots, float radius, float startAngle)
        {
            if (bots.empty())
                return;

            std::vector<Position> nodes;
            BuildCircleNodes(_center, radius, bots.size(), nodes, startAngle);

            for (size_t i = 0; i < bots.size(); ++i)
            {
                Creature* bot = bots[i];
                bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
                if (!ai)
                    continue;

                BotSpreadRecord record;
                record.Guid = bot->GetGUID();
                record.Destination = nodes[i];
                record.OriginalCommandState = ai->GetBotCommandState();
                record.IsHealer = ai->HasRole(BOT_ROLE_HEAL);

                CastBotSwiftnessPotion(bot);

                // Healers must keep casting during the pre-slam spread window. Movement
                // commands may still interrupt them if they are badly out of position,
                // but do not blanket no-cast lock them. Non-healer bots can be briefly
                // restrained from starting greedy hard casts while Gruul is about to
                // apply the Ground Slam slow.
                if (!record.IsHealer)
                {
                    ai->SetBotCommandState(BOT_COMMAND_NO_CAST);
                    record.NoCastApplied = true;
                }

                _assignments.push_back(record);
            }
        }

        void BuildAssignments(Creature* boss)
        {
            _assignments.clear();

            BotRoleGroups groups = CollectNearbyNPCBots(boss);

            std::vector<Creature*> backline;
            backline.reserve(groups.Healers.size() + groups.Ranged.size());
            backline.insert(backline.end(), groups.Healers.begin(), groups.Healers.end());
            backline.insert(backline.end(), groups.Ranged.begin(), groups.Ranged.end());

            std::vector<Creature*> tanks;
            tanks.reserve(groups.MainTanks.size() + groups.OffTanks.size());
            tanks.insert(tanks.end(), groups.MainTanks.begin(), groups.MainTanks.end());
            tanks.insert(tanks.end(), groups.OffTanks.begin(), groups.OffTanks.end());

            float const facing = _center.GetOrientation();
            AddAssignments(backline, GetRingRadius(RANGED_SPREAD_RADIUS, 60.0f, backline.size()),
                Position::NormalizeOrientation(facing + TWO_PI * 0.5f));
            AddAssignments(groups.Melee, GetRingRadius(MELEE_SPREAD_RADIUS, 46.0f, groups.Melee.size()),
                Position::NormalizeOrientation(facing + TWO_PI * 0.25f));
            AddAssignments(tanks, GetRingRadius(TANK_SPREAD_RADIUS, 32.0f, tanks.size()),
                facing);
        }

        void SpreadTick(Creature* boss)
        {
            if (!boss)
                return;

            for (BotSpreadRecord const& record : _assignments)
            {
                Creature* bot = ObjectAccessor::GetCreature(*boss, record.Guid);
                if (!bot || !bot->IsAlive() || !bot->IsInWorld())
                    continue;

                if (bot->GetExactDist2d(record.Destination.GetPositionX(), record.Destination.GetPositionY()) <= ARRIVED_DISTANCE_2D)
                    continue;

                // Do not let long casts waste the pre-slam lead time. Movement will
                // break most casts anyway; this makes the choice explicit and reliable.
                bot->InterruptNonMeleeSpells(false);
                SafeBotMoveTo(bot, record.Destination);
            }
        }

        Position _center;
        std::vector<BotSpreadRecord> _assignments;
        bool _active = false;
        uint32 _tickTimer = 0;
    };
}

struct boss_gruul : public BossAI
{
    boss_gruul(Creature* creature) : BossAI(creature, DATA_GRUUL) { }

    void Reset() override
    {
        _Reset();
        _shatterSpreadDirector.StopAndRestore(me);
        _recentlySpoken = false;
        _caveInTimer = 29s;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(15150ms, [this](TaskContext context)
        {
            Talk(EMOTE_GROW);
            DoCastSelf(SPELL_GROWTH);
            context.Repeat(15150ms);
        }).Schedule(_caveInTimer, [this](TaskContext context)
        {
            DoCastRandomTarget(SPELL_CAVE_IN);
            if (_caveInTimer > 4s)
            {
                _caveInTimer = _caveInTimer - 1500ms;
            }
            context.Repeat(_caveInTimer);
        }).Schedule(39900ms, 55700ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_REVERBERATION);
            context.Repeat(39900ms, 55700ms);
        }).Schedule(5600ms, [this](TaskContext context)
        {
            if (Unit* target = SelectHurtfulStrikeTarget())
            {
                DoCast(target, SPELL_HURTFUL_STRIKE);
            }
            else
            {
                DoCastVictim(SPELL_HURTFUL_STRIKE);
            }
            context.Repeat(8400ms);
        }).Schedule(35s - Milliseconds(GruulBotDirector::PRE_SLAM_SPREAD_LEAD_MS), [this](TaskContext context)
        {
            // Start spreading before Ground Slam. Once the slam slow lands, bots are too
            // late if they are still stacked in melee or finishing casts.
            _shatterSpreadDirector.Start(me, me->GetPosition());

            scheduler.Schedule(Milliseconds(GruulBotDirector::PRE_SLAM_SPREAD_LEAD_MS), [this](TaskContext)
            {
                Talk(SAY_SLAM);
                DoCastSelf(SPELL_GROUND_SLAM);

                scheduler.DelayAll(Milliseconds(GruulBotDirector::GROUND_SLAM_TO_SHATTER_MS + 1));
                scheduler.Schedule(Milliseconds(GruulBotDirector::GROUND_SLAM_TO_SHATTER_MS), [this](TaskContext)
                {
                    Talk(SAY_SHATTER);
                    me->RemoveAurasDueToSpell(SPELL_LOOK_AROUND);
                    DoCastSelf(SPELL_SHATTER);
                    _shatterSpreadDirector.StopAndRestore(me);
                });
            });

            context.Repeat(60s);
        });
    }

    void KilledUnit(Unit* /*who*/) override
    {
        if (!_recentlySpoken)
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;
        }

        scheduler.Schedule(5s, [this](TaskContext)
        {
            _recentlySpoken = false;
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        _shatterSpreadDirector.StopAndRestore(me);
        _JustDied();
        Talk(SAY_DEATH);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        _shatterSpreadDirector.Update(me, diff);

        if (!me->HasUnitState(UNIT_STATE_ROOT))
        {
            DoMeleeAttackIfReady();
        }
    }

private:
    Unit* SelectHurtfulStrikeTarget()
    {
        Unit* currentVictim = me->GetVictim();

        for (ThreatReference const* ref : me->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* target = ref->GetVictim();
            if (!target || !target->IsAlive() || target == currentVictim)
                continue;

            if (!me->IsWithinDistInMap(target, 5.0f))
                continue;

            return target;
        }

        return currentVictim;
    }

    GruulBotDirector::ShatterSpreadDirector _shatterSpreadDirector;
    std::chrono::milliseconds _caveInTimer;
    bool _recentlySpoken;
};

struct npc_invisible_tractor_beam_source : public NullCreatureAI
{
    npc_invisible_tractor_beam_source(Creature* creature) : NullCreatureAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Unit* summonerUnit = summoner->ToUnit())
        {
            DoCast(summonerUnit, SPELL_TRACTOR_BEAM_PULL, true);
        }
    }
};

class spell_gruul_ground_slam : public SpellScript
{
    PrepareSpellScript(spell_gruul_ground_slam);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_TRACTOR_BEAM_CREATOR });
    }

    void ApplyStun()
    {
        if (Unit* caster = GetCaster())
        {
            caster->CastSpell(caster, SPELL_LOOK_AROUND, true);
        }
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            target->CastSpell(target, SPELL_SUMMON_TRACTOR_BEAM_CREATOR, true);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_gruul_ground_slam::ApplyStun);
        OnEffectHitTarget += SpellEffectFn(spell_gruul_ground_slam::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_tractor_beam_creator : public SpellScript
{
    PrepareSpellScript(spell_tractor_beam_creator);

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            std::vector<uint32> tractorBeamSummons = { SPELL_SUMMON_TRACTOR_BEAM_1, SPELL_SUMMON_TRACTOR_BEAM_2, SPELL_SUMMON_TRACTOR_BEAM_3, SPELL_SUMMON_TRACTOR_BEAM_4,
                SPELL_SUMMON_TRACTOR_BEAM_5, SPELL_SUMMON_TRACTOR_BEAM_6, SPELL_SUMMON_TRACTOR_BEAM_7, SPELL_SUMMON_TRACTOR_BEAM_8 };
            uint32 randomTractorSpellID = Acore::Containers::SelectRandomContainerElement(tractorBeamSummons);
            caster->CastSpell(caster, randomTractorSpellID, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_tractor_beam_creator::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_gruul_ground_slam_trigger : public AuraScript
{
    PrepareAuraScript(spell_gruul_ground_slam_trigger);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_STONED });
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        if (GetUnitOwner()->GetAuraCount(GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell) == 5)
        {
            GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_STONED, true);
        }
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_gruul_ground_slam_trigger::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_gruul_shatter : public SpellScript
{
    PrepareSpellScript(spell_gruul_shatter);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHATTER_EFFECT });
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            target->RemoveAurasDueToSpell(SPELL_STONED);

            if (target->IsPlayer() || target->IsNPCBot())
            {
                target->CastSpell(static_cast<Unit*>(nullptr), SPELL_SHATTER_EFFECT, true);
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_gruul_shatter::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_gruul_shatter_effect : public SpellScript
{
    PrepareSpellScript(spell_gruul_shatter_effect);

    void CalculateDamage()
    {
        Unit* hitUnit = GetHitUnit();
        if (!hitUnit)
            return;

        float radius = GetSpellInfo()->Effects[EFFECT_0].CalcRadius(GetCaster());
        if (!radius)
            return;

        int32 damage = GetHitDamage();

        float distance = GetCaster()->GetDistance2d(hitUnit);
        if (distance > 1.0f)
        {
            float const distancePct = std::max(0.0f, (radius - distance) / radius);
            damage = int32(float(damage) * distancePct);
        }

        // NPCBots get a small handicap on Shatter. They still produce Shatter and still
        // punish bad spacing, but take 30% less damage from every Shatter explosion.
        if (hitUnit->IsNPCBot())
            damage = int32(float(damage) * 0.7f);

        SetHitDamage(damage);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_gruul_shatter_effect::CalculateDamage);
    }
};

void AddSC_boss_gruul()
{
    RegisterGruulsLairAI(boss_gruul);
    RegisterGruulsLairAI(npc_invisible_tractor_beam_source);

    RegisterSpellScript(spell_gruul_ground_slam);
    RegisterSpellScript(spell_tractor_beam_creator);
    RegisterSpellScript(spell_gruul_ground_slam_trigger);
    RegisterSpellScript(spell_gruul_shatter);
    RegisterSpellScript(spell_gruul_shatter_effect);
}
