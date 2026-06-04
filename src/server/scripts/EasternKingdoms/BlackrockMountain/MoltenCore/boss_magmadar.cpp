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
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "molten_core.h"

#include <string>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMe(Creature* bot, uint32 spellId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Texts
{
    EMOTE_FRENZY = 0,
};

enum Spells
{
    SPELL_FRENZY = 19451,
    SPELL_MAGMA_SPIT = 19449,
    SPELL_PANIC = 19408,
    SPELL_LAVA_BOMB = 19411, // Calls dummy → 20494 → spawns GO 177704 (30s)
    SPELL_LAVA_BOMB_EFFECT = 20494, // Spawns trap GO 177704 which triggers 19428
    SPELL_LAVA_BOMB_RANGED = 20474, // Calls dummy → 20495 → spawns GO 177704 (60s)
    SPELL_LAVA_BOMB_RANGED_EFFECT = 20495, // Spawns trap GO 177704 which triggers 19428
};

enum Events
{
    EVENT_FRENZY = 1,
    EVENT_PANIC,
    EVENT_LAVA_BOMB,
    EVENT_LAVA_BOMB_RANGED,
};

constexpr float MELEE_TARGET_LOOKUP_DIST = 10.0f;

struct boss_magmadar : public BossAI
{
    boss_magmadar(Creature* creature) : BossAI(creature, DATA_MAGMADAR) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        events.ScheduleEvent(EVENT_FRENZY, 8500ms);
        events.ScheduleEvent(EVENT_PANIC, 9500ms);
        events.ScheduleEvent(EVENT_LAVA_BOMB, 12s);
        events.ScheduleEvent(EVENT_LAVA_BOMB_RANGED, 15s);
    }

    //Dinkle custom
    bool IsPlayerOrNpcBotTarget(Unit* target) const
    {
        if (!target || !target->IsAlive())
            return false;

        bool const isPlayer = target->GetTypeId() == TYPEID_PLAYER;
        bool const isNpcBot = target->GetTypeId() == TYPEID_UNIT && target->ToCreature() && target->ToCreature()->IsNPCBot();
        if (!isPlayer && !isNpcBot)
            return false;

        if (!me->IsValidAttackTarget(target))
            return false;

        return true;
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
        case EVENT_FRENZY:
        {
            Talk(EMOTE_FRENZY);
            DoCastSelf(SPELL_FRENZY);
            events.Repeat(15s, 20s);
            break;
        }
        case EVENT_PANIC:
        {
            DoCastVictim(SPELL_PANIC);
            events.Repeat(31s, 38s);
            break;
        }
        case EVENT_LAVA_BOMB:
        {
            //Dinkle custom
            std::list<Unit*> candidates;
            SelectTargetList(candidates, 1, SelectTargetMethod::Random, 0, [this](Unit* u)
                {
                    return IsPlayerOrNpcBotTarget(u) && u->GetDistance(me) <= MELEE_TARGET_LOOKUP_DIST;
                });

            if (!candidates.empty())
            {
                DoCast(candidates.front(), SPELL_LAVA_BOMB);
                if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(candidates.front()))
                    DBMFTABotCallouts::AnnounceMoveAwayFromMe(bot, SPELL_LAVA_BOMB, "Lava Bomb", DBMFTABotCallouts::GetCooldownMs());
            }

            events.Repeat(12s, 15s);
            break;
        }
        case EVENT_LAVA_BOMB_RANGED:
        {
            //Dinkle custom
            std::list<Unit*> targets;
            SelectTargetList(targets, 1, SelectTargetMethod::Random, 1, [this](Unit* target)
                {
                    if (!IsPlayerOrNpcBotTarget(target))
                        return false;

                    float d = target->GetDistance(me);
                    return d > MELEE_TARGET_LOOKUP_DIST && d < 100.0f;
                });

            if (!targets.empty())
            {
                DoCast(targets.front(), SPELL_LAVA_BOMB_RANGED);
                if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(targets.front()))
                    DBMFTABotCallouts::AnnounceMoveAwayFromMe(bot, SPELL_LAVA_BOMB_RANGED, "Ranged Lava Bomb", DBMFTABotCallouts::GetCooldownMs());
            }

            events.Repeat(12s, 15s);
            break;
        }
        default:
            break;
        }
    }
};

// 19411 Lava Bomb
// 20474 Lava Bomb
class spell_magmadar_lava_bomb : public SpellScript
{
    PrepareSpellScript(spell_magmadar_lava_bomb);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LAVA_BOMB_EFFECT, SPELL_LAVA_BOMB_RANGED_EFFECT });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            uint32 spellId = 0;
            switch (m_scriptSpellId)
            {
            case SPELL_LAVA_BOMB:
                spellId = SPELL_LAVA_BOMB_EFFECT;
                break;
            case SPELL_LAVA_BOMB_RANGED:
                spellId = SPELL_LAVA_BOMB_RANGED_EFFECT;
                break;
            default:
                return;
            }

            // Spawn the trap GO credited to Magmadar
            target->CastSpell(target, spellId, true, nullptr, nullptr, GetCaster()->GetGUID());
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_magmadar_lava_bomb::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_magmadar()
{
    RegisterMoltenCoreCreatureAI(boss_magmadar);
    RegisterSpellScript(spell_magmadar_lava_bomb);
}
