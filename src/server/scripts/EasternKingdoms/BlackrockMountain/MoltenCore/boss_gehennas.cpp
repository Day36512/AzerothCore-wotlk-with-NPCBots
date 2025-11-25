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
#include "ScriptedCreature.h"
#include "molten_core.h"
#include "Containers.h" // Acore::Containers::SelectRandomContainerElement

enum Spells
{
    SPELL_GEHENNAS_CURSE = 19716,
    SPELL_RAIN_OF_FIRE = 19717,
    SPELL_SHADOW_BOLT_RANDOM = 19728,
    SPELL_SHADOW_BOLT_VICTIM = 19729,
};

enum Events
{
    EVENT_GEHENNAS_CURSE = 1,
    EVENT_RAIN_OF_FIRE,
    EVENT_SHADOW_BOLT,
};

class boss_gehennas : public CreatureScript
{
public:
    boss_gehennas() : CreatureScript("boss_gehennas") {}

    struct boss_gehennasAI : public BossAI
    {
        boss_gehennasAI(Creature* creature) : BossAI(creature, DATA_GEHENNAS) {}

        void JustEngagedWith(Unit* /*attacker*/) override
        {
            _JustEngagedWith();
            events.ScheduleEvent(EVENT_GEHENNAS_CURSE, 6s, 9s);
            events.ScheduleEvent(EVENT_RAIN_OF_FIRE, 10s);
            events.ScheduleEvent(EVENT_SHADOW_BOLT, 3s, 5s);
        }

        void ExecuteEvent(uint32 eventId) override
        {
            switch (eventId)
            {
            case EVENT_GEHENNAS_CURSE:
            {
                DoCastVictim(SPELL_GEHENNAS_CURSE);
                events.Repeat(25s, 30s);
                break;
            }

            case EVENT_RAIN_OF_FIRE:
            {
                if (Unit* target = SelectRandomPlayerOrBot(100.0f, /*preferNonVictim=*/true))
                    DoCast(target, SPELL_RAIN_OF_FIRE, true);
                else
                    DoCastVictim(SPELL_RAIN_OF_FIRE, true);

                events.Repeat(6s);
                break;
            }

            case EVENT_SHADOW_BOLT:
            {
                if (urand(0, 1))
                {
                    if (Unit* target = SelectRandomPlayerOrBot(100.0f, /*preferNonVictim=*/true))
                        DoCast(target, SPELL_SHADOW_BOLT_RANDOM);
                    else
                        DoCastVictim(SPELL_SHADOW_BOLT_VICTIM);
                }
                else
                {
                    DoCastVictim(SPELL_SHADOW_BOLT_VICTIM);
                }

                events.Repeat(5s);
                break;
            }
            }
        }

    private:
        Unit* SelectRandomPlayerOrBot(float range, bool preferNonVictim = false)
        {
            std::list<Unit*> targets;
            Acore::AnyUnitInObjectRangeCheck check(me, range);
            Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, targets, check);
            Cell::VisitObjects(me, searcher, range);

            Unit* currentVictim = me->GetVictim();

            targets.remove_if([this, currentVictim, preferNonVictim](Unit* u)
                {
                    if (!u || !u->IsAlive())
                        return true;

                    const bool isPlayer = u->GetTypeId() == TYPEID_PLAYER;
                    const bool isNpcBot = (u->GetTypeId() == TYPEID_UNIT) && u->ToCreature() && u->ToCreature()->IsNPCBot();
                    if (!isPlayer && !isNpcBot)
                        return true;

                    if (!me->IsValidAttackTarget(u))
                        return true;

                    if (preferNonVictim && currentVictim && u->GetGUID() == currentVictim->GetGUID())
                        return true;

                    return false;
                });

            if (targets.empty())
                return nullptr;

            return Acore::Containers::SelectRandomContainerElement(targets);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetMoltenCoreAI<boss_gehennasAI>(creature);
    }
};

void AddSC_boss_gehennas()
{
    new boss_gehennas();
}
