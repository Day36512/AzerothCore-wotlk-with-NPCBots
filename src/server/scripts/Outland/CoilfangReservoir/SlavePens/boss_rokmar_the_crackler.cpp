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

#include "Creature.h"
#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "the_slave_pens.h"

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

#include <chrono>
#include <vector>

using namespace std::chrono_literals;

enum RokmarSpells
{
    SPELL_ENSNARING_MOSS = 31948,
    SPELL_FRENZY = 34970,
    SPELL_GRIEVOUS_WOUND = 31956,
    SPELL_WATER_SPIT = 35008,
    SPELL_TELEPORT_VISUAL = 34656   
};

enum RokmarNPCs
{
    NPC_GREATER_BOGSTROK = 17816
};

struct boss_rokmar_the_crackler : public BossAI
{
    explicit boss_rokmar_the_crackler(Creature* creature) : BossAI(creature, DATA_ROKMAR_THE_CRACKLER)
    {
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    bool _isClone = false;

    bool _clone77Done = false;
    bool _clone33Done = false;
    bool _frenzyDone = false;

    void SetClone(bool value)
    {
        _isClone = value;
    }

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
        auto const& tlist = me->GetThreatMgr().GetThreatList();
        pool.reserve(tlist.size());

        for (auto const* ref : tlist)
        {
            Unit* u = ref ? ref->getTarget() : nullptr;
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

    void SpawnCloneAndBogstrokWave()
    {
        if (_isClone)
            return;

        Position clonePos = me->GetRandomNearPosition(5.0f);

        if (TempSummon* clone = me->SummonCreature(me->GetEntry(), clonePos,
            TEMPSUMMON_CORPSE_DESPAWN, 600000))
        {
            uint32 quarterMaxHealth = me->GetMaxHealth() / 4;
            if (!quarterMaxHealth)
                quarterMaxHealth = 1;

            clone->SetMaxHealth(quarterMaxHealth);
            clone->SetHealth(quarterMaxHealth);

            clone->SetLootRecipient(nullptr);
            clone->SetCorpseDelay(0);

            if (auto* ai = CAST_AI(boss_rokmar_the_crackler, clone->AI()))
                ai->SetClone(true);
        }

        uint8 count = me->GetMap()->IsHeroic() ? 5 : 3;

        for (uint8 i = 0; i < count; ++i)
        {
            Position addPos = me->GetRandomNearPosition(8.0f);

            me->SummonCreature(NPC_GREATER_BOGSTROK, addPos,
                TEMPSUMMON_CORPSE_DESPAWN, 600000);
        }
    }

    void Reset() override
    {
        scheduler.CancelAll();
        _Reset();

        if (!_isClone)
            summons.DespawnAll();

        _clone77Done = false;
        _clone33Done = false;
        _frenzyDone = false;
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);

        if (summon->GetEntry() == me->GetEntry())
        {
            if (auto* ai = CAST_AI(boss_rokmar_the_crackler, summon->AI()))
                ai->SetClone(true);
        }

        summon->CastSpell(summon, SPELL_TELEPORT_VISUAL, true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler.Schedule(1s, [this](TaskContext ctx)
            {
                float pct = me->GetHealthPct();

                // 20% Frenzy on boss AND clones
                if (!_frenzyDone && pct <= 20.0f)
                {
                    DoCastSelf(SPELL_FRENZY);
                    _frenzyDone = true;
                }

                // Clone + bogstrok waves only from the original boss
                if (!_isClone)
                {
                    if (!_clone77Done && pct <= 77.0f)
                    {
                        SpawnCloneAndBogstrokWave();
                        _clone77Done = true;
                    }

                    if (!_clone33Done && pct <= 33.0f)
                    {
                        SpawnCloneAndBogstrokWave();
                        _clone33Done = true;
                    }
                }

                ctx.Repeat(1s);
            });

        scheduler.Schedule(8s, [this](TaskContext context)
            {
                DoCastVictim(SPELL_GRIEVOUS_WOUND);
                context.Repeat(20700ms);
            });

        scheduler.Schedule(15300ms, [this](TaskContext context)
            {
                if (Unit* target = SelectRandomPlayerOrNPCBotFromThreat(false))
                    me->CastSpell(target, SPELL_ENSNARING_MOSS, false);

                context.Repeat(26s);
            });

        scheduler.Schedule(10700ms, [this](TaskContext context)
            {
                DoCastSelf(SPELL_WATER_SPIT);
                context.Repeat(19s);
            });
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();

        if (_isClone)
        {
            me->DespawnOrUnsummon();
            return;
        }

        summons.DespawnAll();

        _JustDied();
    }
};

void AddSC_boss_rokmar_the_crackler()
{
    RegisterTheSlavePensCreatureAI(boss_rokmar_the_crackler);
}
