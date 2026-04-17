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
#include "Creature.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "molten_core.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Config.h"

 //Dinkle custom
#include "bot_ai.h"
#include "botmgr.h"
#include "botdatamgr.h"

enum Texts
{
    EMOTE_LOWHP = 0,
};

enum Spells
{
    // Golemagg
    SPELL_PYROBLAST = 20228,
    SPELL_EARTHQUAKE = 19798,
    SPELL_ATTRACK_RAGER = 20544,
    SPELL_MAGMASPLASH = 13879,
    SPELL_GOLEMAGG_TRUST_AURA = 20556,
    SPELL_DOUBLE_ATTACK = 18943,

    // Core Rager
    SPELL_MANGLE = 19820,
    SPELL_FULL_HEAL = 17683,
};

struct boss_golemagg : public BossAI
{
    boss_golemagg(Creature* creature) : BossAI(creature, DATA_GOLEMAGG),
        earthquakeTimer(0),
        pyroblastTimer(0),
        enraged(false)
    {
    }

    void Reset() override
    {
        _Reset();
        earthquakeTimer = 0;
        pyroblastTimer = urand(3000, 7000);
        enraged = false;
        DoCastSelf(SPELL_MAGMASPLASH);
        DoCastSelf(SPELL_GOLEMAGG_TRUST_AURA);
        DoCastSelf(SPELL_DOUBLE_ATTACK);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (!enraged && me->HealthBelowPctDamaged(10, damage))
        {
            DoCastSelf(SPELL_ATTRACK_RAGER, true);
            DoCastAOE(SPELL_EARTHQUAKE, true);
            earthquakeTimer = 5000;
            enraged = true;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        // Should not get impact by cast state (cast should always happen)
        if (earthquakeTimer)
        {
            if (earthquakeTimer <= diff)
            {
                DoCastSelf(SPELL_EARTHQUAKE, true);
                earthquakeTimer = 5000;
            }
            else
                earthquakeTimer -= diff;
        }

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (pyroblastTimer <= diff)
        {
            DoCastRandomTarget(SPELL_PYROBLAST);
            pyroblastTimer = 7000;
        }
        else
            pyroblastTimer -= diff;

        DoMeleeAttackIfReady();
    }

private:
    uint32 earthquakeTimer;
    uint32 pyroblastTimer;
    bool enraged;
};

struct npc_core_rager : public ScriptedAI
{
    npc_core_rager(Creature* creature) : ScriptedAI(creature),
        instance(creature->GetInstanceScript()),
        mangleTimer(7000),
        rangeCheckTimer(1000)
    {
    }

    void Reset() override
    {
        mangleTimer = 7000;
        rangeCheckTimer = 1000;

        if (instance->GetBossState(DATA_GOLEMAGG) == DONE)
            DoCastSelf(SPELL_CORE_RAGER_QUIET_SUICIDE, true);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*dmgType*/, SpellSchoolMask /*school*/) override
    {
        // Just in case if something will go bad, let players kill this creature
        if (instance->GetBossState(DATA_GOLEMAGG) == DONE)
            return;

        if (me->HealthBelowPctDamaged(50, damage))
        {
            damage = 0;
            DoCastSelf(SPELL_FULL_HEAL, true);
            Talk(EMOTE_LOWHP);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        // Should have no impact from unit state
        if (rangeCheckTimer <= diff)
        {
            Creature const* golemagg = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_GOLEMAGG));
            if (golemagg && me->GetDistance(golemagg) > 100.0f)
            {
                instance->DoAction(ACTION_RESET_GOLEMAGG_ENCOUNTER);
                return;
            }

            rangeCheckTimer = 1000;
        }
        else
            rangeCheckTimer -= diff;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (mangleTimer <= diff)
        {
            DoCastVictim(SPELL_MANGLE);
            mangleTimer = 10000;
        }
        else
            mangleTimer -= diff;

        DoMeleeAttackIfReady();
    }

private:
    InstanceScript* instance;
    uint32 mangleTimer;
    uint32 rangeCheckTimer;
};

//Dinkle custom start
static inline bool IsNPCBot(Unit const* u)
{
    if (!u || u->GetTypeId() != TYPEID_UNIT)
        return false;

    if (Creature const* c = u->ToCreature())
        return c->IsNPCBot();

    return false;
}

class spell_magma_splash_bot_stack_cap : public SpellScript
{
    PrepareSpellScript(spell_magma_splash_bot_stack_cap);

    static uint8 GetBotMaxStacks()
    {
        static uint8 sMax = []() -> uint8
            {
                int v = sConfigMgr->GetOption<int>("MoltenCore.MagmaSplash.BotMaxStacks", 10);
                if (v < 1)
                    v = 1;
                if (v > 255)
                    v = 255;
                return static_cast<uint8>(v);
            }();
        return sMax;
    }

    void HandleAfterHit()
    {
        Unit* target = GetHitUnit();
        if (!target || !target->IsAlive())
            return;

        if (!IsNPCBot(target))
            return;

        uint32 const spellId = GetSpellInfo()->Id;

        if (Aura* a = target->GetAura(spellId))
        {
            uint8 const maxStacks = GetBotMaxStacks();
            if (a->GetStackAmount() > maxStacks)
                a->Remove();
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_magma_splash_bot_stack_cap::HandleAfterHit);
    }
};
//Dinkle custom end

void AddSC_boss_golemagg()
{
    RegisterMoltenCoreCreatureAI(boss_golemagg);
    RegisterMoltenCoreCreatureAI(npc_core_rager);

    //Dinkle custom
    RegisterSpellScript(spell_magma_splash_bot_stack_cap);
}
