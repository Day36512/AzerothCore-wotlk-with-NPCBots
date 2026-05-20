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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"

#include <algorithm>
#include <vector>

namespace
{
    bool IsNPCBotUnit(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && creature->GetBotAI();
    }

    bool IsCuratorEncounterUnit(Unit* unit, Creature const* source, float range)
    {
        if (!unit || !source || !source->GetMap())
            return false;

        if (!unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(source, range))
            return false;

        if (unit->IsPlayer())
            return true;

        return IsNPCBotUnit(unit);
    }

    bool ContainsGuid(std::vector<Unit*> const& units, ObjectGuid const& guid)
    {
        return std::any_of(units.begin(), units.end(), [guid](Unit const* unit)
        {
            return unit && unit->GetGUID() == guid;
        });
    }

    void TryAddCuratorEncounterUnit(std::vector<Unit*>& units, Unit* unit, Creature* source, float range, bool requireAttackTarget = true)
    {
        if (!IsCuratorEncounterUnit(unit, source, range))
            return;

        if (ContainsGuid(units, unit->GetGUID()))
            return;

        if (requireAttackTarget && !source->IsValidAttackTarget(unit))
            return;

        units.push_back(unit);
    }

    std::vector<Unit*> GatherCuratorEncounterUnits(Creature* source, float range = 120.0f, bool requireAttackTarget = true)
    {
        std::vector<Unit*> units;

        if (!source || !source->GetMap())
            return units;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            TryAddCuratorEncounterUnit(units, player, source, range, requireAttackTarget);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    TryAddCuratorEncounterUnit(units, member, source, range, requireAttackTarget);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
            {
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    TryAddCuratorEncounterUnit(units, pair.second, source, range, requireAttackTarget);
            }
        }

        return units;
    }

    Unit* SelectRandomCuratorEncounterUnit(Creature* source, float range = 40.0f)
    {
        std::vector<Unit*> targets = GatherCuratorEncounterUnits(source, range);

        if (targets.empty())
            return nullptr;

        return targets[urand(0, targets.size() - 1)];
    }
}

enum Text
{
    SAY_AGGRO                       = 0,
    SAY_SUMMON                      = 1,
    SAY_EVOCATE                     = 2,
    SAY_ENRAGE                      = 3,
    SAY_KILL                        = 4,
    SAY_DEATH                       = 5
};

enum Spells
{
    SPELL_HATEFUL_BOLT              = 30383,
    SPELL_EVOCATION                 = 30254,
    SPELL_ARCANE_INFUSION           = 30403,
    SPELL_ASTRAL_DECONSTRUCTION     = 30407,

    SPELL_SUMMON_ASTRAL_FLARE1      = 30236,
    SPELL_SUMMON_ASTRAL_FLARE2      = 30239,
    SPELL_SUMMON_ASTRAL_FLARE3      = 30240,
    SPELL_SUMMON_ASTRAL_FLARE4      = 30241
};

struct boss_curator : public BossAI
{
    boss_curator(Creature* creature) : BossAI(creature, DATA_CURATOR)
    {    }

    void Reset() override
    {
        BossAI::Reset();
        me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_ARCANE, true);
        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_PERIODIC_MANA_LEECH, true);
        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_POWER_BURN, true);
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_POWER_BURN, true);
        ScheduleHealthCheckEvent(15, [&] {
            me->InterruptNonMeleeSpells(true);
            DoCastSelf(SPELL_ARCANE_INFUSION, true);
            Talk(SAY_ENRAGE);
        });
        me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA));
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim && (victim->IsPlayer() || IsNPCBotUnit(victim)))
        {
            Talk(SAY_KILL);
        }
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    Unit* SelectHatefulBoltTarget()
    {
        std::vector<Unit*> targets = GatherCuratorEncounterUnits(me, 45.0f);

        if (targets.empty())
            return me->GetVictim();

        std::sort(targets.begin(), targets.end(), [this](Unit* left, Unit* right)
        {
            return DoGetThreat(left) > DoGetThreat(right);
        });

        // Hateful Bolt should prefer the second-highest threat target, matching the original MaxThreat index 1 intent.
        if (targets.size() > 1)
            return targets[1];

        return me->GetVictim() ? me->GetVictim() : targets[0];
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        DoZoneInCombat();
        scheduler.Schedule(10min, [this](TaskContext /*context*/)
        {
            Talk(SAY_ENRAGE);
            me->InterruptNonMeleeSpells(true);
            DoCastSelf(SPELL_ASTRAL_DECONSTRUCTION, true);
        }).Schedule(10s, [this](TaskContext context)
        {
            if (Unit* target = SelectHatefulBoltTarget())
            {
                DoCast(target, SPELL_HATEFUL_BOLT);
            }
            else
            {
                DoCastVictim(SPELL_HATEFUL_BOLT);
            }
            context.Repeat(7s, 15s);
        }).Schedule(6s, [this](TaskContext context)
        {
            if (me->HealthAbovePct(15))
            {
                if (roll_chance_i(50))
                {
                    Talk(SAY_SUMMON);
                }

                uint32 const flareSpell = RAND(SPELL_SUMMON_ASTRAL_FLARE1, SPELL_SUMMON_ASTRAL_FLARE2, SPELL_SUMMON_ASTRAL_FLARE3, SPELL_SUMMON_ASTRAL_FLARE4);

                // Spawn two of the same Astral Flare at the same summon beat.
                // Mana burn remains unchanged: one 10% mana loss per summon cycle, not one per flare.
                DoCastSelf(flareSpell);
                DoCastSelf(flareSpell, true);

                int32 mana = CalculatePct(me->GetMaxPower(POWER_MANA), 10);
                me->ModifyPower(POWER_MANA, -mana);
                if (me->GetPowerPct(POWER_MANA) < 10.0f)
                {
                    Talk(SAY_EVOCATE);
                    DoCastSelf(SPELL_EVOCATION);
                    scheduler.DelayAll(20s);
                    context.Repeat(20s);
                }
                else
                {
                    context.Repeat(10s);
                }
            }
        });
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);

        Unit* target = SelectRandomCuratorEncounterUnit(summon, 40.0f);
        if (!target && me->GetVictim())
            target = me->GetVictim();

        if (target)
        {
            summon->AI()->AttackStart(target);
            summon->AddThreat(target, 1000.0f);
        }

        summon->SetInCombatWithZone();
    }
};

void AddSC_boss_curator()
{
    RegisterKarazhanCreatureAI(boss_curator);
}
