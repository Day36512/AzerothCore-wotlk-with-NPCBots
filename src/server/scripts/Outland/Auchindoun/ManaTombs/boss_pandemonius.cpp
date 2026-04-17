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
#include "Player.h"
#include "ScriptedCreature.h"
#include "mana_tombs.h"

enum Texts
{
    SAY_AGGRO = 0,
    SAY_KILL = 1,
    SAY_DEATH = 2,
    EMOTE_DARK_SHELL = 3
};

enum Spells
{
    SPELL_VOID_BLAST = 32325,
    SPELL_DARK_SHELL = 300396,  // custom Dark Shell
    SPELL_VEIL_OF_SHADOW = 300392,  // custom: Veil of Shadow (random single target)
    SPELL_SHADOW_WORD_PAIN_SINGLE = 300395   // custom: SW:P applied individually to players/bots
};

enum Groups
{
    GROUP_VOID_BLAST = 1,
    GROUP_VEIL_OF_SHADOW = 2
};

enum RoomAdds
{
    NPC_SCAVENGER = 18309,
    NPC_CRYPT_RAIDER = 18311,
    NPC_SORCERER = 18313,
};

float const ROOM_PULL_RANGE = 70.0f;
float const ROOM_ENTERANCE = -50.0f;
float const ROOM_EXIT = -145.0f;

constexpr uint8 MAX_VOID_BLAST = 5;
constexpr uint8 SHADOW_WORD_PAIN_STAGES = 6; // 85, 70, 55, 40, 25, 10

struct boss_pandemonius : public BossAI
{
    boss_pandemonius(Creature* creature)
        : BossAI(creature, DATA_PANDEMONIUS),
        _shadowWordPainStage(0)
    {
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    void Reset() override
    {
        BossAI::Reset();
        _shadowWordPainStage = 0; // stages: 0..5  (85,70,55,40,25,10)
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO);

        scheduler
            // Dark Shell loop
            .Schedule(20s, GROUP_VOID_BLAST, [this](TaskContext context)
                {
                    if (me->IsNonMeleeSpellCast(false))
                        me->InterruptNonMeleeSpells(true);

                    Talk(EMOTE_DARK_SHELL);
                    CastDarkShellCustom();
                    context.Repeat();
                })
            // Void Blast chain (bot-aware random target)
            .Schedule(8s, 23s, [this](TaskContext context)
                {
                    if (!(context.GetRepeatCounter() % (MAX_VOID_BLAST + 1)))
                    {
                        context.Repeat(15s, 25s);
                    }
                    else
                    {
                        DoCastOnRandomPlayerOrNPCBot(SPELL_VOID_BLAST);

                        context.Repeat(500ms);
                        context.DelayGroup(GROUP_VOID_BLAST, 500ms);
                    }
                })
            // Veil of Shadow: periodic random target (players + NPCBots)
            .Schedule(10s, 15s, GROUP_VEIL_OF_SHADOW, [this](TaskContext context)
                {
                    DoCastOnRandomPlayerOrNPCBot(SPELL_VEIL_OF_SHADOW, 100.0f);
                    context.Repeat(12s, 18s);
                })
            // Shadow Word: Pain thresholds
            .Schedule(1s, [this](TaskContext context)
                {
                    if (!me->IsAlive())
                        return;

                    // stages: 0..5 -> thresholds 85, 70, 55, 40, 25, 10
                    if (_shadowWordPainStage < SHADOW_WORD_PAIN_STAGES)
                    {
                        int32 threshold = 85 - 15 * static_cast<int32>(_shadowWordPainStage); // 85, 70, 55, ...
                        // HealthBelowPct triggers when health% < X.
                        // Use threshold+1 so it effectively fires at <= threshold.
                        if (me->HealthBelowPct(threshold + 1))
                        {
                            CastShadowWordPainOnAllPlayersAndBots();
                            ++_shadowWordPainStage;
                        }

                        // Keep polling until we've fired all stages or we die
                        if (_shadowWordPainStage < SHADOW_WORD_PAIN_STAGES && me->IsAlive())
                            context.Repeat(1s);
                    }
                })
            // Room pull on engage
            .Schedule(0s, [this](TaskContext)
                {
                    PullRoom();
                });

        BossAI::JustEngagedWith(who);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_KILL);
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void PullRoom()
    {
        std::list<Creature*> creatureList;
        GetCreatureListWithEntryInGrid(creatureList, me, NPC_SCAVENGER, ROOM_PULL_RANGE);
        GetCreatureListWithEntryInGrid(creatureList, me, NPC_CRYPT_RAIDER, ROOM_PULL_RANGE);
        GetCreatureListWithEntryInGrid(creatureList, me, NPC_SORCERER, ROOM_PULL_RANGE);

        for (Creature* creature : creatureList)
        {
            if (creature && (creature->GetPositionY() < ROOM_ENTERANCE && creature->GetPositionY() > ROOM_EXIT))
                creature->SetInCombatWithZone();
        }

        creatureList.clear();
    }

private:
    // Custom Dark Shell: difficulty-based basepoints on bp0 and bp1.
    void CastDarkShellCustom()
    {
        int32 bp0 = IsHeroic() ? 38000 : 13000;
        int32 bp1 = IsHeroic() ? 250 : 100;

        // Override effects 0 and 1; leave effect 2 to DB (nullptr).
        me->CastCustomSpell(me, SPELL_DARK_SHELL, &bp0, &bp1, nullptr, true);
    }

    // Apply single-target Shadow Word: Pain (300395) to EVERY hostile player or NPCBot on the threat list.
    void CastShadowWordPainOnAllPlayersAndBots()
    {
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* target = ref->GetVictim();
            if (!target || !target->IsAlive())
                continue;

            if (!target->IsPlayer() && !target->IsNPCBot())
                continue;

            // Triggered = true so we can apply to multiple targets in one tick without GCD/cast-time blocking
            DoCast(target, SPELL_SHADOW_WORD_PAIN_SINGLE, true);
        }
    }

    // Select a random hostile that is either a real player or an NPCBot.
    // range = 0 means no additional range check beyond being on the threat list.
    Unit* SelectRandomPlayerOrNPCBot(float range = 0.0f)
    {
        std::list<Unit*> targets;

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* target = ref->GetVictim();
            if (!target || !target->IsAlive())
                continue;

            // Optional range filter
            if (range > 0.0f && !me->IsWithinDistInMap(target, range))
                continue;

            // skip pets, totems, guardians etc.
            if (!target->IsPlayer() && !target->IsNPCBot())
                continue;

            targets.push_back(target);
        }

        if (targets.empty())
            return nullptr;

        uint32 index = urand(0, static_cast<uint32>(targets.size() - 1));
        auto it = targets.begin();
        std::advance(it, index);
        return *it;
    }

    void DoCastOnRandomPlayerOrNPCBot(uint32 spellId, float range = 0.0f)
    {
        if (Unit* target = SelectRandomPlayerOrNPCBot(range))
            DoCast(target, spellId);
    }

    uint8 _shadowWordPainStage; // 0..5
};

void AddSC_boss_pandemonius()
{
    RegisterManaTombsCreatureAI(boss_pandemonius);
}
