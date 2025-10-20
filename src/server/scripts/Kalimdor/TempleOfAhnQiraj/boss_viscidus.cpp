/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
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
#include "temple_of_ahnqiraj.h"
#include "Config.h"
#include "Log.h"

enum Spells
{
    // Viscidus - Glob of Viscidus
    SPELL_POISON_SHOCK = 25993,
    SPELL_POISONBOLT_VOLLEY = 25991,
    SPELL_SUMMON_TOXIN_SLIME = 26584,
    SPELL_SUMMON_TOXIN_SLIME_2 = 26577,
    SPELL_VISCIDUS_SLOWED = 26034,
    SPELL_VISCIDUS_SLOWED_MORE = 26036,
    SPELL_VISCIDUS_FREEZE = 300319, // Frozen Solid visual + stun
    SPELL_REJOIN_VISCIDUS = 25896,
    SPELL_EXPLODE_TRIGGER = 25938,
    SPELL_VISCIDUS_SHRINKS = 25893, // Server-side
    SPELL_INVIS_SELF = 25905,
    SPELL_VISCIDUS_GROWS = 25897,
    SPELL_STUN_SELF = 25900,

    // Toxic slime
    SPELL_TOXIN = 26575,
};

enum Events
{
    EVENT_POISONBOLT_VOLLEY = 1,
    EVENT_POISON_SHOCK = 2,
    EVENT_TOXIN = 3,
    EVENT_RESET_PHASE = 4,
    EVENT_GLOB_WATCHDOG = 5,
};

enum Phases
{
    PHASE_FROST = 1,
    PHASE_MELEE = 2,
    PHASE_GLOB = 3
};

enum Emotes
{
    EMOTE_SLOW = 0,
    EMOTE_FREEZE = 1,
    EMOTE_FROZEN = 2,

    EMOTE_CRACK = 3,
    EMOTE_SHATTER = 4,
    EMOTE_EXPLODE = 5
};

enum MovePoints
{
    ROOM_CENTER = 1
};

enum Misc
{
    MAX_GLOB_SPAWN = 20,
};

// Use the arena center as both leash and home.
static Position const roomCenter = { -7992.36f, 908.19f, -52.62f, 1.68f };
// Old resetPoint is up the ramp; we’ll leash to center instead of evading outright.
static Position const leashCenter = roomCenter;
static float   const leashRadius = 95.0f; // generous ring that covers the fight space

// Pre-resolved spells for spawning globs
static std::array<uint32, MAX_GLOB_SPAWN> const spawnGlobSpells =
{ { 25865, 25866, 25867, 25868, 25869, 25870, 25871, 25872, 25873, 25874,
    25875, 25876, 25877, 25878, 25879, 25880, 25881, 25882, 25883, 25884 } };

// -------------------- Config helpers --------------------

namespace ViscidusCfg
{
    // Defaults (blizzlike-ish)
    constexpr uint32 DEF_SLOW = 100;
    constexpr uint32 DEF_SLOW_MORE = 150;
    constexpr uint32 DEF_FREEZE = 200;

    constexpr uint32 DEF_CRACK = 50;
    constexpr uint32 DEF_SHATTER = 100;
    constexpr uint32 DEF_EXPLODE = 150;

    constexpr bool   DEF_COUNT_ALL_DIRECT_FOR_FREEZE = false;

    inline uint32 ReadU32(char const* key, uint32 def) { return sConfigMgr->GetOption<uint32>(key, def); }
    inline bool   ReadBool(char const* key, bool def) { return sConfigMgr->GetOption<bool>(key, def); }

    struct Thresholds
    {
        uint32 slow;
        uint32 slowMore;
        uint32 freeze;

        uint32 crack;
        uint32 shatter;
        uint32 explode;

        bool countAllDirectForFreeze;

        void Load()
        {
            slow = ReadU32("AQ40.Viscidus.Hits.Slow", DEF_SLOW);
            slowMore = ReadU32("AQ40.Viscidus.Hits.SlowMore", DEF_SLOW_MORE);
            freeze = ReadU32("AQ40.Viscidus.Hits.Freeze", DEF_FREEZE);

            crack = ReadU32("AQ40.Viscidus.Hits.Crack", DEF_CRACK);
            shatter = ReadU32("AQ40.Viscidus.Hits.Shatter", DEF_SHATTER);
            explode = ReadU32("AQ40.Viscidus.Hits.Explode", DEF_EXPLODE);

            countAllDirectForFreeze = ReadBool("AQ40.Viscidus.CountAllDirectDamageForFreeze", DEF_COUNT_ALL_DIRECT_FOR_FREEZE);

            Validate();
        }

        void Validate()
        {
            auto clamp1 = [](uint32& v) { if (v < 1) v = 1; if (v > 10000) v = 10000; };

            clamp1(slow); clamp1(slowMore); clamp1(freeze);
            clamp1(crack); clamp1(shatter); clamp1(explode);

            bool badFreeze = !(slow < slowMore && slowMore < freeze);
            bool badMelee = !(crack < shatter && shatter < explode);

            if (badFreeze || badMelee)
            {
                LOG_ERROR("scripts.aq40",
                    "Viscidus config has non-ascending thresholds. "
                    "Freeze seq: slow<slowMore<freeze ? {}  |  Melee seq: crack<shatter<explode ? {}. "
                    "Reverting to defaults.",
                    !badFreeze, !badMelee);

                slow = DEF_SLOW;
                slowMore = DEF_SLOW_MORE;
                freeze = DEF_FREEZE;

                crack = DEF_CRACK;
                shatter = DEF_SHATTER;
                explode = DEF_EXPLODE;
            }

            LOG_INFO("scripts.aq40",
                "Viscidus thresholds: Slow={} SlowMore={} Freeze={} | Crack={} Shatter={} Explode={} | CountAllDirectForFreeze={}",
                slow, slowMore, freeze, crack, shatter, explode, countAllDirectForFreeze ? "true" : "false");
        }
    };
}

struct boss_viscidus : public BossAI
{
    boss_viscidus(Creature* creature) : BossAI(creature, DATA_VISCIDUS)
    {
        me->LowerPlayerDamageReq(me->GetMaxHealth());
        me->m_CombatDistance = 60.f;
        _cfg.Load();

        // NEW: Make the room center the home position to stabilize movement/evade logic
        me->SetHomePosition(leashCenter);
    }

    // Replace the old ramp-boundary with a gentle leash to center.
    bool CheckInRoom() override
    {
        float dist2d = me->GetExactDist2d(leashCenter);
        if (dist2d > leashRadius + 10.0f) // small buffer
        {
            // Instead of evading, snap back and keep fighting
            me->NearTeleportTo(leashCenter.GetPositionX(), leashCenter.GetPositionY(),
                leashCenter.GetPositionZ(), leashCenter.GetOrientation());
            me->SetHomePosition(leashCenter);
            me->SetInCombatWithZone();
        }
        return true;
    }

    void Reset() override
    {
        BossAI::Reset();
        _cfg.Load();
        SoftReset();
        me->RemoveAurasDueToSpell(SPELL_VISCIDUS_SHRINKS);
        me->SetHomePosition(leashCenter);
    }

    void SoftReset()
    {
        _hitcounter = 0;
        _phase = PHASE_FROST;
        _shatteredOnce = false;
        me->RemoveAurasDueToSpell(SPELL_STUN_SELF);
        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveAurasDueToSpell(SPELL_INVIS_SELF);
        me->RemoveAurasDueToSpell(SPELL_VISCIDUS_FREEZE);
        events.CancelEvent(EVENT_GLOB_WATCHDOG);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        BossAI::JustEngagedWith(nullptr);
        InitSpells();

        me->SetInCombatWithZone();
    }

    // Deterministic explode trigger; thresholds are configurable
    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType effType, SpellSchoolMask spellSchoolMask) override
    {
        if (me->HealthBelowPct(5))
            damage = 0;

        if (!attacker)
            return;

        if (_phase == PHASE_FROST)
        {
            if (effType == DIRECT_DAMAGE &&
                (_cfg.countAllDirectForFreeze || (spellSchoolMask & SPELL_SCHOOL_MASK_FROST)))
            {
                ++_hitcounter;

                if (_hitcounter == _cfg.slow)
                {
                    Talk(EMOTE_SLOW);
                    DoCastSelf(SPELL_VISCIDUS_SLOWED, true);
                }
                else if (_hitcounter == _cfg.slowMore)
                {
                    Talk(EMOTE_FREEZE);
                    me->RemoveAura(SPELL_VISCIDUS_SLOWED);
                    DoCastSelf(SPELL_VISCIDUS_SLOWED_MORE, true);
                }
                else if (_hitcounter >= _cfg.freeze)
                {
                    _hitcounter = 0;
                    Talk(EMOTE_FROZEN);
                    _phase = PHASE_MELEE;

                    me->RemoveAura(SPELL_VISCIDUS_SLOWED_MORE);
                    DoCastSelf(SPELL_VISCIDUS_FREEZE, true);

                    events.CancelEvent(EVENT_RESET_PHASE);
                    events.ScheduleEvent(EVENT_RESET_PHASE, 25s);
                    RefreshFreezeVisual(25000);

                    // Latch combat when entering the shatter window
                    me->SetInCombatWithZone();
                }
            }
            return;
        }

        if (_phase == PHASE_MELEE)
        {
            if (effType == DIRECT_DAMAGE)
            {
                ++_hitcounter;

                if (_hitcounter == _cfg.crack)
                    Talk(EMOTE_CRACK);
                else if (_hitcounter == _cfg.shatter)
                {
                    Talk(EMOTE_SHATTER);
                    _shatteredOnce = true;
                }
                else if (_hitcounter >= _cfg.explode)
                {
                    StartExplodeSequence();
                    return;
                }

                // keep the freeze window open while progressing, and keep combat
                events.RescheduleEvent(EVENT_RESET_PHASE, 25s);
                RefreshFreezeVisual(25000);
                me->SetInCombatWithZone();
            }
        }
    }

    void SpellHit(Unit* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_REJOIN_VISCIDUS)
        {
            me->RemoveAuraFromStack(SPELL_VISCIDUS_SHRINKS);
            return;
        }

        if (_cfg.countAllDirectForFreeze)
            return;

        SpellSchoolMask school = spellInfo->GetSchoolMask();

        if (spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON && (spellInfo->EquippedItemSubClassMask & (1 << ITEM_SUBCLASS_WEAPON_WAND)))
        {
            if (caster->GetTypeId() == TYPEID_UNIT)
            {
                if (Item const* pItem = caster->ToCreature()->GetBotEquips(2/*BOT_SLOT_RANGED*/))
                    school = SpellSchoolMask(uint32(school) | (1ul << pItem->GetTemplate()->Damage[0].DamageType));
            }
            else if (Item* pItem = caster->ToPlayer()->GetWeaponForAttack(RANGED_ATTACK))
            {
                school = SpellSchoolMask(1 << pItem->GetTemplate()->Damage[0].DamageType);
            }
        }

        if ((school & SPELL_SCHOOL_MASK_FROST) && _phase == PHASE_FROST)
        {
            ++_hitcounter;

            if (_hitcounter == _cfg.slow)
            {
                Talk(EMOTE_SLOW);
                DoCastSelf(SPELL_VISCIDUS_SLOWED, true);
            }
            else if (_hitcounter == _cfg.slowMore)
            {
                Talk(EMOTE_FREEZE);
                me->RemoveAura(SPELL_VISCIDUS_SLOWED);
                DoCastSelf(SPELL_VISCIDUS_SLOWED_MORE, true);
            }
            else if (_hitcounter >= _cfg.freeze)
            {
                _hitcounter = 0;
                Talk(EMOTE_FROZEN);
                _phase = PHASE_MELEE;

                me->RemoveAura(SPELL_VISCIDUS_SLOWED_MORE);
                DoCastSelf(SPELL_VISCIDUS_FREEZE, true);

                events.CancelEvent(EVENT_RESET_PHASE);
                events.ScheduleEvent(EVENT_RESET_PHASE, 25s);
                RefreshFreezeVisual(25000);
                me->SetInCombatWithZone();
            }
        }
    }

    void StartExplodeSequence()
    {
        // Allow death at/under 5% ONLY if we've shattered at least once this fight
        if (me->GetHealthPct() <= 5.0f && _shatteredOnce)
        {
            me->RemoveAura(SPELL_VISCIDUS_FREEZE);
            Unit::Kill(me, me);
            return;
        }

        me->SetReactState(REACT_PASSIVE);
        events.Reset();                // stop poison events
        scheduler.CancelAll();
        _phase = PHASE_GLOB;

        DoCastSelf(SPELL_STUN_SELF, true);
        me->AttackStop();
        me->CastStop();
        me->HandleEmoteCommand(EMOTE_ONESHOT_FLYDEATH);

        _shatteredOnce = true; // defensive
        DoCastSelf(SPELL_EXPLODE_TRIGGER, true);

        me->RemoveAura(SPELL_VISCIDUS_FREEZE);

        // IMPORTANT: do NOT clear threat here; that’s what caused evades.
        // Keep combat latched while invisible.
        me->SetInCombatWithZone();

        scheduler.Schedule(500ms, [this](TaskContext /*context*/)
            {
                DoCastSelf(SPELL_INVIS_SELF, true);
                me->SetAuraStack(SPELL_VISCIDUS_SHRINKS, me, 20);
                me->LowerPlayerDamageReq(me->GetMaxHealth());
                me->SetHealth(uint32(me->GetMaxHealth() * 0.01f)); // 1% hp

                // Keep threat, just stop attacking; players fight globs now.
                me->NearTeleportTo(leashCenter.GetPositionX(), leashCenter.GetPositionY(),
                    leashCenter.GetPositionZ(), leashCenter.GetOrientation());
                me->SetHomePosition(leashCenter);
                me->SetInCombatWithZone();
            });

        events.ScheduleEvent(EVENT_GLOB_WATCHDOG, 3s);
    }

    void DoGlobWatchdog()
    {
        if (_phase != PHASE_GLOB)
            return;

        if (!summons.IsAnyCreatureWithEntryAlive(NPC_GLOB_OF_VISCIDUS))
        {
            DoCastSelf(SPELL_EXPLODE_TRIGGER, true);
            events.RescheduleEvent(EVENT_GLOB_WATCHDOG, 2s);
            me->SetInCombatWithZone();
        }
    }

    void SummonedCreatureDies(Creature* summon, Unit* killer) override
    {
        if (summon->GetEntry() != NPC_GLOB_OF_VISCIDUS)
            return;

        if (killer && killer->GetEntry() == NPC_GLOB_OF_VISCIDUS)
        {
            if (_phase == PHASE_GLOB)
            {
                _phase = PHASE_FROST;
                me->RemoveAurasDueToSpell(SPELL_INVIS_SELF);
            }

            int32 heal = int32(me->GetMaxHealth() * 0.05f);
            me->CastCustomSpell(me, SPELL_VISCIDUS_GROWS, &heal, nullptr, nullptr, true);
        }

        if (!summons.IsAnyCreatureWithEntryAlive(NPC_GLOB_OF_VISCIDUS))
        {
            SoftReset();
            InitSpells();
            me->LowerPlayerDamageReq(me->GetMaxHealth());
            me->SetInCombatWithZone(); // re-latch when coming back
        }
    }

    void InitSpells()
    {
        events.ScheduleEvent(EVENT_TOXIN, 15s, 20s);
        events.ScheduleEvent(EVENT_POISONBOLT_VOLLEY, 10s, 15s);
        events.ScheduleEvent(EVENT_POISON_SHOCK, 7s, 12s);
    }

    void UpdateAI(uint32 diff) override
    {
        // Never early-return due to missing victim; that caused evades during freeze/glob.
        if (!CheckInRoom())
            return;

        events.Update(diff);
        scheduler.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_POISONBOLT_VOLLEY:
                if (_phase != PHASE_GLOB)
                    DoCastSelf(SPELL_POISONBOLT_VOLLEY);
                events.ScheduleEvent(EVENT_POISONBOLT_VOLLEY, 10s, 15s);
                break;
            case EVENT_POISON_SHOCK:
                if (_phase != PHASE_GLOB)
                    DoCastSelf(SPELL_POISON_SHOCK);
                events.ScheduleEvent(EVENT_POISON_SHOCK, 7s, 12s);
                break;
            case EVENT_TOXIN:
                if (_phase != PHASE_GLOB)
                    DoCastRandomTarget(SPELL_SUMMON_TOXIN_SLIME);
                events.ScheduleEvent(EVENT_TOXIN, 15s, 20s);
                break;
            case EVENT_RESET_PHASE:
                _hitcounter = 0;
                me->RemoveAura(SPELL_VISCIDUS_FREEZE);
                _phase = PHASE_FROST;
                me->SetReactState(REACT_AGGRESSIVE);
                me->SetInCombatWithZone();
                break;
            case EVENT_GLOB_WATCHDOG:
                DoGlobWatchdog();
                break;
            default:
                break;
            }
        }

        // Only perform melee brain when not in glob phase and we have a target
        if (_phase != PHASE_GLOB)
        {
            if (!me->GetVictim())
                (void)UpdateVictim(); // try to reacquire

            if (me->GetVictim())
                DoMeleeAttackIfReady();
        }
    }

private:
    void RefreshFreezeVisual(int32 ms)
    {
        if (Aura* a = me->GetAura(SPELL_VISCIDUS_FREEZE))
        {
            a->SetDuration(ms);
            a->SetMaxDuration(ms);
        }
    }

    uint16 _hitcounter = 0;
    uint8  _phase = PHASE_FROST;
    bool   _shatteredOnce = false;
    ViscidusCfg::Thresholds _cfg;
};

// Globs
struct boss_glob_of_viscidus : public ScriptedAI
{
    boss_glob_of_viscidus(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void InitializeAI() override
    {
        me->SetInCombatWithZone();
        scheduler.CancelAll();
        scheduler.Schedule(2400ms, [this](TaskContext context)
            {
                me->GetMotionMaster()->MovePoint(ROOM_CENTER, roomCenter);
                float topSpeed = me->GetSpeedRate(MOVE_RUN) + 0.2142855f * 4;
                context.Schedule(1s, [this, topSpeed](TaskContext context)
                    {
                        float newSpeed = me->GetSpeedRate(MOVE_RUN) + 0.2142855f; // sniffed
                        me->SetSpeed(MOVE_RUN, newSpeed < topSpeed ? newSpeed : topSpeed);
                        context.Repeat();
                    });
            });
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        if (id == ROOM_CENTER)
        {
            DoCastSelf(SPELL_REJOIN_VISCIDUS, true);
            me->KillSelf();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

struct npc_toxic_slime : public ScriptedAI
{
    npc_toxic_slime(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void InitializeAI() override
    {
        me->SetCombatMovement(false);
        DoCastSelf(SPELL_TOXIN, true);

        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* viscidus = instance->GetCreature(DATA_VISCIDUS))
                if (viscidus->AI())
                    viscidus->AI()->JustSummoned(me);
    }
};

class spell_explode_trigger : public SpellScript
{
    PrepareSpellScript(spell_explode_trigger);

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Guarantee at least 1 glob, even at 1% HP
        uint8 globsToSpawn = uint8(std::floor(caster->GetHealthPct() / 5.f));
        if (globsToSpawn == 0)
            globsToSpawn = 1;
        if (globsToSpawn > MAX_GLOB_SPAWN)
            globsToSpawn = MAX_GLOB_SPAWN;

        for (uint8 i = 0; i < globsToSpawn; ++i)
            caster->CastSpell((Unit*)nullptr, spawnGlobSpells[i], true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_explode_trigger::HandleOnHit);
    }
};

class spell_summon_toxin_slime : public SpellScript
{
    PrepareSpellScript(spell_summon_toxin_slime);

    void HandleOnHit()
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_SUMMON_TOXIN_SLIME_2, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_summon_toxin_slime::HandleOnHit);
    }
};

class spell_viscidus_freeze_AuraScript : public AuraScript
{
    PrepareAuraScript(spell_viscidus_freeze_AuraScript);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        target->SetControlled(true, UNIT_STATE_STUNNED);
        LOG_DEBUG("scripts.spells", "Viscidus Freeze: Applied stun to unit {}", target->GetGUID().ToString());
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        target->SetControlled(false, UNIT_STATE_STUNNED);
        LOG_DEBUG("scripts.spells", "Viscidus Freeze: Removed stun from unit {}", target->GetGUID().ToString());
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_viscidus_freeze_AuraScript::OnApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_viscidus_freeze_AuraScript::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_viscidus_freeze : public SpellScriptLoader
{
public:
    spell_viscidus_freeze() : SpellScriptLoader("spell_viscidus_freeze") {}

    AuraScript* GetAuraScript() const override
    {
        return new spell_viscidus_freeze_AuraScript();
    }
};

void AddSC_boss_viscidus()
{
    RegisterTempleOfAhnQirajCreatureAI(boss_viscidus);
    RegisterTempleOfAhnQirajCreatureAI(boss_glob_of_viscidus);
    RegisterTempleOfAhnQirajCreatureAI(npc_toxic_slime);
    RegisterSpellScript(spell_explode_trigger);
    RegisterSpellScript(spell_summon_toxin_slime);
    new spell_viscidus_freeze(); // Was trying to test why freeze was breaking immediately. Still don't know why. I think it has to do with constant evasion at this point
}
