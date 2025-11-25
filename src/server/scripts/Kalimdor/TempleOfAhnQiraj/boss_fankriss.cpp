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
#include "temple_of_ahnqiraj.h"

enum Spells
{
    SPELL_MORTAL_WOUND      = 25646,
    SPELL_ENTANGLE_RIGHT    = 720,
    SPELL_ENTANGLE_CENTER   = 731,
    SPELL_ENTANGLE_LEFT     = 1121,

    SPELL_SUMMON_WORM_1     = 518,
    SPELL_SUMMON_WORM_2     = 25831,
    SPELL_SUMMON_WORM_3     = 25832
};

enum Misc
{
    MAX_HATCHLING_SPAWN     = 4,
    NPC_VEKNISS_HATCHLING   = 15962
};

const std::array<Position, 3> hatchlingsSpawnPoints
{
    {
        { -8043.6f, 1254.1f, -84.3f }, // Right
        { -8003.0f, 1222.9f, -82.1f }, // Center
        { -8022.3f, 1149.0f, -89.1f }  // Left
    }
};

const std::array<uint32, 3> entangleSpells = { SPELL_ENTANGLE_RIGHT, SPELL_ENTANGLE_CENTER, SPELL_ENTANGLE_LEFT };

struct boss_fankriss : public BossAI
{
    boss_fankriss(Creature* creature) : BossAI(creature, DATA_FANKRISS)
    {
        me->m_CombatDistance = 80.f;
    }

    void Reset() override
    {
        // Clear any leftovers from previous attempts
        summons.DespawnAll();

        summonWormSpells = { SPELL_SUMMON_WORM_1, SPELL_SUMMON_WORM_2, SPELL_SUMMON_WORM_3 };
        BossAI::Reset();
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        // On wipe/evade, clean the room
        summons.DespawnAll();
        BossAI::EnterEvadeMode(/*why*/);
    }

    void JustDied(Unit* killer) override
    {
        // Clean up all summoned adds immediately
        summons.DespawnAll();
        BossAI::JustDied(killer);
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);
        summons.Summon(summon);                // track it
        summon->SetReactState(REACT_AGGRESSIVE);
        if (Unit* v = me->GetVictim())
            summon->AI()->AttackStart(v);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        BossAI::SummonedCreatureDespawn(summon);
        summons.Despawn(summon);               // untrack on natural despawn
    }

    void SummonWorms()
    {
        uint32 amount = urand(1, 3);
        Acore::Containers::RandomResize(summonWormSpells, amount);
        for (uint32 summonSpell : summonWormSpells)
            DoCastAOE(summonSpell, true);      // JustSummoned() will be called for these
        summonWormSpells = { SPELL_SUMMON_WORM_1, SPELL_SUMMON_WORM_2, SPELL_SUMMON_WORM_3 };
    }

    void SummonHatchlingWaves()
    {
        for (Position spawnPos : hatchlingsSpawnPoints)
        {
            for (uint8 i = 0; i < MAX_HATCHLING_SPAWN; i++)
            {
                Position randSpawn = me->GetRandomPoint(spawnPos, 10.f);
                if (Creature* c = me->SummonCreature(NPC_VEKNISS_HATCHLING, randSpawn, TEMPSUMMON_CORPSE_DESPAWN))
                {
                    JustSummoned(c);           // ensure it’s tracked in SummonList
                }
            }
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        scheduler.Schedule(7s, 14s, [this](TaskContext context)
            {
                DoCastVictim(SPELL_MORTAL_WOUND);
                context.Repeat();
            })
            .Schedule(30s, 50s, [this](TaskContext context)
                {
                    SummonWorms();
                    context.Repeat(22s, 70s);
                })
            .Schedule(15s, 20s, [this](TaskContext context)
                {
                    // Custom selector that includes NPCBots (and players), excludes current tank, requires LOS
                    if (Unit* target = SelectEntangleTarget(100.f))
                    {
                        uint32 spellId = Acore::Containers::SelectRandomContainerElement(entangleSpells);
                        DoCast(target, spellId);
                    }

                    SummonHatchlingWaves();
                    context.Repeat(25s, 55s);
                });
    }

private:
    // Select a random valid target (Player or NPCBot), not the current tank, in LOS
    Unit* SelectEntangleTarget(float range)
    {
        std::list<Unit*> units;
        Acore::AnyUnitInObjectRangeCheck check(me, range);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, units, check);
        Cell::VisitObjects(me, searcher, range);

        units.remove_if([this](Unit* u) -> bool
            {
                if (!u || !u->IsAlive())
                    return true;

                // Only players or creatures that are NPCBots
                bool isPlayer = u->GetTypeId() == TYPEID_PLAYER;
                bool isNpcBot = (u->GetTypeId() == TYPEID_UNIT) && u->ToCreature() && u->ToCreature()->IsNPCBot();
                if (!isPlayer && !isNpcBot)
                    return true;

                // Must be a valid hostile target and in LOS
                if (!me->IsValidAttackTarget(u))
                    return true;
                if (!me->IsWithinLOSInMap(u) || !me->CanSeeOrDetect(u))
                    return true;

                // Exclude current tank (replicates SelectTarget(..., withTank=false))
                if (u == me->GetVictim())
                    return true;

                return false;
            });

        if (units.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(units);
    }

    std::vector<uint32> summonWormSpells;
};

void AddSC_boss_fankriss()
{
    RegisterTempleOfAhnQirajCreatureAI(boss_fankriss);
}
