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

#include "Chat.h"
#include "CreatureScript.h"
#include "DatabaseEnv.h"
#include "Mail.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

#include <array>
#include <deque>
#include <limits>
#include <map>
#include <string>

enum SpellScripts
{
    SPELL_OOZE_ZAP = 42489,
    SPELL_OOZE_ZAP_CHANNEL_END = 42485,
    SPELL_OOZE_CHANNEL_CREDIT = 42486,
    SPELL_ENERGIZED = 42492,
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


namespace TheramoreSpar
{
    enum Npcs : uint32
    {
        NPC_GUARD_NARRISHA = 5093,
        NPC_GUARD_KAHIL = 5091,
        NPC_GUARD_EDWARD = 4922,

        NPC_GUARD_TARK = 5094,
        NPC_GUARD_LANA = 5092,
        NPC_GUARD_JARAD = 4923,

        NPC_MEDIC_HELAINA = 5200, // Red healer
        NPC_MEDIC_TAMBERLYN = 5199, // Blue healer

        NPC_MASTER_SZIGETI = 5090,
        NPC_MASTER_CRITON = 4924,

        NPC_BLUE_CAPTAIN = 5096,
        NPC_RED_CAPTAIN = 5095
    };

    enum Factions : uint32
    {
        FACTION_THERAMORE_FRIENDLY = 151,

        // These are faction template IDs used by the CMaNGOS spar script.
        FACTION_BLUE_TEAM = 1621,
        FACTION_RED_TEAM = 1622
    };

    enum Spells : uint32
    {
        SPELL_FIRST_AID = 7162
    };

    enum Teams : uint8
    {
        TEAM_RED = 0,
        TEAM_BLUE = 1,
        TEAM_NONE = 2
    };

    enum Events : uint32
    {
        EVENT_GET_COMBATANTS = 1,
        EVENT_OPEN_BETTING,
        EVENT_CLOSE_BETTING,
        EVENT_MOVE_INTO_POSITION,
        EVENT_SALUTE,
        EVENT_INTRO,
        EVENT_START_SPAR,
        EVENT_CHECK_HP,
        EVENT_BLUE_WIN,
        EVENT_RED_WIN,
        EVENT_SEPARATE,
        EVENT_SWITCH_MEMBERS,
        EVENT_MOTIVATION
    };

    enum BettingState : uint8
    {
        BETTING_CLOSED = 0,
        BETTING_OPEN,
        BETTING_MATCH_RUNNING,
        BETTING_PAYOUT_DONE
    };

    enum GossipActions : uint32
    {
        ACTION_STATUS = 1,

        ACTION_BET_RED_1G = 101,
        ACTION_BET_RED_5G = 105,
        ACTION_BET_RED_10G = 110,

        ACTION_BET_BLUE_1G = 201,
        ACTION_BET_BLUE_5G = 205,
        ACTION_BET_BLUE_10G = 210,

        ACTION_CANCEL_BET = 900
    };

    static constexpr uint32 COPPER = 1;
    static constexpr uint32 SILVER = 100 * COPPER;
    static constexpr uint32 GOLD = 100 * SILVER;

    static constexpr uint32 BETTING_WINDOW_SECONDS = 60;
    static constexpr uint32 HOUSE_CUT_PERCENT = 10;
    static constexpr uint32 SOLO_WIN_PROFIT_PERCENT = 90;

    static constexpr float SEARCH_RANGE = 20.0f;
    static constexpr float MASTER_RANGE = 15.0f;
    static constexpr float LOSS_HEALTH_PCT = 30.0f;

    static Position const BlueStart(-3643.574707f, -4505.717773f, 9.462863f, 3.855592f);
    static Position const RedStart(-3652.405273f, -4514.126953f, 9.462863f, 0.717925f);

    static std::array<uint32, 3> const BlueTeamEntries =
    {
        NPC_GUARD_EDWARD,
        NPC_GUARD_KAHIL,
        NPC_GUARD_NARRISHA
    };

    static std::array<uint32, 3> const RedTeamEntries =
    {
        NPC_GUARD_JARAD,
        NPC_GUARD_LANA,
        NPC_GUARD_TARK
    };

    static char const* TeamName(uint8 team)
    {
        switch (team)
        {
        case TEAM_RED:
            return "Red Team";
        case TEAM_BLUE:
            return "Blue Team";
        default:
            return "No Team";
        }
    }

    static std::string MoneyToString(uint64 amount)
    {
        uint64 gold = amount / GOLD;
        amount %= GOLD;

        uint64 silver = amount / SILVER;
        uint64 copper = amount % SILVER;

        std::string result;

        if (gold)
            result += std::to_string(gold) + "g ";

        if (silver || gold)
            result += std::to_string(silver) + "s ";

        result += std::to_string(copper) + "c";

        return result;
    }

    struct BetInfo
    {
        uint8 Team = TEAM_NONE;
        uint64 Stake = 0;
    };
}

class npc_theramore_spar_controller : public CreatureScript
{
public:
    npc_theramore_spar_controller() : CreatureScript("npc_theramore_spar_controller") {}

    struct npc_theramore_spar_controllerAI : public ScriptedAI
    {
        npc_theramore_spar_controllerAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            _events.Reset();

            RefundAllBets("The sparring match was reset. Your wager has been refunded.");

            _redBench.clear();
            _blueBench.clear();

            _activeRed.Clear();
            _activeBlue.Clear();

            _redMedic.Clear();
            _blueMedic.Clear();

            _boutFinished = false;
            _winningTeam = TheramoreSpar::TEAM_NONE;
            _bettingState = TheramoreSpar::BETTING_CLOSED;

            me->SetReactState(REACT_PASSIVE);
            SetSzigetiGossip(false);

            _events.ScheduleEvent(TheramoreSpar::EVENT_GET_COMBATANTS, 2s);
            _events.ScheduleEvent(TheramoreSpar::EVENT_OPEN_BETTING, 4s);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            // Master Szigeti is only a controller here. If something bothers him,
            // drop combat and keep the pageant moving.
            me->CombatStop(true);
            me->AttackStop();
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                case TheramoreSpar::EVENT_GET_COMBATANTS:
                    GetCombatants();
                    break;

                case TheramoreSpar::EVENT_OPEN_BETTING:
                    OpenBettingWindow();
                    break;

                case TheramoreSpar::EVENT_CLOSE_BETTING:
                    CloseBettingWindow();
                    break;

                case TheramoreSpar::EVENT_MOVE_INTO_POSITION:
                    MoveIntoPosition();
                    break;

                case TheramoreSpar::EVENT_SALUTE:
                    CombatantSalute();
                    break;

                case TheramoreSpar::EVENT_INTRO:
                    CombatantIntro();
                    break;

                case TheramoreSpar::EVENT_START_SPAR:
                    StartSparring();
                    break;

                case TheramoreSpar::EVENT_CHECK_HP:
                    CheckCombatantHP();
                    break;

                case TheramoreSpar::EVENT_BLUE_WIN:
                    HandleBlueWin();
                    break;

                case TheramoreSpar::EVENT_RED_WIN:
                    HandleRedWin();
                    break;

                case TheramoreSpar::EVENT_SEPARATE:
                    CombatantSeparate();
                    break;

                case TheramoreSpar::EVENT_SWITCH_MEMBERS:
                    SwitchMembers();
                    break;

                case TheramoreSpar::EVENT_MOTIVATION:
                    HandleMotivation();
                    break;

                default:
                    break;
                }
            }
        }

        bool IsBettingOpen() const
        {
            return _bettingState == TheramoreSpar::BETTING_OPEN;
        }

        TheramoreSpar::BetInfo const* GetBetFor(Player const* player) const
        {
            if (!player)
                return nullptr;

            auto itr = _bets.find(player->GetGUID());
            if (itr == _bets.end())
                return nullptr;

            return &itr->second;
        }

        std::string GetBettingSummaryFor(Player const* player) const
        {
            std::string summary = "Current pools: Red ";
            summary += TheramoreSpar::MoneyToString(_redPool);
            summary += " / Blue ";
            summary += TheramoreSpar::MoneyToString(_bluePool);
            summary += ".";

            if (TheramoreSpar::BetInfo const* bet = GetBetFor(player))
            {
                summary += " Your bet: ";
                summary += TheramoreSpar::MoneyToString(bet->Stake);
                summary += " on ";
                summary += TheramoreSpar::TeamName(bet->Team);
                summary += ".";
            }
            else
                summary += " You have no active bet.";

            return summary;
        }

        void PlaceBet(Player* player, uint8 team, uint64 stake)
        {
            if (!player)
                return;

            if (!IsBettingOpen())
            {
                Notify(player, "Bets are closed. Szigeti has already put his book away.");
                return;
            }

            if (team != TheramoreSpar::TEAM_RED && team != TheramoreSpar::TEAM_BLUE)
                return;

            if (!stake)
                return;

            // One active bet per player. If they change their mind, refund the old
            // ticket first, then sell the new one.
            auto existing = _bets.find(player->GetGUID());
            if (existing != _bets.end())
            {
                RemoveBetFromPool(existing->second);
                GiveMoney(player, existing->second.Stake);
                _bets.erase(existing);
            }

            if (player->GetMoney() < stake)
            {
                Notify(player, "You do not have enough coin for that bet.");
                return;
            }

            if (stake > uint64(std::numeric_limits<int32>::max()))
            {
                Notify(player, "That wager is too large for Szigeti's little ledger.");
                return;
            }

            player->ModifyMoney(-int32(stake));

            TheramoreSpar::BetInfo bet;
            bet.Team = team;
            bet.Stake = stake;

            _bets[player->GetGUID()] = bet;
            AddBetToPool(bet);

            std::string message = "You place ";
            message += TheramoreSpar::MoneyToString(stake);
            message += " on ";
            message += TheramoreSpar::TeamName(team);
            message += ".";

            Notify(player, message.c_str());
        }

        void CancelBet(Player* player)
        {
            if (!player)
                return;

            if (!IsBettingOpen())
            {
                Notify(player, "Too late to cancel. The coin is on the table and the swords are out.");
                return;
            }

            auto itr = _bets.find(player->GetGUID());
            if (itr == _bets.end())
            {
                Notify(player, "You have no active bet to cancel.");
                return;
            }

            uint64 stake = itr->second.Stake;
            RemoveBetFromPool(itr->second);
            GiveMoney(player, stake);
            _bets.erase(itr);

            std::string message = "Your bet has been cancelled and ";
            message += TheramoreSpar::MoneyToString(stake);
            message += " has been returned.";

            Notify(player, message.c_str());
        }

    private:
        EventMap _events;

        std::deque<ObjectGuid> _redBench;
        std::deque<ObjectGuid> _blueBench;

        ObjectGuid _activeRed;
        ObjectGuid _activeBlue;

        ObjectGuid _redMedic;
        ObjectGuid _blueMedic;

        uint8 _winningTeam = TheramoreSpar::TEAM_NONE;
        uint8 _bettingState = TheramoreSpar::BETTING_CLOSED;
        bool _boutFinished = false;

        std::map<ObjectGuid, TheramoreSpar::BetInfo> _bets;
        uint64 _redPool = 0;
        uint64 _bluePool = 0;

        static uint8 OpposingTeam(uint8 team)
        {
            return team == TheramoreSpar::TEAM_RED ? TheramoreSpar::TEAM_BLUE : TheramoreSpar::TEAM_RED;
        }

        static uint32 TeamFaction(uint8 team)
        {
            return team == TheramoreSpar::TEAM_RED ? TheramoreSpar::FACTION_RED_TEAM : TheramoreSpar::FACTION_BLUE_TEAM;
        }

        static Position const& TeamStartPosition(uint8 team)
        {
            return team == TheramoreSpar::TEAM_RED ? TheramoreSpar::RedStart : TheramoreSpar::BlueStart;
        }

        std::deque<ObjectGuid>& Bench(uint8 team)
        {
            return team == TheramoreSpar::TEAM_RED ? _redBench : _blueBench;
        }

        ObjectGuid& ActiveGuid(uint8 team)
        {
            return team == TheramoreSpar::TEAM_RED ? _activeRed : _activeBlue;
        }

        ObjectGuid const& ActiveGuid(uint8 team) const
        {
            return team == TheramoreSpar::TEAM_RED ? _activeRed : _activeBlue;
        }

        Creature* GetCreature(ObjectGuid const& guid) const
        {
            if (guid.IsEmpty())
                return nullptr;

            return ObjectAccessor::GetCreature(*me, guid);
        }

        Creature* GetActive(uint8 team) const
        {
            Creature* creature = GetCreature(ActiveGuid(team));
            return creature && creature->IsAlive() ? creature : nullptr;
        }

        Creature* FindNearby(uint32 entry, float range = TheramoreSpar::SEARCH_RANGE) const
        {
            return me->FindNearestCreature(entry, range, true);
        }

        Creature* GetSzigeti() const
        {
            if (me->GetEntry() == TheramoreSpar::NPC_MASTER_SZIGETI)
                return me;

            return FindNearby(TheramoreSpar::NPC_MASTER_SZIGETI, TheramoreSpar::MASTER_RANGE);
        }

        void SetSzigetiGossip(bool enabled)
        {
            Creature* szigeti = GetSzigeti();
            if (!szigeti)
                szigeti = me;

            if (enabled)
                szigeti->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            else
                szigeti->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        }

        Creature* GetCriton() const
        {
            return FindNearby(TheramoreSpar::NPC_MASTER_CRITON, TheramoreSpar::MASTER_RANGE);
        }

        Creature* GetCaptain(uint8 team) const
        {
            return FindNearby(team == TheramoreSpar::TEAM_RED ? TheramoreSpar::NPC_RED_CAPTAIN : TheramoreSpar::NPC_BLUE_CAPTAIN);
        }

        Creature* GetMedic(uint8 team) const
        {
            return GetCreature(team == TheramoreSpar::TEAM_RED ? _redMedic : _blueMedic);
        }

        static void Say(Creature* speaker, char const* text)
        {
            if (speaker && speaker->IsAlive())
                speaker->Say(text, LANG_UNIVERSAL);
        }

        static void Emote(Creature* creature, uint32 emote)
        {
            if (creature && creature->IsAlive())
                creature->HandleEmoteCommand(emote);
        }

        static void Notify(Player* player, char const* text)
        {
            if (player && player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage(text);
        }

        static void Notify(Player* player, std::string const& text)
        {
            Notify(player, text.c_str());
        }

        static void GiveMoney(Player* player, uint64 amount)
        {
            if (!player || !amount)
                return;

            while (amount > uint64(std::numeric_limits<int32>::max()))
            {
                player->ModifyMoney(std::numeric_limits<int32>::max());
                amount -= uint64(std::numeric_limits<int32>::max());
            }

            player->ModifyMoney(int32(amount));
        }

        void SendSparMail(ObjectGuid receiverGuid, uint64 amount, char const* subject, char const* body) const
        {
            if (receiverGuid.IsEmpty())
                return;

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            do
            {
                uint32 mailAmount = amount > uint64(std::numeric_limits<uint32>::max())
                    ? std::numeric_limits<uint32>::max()
                    : uint32(amount);

                MailDraft(subject, body)
                    .AddMoney(mailAmount)
                    .SendMailTo(trans, MailReceiver(receiverGuid.GetCounter()), MailSender(MAIL_CREATURE, TheramoreSpar::NPC_MASTER_SZIGETI));

                amount -= mailAmount;
            }
            while (amount);

            CharacterDatabase.CommitTransaction(trans);
        }

        bool GiveMoneyOrMail(ObjectGuid receiverGuid, Player* player, uint64 amount, char const* subject, char const* body) const
        {
            if (!amount)
                return true;

            if (player)
            {
                GiveMoney(player, amount);
                return true;
            }

            SendSparMail(receiverGuid, amount, subject, body);
            return false;
        }

        void AddBetToPool(TheramoreSpar::BetInfo const& bet)
        {
            if (bet.Team == TheramoreSpar::TEAM_RED)
                _redPool += bet.Stake;
            else if (bet.Team == TheramoreSpar::TEAM_BLUE)
                _bluePool += bet.Stake;
        }

        void RemoveBetFromPool(TheramoreSpar::BetInfo const& bet)
        {
            if (bet.Team == TheramoreSpar::TEAM_RED)
                _redPool = bet.Stake > _redPool ? 0 : _redPool - bet.Stake;
            else if (bet.Team == TheramoreSpar::TEAM_BLUE)
                _bluePool = bet.Stake > _bluePool ? 0 : _bluePool - bet.Stake;
        }

        void ClearBets()
        {
            _bets.clear();
            _redPool = 0;
            _bluePool = 0;
        }

        void RefundAllBets(char const* reason)
        {
            if (_bets.empty())
            {
                ClearBets();
                return;
            }

            for (auto const& pair : _bets)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, pair.first);
                bool paidOnline = GiveMoneyOrMail(pair.first, player, pair.second.Stake,
                    "Theramore Sparring Wager Refund",
                    "Your Theramore sparring wager was refunded while you were away from the ring.");

                if (paidOnline)
                    Notify(player, reason);
            }

            ClearBets();
        }

        static void MakeFriendlyIdle(Creature* creature)
        {
            if (!creature)
                return;

            creature->SetFaction(TheramoreSpar::FACTION_THERAMORE_FRIENDLY);
            creature->SetReactState(REACT_PASSIVE);
            creature->CombatStop(true);
            creature->AttackStop();
        }

        static void PrepareCombatant(Creature* creature)
        {
            if (!creature || !creature->IsAlive())
                return;

            MakeFriendlyIdle(creature);
            creature->SetStandState(UNIT_STAND_STATE_STAND);
            creature->SetHealth(creature->GetMaxHealth());
        }

        static void MoveToPoint(Creature* creature, Position const& position, ForcedMovement movement)
        {
            if (!creature || !creature->IsAlive())
                return;

            creature->SetReactState(REACT_PASSIVE);
            creature->CombatStop(true);
            creature->AttackStop();
            creature->GetMotionMaster()->Clear();
            creature->GetMotionMaster()->MovePoint(1, position, movement);
        }

        static void MoveHome(Creature* creature, ForcedMovement movement)
        {
            if (!creature || !creature->IsAlive())
                return;

            MakeFriendlyIdle(creature);
            creature->GetMotionMaster()->Clear();
            creature->GetMotionMaster()->MovePoint(0, creature->GetHomePosition(), movement);
        }

        void OpenBettingWindow()
        {
            if (!_bets.empty())
                RefundAllBets("The previous Theramore sparring wager was not resolved cleanly. Your wager has been refunded.");
            else
                ClearBets();

            if (_activeRed.IsEmpty() && _activeBlue.IsEmpty() && (_redBench.empty() || _blueBench.empty()))
                GetCombatants();

            if (_activeRed.IsEmpty() && _activeBlue.IsEmpty() && (_redBench.empty() || _blueBench.empty()))
            {
                Say(GetSzigeti(), "The ring is not ready. Fighters, report in!");
                _events.ScheduleEvent(TheramoreSpar::EVENT_GET_COMBATANTS, 10s);
                _events.ScheduleEvent(TheramoreSpar::EVENT_OPEN_BETTING, 12s);
                return;
            }

            _boutFinished = false;
            _winningTeam = TheramoreSpar::TEAM_NONE;
            _bettingState = TheramoreSpar::BETTING_OPEN;

            SetSzigetiGossip(true);

            Say(GetSzigeti(), "Place your bets! Red team or blue team. Coin on the table, courage in the heart!");
            Say(GetCriton(), "The betting window is open. The match begins in one minute.");

            _events.ScheduleEvent(TheramoreSpar::EVENT_CLOSE_BETTING, Seconds(TheramoreSpar::BETTING_WINDOW_SECONDS));
        }

        void CloseBettingWindow()
        {
            if (_bettingState != TheramoreSpar::BETTING_OPEN)
                return;

            _bettingState = TheramoreSpar::BETTING_CLOSED;
            SetSzigetiGossip(false);

            Say(GetSzigeti(), "Bets are closed! Step back from the ring!");
            Say(GetCriton(), "Combatants, to your marks!");

            _events.ScheduleEvent(TheramoreSpar::EVENT_MOVE_INTO_POSITION, 2s);
        }

        void GetCombatants()
        {
            _redBench.clear();
            _blueBench.clear();

            _activeRed.Clear();
            _activeBlue.Clear();

            for (uint32 entry : TheramoreSpar::RedTeamEntries)
            {
                if (Creature* member = FindNearby(entry))
                {
                    PrepareCombatant(member);
                    _redBench.push_back(member->GetGUID());
                }
            }

            for (uint32 entry : TheramoreSpar::BlueTeamEntries)
            {
                if (Creature* member = FindNearby(entry))
                {
                    PrepareCombatant(member);
                    _blueBench.push_back(member->GetGUID());
                }
            }

            if (Creature* medic = FindNearby(TheramoreSpar::NPC_MEDIC_HELAINA))
                _redMedic = medic->GetGUID();
            else
                _redMedic.Clear();

            if (Creature* medic = FindNearby(TheramoreSpar::NPC_MEDIC_TAMBERLYN))
                _blueMedic = medic->GetGUID();
            else
                _blueMedic.Clear();
        }

        void MoveIntoPosition()
        {
            if (_activeRed.IsEmpty() && _activeBlue.IsEmpty())
            {
                if (_redBench.empty() || _blueBench.empty())
                {
                    Say(GetSzigeti(), "Hold! We are missing combatants. Nobody wins a purse by stabbing empty air.");
                    RefundAllBets("The Theramore sparring match could not start. Your wager has been refunded.");
                    _events.ScheduleEvent(TheramoreSpar::EVENT_GET_COMBATANTS, 5s);
                    _events.ScheduleEvent(TheramoreSpar::EVENT_OPEN_BETTING, 10s);
                    return;
                }

                _activeRed = _redBench.front();
                _redBench.pop_front();

                _activeBlue = _blueBench.front();
                _blueBench.pop_front();

                Say(GetCriton(), "First combatants, step forward!");
                Say(GetCaptain(TheramoreSpar::TEAM_RED), "Jarad, take the line. Fight clean, fight hard!");
                Say(GetCaptain(TheramoreSpar::TEAM_BLUE), "Edward, front and center. Show them blue steel!");
            }

            Creature* red = GetActive(TheramoreSpar::TEAM_RED);
            Creature* blue = GetActive(TheramoreSpar::TEAM_BLUE);

            if (!red || !blue)
            {
                RefundAllBets("The Theramore sparring match broke before it began. Your wager has been refunded.");
                Reset();
                return;
            }

            MoveToPoint(red, TheramoreSpar::RedStart, FORCED_MOVEMENT_WALK);
            MoveToPoint(blue, TheramoreSpar::BlueStart, FORCED_MOVEMENT_WALK);

            _events.ScheduleEvent(TheramoreSpar::EVENT_SALUTE, 5s);
        }

        void CombatantSalute()
        {
            Creature* red = GetActive(TheramoreSpar::TEAM_RED);
            Creature* blue = GetActive(TheramoreSpar::TEAM_BLUE);

            if (!red || !blue)
            {
                RefundAllBets("The Theramore sparring match failed to form. Your wager has been refunded.");
                Reset();
                return;
            }

            Emote(red, EMOTE_ONESHOT_SALUTE);
            Say(red, "For the red team!");

            Emote(blue, EMOTE_ONESHOT_SALUTE);
            Say(blue, "For the blue team!");

            _events.ScheduleEvent(TheramoreSpar::EVENT_INTRO, 3s);
        }

        void CombatantIntro()
        {
            Say(GetCriton(), "Combatants, ready yourselves. No killing blows. At thirty percent, the round is over.");

            _events.ScheduleEvent(TheramoreSpar::EVENT_START_SPAR, 3s);
        }

        void StartSparring()
        {
            Creature* red = GetActive(TheramoreSpar::TEAM_RED);
            Creature* blue = GetActive(TheramoreSpar::TEAM_BLUE);

            if (!red || !blue)
            {
                RefundAllBets("The Theramore sparring match failed to start. Your wager has been refunded.");
                Reset();
                return;
            }

            _bettingState = TheramoreSpar::BETTING_MATCH_RUNNING;
            SetSzigetiGossip(false);

            Say(GetCriton(), "Begin!");

            red->SetFaction(TeamFaction(TheramoreSpar::TEAM_RED));
            red->SetReactState(REACT_DEFENSIVE);

            blue->SetFaction(TeamFaction(TheramoreSpar::TEAM_BLUE));
            blue->SetReactState(REACT_DEFENSIVE);

            red->AI()->AttackStart(blue);
            blue->AI()->AttackStart(red);

            _events.ScheduleEvent(TheramoreSpar::EVENT_CHECK_HP, 1s);
        }

        void CheckCombatantHP()
        {
            Creature* red = GetActive(TheramoreSpar::TEAM_RED);
            Creature* blue = GetActive(TheramoreSpar::TEAM_BLUE);

            if (!red || !blue)
            {
                RefundAllBets("The Theramore sparring match broke during combat. Your wager has been refunded.");
                Reset();
                return;
            }

            float redHealth = red->GetHealthPct();
            float blueHealth = blue->GetHealthPct();

            if (redHealth <= TheramoreSpar::LOSS_HEALTH_PCT || blueHealth <= TheramoreSpar::LOSS_HEALTH_PCT)
            {
                uint8 winningTeam = redHealth < blueHealth ? TheramoreSpar::TEAM_BLUE : TheramoreSpar::TEAM_RED;

                MakeFriendlyIdle(red);
                MakeFriendlyIdle(blue);

                if (Creature* redMedic = GetMedic(TheramoreSpar::TEAM_RED))
                    redMedic->CastSpell(red, TheramoreSpar::SPELL_FIRST_AID, false);

                if (Creature* blueMedic = GetMedic(TheramoreSpar::TEAM_BLUE))
                    blueMedic->CastSpell(blue, TheramoreSpar::SPELL_FIRST_AID, false);

                _events.ScheduleEvent(winningTeam == TheramoreSpar::TEAM_BLUE ? TheramoreSpar::EVENT_BLUE_WIN : TheramoreSpar::EVENT_RED_WIN, 100ms);
                return;
            }

            _events.ScheduleEvent(TheramoreSpar::EVENT_CHECK_HP, 1s);
        }

        void CheerBench(uint8 winningTeam)
        {
            ObjectGuid active = ActiveGuid(winningTeam);
            auto const& entries = winningTeam == TheramoreSpar::TEAM_RED ? TheramoreSpar::RedTeamEntries : TheramoreSpar::BlueTeamEntries;

            for (uint32 entry : entries)
            {
                if (Creature* combatant = FindNearby(entry))
                {
                    if (combatant->GetGUID() != active)
                        Emote(combatant, EMOTE_ONESHOT_CHEER);
                }
            }
        }

        static char const* RedLossLine(uint32 entry)
        {
            switch (entry)
            {
            case TheramoreSpar::NPC_GUARD_JARAD:
                return "Jarad, shake it off. Back to the line with you.";
            case TheramoreSpar::NPC_GUARD_LANA:
                return "Lana, breathe. You gave ground, not honor.";
            case TheramoreSpar::NPC_GUARD_TARK:
                return "Tark, that was a hard lesson. Learn it.";
            default:
                return "Red team, regroup!";
            }
        }

        static char const* BlueLossLine(uint32 entry)
        {
            switch (entry)
            {
            case TheramoreSpar::NPC_GUARD_EDWARD:
                return "Edward, you are not done. Back to drills.";
            case TheramoreSpar::NPC_GUARD_KAHIL:
                return "Kahil, guard up next time. That one was clean.";
            case TheramoreSpar::NPC_GUARD_NARRISHA:
                return "Narrisha, reset your footing and remember your training.";
            default:
                return "Blue team, regroup!";
            }
        }

        void HandleBlueWin()
        {
            Creature* szigeti = GetSzigeti();
            Creature* redCaptain = GetCaptain(TheramoreSpar::TEAM_RED);
            Creature* redLoser = GetActive(TheramoreSpar::TEAM_RED);

            Emote(szigeti, EMOTE_ONESHOT_POINT);
            Say(szigeti, "Point to the blue team!");

            CheerBench(TheramoreSpar::TEAM_BLUE);

            if (redCaptain && redLoser)
                Say(redCaptain, RedLossLine(redLoser->GetEntry()));

            _winningTeam = TheramoreSpar::TEAM_BLUE;
            HandleWin(TheramoreSpar::TEAM_BLUE);
        }

        void HandleRedWin()
        {
            Creature* szigeti = GetSzigeti();
            Creature* blueCaptain = GetCaptain(TheramoreSpar::TEAM_BLUE);
            Creature* blueLoser = GetActive(TheramoreSpar::TEAM_BLUE);

            Emote(szigeti, EMOTE_ONESHOT_POINT);
            Say(szigeti, "Point to the red team!");

            CheerBench(TheramoreSpar::TEAM_RED);

            if (blueCaptain && blueLoser)
                Say(blueCaptain, BlueLossLine(blueLoser->GetEntry()));

            _winningTeam = TheramoreSpar::TEAM_RED;
            HandleWin(TheramoreSpar::TEAM_RED);
        }

        void HandleWin(uint8 winningTeam)
        {
            uint8 losingTeam = OpposingTeam(winningTeam);
            bool matchOver = Bench(losingTeam).empty();

            Creature* criton = GetCriton();
            if (criton)
            {
                Say(criton, "Back to your corners!");

                if (matchOver)
                    Say(criton, winningTeam == TheramoreSpar::TEAM_BLUE
                        ? "Blue team takes the match!"
                        : "Red team takes the match!");
            }

            FinishBout(winningTeam);

            _events.ScheduleEvent(TheramoreSpar::EVENT_SEPARATE, 100ms);
        }

        void FinishBout(uint8 winningTeam)
        {
            if (_boutFinished)
                return;

            _boutFinished = true;
            _winningTeam = winningTeam;

            PayOutBets(winningTeam);

            _bettingState = TheramoreSpar::BETTING_PAYOUT_DONE;
            _events.ScheduleEvent(TheramoreSpar::EVENT_MOTIVATION, 8s);
        }

        void PayOutBets(uint8 winningTeam)
        {
            if (_bets.empty())
            {
                Say(GetSzigeti(), "No bets on the table this round. A clean fight, then. How dull.");
                ClearBets();
                return;
            }

            uint64 winningPool = winningTeam == TheramoreSpar::TEAM_RED ? _redPool : _bluePool;
            uint64 losingPool = winningTeam == TheramoreSpar::TEAM_RED ? _bluePool : _redPool;

            if (!winningPool)
            {
                Say(GetSzigeti(), "No winning tickets! The house thanks you for your generous donations.");

                for (auto const& pair : _bets)
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, pair.first);
                    if (player)
                        Notify(player, "Your Theramore sparring bet lost. Nobody backed the winning side.");
                    else
                        SendSparMail(pair.first, 0,
                            "Theramore Sparring Wager Result",
                            "Your Theramore sparring wager lost while you were away from the ring. Nobody backed the winning side.");
                }

                ClearBets();
                return;
            }

            uint64 losingPoolAfterCut = losingPool;
            if (losingPoolAfterCut)
                losingPoolAfterCut = losingPoolAfterCut * (100 - TheramoreSpar::HOUSE_CUT_PERCENT) / 100;

            for (auto const& pair : _bets)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, pair.first);
                TheramoreSpar::BetInfo const& bet = pair.second;
                if (bet.Team != winningTeam)
                {
                    if (player)
                    {
                        std::string message = "Your Theramore sparring bet lost. Fortune has a wooden shield and poor manners.";
                        Notify(player, message);
                    }
                    else
                        SendSparMail(pair.first, 0,
                            "Theramore Sparring Wager Result",
                            "Your Theramore sparring wager lost while you were away from the ring.");

                    continue;
                }

                uint64 profit = 0;

                if (losingPoolAfterCut)
                    profit = bet.Stake * losingPoolAfterCut / winningPool;
                else
                    profit = bet.Stake * TheramoreSpar::SOLO_WIN_PROFIT_PERCENT / 100;

                uint64 payout = bet.Stake + profit;

                bool paidOnline = GiveMoneyOrMail(pair.first, player, payout,
                    "Theramore Sparring Wager Payout",
                    "Your Theramore sparring wager won while you were away from the ring.");

                std::string message = "Your Theramore sparring bet won! You receive ";
                message += TheramoreSpar::MoneyToString(payout);
                message += ".";

                if (paidOnline)
                    Notify(player, message);
            }

            Say(GetSzigeti(), "Winning tickets are paid! Losers may deposit their complaints in the harbor.");
            ClearBets();
        }

        void HandleMotivation()
        {
            Creature* redCaptain = GetCaptain(TheramoreSpar::TEAM_RED);
            Creature* blueCaptain = GetCaptain(TheramoreSpar::TEAM_BLUE);

            if (!redCaptain || !blueCaptain)
                return;

            switch (_winningTeam)
            {
            case TheramoreSpar::TEAM_RED:
                Say(redCaptain, "That is how red holds the field!");
                Say(blueCaptain, "Blue team, heads up. Defeat only matters if you learn nothing from it.");
                break;

            case TheramoreSpar::TEAM_BLUE:
                Say(blueCaptain, "That is how blue carries the day!");
                Say(redCaptain, "Red team, gather yourselves. Steel is tempered by the hammer.");
                break;

            default:
                break;
            }
        }

        void CombatantSeparate()
        {
            Creature* red = GetActive(TheramoreSpar::TEAM_RED);
            Creature* blue = GetActive(TheramoreSpar::TEAM_BLUE);

            if (!red || !blue)
            {
                Reset();
                return;
            }

            MoveToPoint(red, TheramoreSpar::RedStart, FORCED_MOVEMENT_RUN);
            MoveToPoint(blue, TheramoreSpar::BlueStart, FORCED_MOVEMENT_RUN);

            _events.ScheduleEvent(TheramoreSpar::EVENT_SWITCH_MEMBERS, 5s);
        }

        void SwitchMembers()
        {
            if (_winningTeam == TheramoreSpar::TEAM_NONE)
            {
                Reset();
                return;
            }

            uint8 losingTeam = OpposingTeam(_winningTeam);

            Creature* loser = GetActive(losingTeam);
            Creature* winner = GetActive(_winningTeam);

            if (loser)
                MoveHome(loser, FORCED_MOVEMENT_RUN);

            if (winner)
                MoveToPoint(winner, TeamStartPosition(_winningTeam), FORCED_MOVEMENT_WALK);

            if (!Bench(losingTeam).empty())
            {
                ActiveGuid(losingTeam) = Bench(losingTeam).front();
                Bench(losingTeam).pop_front();

                _winningTeam = TheramoreSpar::TEAM_NONE;
                _events.ScheduleEvent(TheramoreSpar::EVENT_OPEN_BETTING, 8s);
                return;
            }

            // Match over. Send the winner home too, rebuild both benches, and
            // open the betting window again after a breather.
            if (winner)
                MoveHome(winner, FORCED_MOVEMENT_RUN);

            if (!_boutFinished)
                FinishBout(_winningTeam);

            _activeRed.Clear();
            _activeBlue.Clear();
            _winningTeam = TheramoreSpar::TEAM_NONE;

            _events.ScheduleEvent(TheramoreSpar::EVENT_GET_COMBATANTS, 15s);
            _events.ScheduleEvent(TheramoreSpar::EVENT_OPEN_BETTING, 20s);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_theramore_spar_controllerAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return true;

        npc_theramore_spar_controllerAI* ai = CAST_AI(npc_theramore_spar_controllerAI, creature->AI());

        ClearGossipMenuFor(player);

        if (!ai || !ai->IsBettingOpen())
        {
            AddGossipItemFor(player, 0, "Bets are closed. The match is underway.", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_STATUS);
            SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
            return true;
        }

        AddGossipItemFor(player, 0, ai->GetBettingSummaryFor(player), GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_STATUS);

        AddGossipItemFor(player, 0, "Bet 1 gold on Red Team", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_BET_RED_1G);
        AddGossipItemFor(player, 0, "Bet 5 gold on Red Team", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_BET_RED_5G);
        AddGossipItemFor(player, 0, "Bet 10 gold on Red Team", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_BET_RED_10G);

        AddGossipItemFor(player, 0, "Bet 1 gold on Blue Team", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_BET_BLUE_1G);
        AddGossipItemFor(player, 0, "Bet 5 gold on Blue Team", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_BET_BLUE_5G);
        AddGossipItemFor(player, 0, "Bet 10 gold on Blue Team", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_BET_BLUE_10G);

        AddGossipItemFor(player, 0, "Cancel my bet", GOSSIP_SENDER_MAIN, TheramoreSpar::ACTION_CANCEL_BET);

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!player || !creature)
            return true;

        ClearGossipMenuFor(player);

        npc_theramore_spar_controllerAI* ai = CAST_AI(npc_theramore_spar_controllerAI, creature->AI());
        if (!ai)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        switch (action)
        {
        case TheramoreSpar::ACTION_STATUS:
            break;

        case TheramoreSpar::ACTION_BET_RED_1G:
            ai->PlaceBet(player, TheramoreSpar::TEAM_RED, 1 * TheramoreSpar::GOLD);
            break;
        case TheramoreSpar::ACTION_BET_RED_5G:
            ai->PlaceBet(player, TheramoreSpar::TEAM_RED, 5 * TheramoreSpar::GOLD);
            break;
        case TheramoreSpar::ACTION_BET_RED_10G:
            ai->PlaceBet(player, TheramoreSpar::TEAM_RED, 10 * TheramoreSpar::GOLD);
            break;

        case TheramoreSpar::ACTION_BET_BLUE_1G:
            ai->PlaceBet(player, TheramoreSpar::TEAM_BLUE, 1 * TheramoreSpar::GOLD);
            break;
        case TheramoreSpar::ACTION_BET_BLUE_5G:
            ai->PlaceBet(player, TheramoreSpar::TEAM_BLUE, 5 * TheramoreSpar::GOLD);
            break;
        case TheramoreSpar::ACTION_BET_BLUE_10G:
            ai->PlaceBet(player, TheramoreSpar::TEAM_BLUE, 10 * TheramoreSpar::GOLD);
            break;

        case TheramoreSpar::ACTION_CANCEL_BET:
            ai->CancelBet(player);
            break;

        default:
            break;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_dustwallow_marsh()
{
    RegisterSpellScript(spell_ooze_zap);
    RegisterSpellScript(spell_ooze_zap_channel_end);
    RegisterSpellScript(spell_energize_aoe);

    RegisterCreatureAI(npc_theramore_practicing_guard);
    new npc_theramore_spar_controller();
}
