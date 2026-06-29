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
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "WorldSession.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "sunwell_plateau.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

namespace BonusLootRolls
{
    void AddExplicitBossDeathOpportunities(Player* killer, Creature* killed);
}

enum Yells
{
    SAY_SATH_AGGRO                              = 0,
    SAY_SATH_SLAY                               = 1,
    SAY_SATH_DEATH                              = 2,
    SAY_SATH_SPELL1                             = 3,
    SAY_SATH_SPELL2                             = 4,

    SAY_EVIL_AGGRO                              = 0,
    SAY_EVIL_SLAY                               = 1,
    SAY_GOOD_PLRWIN                             = 2,
    SAY_EVIL_ENRAGE                             = 3,
    SAY_SATH_ENRAGE_ME                          = 4,
    SAY_KALEC_ENRAGE_SATH                       = 5,

    SAY_GOOD_AGGRO                              = 0,
    SAY_GOOD_NEAR_DEATH                         = 1,
    SAY_GOOD_NEAR_DEATH2                        = 2,
    SAY_GOOD_MADRIGOSA                          = 3 // Madrigosa deserved a far better fate. You did what had to be done, but this battle is far from over!
};

enum Spells
{
    SPELL_SPECTRAL_EXHAUSTION              = 44867,
    SPELL_SPECTRAL_BLAST                   = 44869,
    SPELL_SPECTRAL_BLAST_PORTAL            = 44866,
    SPELL_SPECTRAL_BLAST_AA                = 46648,
    SPELL_TELEPORT_SPECTRAL                = 46019,

    SPELL_TELEPORT_NORMAL_REALM            = 46020,
    SPELL_SPECTRAL_REALM                   = 46021,
    SPELL_SPECTRAL_INVISIBILITY            = 44801,
    SPELL_DEMONIC_VISUAL                   = 44800,

    SPELL_ARCANE_BUFFET                    = 45018,
    SPELL_FROST_BREATH                     = 44799,
    SPELL_TAIL_LASH                        = 45122,

    SPELL_BANISH                           = 44836,
    SPELL_TRANSFORM_KALEC                  = 44670,
    SPELL_CRAZED_RAGE                      = 44807,

    SPELL_CORRUPTION_STRIKE                = 45029,
    SPELL_CURSE_OF_BOUNDLESS_AGONY         = 45032,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR     = 45034,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_REMOVE  = 45050,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_1 = 45083,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_2 = 45085,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_3 = 45084,
    SPELL_SHADOW_BOLT                      = 45031,

    SPELL_HEROIC_STRIKE                    = 45026,
    SPELL_REVITALIZE                       = 45027
};

enum SWPActions
{
    ACTION_ENRAGE                       = 1,
    ACTION_BANISH                       = 2,
    ACTION_SATH_BANISH                  = 3,
    ACTION_KALEC_DIED                   = 4,
    ACTION_ENRAGE_OTHER                 = 5,
};

#define DRAGON_REALM_Z  53.079f

namespace
{
    constexpr float KALECGOS_ENCOUNTER_RANGE = 160.0f;
    constexpr float KALECGOS_PRESSURE_WARNING_GAP = 15.0f;
    constexpr float KALECGOS_PRESSURE_ENRAGE_GAP = 25.0f;

    Position const SathrovarrManifestPosition = {1696.20f, 915.0f, DRAGON_REALM_Z, 5.07f};

    bool IsOwnedNPCBot(Unit const* unit)
    {
        Creature const* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }

    bool IsKalecgosEncounterTarget(Unit const* unit, WorldObject const* source, float range = KALECGOS_ENCOUNTER_RANGE)
    {
        if (!unit || !source || !unit->IsAlive() || !unit->IsInWorld() || unit->GetMap() != source->GetMap())
            return false;

        if (!unit->InSamePhase(source))
            return false;

        if (range > 0.0f && !source->IsWithinDistInMap(unit, range))
            return false;

        if (Player const* player = unit->ToPlayer())
            return !player->IsGameMaster();

        return IsOwnedNPCBot(unit);
    }

    void AddKalecgosEncounterUnit(std::vector<Unit*>& units, Unit* unit, WorldObject const* source)
    {
        if (!IsKalecgosEncounterTarget(unit, source))
            return;

        ObjectGuid const guid = unit->GetGUID();
        if (std::any_of(units.begin(), units.end(), [guid](Unit* existing) { return existing && existing->GetGUID() == guid; }))
            return;

        units.push_back(unit);
    }

    std::vector<Unit*> GatherKalecgosEncounterUnits(WorldObject const* source)
    {
        std::vector<Unit*> units;
        if (!source || !source->GetMap())
            return units;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            AddKalecgosEncounterUnit(units, player, source);

            if (Group* group = player->GetGroup())
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    AddKalecgosEncounterUnit(units, member, source);

            if (BotMgr* botMgr = player->GetBotMgr())
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    AddKalecgosEncounterUnit(units, pair.second, source);
        }

        return units;
    }

    Unit* SelectKalecgosEncounterTarget(Creature* source, ObjectGuid avoid = ObjectGuid::Empty)
    {
        std::vector<Unit*> targets = GatherKalecgosEncounterUnits(source);
        targets.erase(std::remove_if(targets.begin(), targets.end(), [avoid](Unit* unit)
        {
            return !unit || unit->GetGUID() == avoid;
        }), targets.end());

        if (targets.empty())
            return nullptr;

        return targets[urand(0, uint32(targets.size() - 1))];
    }

    Player* SelectKalecgosBonusLootKiller(WorldObject const* source)
    {
        for (Unit* unit : GatherKalecgosEncounterUnits(source))
            if (Player* player = unit->GetCharmerOrOwnerPlayerOrPlayerItself())
                return player;

        return nullptr;
    }

    void AppendKalecgosEncounterTargets(std::list<WorldObject*>& targets, WorldObject const* source, ObjectGuid avoid = ObjectGuid::Empty)
    {
        for (Unit* unit : GatherKalecgosEncounterUnits(source))
        {
            if (!unit || unit->GetGUID() == avoid)
                continue;

            ObjectGuid const guid = unit->GetGUID();
            if (std::any_of(targets.begin(), targets.end(), [guid](WorldObject* existing) { return existing && existing->GetGUID() == guid; }))
                continue;

            targets.push_back(unit);
        }
    }

    bool HasBoundlessAgony(Unit const* unit)
    {
        return unit && (unit->HasAura(SPELL_CURSE_OF_BOUNDLESS_AGONY) || unit->HasAura(SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR));
    }

    bool IsSummonObjectEffect(uint32 effect)
    {
        switch (effect)
        {
            case SPELL_EFFECT_SUMMON_OBJECT_WILD:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT1:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT2:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT3:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT4:
                return true;
            default:
                return false;
        }
    }

    void EngageKalecgosEncounterUnit(Creature* source, Unit* target)
    {
        if (!source || !target || !source->IsValidAttackTarget(target))
            return;

        source->SetInCombatWith(target);
        target->SetInCombatWith(source);
        source->AddThreat(target, 1.0f);

        if (!source->GetVictim())
            source->AI()->AttackStart(target);
    }
}

struct boss_kalecgos : public BossAI
{
    boss_kalecgos(Creature* creature) : BossAI(creature, DATA_KALECGOS)
    {
        SetInvincibility(true);
    }

    bool CanAIAttack(Unit const* target) const override
    {
        return IsKalecgosEncounterTarget(target, me);
    }

    bool CheckInRoom() override
    {
        if (me->GetDistance(me->GetHomePosition()) > 50.0f)
            return false;

        return true;
    }

    void Reset() override
    {
        BossAI::Reset();
        me->SetFullHealth();
        me->SetStandState(UNIT_STAND_STATE_SLEEP);
        me->SetDisableGravity(false);
        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

        _sathBanished = false;
        _encounterComplete = false;
        ClearPlayerAuras();

        ScheduleHealthCheckEvent(10, [&] {
            if (Creature* Sath = instance->GetCreature(DATA_SATHROVARR))
                Sath->AI()->DoAction(ACTION_ENRAGE_OTHER);
            DoAction(ACTION_ENRAGE);
        });

        ScheduleHealthCheckEvent(1, [&] {
            DoAction(ACTION_BANISH);
        });
    }

    void ClearPlayerAuras() const
    {
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_CURSE_OF_BOUNDLESS_AGONY);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_SPECTRAL_REALM);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_ENRAGE || param == ACTION_ENRAGE_OTHER)
        {
            Talk(param == ACTION_ENRAGE ? SAY_KALEC_ENRAGE_SATH : SAY_SATH_ENRAGE_ME);
            DoCastSelf(SPELL_CRAZED_RAGE, true);
            return;
        }
        else if (param == ACTION_BANISH)
        {
            DoCastSelf(SPELL_BANISH, true);
            scheduler.CancelAll();
        }
        else if (param == ACTION_SATH_BANISH)
        {
            _sathBanished = true;
            TryCompleteEncounter();
        }
        else if (param == ACTION_KALEC_DIED)
        {
            scheduler.CancelAll();

            me->m_Events.AddEventAtOffset([&] {
                me->SetReactState(REACT_PASSIVE);
                me->RemoveAllAuras();
                me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                Talk(SAY_EVIL_ENRAGE);
            }, 1s);

            me->m_Events.AddEventAtOffset([&] {
                me->SetDisableGravity(true);
                me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
            }, 4s);

            me->m_Events.AddEventAtOffset([&] {
                EnterEvadeMode();
            }, 9s);

            ClearPlayerAuras();
            return;
        }

        TryCompleteEncounter();
    }

    void TryCompleteEncounter()
    {
        if (_encounterComplete || !me->HasAura(SPELL_BANISH) || !_sathBanished)
            return;

        _encounterComplete = true;
        scheduler.CancelAll();

        if (Creature* Sath = instance->GetCreature(DATA_SATHROVARR))
            BonusLootRolls::AddExplicitBossDeathOpportunities(SelectKalecgosBonusLootKiller(me), Sath);

        me->m_Events.AddEventAtOffset([&] {
            me->SetRegeneratingHealth(false);
            me->RemoveAllAuras();
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->SetFaction(FACTION_FRIENDLY);
        }, 1s);

        me->m_Events.AddEventAtOffset([&] {
            if (Creature* Sath = instance->GetCreature(DATA_SATHROVARR))
            {
                summons.Despawn(Sath);
                Unit::Kill(me, Sath);
            }
        }, 2s);

        Talk(SAY_GOOD_PLRWIN, 10s);

        me->m_Events.AddEventAtOffset([&] {
            me->SetDisableGravity(true);
            me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
        }, 15s);

        me->m_Events.AddEventAtOffset([&] {
            me->SetVisible(false);
            me->KillSelf();
        }, 20s);

        ClearPlayerAuras();
        if (Creature* Sath = instance->GetCreature(DATA_SATHROVARR))
        {
            Sath->RemoveAllAuras();
            Sath->GetMotionMaster()->MovementExpired();
            Sath->SetReactState(REACT_PASSIVE);
            Sath->NearTeleportTo(SathrovarrManifestPosition.GetPositionX(), SathrovarrManifestPosition.GetPositionY(), SathrovarrManifestPosition.GetPositionZ(), Sath->GetOrientation());
        }
    }

    void ManifestSathrovarr()
    {
        Creature* Sath = instance->GetCreature(DATA_SATHROVARR);
        if (!Sath)
            Sath = me->SummonCreature(NPC_SATHROVARR, SathrovarrManifestPosition.GetPositionX(), SathrovarrManifestPosition.GetPositionY(), SathrovarrManifestPosition.GetPositionZ(), SathrovarrManifestPosition.GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN);

        if (!Sath)
            return;

        Sath->NearTeleportTo(SathrovarrManifestPosition.GetPositionX(), SathrovarrManifestPosition.GetPositionY(), SathrovarrManifestPosition.GetPositionZ(), SathrovarrManifestPosition.GetOrientation());
        Sath->SetHomePosition(SathrovarrManifestPosition);
        Sath->RemoveAurasDueToSpell(SPELL_SPECTRAL_INVISIBILITY);
        Sath->SetVisible(true);
        Sath->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        Sath->SetReactState(REACT_AGGRESSIVE);

        Unit* target = SelectKalecgosEncounterTarget(me, me->GetVictim() ? me->GetVictim()->GetGUID() : ObjectGuid::Empty);
        if (!target)
            target = me->GetVictim();

        if (target)
            EngageKalecgosEncounterUnit(Sath, target);

        DoZoneInCombat(Sath);
    }

    void UpdateCorruptionPressure()
    {
        if (_encounterComplete || me->HasAura(SPELL_BANISH))
            return;

        Creature* Sath = instance->GetCreature(DATA_SATHROVARR);
        if (!Sath || !Sath->IsAlive() || Sath->HasAura(SPELL_BANISH))
            return;

        float const healthGap = me->GetHealthPct() - Sath->GetHealthPct();
        float const absGap = std::fabs(healthGap);

        if (absGap < KALECGOS_PRESSURE_WARNING_GAP)
        {
            me->RemoveAurasDueToSpell(SPELL_CRAZED_RAGE);
            Sath->RemoveAurasDueToSpell(SPELL_CRAZED_RAGE);
            return;
        }

        Creature* neglected = healthGap > 0.0f ? me : Sath;
        Creature* pressed = neglected == me ? Sath : me;

        if (absGap >= KALECGOS_PRESSURE_ENRAGE_GAP && !neglected->HasAura(SPELL_CRAZED_RAGE))
        {
            neglected->CastSpell(neglected, SPELL_CRAZED_RAGE, true);
            pressed->RemoveAurasDueToSpell(SPELL_CRAZED_RAGE);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        ScheduleTimedEvent(8s, [&] {
            DoCastAOE(SPELL_ARCANE_BUFFET);
        }, 8s);

        ScheduleTimedEvent(15s, [&] {
            DoCastVictim(SPELL_FROST_BREATH);
        }, 15s);

        ScheduleTimedEvent(6s, [&] {
            me->CastCustomSpell(RAND(44978, 45001, 45002, 45004, 45006, 45010), SPELLVALUE_MAX_TARGETS, 1, me, false);
        }, 6s, 7s);

        ScheduleTimedEvent(25s, [&] {
            DoCastVictim(SPELL_TAIL_LASH);
        }, 15s);

        ScheduleTimedEvent(13s, [&] {
            DoCastAOE(SPELL_SPECTRAL_BLAST);
        }, 20s, 30s);

        scheduler.Schedule(3s, [this](TaskContext)
        {
            ManifestSathrovarr();
        });

        scheduler.Schedule(10s, [this](TaskContext context)
        {
            UpdateCorruptionPressure();
            context.Repeat(10s);
        });

        me->SetStandState(UNIT_STAND_STATE_STAND);
        Talk(SAY_EVIL_AGGRO);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && roll_chance_i(50))
            Talk(SAY_EVIL_SLAY);
    }

    private:
        bool _sathBanished;
        bool _encounterComplete;
};

struct boss_kalec : public ScriptedAI
{
    boss_kalec(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit*) override
    {
        ScheduleTimedEvent(5s, [&] {
            DoCastSelf(SPELL_REVITALIZE);
        }, 10s);

        ScheduleTimedEvent(3s, [&] {
            DoCastVictim(SPELL_HEROIC_STRIKE);
        }, 5s);

        scheduler.Schedule(1s, [this](TaskContext context)
        {
            if (me->HealthBelowPct(50))
                Talk(SAY_GOOD_NEAR_DEATH);
            else
                context.Repeat();
        });

        scheduler.Schedule(1s, [this](TaskContext context)
        {
            if (me->HealthBelowPct(10))
                Talk(SAY_GOOD_NEAR_DEATH2);
            else
                context.Repeat();
        });

        Talk(SAY_GOOD_AGGRO);
    }

    void JustDied(Unit*) override
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* kalecgos = instance->GetCreature(DATA_KALECGOS))
                kalecgos->AI()->DoAction(ACTION_KALEC_DIED);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff,
            std::bind(&BossAI::DoMeleeAttackIfReady, this));
    }
};

struct boss_sathrovarr : public ScriptedAI
{
    boss_sathrovarr(Creature* creature) : ScriptedAI(creature)
    {
        _instance = creature->GetInstanceScript();
        SetInvincibility(true);
    }

    bool CanAIAttack(Unit const* target) const override
    {
        return IsKalecgosEncounterTarget(target, me);
    }

    void Reset() override
    {
        scheduler.CancelAll();
        me->SetFullHealth();
        me->SetVisible(true);
        me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveAurasDueToSpell(SPELL_SPECTRAL_INVISIBILITY);
        DoCastSelf(SPELL_DEMONIC_VISUAL, true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_SATH_AGGRO);

        ScheduleTimedEvent(7s, [&] {
            if (roll_chance_i(20))
                Talk(SAY_SATH_SPELL1);
            DoCastVictim(SPELL_SHADOW_BOLT);
        }, 9s);

        ScheduleTimedEvent(40s, [&] {
            me->CastCustomSpell(SPELL_CURSE_OF_BOUNDLESS_AGONY, SPELLVALUE_MAX_TARGETS, 1, me, false);
        }, 40s);

        ScheduleTimedEvent(20s, [&] {
            if (roll_chance_i(20))
                Talk(SAY_SATH_SPELL2);
            DoCastVictim(SPELL_CORRUPTION_STRIKE);
        }, 9s);

        scheduler.Schedule(1s, [this](TaskContext context)
        {
            if (me->HealthBelowPct(10))
            {
                if (Creature* kalecgos = _instance->GetCreature(DATA_KALECGOS))
                    kalecgos->AI()->DoAction(ACTION_ENRAGE_OTHER);
                DoAction(ACTION_ENRAGE);
            }
            else
                context.Repeat();
        });

        scheduler.Schedule(1s, [this](TaskContext context)
        {
            if (me->HealthBelowPct(1))
            {
                if (Creature* kalecgos = _instance->GetCreature(DATA_KALECGOS))
                    kalecgos->AI()->DoAction(ACTION_SATH_BANISH);
                DoAction(ACTION_BANISH);
            }
            else
                context.Repeat();
        });
    }

    void KilledUnit(Unit* target) override
    {
        if (target->IsPlayer())
            Talk(SAY_SATH_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_CURSE_OF_BOUNDLESS_AGONY_REMOVE, true);
        Talk(SAY_SATH_DEATH);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_ENRAGE || param == ACTION_ENRAGE_OTHER)
            DoCastSelf(SPELL_CRAZED_RAGE, true);
        else if (param == ACTION_BANISH)
        {
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            DoCastSelf(SPELL_BANISH, true);
            scheduler.CancelAll();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            if (Unit* target = SelectKalecgosEncounterTarget(me))
                EngageKalecgosEncounterUnit(me, target);
            return;
        }

        scheduler.Update(diff,
            std::bind(&BossAI::DoMeleeAttackIfReady, this));
    }

private:
    InstanceScript* _instance;
};

class SpectralBlastCheck
{
public:
    SpectralBlastCheck(Unit* caster, Unit* victim) : _caster(caster), _victim(victim) { }

    bool operator()(WorldObject* object)
    {
        Unit* target = object ? object->ToUnit() : nullptr;
        return !IsKalecgosEncounterTarget(target, _caster) || target->HasAura(SPELL_SPECTRAL_EXHAUSTION) || (_victim && target->GetGUID() == _victim->GetGUID());
    }
private:
    Unit* _caster;
    Unit* _victim;
};

class spell_kalecgos_spectral_blast_dummy : public SpellScript
{
    PrepareSpellScript(spell_kalecgos_spectral_blast_dummy);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SPECTRAL_EXHAUSTION });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
        {
            targets.clear();
            return;
        }

        AppendKalecgosEncounterTargets(targets, caster, caster->GetVictim() ? caster->GetVictim()->GetGUID() : ObjectGuid::Empty);
        targets.remove_if(SpectralBlastCheck(caster, caster->GetVictim()));

        if (!_targetGuid.IsEmpty())
        {
            targets.remove_if([this](WorldObject* target) { return !target || target->GetGUID() != _targetGuid; });
            return;
        }

        Acore::Containers::RandomResize(targets, 1);
        if (!targets.empty())
            _targetGuid = targets.front()->GetGUID();
    }

    void PreventPortalObject(SpellEffIndex effIndex)
    {
        if (SpellInfo const* spellInfo = GetSpellInfo())
            if (!IsSummonObjectEffect(spellInfo->Effects[effIndex].Effect))
                return;

        PreventHitDefaultEffect(effIndex);
    }

    void ApplySpectralExhaustion()
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_SPECTRAL_EXHAUSTION, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kalecgos_spectral_blast_dummy::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHit += SpellEffectFn(spell_kalecgos_spectral_blast_dummy::PreventPortalObject, EFFECT_0, SPELL_EFFECT_ANY);
        AfterHit += SpellHitFn(spell_kalecgos_spectral_blast_dummy::ApplySpectralExhaustion);
    }

private:
    ObjectGuid _targetGuid = ObjectGuid::Empty;
};

class BoundlessAgonyTargetCheck
{
public:
    explicit BoundlessAgonyTargetCheck(Unit* caster) : _caster(caster) { }

    bool operator()(WorldObject* object)
    {
        Unit* target = object ? object->ToUnit() : nullptr;
        return !IsKalecgosEncounterTarget(target, _caster) || HasBoundlessAgony(target) || (_caster && target->GetGUID() == _caster->GetGUID());
    }

private:
    Unit* _caster;
};

void SelectBoundlessAgonyTarget(std::list<WorldObject*>& targets, Unit* caster)
{
    if (!caster)
    {
        targets.clear();
        return;
    }

    AppendKalecgosEncounterTargets(targets, caster, caster->GetGUID());
    targets.remove_if(BoundlessAgonyTargetCheck(caster));
    Acore::Containers::RandomResize(targets, 1);
}

class spell_kalecgos_curse_of_boundless_agony : public SpellScript
{
    PrepareSpellScript(spell_kalecgos_curse_of_boundless_agony);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CURSE_OF_BOUNDLESS_AGONY });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        SelectBoundlessAgonyTarget(targets, GetCaster());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kalecgos_curse_of_boundless_agony::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_kalecgos_curse_of_boundless_agony_player : public SpellScript
{
    PrepareSpellScript(spell_kalecgos_curse_of_boundless_agony_player);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        SelectBoundlessAgonyTarget(targets, GetCaster());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kalecgos_curse_of_boundless_agony_player::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
    }
};

class spell_kalecgos_curse_of_boundless_agony_aura : public AuraScript
{
    PrepareAuraScript(spell_kalecgos_curse_of_boundless_agony_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR, SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_1, SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_2, SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_3 });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (InstanceScript* instance = GetUnitOwner()->GetInstanceScript())
            if (instance->IsEncounterInProgress())
                GetUnitOwner()->CastCustomSpell(SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR, SPELLVALUE_MAX_TARGETS, 1, GetUnitOwner(), true);
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        uint32 tickNumber = aurEff->GetTickNumber();
        if (tickNumber > 1 && tickNumber % 5 == 1)
            GetAura()->GetEffect(aurEff->GetEffIndex())->SetAmount(aurEff->GetAmount() * 2);

        uint32 spellId = 0;
        if (tickNumber <= 10)
            spellId = SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_1;
        else if (tickNumber <= 20)
            spellId = SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_2;
        else
            spellId = SPELL_CURSE_OF_BOUNDLESS_AGONY_DUMMY_3;
        GetTarget()->CastSpell(GetTarget(), spellId, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_kalecgos_curse_of_boundless_agony_aura::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kalecgos_curse_of_boundless_agony_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

class spell_kalecgos_spectral_realm_dummy : public SpellScript
{
    PrepareSpellScript(spell_kalecgos_spectral_realm_dummy);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SPECTRAL_EXHAUSTION });
    }

    SpellCastResult CheckCast()
    {
        if (GetCaster()->HasAura(SPELL_SPECTRAL_EXHAUSTION))
            return SPELL_FAILED_CASTER_AURASTATE;

        return SPELL_CAST_OK;
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetCaster()->CastSpell(GetCaster(), SPELL_SPECTRAL_EXHAUSTION, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_kalecgos_spectral_realm_dummy::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_kalecgos_spectral_realm_dummy::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_kalecgos_spectral_realm_aura : public AuraScript
{
    PrepareAuraScript(spell_kalecgos_spectral_realm_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SPECTRAL_EXHAUSTION });
    }

    void OnRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SPECTRAL_EXHAUSTION, true);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_kalecgos_spectral_realm_aura::OnRemove, EFFECT_1, SPELL_AURA_MOD_INVISIBILITY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_kalecgos()
{
    RegisterSunwellPlateauCreatureAI(boss_kalecgos);
    RegisterSunwellPlateauCreatureAI(boss_sathrovarr);
    RegisterSunwellPlateauCreatureAI(boss_kalec);
    RegisterSpellScript(spell_kalecgos_spectral_blast_dummy);
    RegisterSpellScript(spell_kalecgos_curse_of_boundless_agony);
    RegisterSpellAndAuraScriptPair(spell_kalecgos_curse_of_boundless_agony_player, spell_kalecgos_curse_of_boundless_agony_aura);
    RegisterSpellScript(spell_kalecgos_spectral_realm_dummy);
    RegisterSpellScript(spell_kalecgos_spectral_realm_aura);
}
