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
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"

#include <algorithm>
#include <vector>

enum Yells
{
    SAY_AGGRO                   = 0,
    SAY_SPECIAL                 = 1,
    SAY_KILL                    = 2,
    SAY_DEATH                   = 3,
    SAY_OUT_OF_COMBAT           = 4,

    SAY_GUEST                   = 0
};

enum Spells
{
    SPELL_VANISH                = 29448,
    SPELL_GARROTE_DUMMY         = 29433,
    SPELL_GARROTE               = 37066,
    SPELL_BLIND                 = 34694,
    SPELL_GOUGE                 = 29425,
    SPELL_FRENZY                = 37023,
    SPELL_DUAL_WIELD            = 29651,
    SPELL_BERSERK               = 26662,
    SPELL_VANISH_TELEPORT       = 29431
};

enum Misc
{
    ACTIVE_GUEST_COUNT          = 4,
    MAX_GUEST_COUNT             = 6
};

enum Groups
{
    GROUP_PRECOMBAT_TALK        = 0
};

const Position GuestsPosition[ACTIVE_GUEST_COUNT] =
{
    {-10987.38f, -1883.38f, 81.73f, 1.50f},
    {-10989.60f, -1881.27f, 81.73f, 0.73f},
    {-10978.81f, -1884.08f, 81.73f, 1.50f},
    {-10976.38f, -1882.59f, 81.73f, 2.31f}
};

const uint32 GuestEntries[MAX_GUEST_COUNT] =
{
    17007, 19872, 19873,
    19874, 19875, 19876
};

namespace
{
    bool IsNPCBotUnit(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && creature->GetBotAI();
    }

    bool IsMoroesGuestEntry(uint32 entry)
    {
        for (uint32 guestEntry : GuestEntries)
            if (entry == guestEntry)
                return true;

        return false;
    }

    bool IsMoroesEncounterUnit(Unit* unit, Creature const* source, float range)
    {
        if (!unit || !source || !source->GetMap())
            return false;

        if (!unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(source, range))
            return false;

        if (unit->IsPlayer())
            return true;

        if (Creature* creature = unit->ToCreature())
            return creature->IsNPCBot() && creature->GetBotAI();

        return false;
    }

    bool ContainsGuid(std::vector<Unit*> const& units, ObjectGuid const& guid)
    {
        return std::any_of(units.begin(), units.end(), [guid](Unit const* unit)
        {
            return unit && unit->GetGUID() == guid;
        });
    }

    void TryAddMoroesEncounterUnit(std::vector<Unit*>& units, Unit* unit, Creature* source, float range, bool requireAttackTarget = true)
    {
        if (!IsMoroesEncounterUnit(unit, source, range))
            return;

        if (ContainsGuid(units, unit->GetGUID()))
            return;

        if (requireAttackTarget && !source->IsValidAttackTarget(unit))
            return;

        units.push_back(unit);
    }

    std::vector<Unit*> GatherMoroesEncounterUnits(Creature* source, float range = 120.0f, bool requireAttackTarget = true)
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

            TryAddMoroesEncounterUnit(units, player, source, range, requireAttackTarget);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    TryAddMoroesEncounterUnit(units, member, source, range, requireAttackTarget);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
            {
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    TryAddMoroesEncounterUnit(units, pair.second, source, range, requireAttackTarget);
            }
        }

        return units;
    }

    std::vector<Unit*> GatherMoroesThreatUnits(Creature* source, float range = 120.0f, bool includeVictim = true)
    {
        std::vector<Unit*> units;

        if (!source || !source->GetMap())
            return units;

        Unit* victim = source->GetVictim();

        for (ThreatReference const* ref : source->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* unit = ref->GetVictim();
            if (!unit)
                continue;

            if (!includeVictim && victim && unit == victim)
                continue;

            TryAddMoroesEncounterUnit(units, unit, source, range, true);
        }

        return units;
    }

    Unit* SelectRandomMoroesThreatUnit(Creature* source, float range = 100.0f, bool includeVictim = false)
    {
        std::vector<Unit*> targets = GatherMoroesThreatUnits(source, range, includeVictim);

        if (targets.empty() && !includeVictim)
            targets = GatherMoroesThreatUnits(source, range, true);

        if (targets.empty())
            return nullptr;

        return targets[urand(0, targets.size() - 1)];
    }

    void RemoveGarroteFromMoroesEncounterUnits(Creature* source)
    {
        for (Unit* unit : GatherMoroesEncounterUnits(source, 200.0f, false))
            unit->RemoveAurasDueToSpell(SPELL_GARROTE);
    }

    void KeepMoroesInCombatWith(Creature* moroes, Unit* target, bool attackStart = true)
    {
        if (!moroes || !target || !moroes->IsAlive() || !target->IsAlive())
            return;

        if (target->GetMap() != moroes->GetMap())
            return;

        moroes->SetInCombatWithZone();
        moroes->SetInCombatWith(target);
        target->SetInCombatWith(moroes);
        moroes->GetThreatMgr().AddThreat(target, 1000.0f);

        // Do not force AttackStart during the Vanish/Garrote dummy hit.
        // AttackStart can break the hidden window early, especially when the
        // selected Garrote target is an NPCBot.  The reveal task reacquires
        // the victim once Moroes is supposed to be visible again.
        if (attackStart && moroes->AI())
            moroes->AI()->AttackStart(target);
    }
}

struct boss_moroes : public BossAI
{
    boss_moroes(Creature* creature) : BossAI(creature, DATA_MOROES)
    {
        _activeGuests = 0;
        _encounterStarted = false;
        _vanished = false;
        _recentlySpoken = false;
        _outOfCombatGuestCheckTimer = 1000;
    }

    void InitializeAI() override
    {
        BossAI::InitializeAI();
        InitializeGuests();
    }

    void Reset() override
    {
        BossAI::Reset();
        DoCastSelf(SPELL_DUAL_WIELD, true);
        _recentlySpoken = false;
        _vanished = false;
        me->RemoveAurasDueToSpell(SPELL_VANISH);
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_AGGRESSIVE);
        _encounterStarted = false;
        _outOfCombatGuestCheckTimer = 1000;

        ScheduleHealthCheckEvent(30, [&]
        {
            DoCastSelf(SPELL_FRENZY, true);
        });
    }

    void JustReachedHome() override
    {
        BossAI::JustReachedHome();
        InitializeGuests(true);
    }

    void InitializeGuests(bool forceRespawn = false)
    {
        if (!me->IsAlive())
            return;

        if (_activeGuests == 0)
        {
            _activeGuests |= 0x3F;
            uint8 rand1 = RAND(0x01, 0x02, 0x04);
            uint8 rand2 = RAND(0x08, 0x10, 0x20);
            _activeGuests &= ~(rand1 | rand2);
        }

        if (forceRespawn || !HasAllActiveGuestsAlive())
            RespawnActiveGuests();

        scheduler.CancelGroup(GROUP_PRECOMBAT_TALK);
        scheduler.Schedule(10s, GROUP_PRECOMBAT_TALK, [this](TaskContext context)
        {
            if (Creature* guest = GetRandomGuest())
                guest->AI()->Talk(SAY_GUEST);

            context.Repeat(5s);
        }).Schedule(1min, 2min, GROUP_PRECOMBAT_TALK, [this](TaskContext context)
        {
            Talk(SAY_OUT_OF_COMBAT);
            context.Repeat(1min, 2min);
        });
    }

    bool HasAllActiveGuestsAlive() const
    {
        uint8 livingGuests = 0;

        for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
        {
            if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                if (summon->IsAlive() && IsMoroesGuestEntry(summon->GetEntry()))
                    ++livingGuests;
        }

        return livingGuests >= ACTIVE_GUEST_COUNT;
    }

    void RespawnActiveGuests()
    {
        summons.DespawnAll();

        uint8 spawnSlot = 0;
        for (uint8 i = 0; i < MAX_GUEST_COUNT && spawnSlot < ACTIVE_GUEST_COUNT; ++i)
        {
            if (!((1 << i) & _activeGuests))
                continue;

            me->SummonCreature(GuestEntries[i], GuestsPosition[spawnSlot], TEMPSUMMON_MANUAL_DESPAWN);
            ++spawnSlot;
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        StartMoroesEncounter(who, true);
    }

    void PrepareVanishWindow()
    {
        _vanished = true;

        // Vanish must win against whatever Moroes was already doing.  If he is
        // still casting or being dragged forward by combat movement when the
        // Vanish event fires, the aura can fail to present cleanly or can be
        // stripped immediately by incoming periodic/channel damage.
        me->InterruptNonMeleeSpells(true);
        me->AttackStop();
        me->GetMotionMaster()->Clear();
        me->SetReactState(REACT_PASSIVE);
        me->SetTarget(ObjectGuid::Empty);

        // Immunity is applied before Vanish so already-ticking DoTs/channels do
        // not immediately break the stealth aura.  The boss remains in combat;
        // UpdateAI deliberately avoids UpdateVictim while _vanished is true.
        me->SetImmuneToAll(true, true);

        // Triggered cast avoids the cast being blocked by a previous spell state.
        me->CastSpell(me, SPELL_VANISH, true);

        // Some periodic effects can still race the first aura application on
        // custom cores.  Reassert the visual aura during the hidden window only;
        // do not Garrote manually here, because the Vanish SpellScript handles
        // the Garrote target and teleport.
        ObjectGuid moroesGuid = me->GetGUID();
        scheduler.Schedule(250ms, [this, moroesGuid](TaskContext /*context*/)
        {
            if (me->GetGUID() == moroesGuid && _vanished && me->IsAlive() && !me->HasAura(SPELL_VANISH))
                me->AddAura(SPELL_VANISH, me);
        });

        scheduler.Schedule(1250ms, [this, moroesGuid](TaskContext /*context*/)
        {
            if (me->GetGUID() == moroesGuid && _vanished && me->IsAlive() && !me->HasAura(SPELL_VANISH))
                me->AddAura(SPELL_VANISH, me);
        });
    }

    void RevealFromVanish()
    {
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_AGGRESSIVE);
        DoCastSelf(SPELL_VANISH_TELEPORT);
        me->RemoveAurasDueToSpell(SPELL_VANISH);
        _vanished = false;
        ReacquireVictimAfterVanish();
    }

    void StartMoroesEncounter(Unit* who, bool calledByCore)
    {
        if (!who || _encounterStarted)
            return;

        _encounterStarted = true;

        if (calledByCore)
            BossAI::JustEngagedWith(who);
        else
        {
            me->SetInCombatWith(who);
            who->SetInCombatWith(me);
            me->GetThreatMgr().AddThreat(who, 1.0f);
            AttackStart(who);
            BossAI::JustEngagedWith(who);
        }

        Talk(SAY_AGGRO);
        me->CallForHelp(20.0f);
        DoZoneInCombat();
        EngageGuests(who);
        scheduler.CancelGroup(GROUP_PRECOMBAT_TALK);

        scheduler.Schedule(30s, [this](TaskContext context)
        {
            scheduler.DelayAll(9s);
            Talk(SAY_SPECIAL);
            PrepareVanishWindow();

            scheduler.Schedule(5s, 7s, [this](TaskContext)
            {
                RevealFromVanish();
            });

            context.Repeat(30s);
        }).Schedule(20s, [this](TaskContext context)
        {
            if (Unit* target = SelectMaxThreatMoroesEncounterUnit(1, 10.0f))
                DoCast(target, SPELL_BLIND);

            context.Repeat(25s, 40s);
        }).Schedule(13s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_GOUGE);
            context.Repeat(25s, 40s);
        }).Schedule(10min, [this](TaskContext)
        {
            DoCastSelf(SPELL_BERSERK, true);
        });
    }

    void ReacquireVictimAfterVanish()
    {
        std::vector<Unit*> targets = GatherMoroesThreatUnits(me, 120.0f, true);

        if (targets.empty())
            return;

        std::sort(targets.begin(), targets.end(), [this](Unit* left, Unit* right)
        {
            return DoGetThreat(left) > DoGetThreat(right);
        });

        Unit* target = targets.front();
        if (!target)
            return;

        KeepMoroesInCombatWith(me, target);
        me->GetMotionMaster()->MoveChase(target);
    }

    void EngageGuests(Unit* target)
    {
        if (!target)
            return;

        summons.DoForAllSummons([target](WorldObject* summon)
        {
            Creature* guest = summon ? summon->ToCreature() : nullptr;
            if (!guest || !guest->IsAlive() || !IsMoroesGuestEntry(guest->GetEntry()))
                return;

            guest->SetInCombatWithZone();
            guest->SetInCombatWith(target);
            target->SetInCombatWith(guest);
            guest->GetThreatMgr().AddThreat(target, 1.0f);

            if (guest->AI())
                guest->AI()->AttackStart(target);
        });
    }

    bool FindPulledGuestVictim(Unit*& guestVictim)
    {
        guestVictim = nullptr;

        summons.DoForAllSummons([this, &guestVictim](WorldObject* summon)
        {
            if (guestVictim)
                return;

            Creature* guest = summon ? summon->ToCreature() : nullptr;
            if (!guest || !guest->IsAlive() || !IsMoroesGuestEntry(guest->GetEntry()))
                return;

            if (!guest->IsInCombat())
                return;

            Unit* victim = guest->GetVictim();
            if (!victim || !victim->IsAlive() || victim->GetMap() != me->GetMap())
                return;

            guestVictim = victim;
        });

        return guestVictim != nullptr;
    }

    bool PullMoroesIfGuestPulled()
    {
        if (_encounterStarted || !me->IsAlive())
            return false;

        Unit* guestVictim = nullptr;
        if (!FindPulledGuestVictim(guestVictim))
            return false;

        StartMoroesEncounter(guestVictim, false);
        return true;
    }

    void OutOfCombatGuestCheck()
    {
        if (_encounterStarted || me->IsInCombat() || !me->IsAlive())
            return;

        // If a guest was body-pulled while Moroes himself is still idle, pull
        // Moroes into the encounter from this periodic OOC check instead of
        // relying on the boss being directly attacked.
        if (PullMoroesIfGuestPulled())
            return;

        // If a wipe/reset left the dinner guests partially dead/despawned,
        // rebuild the selected four while Moroes is truly out of combat.
        if (!HasAllActiveGuestsAlive())
            InitializeGuests(true);
    }

    Unit* SelectMaxThreatMoroesEncounterUnit(uint8 threatIndex, float range)
    {
        std::vector<Unit*> targets = GatherMoroesThreatUnits(me, range, true);

        if (targets.empty())
            return nullptr;

        std::sort(targets.begin(), targets.end(), [this](Unit* left, Unit* right)
        {
            return DoGetThreat(left) > DoGetThreat(right);
        });

        if (threatIndex >= targets.size())
            return nullptr;

        return targets[threatIndex];
    }

    void KilledUnit(Unit* victim) override
    {
        if (!_recentlySpoken && (victim->IsPlayer() || IsNPCBotUnit(victim)))
        {
            Talk(SAY_KILL);
            _recentlySpoken = true;
            scheduler.Schedule(5s, [this](TaskContext)
            {
                _recentlySpoken = false;
            });
        }
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
        me->RemoveAurasDueToSpell(SPELL_VANISH);
        me->SetImmuneToAll(false);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_GARROTE);
        RemoveGarroteFromMoroesEncounterUnits(me);
    }

    Creature* GetRandomGuest()
    {
        std::list<Creature*> guestList;
        for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
        {
            if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                if (summon->IsAlive() && IsMoroesGuestEntry(summon->GetEntry()))
                    guestList.push_back(summon);
        }

        if (guestList.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(guestList);
    }

    bool CheckGuestsInRoom()
    {
        bool guestsInRoom = true;
        summons.DoForAllSummons([&guestsInRoom](WorldObject* summon)
        {
            Creature* guest = summon ? summon->ToCreature() : nullptr;
            if (!guest || !guest->IsAlive() || !IsMoroesGuestEntry(guest->GetEntry()))
                return true;

            if (guest->GetPositionX() < -11028.f || guest->GetPositionY() < -1955.f) // boundaries of the two doors
            {
                guestsInRoom = false;
                return false;
            }

            return true;
        });

        return guestsInRoom;
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!CheckGuestsInRoom())
        {
            EnterEvadeMode();
            return;
        }

        if (!_encounterStarted && !me->IsInCombat())
        {
            if (_outOfCombatGuestCheckTimer <= diff)
            {
                OutOfCombatGuestCheck();
                _outOfCombatGuestCheckTimer = 1000;
            }
            else
                _outOfCombatGuestCheckTimer -= diff;

            return;
        }

        PullMoroesIfGuestPulled();

        // During Vanish/Garrote Moroes is intentionally immune and can briefly have no
        // ordinary melee victim, especially when the Garrote target is an NPCBot.  Do not
        // let UpdateVictim turn that short scripted window into an evade/reset.
        if (_vanished)
        {
            me->SetInCombatWithZone();
            return;
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events2;
    uint8 _activeGuests;
    bool _recentlySpoken;
    bool _vanished;
    bool _encounterStarted;
    uint32 _outOfCombatGuestCheckTimer;
};

class spell_moroes_vanish : public SpellScript
{
    PrepareSpellScript(spell_moroes_vanish);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        if (!caster)
            return;

        Unit* selected = SelectRandomMoroesThreatUnit(caster, 100.0f, false);
        if (!selected)
            return;

        targets.clear();
        targets.push_back(selected);
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        Creature* caster = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        Position pos = target->GetFirstCollisionPosition(5.0f, M_PI);
        caster->CastSpell(target, SPELL_GARROTE_DUMMY, true);
        caster->NearTeleportTo(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), target->GetOrientation());

        // NPCBot Garrote targets need an explicit combat/threat refresh.  Without this,
        // Moroes can finish his vanish window with no stable victim and evade/reset.
        // Do not AttackStart here, though; doing so can make Moroes visibly re-engage
        // before the scripted Vanish reveal timer has elapsed.
        KeepMoroesInCombatWith(caster, target, false);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_moroes_vanish::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_moroes_vanish::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_moroes()
{
    RegisterKarazhanCreatureAI(boss_moroes);
    RegisterSpellScript(spell_moroes_vanish);
}
