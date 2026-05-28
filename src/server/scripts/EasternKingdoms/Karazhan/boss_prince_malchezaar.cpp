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
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"

#include <cmath>
#include <list>
#include <map>
#include <vector>

enum PrinceSay
{
    SAY_AGGRO     = 0,
    SAY_AXE_TOSS1 = 1,
    SAY_AXE_TOSS2 = 2,
    SAY_SLAY      = 6,
    SAY_SUMMON    = 7,
    SAY_DEATH     = 8,
};

enum Spells
{
    SPELL_ENFEEBLE         = 30843,
    SPELL_ENFEEBLE_EFFECT  = 41624,
    SPELL_SHADOW_NOVA      = 30852,
    SPELL_SHADOW_WORD_PAIN = 30854,
    SPELL_THRASH_PASSIVE   = 12787,
    SPELL_SUNDER_ARMOR     = 30901,
    SPELL_THRASH_AURA      = 12787,
    SPELL_EQUIP_AXES       = 30857,
    SPELL_AMPLIFY_DAMAGE   = 39095,
    SPELL_CLEAVE           = 30131,
    SPELL_HELLFIRE         = 30859,
};

enum creatures
{
    NPC_NETHERSPITE_INFERNAL    = 17646,
    NPC_MALCHEZAARS_AXE         = 17650,
    INFERNAL_MODEL_INVISIBLE    = 11686,
    SPELL_INFERNAL_RELAY        = 33814,   // 30835,
    SPELL_INFERNAL_RELAY_ONE    = 30834,
    SPELL_INFERNAL_RELAY_TWO    = 30835,
    EQUIP_ID_AXE                = 33542
};

enum EventGroups
{
    GROUP_ENFEEBLE,
    GROUP_SHADOW_NOVA,
    GROUP_SHADOW_WORD_PAIN,
};

enum Phases
{
    PHASE_ONE   = 1,
    PHASE_TWO   = 2,
    PHASE_THREE = 3
};

enum MiscActions
{
    ACTION_ENFEEBLE_AURA_REMOVED = 1
};

static bool IsNPCBotUnit(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && creature->IsNPCBot() && creature->GetBotAI();
}

static void SafeBotMoveTo(Creature* bot, Position const& dest)
{
    if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
        return;

    float x = dest.GetPositionX();
    float y = dest.GetPositionY();
    float z = dest.GetPositionZ();

    if (!bot->CanFly())
        bot->UpdateAllowedPositionZ(x, y, z);

    Position p;
    p.Relocate(x, y, z, dest.GetOrientation());

    if (bot_ai* ai = bot->GetBotAI())
        ai->MoveToSendPosition(p);
}

struct boss_malchezaar : public BossAI
{
    boss_malchezaar(Creature* creature) : BossAI(creature, DATA_MALCHEZAAR) { }

    struct EnfeebledBotState
    {
        uint32 originalCommandState = 0;
        bool noCastSet = false;
        bool inactionSet = false;
        bool staySet = false;
    };

    std::list<Creature*> relays;
    std::list<Creature*> infernalTargets;

    void Initialize()
    {
        _phase = 1;
        clearweapons();
        relays.clear();
        infernalTargets.clear();
        _enfeebleTargets.clear();
        _enfeebledBots.clear();
    }

    void clearweapons()
    {
        SetEquipmentSlots(false, EQUIP_UNEQUIP, EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
        me->SetCanDualWield(false);
    }

    void Reset() override
    {
        ReleaseAllEnfeebledBots();
        EnfeebleResetHealth();
        Initialize();
        _Reset();

        ScheduleHealthCheckEvent(60, [&] {
            me->InterruptNonMeleeSpells(false);
            _phase = 2;
            DoCastSelf(SPELL_EQUIP_AXES);
            Talk(SAY_AXE_TOSS1);
            DoCastSelf(SPELL_THRASH_AURA, true);
            SetEquipmentSlots(false, EQUIP_ID_AXE, EQUIP_ID_AXE, EQUIP_NO_CHANGE);
            me->SetCanDualWield(true);
            me->SetAttackTime(OFF_ATTACK, (me->GetAttackTime(BASE_ATTACK) * 150) / 100);

            scheduler.Schedule(5s, 10s, [this](TaskContext context)
            {
                DoCastVictim(SPELL_SUNDER_ARMOR);
                context.Repeat();
            });

            scheduler.CancelGroup(GROUP_SHADOW_WORD_PAIN);
        });

        ScheduleHealthCheckEvent(30, [&] {
            me->RemoveAurasDueToSpell(SPELL_THRASH_AURA);
            Talk(SAY_AXE_TOSS2);
            _phase = PHASE_THREE;
            clearweapons();

            me->SummonCreature(NPC_MALCHEZAARS_AXE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 10000);

            scheduler.Schedule(20s, 30s, [this](TaskContext context)
            {
                DoCastRandomTarget(SPELL_AMPLIFY_DAMAGE, 0, 0.0f, true, false, false);
                context.Repeat();
            }).Schedule(20s, [this](TaskContext context)
            {
                DoCastRandomTarget(SPELL_SHADOW_WORD_PAIN);
                context.SetGroup(GROUP_SHADOW_WORD_PAIN);
                context.Repeat();
            });

            ReleaseAllEnfeebledBots();
            scheduler.CancelGroup(GROUP_ENFEEBLE);
        });
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        ReleaseAllEnfeebledBots();
        EnfeebleResetHealth();
        _JustDied();
        Talk(SAY_DEATH);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ReleaseAllEnfeebledBots();
        EnfeebleResetHealth();
        _EnterEvadeMode(why);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == ACTION_ENFEEBLE_AURA_REMOVED)
            HandleEnfeebleAuraRemoved(guid);
    }

    void SpawnInfernal(Creature* relay, Creature* target)
    {
        if (!relay || !target)
            return;

        if (Creature* infernal = relay->SummonCreature(NPC_NETHERSPITE_INFERNAL, target->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 180000))
        {
            infernal->SetDisplayId(INFERNAL_MODEL_INVISIBLE);
            relay->CastSpell(infernal, SPELL_INFERNAL_RELAY);
            infernal->SetFaction(me->GetFaction());
            infernal->SetControlled(true, UNIT_STATE_ROOT);
            relay->CastSpell(infernal, SPELL_INFERNAL_RELAY);
            summons.Summon(infernal);
        }
    }

    bool HasAvailableInfernalTarget(std::list<Creature*> const& targets) const
    {
        return !targets.empty();
    }

    Creature* PickTarget(std::list<Creature*> const& pickList)
    {
        if (pickList.empty())
            return nullptr;

        uint32 index = urand(0, uint32(pickList.size() - 1));
        uint32 counter = 0;
        for (Creature* creature : pickList)
        {
            if (counter == index)
            {
                return creature;
            }
            counter++;
        }
        return nullptr;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        _JustEngagedWith();

        me->GetCreaturesWithEntryInRange(relays, 250.0f, NPC_INFERNAL_RELAY);
        me->GetCreaturesWithEntryInRange(infernalTargets, 100.0f, NPC_INFERNAL_TARGET);

        scheduler.Schedule(30s, [this](TaskContext context)
        {
            DoCastAOE(SPELL_ENFEEBLE);

            scheduler.Schedule(9s, [this](TaskContext)
            {
                EnfeebleResetHealth();
            });

            context.SetGroup(GROUP_ENFEEBLE);
            context.Repeat();
        }).Schedule(35500ms, [this](TaskContext context)
        {
            DoCastAOE(SPELL_SHADOW_NOVA);
            context.SetGroup(GROUP_SHADOW_NOVA);
            context.Repeat(30s);
        }).Schedule(40s, [this](TaskContext context)
        {
            if (HasAvailableInfernalTarget(infernalTargets) && !relays.empty()) // only spawn infernal when the area is not full
            {
                Talk(SAY_SUMMON);
                Creature* infernalRelayOne = relays.back();
                Creature* infernalRelayTwo = relays.front();
                if (infernalRelayOne && infernalRelayTwo)
                {
                    infernalRelayOne->CastSpell(infernalRelayTwo, SPELL_INFERNAL_RELAY_ONE, true);

                    if (Creature* infernalTarget = PickTarget(infernalTargets))
                    {
                        infernalTargets.remove(infernalTarget);
                        SpawnInfernal(infernalRelayTwo, infernalTarget);

                        scheduler.Schedule(3min, [this, infernalTarget](TaskContext)
                        {
                            infernalTargets.push_back(infernalTarget); //adds to list again
                        });
                    }
                }
            }
            context.Repeat(_phase == PHASE_THREE ? 15s : 45s);
        }).Schedule(20s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_SHADOW_WORD_PAIN);
            context.SetGroup(GROUP_SHADOW_WORD_PAIN);
            context.Repeat();
        });
    }

    void SpellHitTarget(Unit* target, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_ENFEEBLE)
        {
            _enfeebleTargets[target->GetGUID()] = target->GetHealth();
            target->SetHealth(1);

            if (Creature* bot = target->ToCreature())
                if (IsNPCBotUnit(bot))
                    HandleEnfeebledBot(bot);
        }
    }

    void RestoreEnfeebledTargetHealth(ObjectGuid const& guid)
    {
        std::map<ObjectGuid, uint32>::iterator itr = _enfeebleTargets.find(guid);
        if (itr == _enfeebleTargets.end())
            return;

        if (Unit* target = ObjectAccessor::GetUnit(*me, guid))
        {
            if (target->IsAlive())
                target->SetHealth(itr->second);
        }

        _enfeebleTargets.erase(itr);
    }

    void HandleEnfeebleAuraRemoved(ObjectGuid const& guid)
    {
        RestoreEnfeebledTargetHealth(guid);
        ReleaseEnfeebledBot(guid);
        _enfeebledBots.erase(guid);
    }

    void EnfeebleResetHealth()
    {
        for (std::map<ObjectGuid, uint32>::iterator itr = _enfeebleTargets.begin(); itr != _enfeebleTargets.end();)
        {
            if (Unit* target = ObjectAccessor::GetUnit(*me, itr->first))
            {
                if (target->IsAlive())
                    target->SetHealth(itr->second);
            }

            itr = _enfeebleTargets.erase(itr);
        }
    }

    void ReleaseEnfeebledBot(ObjectGuid const& guid)
    {
        std::map<ObjectGuid, EnfeebledBotState>::iterator stateItr = _enfeebledBots.find(guid);
        if (stateItr == _enfeebledBots.end())
            return;

        Creature* bot = ObjectAccessor::GetCreature(*me, guid);
        if (!bot)
            return;

        if (bot_ai* ai = bot->GetBotAI())
        {
            EnfeebledBotState const& state = stateItr->second;

            if (state.inactionSet)
                ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

            if (state.noCastSet)
                ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);

            if (state.staySet)
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);

            if (bot->IsAlive() && me->IsAlive() && me->IsEngaged() && me->GetVictim())
            {
                ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                ai->AttackStart(me);
            }
            else if (bot->IsAlive() && !ai->IAmFree())
            {
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
            }
        }
    }

    void ReleaseAllEnfeebledBots()
    {
        std::vector<ObjectGuid> botGuids;
        botGuids.reserve(_enfeebledBots.size());

        for (std::map<ObjectGuid, EnfeebledBotState>::value_type const& pair : _enfeebledBots)
            botGuids.push_back(pair.first);

        for (ObjectGuid const& guid : botGuids)
            ReleaseEnfeebledBot(guid);

        _enfeebledBots.clear();
    }

    bool IsUnitSafelyAwayFromShadowNova(Unit* unit) const
    {
        return unit && unit->GetExactDist2d(me) >= 35.0f;
    }

    Position GetEnfeebleRetreatPosition(Creature* bot) const
    {
        if (!bot || IsUnitSafelyAwayFromShadowNova(bot))
        {
            Position current;
            if (bot)
                current.Relocate(bot);

            return current;
        }

        static constexpr float Pi = 3.14159265f;
        static constexpr float DesiredDistance = 40.0f;

        float dx = bot->GetPositionX() - me->GetPositionX();
        float dy = bot->GetPositionY() - me->GetPositionY();
        float length = std::sqrt(dx * dx + dy * dy);

        if (length <= 0.1f)
        {
            float angle = Position::NormalizeOrientation(me->GetOrientation() + Pi);
            dx = std::cos(angle);
            dy = std::sin(angle);
            length = 1.0f;
        }

        dx /= length;
        dy /= length;

        float x = me->GetPositionX() + dx * DesiredDistance;
        float y = me->GetPositionY() + dy * DesiredDistance;
        float z = bot->GetPositionZ();
        float orientation = Position::NormalizeOrientation(std::atan2(dy, dx));

        Position retreat;
        retreat.Relocate(x, y, z, orientation);
        return retreat;
    }

    void HandleEnfeebledBot(Creature* bot)
    {
        if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        // Enfeeble should already exclude the current victim, but keep the tank out
        // of scripted movement if custom target filtering ever changes.
        if (me->GetVictim() == bot)
            return;

        ObjectGuid botGuid = bot->GetGUID();
        std::map<ObjectGuid, EnfeebledBotState>::iterator stateItr = _enfeebledBots.find(botGuid);
        if (stateItr == _enfeebledBots.end())
        {
            EnfeebledBotState state;
            state.originalCommandState = ai->GetBotCommandState();
            state.inactionSet = !(state.originalCommandState & BOT_COMMAND_INACTION);
            state.noCastSet = !(state.originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY);
            state.staySet = !(state.originalCommandState & BOT_COMMAND_STAY);
            stateItr = _enfeebledBots.emplace(botGuid, state).first;
        }

        EnfeebledBotState const& state = stateItr->second;

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();

        if (state.inactionSet)
            ai->SetBotCommandState(BOT_COMMAND_INACTION);

        if (state.staySet)
            ai->SetBotCommandState(BOT_COMMAND_STAY);

        if (uint32 originalNoCast = state.originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY)
            ai->SetBotCommandState(originalNoCast);
        else
            ai->SetBotCommandState(BOT_COMMAND_NO_CAST);

        Position retreat = GetEnfeebleRetreatPosition(bot);
        SafeBotMoveTo(bot, retreat);

        scheduler.Schedule(2500ms, [this, botGuid](TaskContext)
        {
            Creature* bot = ObjectAccessor::GetCreature(*me, botGuid);
            if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
                return;

            if (!IsUnitSafelyAwayFromShadowNova(bot))
                SafeBotMoveTo(bot, GetEnfeebleRetreatPosition(bot));
        });

        scheduler.Schedule(9500ms, [this, botGuid](TaskContext)
        {
            ReleaseEnfeebledBot(botGuid);
            _enfeebledBots.erase(botGuid);
        });
    }

    private:
        uint32 _phase;
        std::map<ObjectGuid, uint32> _enfeebleTargets;
        std::map<ObjectGuid, EnfeebledBotState> _enfeebledBots;
};

struct npc_netherspite_infernal : public ScriptedAI
{
    npc_netherspite_infernal(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

    void KilledUnit(Unit* who) override
    {
        if (me->ToTempSummon())
        {
            if (WorldObject* summoner = me->ToTempSummon()->GetSummoner())
            {
                if (Creature* creature = summoner->ToCreature())
                {
                    creature->AI()->KilledUnit(who);
                }
            }
        }
    }

    void SpellHit(Unit* /*who*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_INFERNAL_RELAY)
        {
            me->SetDisplayId(me->GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID));
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);

            scheduler.Schedule(4s, [this](TaskContext /*context*/)
            {
                DoCastSelf(SPELL_HELLFIRE);
            });
        }
    }

    void DamageTaken(Unit* /*done_by*/, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        damage = 0;
    }
};

struct npc_malchezaar_axe : public ScriptedAI
{
    npc_malchezaar_axe(Creature* creature) : ScriptedAI(creature)
    {
        creature->SetCanDualWield(true);
    }

    void Initialize()
    {
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
        scheduler.Schedule(7500ms, [this](TaskContext context)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
            {
                if (me->GetVictim())
                {
                    DoModifyThreatByPercent(me->GetVictim(), -100);
                }

                me->AddThreat(target, 1000000.0f);
            }

            context.Repeat(7500ms, 20s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff,
            std::bind(&ScriptedAI::DoMeleeAttackIfReady, this));
    }
};

// 30843 - Enfeeble
class spell_malchezaar_enfeeble : public SpellScript
{
    PrepareSpellScript(spell_malchezaar_enfeeble);

    bool Load() override
    {
        return GetCaster()->ToCreature();
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        uint8 maxSize = 5;
        Unit* caster = GetCaster();

        targets.remove_if([caster](WorldObject const* target) -> bool
        {
            // Should not target current victim.
            return caster->GetVictim() == target;
        });

        if (targets.size() > maxSize)
        {
            Acore::Containers::RandomResize(targets, maxSize);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_malchezaar_enfeeble::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};


// 41624 - Enfeeble effect
class spell_malchezaar_enfeeble_effect : public AuraScript
{
    PrepareAuraScript(spell_malchezaar_enfeeble_effect);

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        InstanceScript* instance = target->GetInstanceScript();
        if (!instance)
            return;

        Creature* prince = instance->GetCreature(DATA_MALCHEZAAR);
        if (!prince || !prince->AI())
            return;

        prince->AI()->SetGUID(target->GetGUID(), ACTION_ENFEEBLE_AURA_REMOVED);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_malchezaar_enfeeble_effect::HandleRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_malchezaar()
{
    RegisterKarazhanCreatureAI(boss_malchezaar);
    RegisterKarazhanCreatureAI(npc_malchezaar_axe);
    RegisterKarazhanCreatureAI(npc_netherspite_infernal);
    RegisterSpellScript(spell_malchezaar_enfeeble);
    RegisterSpellScript(spell_malchezaar_enfeeble_effect);
}
