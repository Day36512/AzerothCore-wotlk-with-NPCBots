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

#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include "the_underbog.h"

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

#include <chrono>
#include <vector>

using namespace std::chrono_literals;

enum Spells
{
    // Hungarfen
    SPELL_SPAWN_MUSHROOMS = 31692,
    SPELL_DESPAWN_MUSHROOMS = 34874,
    SPELL_FOUL_SPORES = 31673,
    SPELL_ACID_GEYSER = 38739,
    SPELL_ENTANGLING_ROOTS = 339,    // new: random root on mushroom targets

    // Underbog Mushroom
    SPELL_SHRINK = 31691,
    SPELL_GROW = 31698,
    SPELL_SPORE_CLOUD = 34168
};

enum Misc
{
    MAX_GROW_REPEAT = 9,
    EMOTE_ROARS = 0
};

struct boss_hungarfen : public BossAI
{
    boss_hungarfen(Creature* creature) : BossAI(creature, DATA_HUNGARFEN) {}

    // -------- Bot-aware helpers --------
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

    // Random threat target: players + NPCBots, optionally excluding current victim
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

    void Reset() override
    {
        _Reset();
        _scheduler.CancelAll();
        DoCastAOE(SPELL_DESPAWN_MUSHROOMS, true);

        ScheduleHealthCheckEvent(20, [&]
            {
                me->AddUnitState(UNIT_STATE_ROOT);
                Talk(EMOTE_ROARS);
                DoCastSelf(SPELL_FOUL_SPORES);
                _scheduler.DelayAll(11s);
                _scheduler.Schedule(11s, [this](TaskContext /*context*/)
                    {
                        me->ClearUnitState(UNIT_STATE_ROOT);
                    });
            });
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        _scheduler.Schedule(IsHeroic() ? randtime(2400ms, 3600ms) : 10s, [this](TaskContext context)
            {
                // Bot-aware mushroom spawn target selection
                if (Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false))
                {
                    // Spawn mushrooms on the target
                    target->CastSpell(target, SPELL_SPAWN_MUSHROOMS, true);

                    // 40% chance to have Hungarfen root that target
                    if (urand(1, 100) <= 40)
                        me->CastSpell(target, SPELL_ENTANGLING_ROOTS, true);
                }
                else
                {
                    // Fallback to original behavior if something weird happens
                    if (Unit* fallback = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                    {
                        fallback->CastSpell(fallback, SPELL_SPAWN_MUSHROOMS, true);

                        if (urand(1, 100) <= 40)
                            me->CastSpell(fallback, SPELL_ENTANGLING_ROOTS, false);
                    }
                }

                context.Repeat();
            });

        if (IsHeroic())
        {
            _scheduler.Schedule(6s, [this](TaskContext context)
                {
                    DoCastAOE(SPELL_ACID_GEYSER);
                    context.Repeat(8500ms, 11s);
                });
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _scheduler.Update(diff, [this]
            {
                DoMeleeAttackIfReady();
            });
    }

private:
    TaskScheduler _scheduler;
};

struct npc_underbog_mushroom : public ScriptedAI
{
    npc_underbog_mushroom(Creature* creature) : ScriptedAI(creature) {}

    void InitializeAI() override
    {
        DoCastSelf(SPELL_SHRINK, true);

        _scheduler.Schedule(2s, [this](TaskContext context)
            {
                DoCastSelf(SPELL_GROW, true);

                if (context.GetRepeatCounter() == MAX_GROW_REPEAT)
                {
                    DoCastSelf(SPELL_SPORE_CLOUD);

                    context.Schedule(4s, [this](TaskContext /*context*/)
                        {
                            me->RemoveAurasDueToSpell(SPELL_GROW);
                            me->DespawnOrUnsummon(2s);
                        });
                }
                else
                    context.Repeat();
            });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

protected:
    TaskScheduler _scheduler;
};

class spell_spore_cloud : public AuraScript
{
    PrepareAuraScript(spell_spore_cloud);

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        PreventDefaultAction();

        if (Unit* caster = GetCaster())
        {
            if (InstanceScript* instance = caster->GetInstanceScript())
            {
                if (Creature* hungarfen = instance->GetCreature(DATA_HUNGARFEN))
                    caster->CastSpell((Unit*)nullptr, GetSpellInfo()->Effects[EFFECT_0].TriggerSpell, true, nullptr, nullptr, hungarfen->GetGUID());
            }
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_spore_cloud::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_despawn_underbog_mushrooms : public SpellScript
{
    PrepareSpellScript(spell_despawn_underbog_mushrooms);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            if (Creature* cTarget = target->ToCreature())
                if (cTarget->GetEntry() == NPC_UNDERBOG_MUSHROOM)
                    cTarget->DespawnOrUnsummon();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_despawn_underbog_mushrooms::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_boss_hungarfen()
{
    RegisterUnderbogCreatureAI(boss_hungarfen);
    RegisterUnderbogCreatureAI(npc_underbog_mushroom);
    RegisterSpellScript(spell_spore_cloud);
    RegisterSpellScript(spell_despawn_underbog_mushrooms);
}
