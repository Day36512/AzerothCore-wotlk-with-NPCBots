/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AreaTriggerScript.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "the_underbog.h"

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono_literals;

enum eBlackStalker
{
    SPELL_ACID_BREATH = 34268,
    SPELL_ACID_SPIT = 34290,
    SPELL_TAIL_SWEEP = 34267,
    SPELL_ENRAGE = 15716,

    // Custom Venom Fangs spell: configured in DBC/DB as
    //  - EFFECT_0: PERIODIC_DAMAGE (1s tick, 10s duration baseline)
    //  - EFFECT_1: SCHOOL_DAMAGE (initial hit)
    SPELL_VENOM_FANGS = 300388,

    ACTION_MOVE_TO_PLATFORM = 1
};

struct boss_ghazan : public BossAI
{
    boss_ghazan(Creature* creature) : BossAI(creature, DATA_GHAZAN)
    {
        //Dinkle custom
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    // ------------ BOT / PLAYER AWARE HELPERS ------------
    static bool IsPlayerOrNPCBot(Unit* u)
    {
        if (!u || !u->IsAlive())
            return false;

        if (u->GetTypeId() == TYPEID_PLAYER)
            return true;

        if (u->GetTypeId() == TYPEID_UNIT)
        {
            if (Creature* c = u->ToCreature())
                return c->IsNPCBot();
        }

        return false;
    }

    Unit* SelectRandomPlayerOrNPCBotFromThreat(bool excludeVictim = true)
    {
        std::vector<Unit*> pool;
        pool.reserve(me->GetThreatMgr().GetThreatListSize());

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (excludeVictim && u == me->GetVictim())
                continue;

            if (!IsPlayerOrNPCBot(u))
                continue;

            pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0u, static_cast<uint32>(pool.size() - 1))];
    }

    // ------------ CORE AI ------------
    void InitializeAI() override
    {
        _movedToPlatform = false;
        _reachedPlatform = false;
        Reset();
    }

    void Reset() override
    {
        _Reset();
        if (!_reachedPlatform)
            _movedToPlatform = false;

        ScheduleHealthCheckEvent(20, [&]
            {
                DoCastSelf(SPELL_ENRAGE);
            });
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Acid Breath on tank
        scheduler.Schedule(3s, [this](TaskContext context)
            {
                DoCastVictim(SPELL_ACID_BREATH);
                context.Repeat(10s, 14s);
            });

        // Acid Spit — bot + player aware
        scheduler.Schedule(1s, [this](TaskContext context)
            {
                if (Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false))
                {
                    me->CastSpell(target, SPELL_ACID_SPIT, false);
                }
                else
                {
                    // fallback to existing behavior
                    DoCastRandomTarget(SPELL_ACID_SPIT);
                }

                context.Repeat(7s, 9s);
            });

        // Tail Sweep
        scheduler.Schedule(DUNGEON_MODE<Milliseconds>(5900ms, 10s), [this](TaskContext context)
            {
                DoCastVictim(SPELL_TAIL_SWEEP);
                context.Repeat(7s, 9s);
            });

        // Venom Fangs – healer check on random player/bot
        scheduler.Schedule(8s, [this](TaskContext context)
            {
                if (IsHeroic())
                {
                    // Heroic: try to hit up to two distinct targets
                    std::vector<Unit*> pool;
                    pool.reserve(me->GetThreatMgr().GetThreatListSize());

                    for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
                    {
                        if (!ref || ref->IsOffline())
                            continue;

                        Unit* u = ref->GetVictim();
                        if (!u || !u->IsAlive())
                            continue;

                        if (!IsPlayerOrNPCBot(u))
                            continue;

                        pool.push_back(u);
                    }

                    if (!pool.empty())
                    {
                        Unit* first = nullptr;
                        Unit* second = nullptr;

                        uint32 idx1 = urand(0u, static_cast<uint32>(pool.size() - 1));
                        first = pool[idx1];

                        if (pool.size() > 1)
                        {
                            // remove first from pool so second is guaranteed different
                            pool.erase(pool.begin() + idx1);
                            uint32 idx2 = urand(0u, static_cast<uint32>(pool.size() - 1));
                            second = pool[idx2];
                        }

                        if (first)
                            me->CastSpell(first, SPELL_VENOM_FANGS, false);
                        if (second)
                            me->CastSpell(second, SPELL_VENOM_FANGS, false);
                    }
                }
                else
                {
                    // Normal: single target
                    if (Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false))
                        me->CastSpell(target, SPELL_VENOM_FANGS, false);
                }

                context.Repeat(12s, 16s);
            });

        _JustEngagedWith();
    }

    void DoAction(int32 type) override
    {
        if (type == ACTION_MOVE_TO_PLATFORM && !_movedToPlatform)
        {
            _movedToPlatform = true;
            me->GetMotionMaster()->MoveWaypoint((me->GetSpawnId() * 10) + 1, false);
        }
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (!_movedToPlatform || type != WAYPOINT_MOTION_TYPE || pointId != 20)
            return;

        _reachedPlatform = true;
        me->SetHomePosition(me->GetPosition());

        me->m_Events.AddEventAtOffset([this]()
            {
                me->StopMoving();
                me->GetMotionMaster()->MoveRandom(12.f);
            }, 1ms);
    }

    void JustReachedHome() override
    {
        if (_reachedPlatform)
            me->GetMotionMaster()->MoveRandom(12.f);

        _JustReachedHome();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        DoMeleeAttackIfReady();
    }

private:
    bool _movedToPlatform;
    bool _reachedPlatform;
};

// ---------------- Venom Fangs SpellScript (initial hit) ----------------
// EFFECT_1: SCHOOL_DAMAGE
//  - If target HP >= 50%: deals 20% of MAX HP.
//  - If target HP <  50%: deals 10% of MAX HP.
class spell_ghazan_venom_fangs : public SpellScript
{
    PrepareSpellScript(spell_ghazan_venom_fangs);

    void HandleDamage(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        Unit* target = GetHitUnit();
        if (!target || !target->IsAlive())
            return;

        float pct = target->HealthBelowPct(50) ? 0.1f : 0.2f;
        float raw = float(target->GetMaxHealth()) * pct;
        int32 damage = int32(raw);

        if (damage < 1)
            damage = 1;

        SetHitDamage(damage);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(
            spell_ghazan_venom_fangs::HandleDamage,
            EFFECT_1,
            SPELL_EFFECT_SCHOOL_DAMAGE
        );
    }
};

// ---------------- Venom Fangs Aura Script (stacking DoT) ----------------
// EFFECT_0: PERIODIC_DAMAGE
//  - Baseline: 10s, 1s ticks (configured in DBC).
//  - Each tick deals 1.3x more damage than the previous tick.
//  - If the target is above 90% HP at the start of a tick, the aura ends early.
class spell_ghazan_venom_fangs_aura : public AuraScript
{
    PrepareAuraScript(spell_ghazan_venom_fangs_aura);

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        // Override default periodic damage
        PreventDefaultAction();

        Unit* target = GetTarget();
        if (!target || !target->IsAlive())
            return;

        // If they have been healed high enough since the last tick, venom breaks early.
        if (target->HealthAbovePct(90))
        {
            target->RemoveAura(GetId());
            return;
        }

        Unit* caster = GetCaster();
        if (!caster)
            caster = target;

        // Base damage from DBC (first tick baseline)
        int32 baseAmount = aurEff->GetAmount();
        if (baseAmount <= 0)
            return;

        // Tick number starts at 0; each tick is 1.3x the previous
        uint32 tickNumber = aurEff->GetTickNumber();
        float multiplier = 1.0f;

        if (tickNumber > 0)
            multiplier = std::pow(1.3f, static_cast<float>(tickNumber));

        float scaledFloat = float(baseAmount) * multiplier;
        int32 scaledDamage = int32(scaledFloat);
        if (scaledDamage < 1)
            scaledDamage = 1;

        SpellInfo const* info = GetSpellInfo();
        SpellSchoolMask school = info ? info->GetSchoolMask() : SPELL_SCHOOL_MASK_NATURE;

        Unit::DealDamage(caster, target, uint32(scaledDamage), nullptr, DIRECT_DAMAGE, school, info);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(
            spell_ghazan_venom_fangs_aura::HandlePeriodic,
            EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE
        );
    }
};

class at_underbog_ghazan : public OnlyOnceAreaTriggerScript
{
public:
    at_underbog_ghazan() : OnlyOnceAreaTriggerScript("at_underbog_ghazan") {}

    bool _OnTrigger(Player* player, const AreaTrigger* /*at*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
        {
            if (Creature* ghazan = instance->GetCreature(DATA_GHAZAN))
            {
                ghazan->AI()->DoAction(ACTION_MOVE_TO_PLATFORM);
                return true;
            }
        }
        return false;
    }
};

void AddSC_boss_ghazan()
{
    RegisterUnderbogCreatureAI(boss_ghazan);
    RegisterSpellScript(spell_ghazan_venom_fangs);
    RegisterSpellScript(spell_ghazan_venom_fangs_aura);
    new at_underbog_ghazan();
}
