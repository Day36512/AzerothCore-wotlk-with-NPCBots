/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ruins_of_ahnqiraj.h"
#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

enum Spells
{
    // Hive'Zara Stinger
    SPELL_HIVEZARA_CATALYST             = 25187,
    SPELL_STINGER_CHARGE_NORMAL         = 25190,
    SPELL_STINGER_CHARGE_BUFFED         = 25191,

    // Obsidian Destroyer
    SPELL_PURGE                         = 25756,
    SPELL_DRAIN_MANA                    = 25755,
    SPELL_DRAIN_MANA_VISUAL             = 26639,
    SPELL_SUMMON_SMALL_OBSIDIAN_CHUNK   = 27627, // Server-side
};

struct npc_hivezara_stinger : public ScriptedAI
{
    npc_hivezara_stinger(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        scheduler.CancelAll();
    }

    void JustEngagedWith(Unit* who) override
    {
        DoCast(who ,who->HasAura(SPELL_HIVEZARA_CATALYST) ? SPELL_STINGER_CHARGE_BUFFED : SPELL_STINGER_CHARGE_NORMAL, true);

        scheduler.Schedule(5s, [this](TaskContext context)
        {
            Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 20.0f, false, false, SPELL_HIVEZARA_CATALYST);
            if (!target)
            {
                target = SelectTarget(SelectTargetMethod::Random, 0, 20.0f, false, false);
            }

            if (target)
            {
                DoCast(target, target->HasAura(SPELL_HIVEZARA_CATALYST) ? SPELL_STINGER_CHARGE_BUFFED : SPELL_STINGER_CHARGE_NORMAL, true);
            }

            context.Repeat(4500ms, 6500ms);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            return;
        }

        scheduler.Update(diff,
            std::bind(&ScriptedAI::DoMeleeAttackIfReady, this));
    }
};

struct npc_obsidian_destroyer : public ScriptedAI
{
    npc_obsidian_destroyer(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        scheduler.CancelAll();
        me->SetPower(POWER_MANA, 0);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        scheduler.Schedule(6s, [this](TaskContext context)
            {
                GuidVector selection;
                CollectManaTargets(selection);

                // Randomly keep up to 6 targets, matching original SelectTargetList(..., 6, Random, ...)
                Acore::Containers::RandomResize(selection, 6);

                for (ObjectGuid guid : selection)
                {
                    if (Unit* target = ObjectAccessor::GetUnit(*me, guid))
                        DoCast(target, SPELL_DRAIN_MANA, true);
                }

                if (me->GetPowerPct(POWER_MANA) >= 100.f)
                    DoCastAOE(SPELL_PURGE, true);

                context.Repeat(6s);
            });
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_SUMMON_SMALL_OBSIDIAN_CHUNK, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff, std::bind(&ScriptedAI::DoMeleeAttackIfReady, this));
    }

private:
    // ---- Internal helpers/consts (encapsulated) ----
    static constexpr float BOT_SCAN_RANGE_YARDS = 60.0f;

    bool IsDrainableManaTarget(Unit* unit) const
    {
        // Alive, hostile to the destroyer, has any mana now
        return unit && unit->IsAlive() &&
            me->IsValidAttackTarget(unit) &&
            unit->GetPower(POWER_MANA) > 0;
    }

    void CollectManaTargets(GuidVector& out)
    {
        out.clear();
        out.reserve(32);

        // 1) Players across the map (exclude GMs/spectators), must be drainable
        me->GetMap()->DoForAllPlayers([&](Player* player)
            {
                if (player->IsGameMaster() || player->IsSpectator())
                    return;

                if (IsDrainableManaTarget(player))
                    out.push_back(player->GetGUID());
            });

        // 2) NPCBots in range (BOT_SCAN_RANGE_YARDS), must be drainable
        std::list<Unit*> nearby;
        {
            Acore::AnyUnitInObjectRangeCheck check(me, BOT_SCAN_RANGE_YARDS);
            Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, nearby, check);
            Cell::VisitObjects(me, searcher, BOT_SCAN_RANGE_YARDS);
        }

        for (Unit* u : nearby)
        {
            if (u->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* c = u->ToCreature();
            if (!c)
                continue;

            // Only NPCBots (function provided by the NPCBots module)
            if (!c->IsNPCBot())
                continue;

            if (IsDrainableManaTarget(c))
                out.push_back(c->GetGUID());
        }
    }
};

class spell_drain_mana : public SpellScript
{
    PrepareSpellScript(spell_drain_mana);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            if (Unit* target = GetHitUnit())
            {
                target->CastSpell(caster, SPELL_DRAIN_MANA_VISUAL, true);
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_drain_mana::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_ruins_of_ahnqiraj()
{
    RegisterRuinsOfAhnQirajCreatureAI(npc_hivezara_stinger);
    RegisterRuinsOfAhnQirajCreatureAI(npc_obsidian_destroyer);
    RegisterSpellScript(spell_drain_mana);
}
