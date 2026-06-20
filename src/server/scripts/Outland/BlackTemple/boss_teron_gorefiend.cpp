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
#include "Map.h"
#include "ScriptedCreature.h"
#include "SpellScriptLoader.h"
#include "black_temple.h"
#include "PassiveAI.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "botmgr.h"
#include <vector>

enum Says
{
    SAY_INTRO                       = 0,
    SAY_AGGRO                       = 1,
    SAY_SLAY                        = 2,
    SAY_BLOSSOM                     = 3,
    SAY_INCINERATE                  = 4,
    SAY_CRUSHING                    = 5,
    SAY_DEATH                       = 6
};

enum Spells
{
    SPELL_INCINERATE                = 40239,
    SPELL_SUMMON_DOOM_BLOSSOM       = 40188,
    SPELL_CRUSHING_SHADOWS          = 40243,
    SPELL_SHADOW_OF_DEATH           = 40251,
    SPELL_SHADOW_OF_DEATH_REMOVE    = 41999,
    SPELL_SUMMON_SPIRIT             = 40266,
    SPELL_SUMMON_SKELETON1          = 40270,
    SPELL_SUMMON_SKELETON2          = 41948,
    SPELL_SUMMON_SKELETON3          = 41949,
    SPELL_SUMMON_SKELETON4          = 41950,
    SPELL_POSSESS_SPIRIT_IMMUNE     = 40282,
    SPELL_SPIRITUAL_VENGEANCE       = 40268,
    SPELL_BRIEF_STUN                = 41421,
    SPELL_BERSERK                   = 45078,
    SPELL_SOULSTONE_RESURRECTION    = 47883,

    SPELL_SPIRIT_LANCE              = 40157,
    SPELL_SPIRIT_CHAINS             = 40175,
    SPELL_SPIRIT_VOLLEY             = 40314
};

enum Misc
{
    SET_DATA_INTRO                  = 1,
    ACTION_SMALL_RAID_WIPE_CHECK    = 1,
    EVENT_RESET_RECENTLY_SPOKEN     = 1,
    EVENT_SMALL_RAID_WIPE_CHECK     = 2,
    SMALL_RAID_REAL_PLAYER_LIMIT    = 2
};

constexpr float TERON_BOT_WIPE_KILL_RANGE = 180.0f;
constexpr auto TERON_SMALL_RAID_WIPE_CHECK_DELAY = 8s;
constexpr char const* TERON_SMALL_RAID_WIPE_YELL = "No living soul remains to defy me. Your failure is complete!";

struct TeronRealPlayerState
{
    uint32 total = 0;
    uint32 living = 0;
    uint32 activeVengefulSpirit = 0;
};

bool IsTeronEncounterPlayer(Creature const* teron, Player const* player);

bool IsShadowOfDeathEligibleTarget(Creature* teron, Unit* target, Unit const* tank, bool allowTank)
{
    if (!teron || !target || !target->IsAlive())
        return false;

    if (!allowTank && target == tank)
        return false;

    if (target->HasAura(SPELL_SHADOW_OF_DEATH) || target->HasAura(SPELL_POSSESS_SPIRIT_IMMUNE))
        return false;

    if (!target->IsInMap(teron) || !target->InSamePhase(teron))
        return false;

    return teron->IsValidAttackTarget(target);
}

Unit* SelectRandomShadowOfDeathTarget(std::vector<Unit*>& targets)
{
    if (targets.empty())
        return nullptr;

    return targets[urand(0u, uint32(targets.size() - 1))];
}

Unit* SelectShadowOfDeathTarget(Creature* teron)
{
    if (!teron)
        return nullptr;

    Unit const* tank = teron->GetThreatMgr().GetCurrentVictim();
    std::vector<Unit*> playerTargets;
    std::vector<Unit*> tankPlayerTargets;
    std::vector<Unit*> botTargets;

    playerTargets.reserve(teron->GetThreatMgr().GetThreatListSize());
    tankPlayerTargets.reserve(1);
    botTargets.reserve(teron->GetThreatMgr().GetThreatListSize());

    for (ThreatReference const* ref : teron->GetThreatMgr().GetUnsortedThreatList())
    {
        if (!ref || ref->IsOffline())
            continue;

        Unit* target = ref->GetVictim();
        if (!target)
            continue;

        if (Player* player = target->ToPlayer())
        {
            if (!IsTeronEncounterPlayer(teron, player))
                continue;

            if (IsShadowOfDeathEligibleTarget(teron, target, tank, false))
                playerTargets.push_back(target);
            else if (target == tank && IsShadowOfDeathEligibleTarget(teron, target, tank, true))
                tankPlayerTargets.push_back(target);

            continue;
        }

        Creature* creature = target->ToCreature();
        if (!creature || !creature->IsNPCBot() || creature->IsTempBot() || creature->IsFreeBot())
            continue;

        if (IsShadowOfDeathEligibleTarget(teron, target, tank, false))
            botTargets.push_back(target);
    }

    if (Unit* target = SelectRandomShadowOfDeathTarget(playerTargets))
        return target;

    if (Unit* target = SelectRandomShadowOfDeathTarget(tankPlayerTargets))
        return target;

    return SelectRandomShadowOfDeathTarget(botTargets);
}

bool IsTeronEncounterPlayer(Creature const* teron, Player const* player)
{
    return teron && player && player->IsInWorld() && player->IsInMap(teron) && player->InSamePhase(teron);
}

bool IsTeronPlayerActiveAsVengefulSpirit(Player const* player)
{
    return player && (player->HasAura(SPELL_SPIRITUAL_VENGEANCE) || player->HasAura(SPELL_POSSESS_SPIRIT_IMMUNE));
}

bool HasSoulstoneResurrection(Player const* player)
{
    return player &&
        (player->HasAura(20707) ||
            player->HasAura(20762) ||
            player->HasAura(20763) ||
            player->HasAura(20764) ||
            player->HasAura(20765) ||
            player->HasAura(27239) ||
            player->HasAura(SPELL_SOULSTONE_RESURRECTION));
}

Creature* GetTeronGorefiend(Unit* source)
{
    if (!source)
        return nullptr;

    if (Creature* creature = source->ToCreature())
        if (creature->GetEntry() == NPC_TERON_GOREFIEND)
            return creature;

    InstanceScript* instance = source->GetInstanceScript();
    return instance ? instance->GetCreature(DATA_TERON_GOREFIEND) : nullptr;
}

TeronRealPlayerState GetTeronRealPlayerState(Creature const* teron)
{
    TeronRealPlayerState state;
    if (!teron || !teron->GetMap())
        return state;

    for (MapReference const& ref : teron->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!IsTeronEncounterPlayer(teron, player))
            continue;

        ++state.total;

        if (IsTeronPlayerActiveAsVengefulSpirit(player))
            ++state.activeVengefulSpirit;
        else if (player->IsAlive())
            ++state.living;
    }

    return state;
}

void ApplySmallRaidSoulstoneForShadowOfDeath(Unit* caster, Unit* target)
{
    Player* player = target ? target->ToPlayer() : nullptr;
    Creature* teron = GetTeronGorefiend(caster);
    if (!player || !teron || !IsTeronEncounterPlayer(teron, player))
        return;

    if (GetTeronRealPlayerState(teron).total > SMALL_RAID_REAL_PLAYER_LIMIT)
        return;

    if (HasSoulstoneResurrection(player))
        return;

    teron->CastSpell(player, SPELL_SOULSTONE_RESURRECTION, true);
}

void KillTeronEncounterBotUnit(Creature* teron, Unit* unit, GuidSet& killed)
{
    if (!teron || !unit || !unit->IsAlive())
        return;

    if (!unit->IsNPCBot() && !unit->IsNPCBotPet())
        return;

    if (!unit->IsInMap(teron) || !unit->InSamePhase(teron) || !teron->IsWithinDistInMap(unit, TERON_BOT_WIPE_KILL_RANGE))
        return;

    if (!killed.insert(unit->GetGUID()).second)
        return;

    Unit::Kill(teron, unit);
}

void KillTeronEncounterBotAndPet(Creature* teron, Creature* bot, GuidSet& killed)
{
    if (!bot)
        return;

    KillTeronEncounterBotUnit(teron, bot, killed);

    if (bot->IsNPCBot())
        KillTeronEncounterBotUnit(teron, bot->GetBotsPet(), killed);
}

void KillTeronEncounterBots(Creature* teron)
{
    if (!teron || !teron->GetMap())
        return;

    GuidSet killed;

    for (MapReference const& ref : teron->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->HaveBot())
            continue;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            continue;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            KillTeronEncounterBotAndPet(teron, pair.second, killed);
    }
}

bool ForceTeronWipeIfNoRealPlayersRemain(Creature* teron)
{
    if (!teron || !teron->IsAlive())
        return false;

    InstanceScript* instance = teron->GetInstanceScript();
    if (!instance || instance->GetBossState(DATA_TERON_GOREFIEND) != IN_PROGRESS)
        return false;

    TeronRealPlayerState const state = GetTeronRealPlayerState(teron);
    if (state.living || state.activeVengefulSpirit)
        return false;

    teron->Yell(TERON_SMALL_RAID_WIPE_YELL, LANG_UNIVERSAL);
    KillTeronEncounterBots(teron);
    teron->AI()->EnterEvadeMode(CreatureAI::EVADE_REASON_NO_HOSTILES);
    return true;
}

struct boss_teron_gorefiend : public BossAI
{
    boss_teron_gorefiend(Creature* creature) : BossAI(creature, DATA_TERON_GOREFIEND)
    {
        _recentlySpoken = false;
        _intro = false;
        _smallRaidWipeResetting = false;
    }

    void Reset() override
    {
        BossAI::Reset();
        DoCastSelf(SPELL_SHADOW_OF_DEATH_REMOVE, true);
        _smallRaidWipeResetting = false;
    }

    void JustEngagedWith(Unit* who) override
    {
        ScheduleTimedEvent(20s, 30s, [&]
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
            {
                if (roll_chance_i(50))
                    Talk(SAY_INCINERATE);
                me->CastSpell(target, SPELL_INCINERATE, false);
            }
        }, 20s, 50s);

        ScheduleTimedEvent(5s, 10s, [&]
        {
            if (roll_chance_i(50))
                Talk(SAY_BLOSSOM);
            me->CastSpell(me, SPELL_SUMMON_DOOM_BLOSSOM, false);
        }, 35s);

        ScheduleTimedEvent(17s, 22s, [&]
        {
            if (roll_chance_i(20))
                Talk(SAY_CRUSHING);
            me->CastCustomSpell(SPELL_CRUSHING_SHADOWS, SPELLVALUE_MAX_TARGETS, 5, me, false);
        }, 10s, 26s);

        ScheduleTimedEvent(10s, [&]
        {
            if (Unit* target = SelectShadowOfDeathTarget(me))
                me->CastSpell(target, SPELL_SHADOW_OF_DEATH, false);
        }, 30s, 50s);

        ScheduleTimedEvent(10min, [&]
        {
            DoCastSelf(SPELL_BERSERK);
        }, 5min);

        BossAI::JustEngagedWith(who);
    }

    void KilledUnit(Unit*  victim) override
    {
        if (!_recentlySpoken && victim->IsPlayer())
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;

            ScheduleUniqueTimedEvent(6s, [&]
            {
                _recentlySpoken = false;
            }, EVENT_RESET_RECENTLY_SPOKEN);
        }
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_SMALL_RAID_WIPE_CHECK || _smallRaidWipeResetting)
            return;

        ScheduleUniqueTimedEvent(TERON_SMALL_RAID_WIPE_CHECK_DELAY, [this]
        {
            _smallRaidWipeResetting = ForceTeronWipeIfNoRealPlayersRemain(me);
        }, EVENT_SMALL_RAID_WIPE_CHECK);
    }

    void SetData(uint32 type, uint32 id) override
    {
        if (type || !me->IsAlive())
            return;

        if (id == SET_DATA_INTRO && !_intro)
        {
            _intro = true;
            Talk(SAY_INTRO);
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
        DoCastSelf(SPELL_SHADOW_OF_DEATH_REMOVE, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        DoMeleeAttackIfReady();
    }

    private:
        bool _recentlySpoken;
        bool _intro;
        bool _smallRaidWipeResetting;
};

struct npc_vengeful_spirit : public NullCreatureAI
{
    npc_vengeful_spirit(Creature* creature) : NullCreatureAI(creature) { }

    void OnCharmed(bool apply)
    {
        if (!apply)
            me->DespawnOnEvade();
    }
};

class spell_teron_gorefiend_shadow_of_death : public AuraScript
{
    PrepareAuraScript(spell_teron_gorefiend_shadow_of_death);

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        ApplySmallRaidSoulstoneForShadowOfDeath(GetCaster(), GetTarget());
    }

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo&   /*dmgInfo*/, uint32&   /*absorbAmount*/)
    {
        PreventDefaultAction();
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        InstanceScript* instance = GetTarget()->GetInstanceScript();
        if (!GetCaster() || !instance || !instance->IsEncounterInProgress())
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_SPIRIT, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_POSSESS_SPIRIT_IMMUNE, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_SPIRITUAL_VENGEANCE, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_SKELETON1, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_SKELETON2, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_SKELETON3, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_SKELETON4, true);

        if (Creature* teron = GetTeronGorefiend(GetTarget()))
            teron->AI()->DoAction(ACTION_SMALL_RAID_WIPE_CHECK);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_teron_gorefiend_shadow_of_death::HandleEffectApply, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_teron_gorefiend_shadow_of_death::Absorb, EFFECT_0);
        AfterEffectRemove += AuraEffectRemoveFn(spell_teron_gorefiend_shadow_of_death::HandleEffectRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_teron_gorefiend_spirit_lance : public AuraScript
{
    PrepareAuraScript(spell_teron_gorefiend_spirit_lance);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (AuraEffect* effect = GetAura()->GetEffect(EFFECT_2))
            amount -= (amount / effect->GetTotalTicks()) * effect->GetTickNumber();
    }

    void Update(AuraEffect const*  /*effect*/)
    {
        PreventDefaultAction();
        if (AuraEffect* effect = GetAura()->GetEffect(EFFECT_1))
            effect->RecalculateAmount();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_teron_gorefiend_spirit_lance::CalculateAmount, EFFECT_1, SPELL_AURA_MOD_DECREASE_SPEED);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_teron_gorefiend_spirit_lance::Update, EFFECT_2, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_teron_gorefiend_spiritual_vengeance : public AuraScript
{
    PrepareAuraScript(spell_teron_gorefiend_spiritual_vengeance);

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        Creature* teron = GetTeronGorefiend(target);
        Unit::Kill(nullptr, target);

        if (teron)
            teron->AI()->DoAction(ACTION_SMALL_RAID_WIPE_CHECK);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_teron_gorefiend_spiritual_vengeance::HandleEffectRemove, EFFECT_2, SPELL_AURA_MOD_PACIFY_SILENCE, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_teron_gorefiend_shadowy_construct : public AuraScript
{
    PrepareAuraScript(spell_teron_gorefiend_shadowy_construct);

    bool Load() override
    {
        return GetUnitOwner()->IsCreature();
    }

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_NORMAL, true);
        GetUnitOwner()->ApplySpellImmune(0, IMMUNITY_ALLOW_ID, SPELL_SPIRIT_LANCE, true);
        GetUnitOwner()->ApplySpellImmune(0, IMMUNITY_ALLOW_ID, SPELL_SPIRIT_CHAINS, true);
        GetUnitOwner()->ApplySpellImmune(0, IMMUNITY_ALLOW_ID, SPELL_SPIRIT_VOLLEY, true);

        GetUnitOwner()->ToCreature()->SetInCombatWithZone();
        Map::PlayerList const& playerList = GetUnitOwner()->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator i = playerList.begin(); i != playerList.end(); ++i)
            if (Player* player = i->GetSource())
            {
                if (GetUnitOwner()->IsValidAttackTarget(player))
                    GetUnitOwner()->AddThreat(player, 1000000.0f);
            }

        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_BRIEF_STUN, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_teron_gorefiend_shadowy_construct::HandleEffectApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_teron_gorefiend_shadow_of_death_remove : public SpellScript
{
    PrepareSpellScript(spell_teron_gorefiend_shadow_of_death_remove);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_SHADOW_OF_DEATH,
            SPELL_POSSESS_SPIRIT_IMMUNE,
            SPELL_SPIRITUAL_VENGEANCE
        });
    }

    void HandleOnHit()
    {
        if (Unit* target = GetHitUnit())
        {
            target->RemoveAurasDueToSpell(SPELL_POSSESS_SPIRIT_IMMUNE);
            target->RemoveAurasDueToSpell(SPELL_SPIRITUAL_VENGEANCE);
            target->RemoveAurasDueToSpell(SPELL_SHADOW_OF_DEATH);
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_teron_gorefiend_shadow_of_death_remove::HandleOnHit);
    }
};

void AddSC_boss_teron_gorefiend()
{
    RegisterBlackTempleCreatureAI(boss_teron_gorefiend);
    RegisterBlackTempleCreatureAI(npc_vengeful_spirit);
    RegisterSpellScript(spell_teron_gorefiend_shadow_of_death);
    RegisterSpellScript(spell_teron_gorefiend_spirit_lance);
    RegisterSpellScript(spell_teron_gorefiend_spiritual_vengeance);
    RegisterSpellScript(spell_teron_gorefiend_shadowy_construct);
    RegisterSpellScript(spell_teron_gorefiend_shadow_of_death_remove);
}
