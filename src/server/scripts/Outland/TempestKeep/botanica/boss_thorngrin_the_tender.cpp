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

#include "Containers.h"
#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "ThreatManager.h"
#include "the_botanica.h"

#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceCustomForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& message, uint32 cooldownMs = 5000);
}

enum Says
{
    SAY_AGGRO = 0,
    SAY_20_PERCENT_HP = 1,
    SAY_KILL = 2,
    SAY_CAST_SACRIFICE = 3,
    SAY_50_PERCENT_HP = 4,
    SAY_CAST_HELLFIRE = 5,
    SAY_DEATH = 6,
    EMOTE_ENRAGE = 7,
    SAY_INTRO = 8
};

enum Spells
{
    SPELL_SACRIFICE = 34661,
    SPELL_HELLFIRE = 34659,
    SPELL_ENRAGE = 34670
};

struct boss_thorngrin_the_tender : public BossAI
{
    boss_thorngrin_the_tender(Creature* creature) : BossAI(creature, DATA_THORNGRIN_THE_TENDER)
    {
        me->m_SightDistance = 100.0f;
        _intro = false;
    }

    void Reset() override
    {
        _Reset();
        ScheduleHealthCheckEvent(20, [&]() {
            Talk(SAY_20_PERCENT_HP);
            });
        ScheduleHealthCheckEvent(50, [&]() {
            Talk(SAY_50_PERCENT_HP);
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

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(6s, [this](TaskContext context)
            {
                if (CastSacrificeOnRandomNonTankTarget() == SPELL_CAST_OK)
                    Talk(SAY_CAST_SACRIFICE);

                context.Repeat(30s);
            }).Schedule(18s, [this](TaskContext context)
                {
                    if (roll_chance_i(50))
                        Talk(SAY_CAST_HELLFIRE);

                    DoCastAOE(SPELL_HELLFIRE);
                    context.Repeat(22s);
                }).Schedule(15s, [this](TaskContext context)
                    {
                        Talk(EMOTE_ENRAGE);
                        DoCastSelf(SPELL_ENRAGE);
                        context.Repeat(30s);
                    });
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

private:
    bool _intro;

    bool IsValidSacrificeTarget(Unit* target, Unit const* tank) const
    {
        if (!target)
            return false;

        if (target == me || target == tank)
            return false;

        if (!target->IsAlive() || !target->IsInWorld())
            return false;

        // Sacrifice should choose real players or NPCBots only.
        if (!target->IsPlayer() && !target->IsNPCBot())
            return false;

        if (target->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        if (!target->IsInMap(me))
            return false;

        if (!target->InSamePhase(me))
            return false;

        if (!me->CanSeeOrDetect(target))
            return false;

        if (!me->IsWithinLOSInMap(target))
            return false;

        return true;
    }

    Unit* SelectRandomSacrificeTarget()
    {
        Unit* tank = me->GetThreatMgr().GetCurrentVictim();
        if (!tank)
            tank = me->GetVictim();

        std::vector<Unit*> candidates;
        candidates.reserve(me->GetThreatMgr().GetThreatListSize());

        for (ThreatReference const* ref : me->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            Unit* target = ref->GetVictim();
            if (!IsValidSacrificeTarget(target, tank))
                continue;

            candidates.push_back(target);
        }

        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }

    SpellCastResult CastSacrificeOnRandomNonTankTarget()
    {
        Unit* target = SelectRandomSacrificeTarget();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        SpellCastResult result = me->CastSpell(target, SPELL_SACRIFICE, false);
        if (result == SPELL_CAST_OK)
            if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target))
                DBMFTABotCallouts::AnnounceCustomForModule(bot, SPELL_SACRIFICE, "DBM-Party-BC", "560", "Sacrifice on me!", DBMFTABotCallouts::GetCooldownMs());

        return result;
    }
};

void AddSC_boss_thorngrin_the_tender()
{
    RegisterTheBotanicaCreatureAI(boss_thorngrin_the_tender);
}
