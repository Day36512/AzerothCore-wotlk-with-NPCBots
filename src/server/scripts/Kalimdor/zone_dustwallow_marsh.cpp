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
#include "SpellScript.h"
#include "SpellScriptLoader.h"

enum SpellScripts
{
    SPELL_OOZE_ZAP              = 42489,
    SPELL_OOZE_ZAP_CHANNEL_END  = 42485,
    SPELL_OOZE_CHANNEL_CREDIT   = 42486,
    SPELL_ENERGIZED             = 42492,
};

class spell_ooze_zap : public SpellScript
{
    PrepareSpellScript(spell_ooze_zap);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_OOZE_ZAP });
    }

    SpellCastResult CheckRequirement()
    {
        if (!GetCaster()->HasAura(GetSpellInfo()->Effects[EFFECT_1].CalcValue()))
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW; // This is actually correct

        if (!GetExplTargetUnit())
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (GetHitUnit())
            GetCaster()->CastSpell(GetHitUnit(), uint32(GetEffectValue()), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_ooze_zap::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnCheckCast += SpellCheckCastFn(spell_ooze_zap::CheckRequirement);
    }
};

class spell_ooze_zap_channel_end : public SpellScript
{
    PrepareSpellScript(spell_ooze_zap_channel_end);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_OOZE_ZAP_CHANNEL_END });
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Player* player = GetCaster()->ToPlayer())
            player->CastSpell(player, SPELL_OOZE_CHANNEL_CREDIT, true);
        Unit::Kill(GetHitUnit(), GetHitUnit());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_ooze_zap_channel_end::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_energize_aoe : public SpellScript
{
    PrepareSpellScript(spell_energize_aoe);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ENERGIZED });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end();)
        {
            if ((*itr)->IsPlayer() && (*itr)->ToPlayer()->GetQuestStatus(GetSpellInfo()->Effects[EFFECT_1].CalcValue()) == QUEST_STATUS_INCOMPLETE)
                ++itr;
            else
                targets.erase(itr++);
        }
        targets.push_back(GetCaster());
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetCaster()->CastSpell(GetCaster(), uint32(GetEffectValue()), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_energize_aoe::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_energize_aoe::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_energize_aoe::FilterTargets, EFFECT_1, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

enum TheramorePracticingGuardData
{
    NPC_THERAMORE_PRACTICING_GUARD = 4951,
    NPC_THERAMORE_COMBAT_DUMMY = 4952,

    EVENT_THERAMORE_START_PRACTICE = 1,
    EVENT_THERAMORE_STOP_PRACTICE,
    EVENT_THERAMORE_SIT_DOWN,
    EVENT_THERAMORE_STAND_UP
};

// vMaNGOS uses 5 yards for both the OOC condition and the attack target search.
static constexpr float THERAMORE_DUMMY_SEARCH_RANGE = 5.0f;

struct npc_theramore_practicing_guard : public ScriptedAI
{
    npc_theramore_practicing_guard(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        _events.Reset();
        _dummyGUID.Clear();

        _practicing = false;
        _fledLowHealth = false;

        me->SetStandState(UNIT_STAND_STATE_STAND);
        _events.ScheduleEvent(EVENT_THERAMORE_START_PRACTICE, 1s);
    }

    void JustEngagedWith(Unit* who) override
    {
        // Starting practice on the combat dummy should not wipe the practice timers.
        if (IsPracticeTarget(who))
            return;

        // Real combat interrupts the dummy routine.
        _events.Reset();
        _dummyGUID.Clear();

        _practicing = false;
        me->SetStandState(UNIT_STAND_STATE_STAND);
    }

    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (!attacker || IsPracticeTarget(attacker))
            return;

        // Do not let the guard ignore a real hostile attacker because he is busy
        // heroically mugging a wooden post.
        if (_practicing && me->IsValidAttackTarget(attacker))
            BreakPracticeForRealCombat(attacker);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_THERAMORE_START_PRACTICE:
                StartPractice();
                break;

            case EVENT_THERAMORE_STOP_PRACTICE:
                StopPracticeAndRest();
                break;

            case EVENT_THERAMORE_SIT_DOWN:
                if (!me->IsInCombat())
                    me->SetStandState(UNIT_STAND_STATE_SIT);
                break;

            case EVENT_THERAMORE_STAND_UP:
                me->SetStandState(UNIT_STAND_STATE_STAND);
                break;

            default:
                break;
            }
        }

        if (!UpdateVictim())
            return;

        // vMaNGOS has a low-health flee event at 15%. Keep that behavior for
        // real combat, but never let dummy practice trigger it.
        if (!IsPracticeTarget(me->GetVictim()))
        {
            if (!_fledLowHealth && me->HealthBelowPct(15))
            {
                _fledLowHealth = true;
                me->DoFleeToGetAssistance();
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _dummyGUID;

    bool _practicing = false;
    bool _fledLowHealth = false;

    static bool IsPracticeTarget(Unit const* unit)
    {
        return unit && unit->GetTypeId() == TYPEID_UNIT && unit->GetEntry() == NPC_THERAMORE_COMBAT_DUMMY;
    }

    Creature* GetPracticeDummy() const
    {
        if (!_dummyGUID.IsEmpty())
            if (Creature* dummy = ObjectAccessor::GetCreature(*me, _dummyGUID))
                return dummy;

        if (Unit* victim = me->GetVictim())
            if (IsPracticeTarget(victim))
                return victim->ToCreature();

        return nullptr;
    }

    void StartPractice()
    {
        if (me->IsInCombat())
        {
            _events.ScheduleEvent(EVENT_THERAMORE_START_PRACTICE, 5s);
            return;
        }

        Creature* dummy = me->FindNearestCreature(NPC_THERAMORE_COMBAT_DUMMY, THERAMORE_DUMMY_SEARCH_RANGE, true);
        if (!dummy)
        {
            // Match the visible idle state: no dummy found, so sit and retry.
            me->SetStandState(UNIT_STAND_STATE_SIT);
            _events.ScheduleEvent(EVENT_THERAMORE_START_PRACTICE, 5s);
            return;
        }

        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetFacingToObject(dummy);

        _dummyGUID = dummy->GetGUID();
        _practicing = true;

        AttackStart(dummy);

        // If AttackStart failed because the dummy template/faction is wrong,
        // do not get stuck in a fake practice state.
        if (!IsPracticeTarget(me->GetVictim()))
        {
            _dummyGUID.Clear();
            _practicing = false;
            _events.ScheduleEvent(EVENT_THERAMORE_START_PRACTICE, 5s);
            return;
        }

        _events.ScheduleEvent(EVENT_THERAMORE_STOP_PRACTICE, 120s);
    }

    void StopPracticeAndRest()
    {
        if (!_practicing)
            return;

        if (Creature* dummy = GetPracticeDummy())
            dummy->CombatStop(true);

        me->CombatStop(true);
        me->AttackStop();

        _dummyGUID.Clear();
        _practicing = false;

        // Keeps them from slowly creeping into the dummy over repeated cycles.
        me->GetMotionMaster()->MoveTargetedHome();

        // vMaNGOS does: sit at 2s, stand at 30s, reset phase at 32s.
        // Since our script has no EventAI phase mask, simply restart one second
        // after that reset point.
        _events.ScheduleEvent(EVENT_THERAMORE_SIT_DOWN, 2s);
        _events.ScheduleEvent(EVENT_THERAMORE_STAND_UP, 30s);
        _events.ScheduleEvent(EVENT_THERAMORE_START_PRACTICE, 33s);
    }

    void BreakPracticeForRealCombat(Unit* attacker)
    {
        if (Creature* dummy = GetPracticeDummy())
            dummy->CombatStop(true);

        _events.Reset();
        _dummyGUID.Clear();

        _practicing = false;

        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->CombatStop(true);
        me->AttackStop();

        AttackStart(attacker);
    }
};

void AddSC_dustwallow_marsh()
{
    RegisterSpellScript(spell_ooze_zap);
    RegisterSpellScript(spell_ooze_zap_channel_end);
    RegisterSpellScript(spell_energize_aoe);

    RegisterCreatureAI(npc_theramore_practicing_guard);
}
