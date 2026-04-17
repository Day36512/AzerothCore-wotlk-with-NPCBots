/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * Exarch Maladaar with NPCBot-aware targeting and an added random charge.
 *
 * Behavior kept:
 * - Intro yell on proximity
 * - Soul Scream
 * - Ribbon of Souls
 * - Stolen Soul + Stolen Soul summon copy
 * - Avatar summon at 25%
 *
 * Added:
 * - Random charge on a player or NPCBot encounter target
 * - Boss target selection helpers that include alive NPCBots on the same map
 * - Player-only mechanics updated to also consider NPCBots where appropriate
 *
 * Assumptions:
 * - SPELL_CHARGE uses generic Charge spell 22911. Replace if your pack prefers another charge spell.
 * - NPCBots expose valid class/display info for Stolen Soul via Unit APIs already used elsewhere.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Map.h"
#include "Containers.h"
#include "botmgr.h"
#include "bot_ai.h"
#include "auchenai_crypts.h"

#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;

enum Text
{
    SAY_INTRO = 0,
    SAY_SUMMON = 1,
    SAY_AGGRO = 2,
    SAY_ROAR = 3,
    SAY_SLAY = 4,
    SAY_DEATH = 5
};

enum Spells
{
    // Exarch Maladaar
    SPELL_RIBBON_OF_SOULS = 32422,
    SPELL_SOUL_SCREAM = 32421,
    SPELL_STOLEN_SOUL = 32346,
    SPELL_STOLEN_SOUL_VISUAL = 32395,
    SPELL_SUMMON_AVATAR = 32424,
    SPELL_CHARGE = 22911, // assumption: generic NPC charge

    // Stolen Soul
    SPELL_MOONFIRE = 37328,
    SPELL_FIREBALL = 37329,
    SPELL_MIND_FLAY = 37330,
    SPELL_HEMORRHAGE = 37331,
    SPELL_FROSTSHOCK = 37332,
    SPELL_CURSE_OF_AGONY = 37334,
    SPELL_MORTAL_STRIKE = 37335,
    SPELL_FREEZING_TRAP = 37368,
    SPELL_HAMMER_OF_JUSTICE = 37369,
    SPELL_PLAGUE_STRIKE = 58839
};

enum Npc
{
    ENTRY_STOLEN_SOUL = 18441
};

struct boss_exarch_maladaar : public BossAI
{
    boss_exarch_maladaar(Creature* creature) : BossAI(creature, DATA_EXARCH_MALADAAR)
    {
        _talked = false;

        //Dinkle custom
        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    //Dinkle custom start
    bool IsEncounterPlayer(Player const* player, bool aliveOnly = true) const
    {
        if (!player)
            return false;

        if (player->GetMap() != me->GetMap())
            return false;

        if (player->IsGameMaster())
            return false;

        if (aliveOnly && !player->IsAlive())
            return false;

        if (!me->IsWithinDistInMap(player, 150.0f))
            return false;

        return true;
    }

    bool IsEncounterBot(Creature const* bot, bool aliveOnly = true) const
    {
        if (!bot)
            return false;

        if (!bot->IsNPCBot())
            return false;

        if (bot->GetMap() != me->GetMap())
            return false;

        if (aliveOnly && !bot->IsAlive())
            return false;

        if (!me->IsWithinDistInMap(bot, 150.0f))
            return false;

        return true;
    }

    std::vector<Player*> GetEncounterPlayers(bool aliveOnly) const
    {
        std::vector<Player*> players;
        Map::PlayerList const& playerList = me->GetMap()->GetPlayers();

        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsEncounterPlayer(player, aliveOnly))
                continue;

            players.push_back(player);
        }

        return players;
    }

    std::vector<Creature*> GetEncounterBots(bool aliveOnly) const
    {
        std::vector<Creature*> bots;
        std::unordered_set<ObjectGuid::LowType> seen;

        Map::PlayerList const& playerList = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsEncounterPlayer(player, false))
                continue;

            for (Unit* member : BotMgr::GetAllGroupMembers(player))
            {
                if (!member || !member->IsInWorld())
                    continue;

                if (member->FindMap() != me->GetMap())
                    continue;

                Creature* bot = member->ToCreature();
                if (!bot || !IsEncounterBot(bot, aliveOnly))
                    continue;

                if (!seen.insert(bot->GetGUID().GetCounter()).second)
                    continue;

                bots.push_back(bot);
            }
        }

        return bots;
    }

    std::vector<Unit*> GetEncounterTargets(bool aliveOnly) const
    {
        std::vector<Unit*> targets;

        std::vector<Player*> players = GetEncounterPlayers(aliveOnly);
        targets.reserve(players.size() + 8);

        for (Player* player : players)
            targets.push_back(player);

        for (Creature* bot : GetEncounterBots(aliveOnly))
            targets.push_back(bot);

        return targets;
    }

    Unit* SelectRandomEncounterTarget(float maxRange, bool mustBeAlive = true, Unit const* exclude = nullptr, bool preferOutsideMelee = false)
    {
        std::vector<Unit*> candidates;
        candidates.reserve(16);

        for (Unit* unit : GetEncounterTargets(mustBeAlive))
        {
            if (!unit)
                continue;

            if (exclude && unit == exclude)
                continue;

            if (!me->IsWithinDistInMap(unit, maxRange))
                continue;

            if (preferOutsideMelee && me->IsWithinDistInMap(unit, 8.0f))
                continue;

            candidates.push_back(unit);
        }

        if (candidates.empty() && preferOutsideMelee)
        {
            for (Unit* unit : GetEncounterTargets(mustBeAlive))
            {
                if (!unit)
                    continue;

                if (exclude && unit == exclude)
                    continue;

                if (!me->IsWithinDistInMap(unit, maxRange))
                    continue;

                candidates.push_back(unit);
            }
        }

        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }
    //Dinkle custom end

    void Reset() override
    {
        _Reset();

        scheduler.CancelAll();

        ScheduleHealthCheckEvent(25, [&]
            {
                Talk(SAY_SUMMON);
                scheduler.Schedule(100ms, [this](TaskContext)
                    {
                        DoCastSelf(SPELL_SUMMON_AVATAR);
                    });
            });
    }

    void MoveInLineOfSight(Unit* who) override
    {
        //Dinkle custom
        if (!_talked && who && (who->IsPlayer() || who->IsNPCBot()) && me->IsWithinDistInMap(who, 150.0f))
        {
            _talked = true;
            Talk(SAY_INTRO);
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void JustEngagedWith(Unit*) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(15s, [this](TaskContext context)
            {
                DoCastSelf(SPELL_SOUL_SCREAM);
                context.Repeat(15s, 25s);
            }).Schedule(5s, [this](TaskContext context)
                {
                    //Dinkle custom
                    if (Unit* target = SelectRandomEncounterTarget(100.0f))
                        DoCast(target, SPELL_RIBBON_OF_SOULS);

                    context.Repeat(10s, 20s);
                }).Schedule(12s, [this](TaskContext context)
                    {
                        //Dinkle custom
                        Unit* currentVictim = me->GetVictim();
                        if (Unit* target = SelectRandomEncounterTarget(100.0f, true, currentVictim, true))
                            DoCast(target, SPELL_CHARGE);

                        context.Repeat(15s, 22s);
                    }).Schedule(25s, [this](TaskContext context)
                        {
                            //Dinkle custom
                            if (Unit* target = SelectRandomEncounterTarget(100.0f))
                            {
                                Talk(SAY_ROAR);
                                DoCast(target, SPELL_STOLEN_SOUL);

                                if (Creature* summon = me->SummonCreature(
                                    ENTRY_STOLEN_SOUL,
                                    0.0f, 0.0f, 0.0f, 0.0f,
                                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                                    10000))
                                {
                                    summon->CastSpell(summon, SPELL_STOLEN_SOUL_VISUAL, false);
                                    summon->SetDisplayId(target->GetDisplayId());
                                    summon->AI()->SetGUID(target->GetGUID());
                                    summon->AI()->DoAction(target->getClass());
                                    summon->AI()->AttackStart(target);
                                }
                            }

                            context.Repeat(25s, 30s);
                        });
    }

    void KilledUnit(Unit* victim) override
    {
        //Dinkle custom
        if (victim && (victim->IsPlayer() || victim->IsNPCBot()) && urand(0, 1))
            Talk(SAY_SLAY);
    }

    void JustDied(Unit*) override
    {
        Talk(SAY_DEATH);
        me->SummonCreature(19412, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 600000);
        _JustDied();
    }

    void JustSummoned(Creature* /*creature*/) override
    {
        // Override JustSummoned() so we don't despawn the Avatar.
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _talked;
};

struct npc_stolen_soul : public ScriptedAI
{
    npc_stolen_soul(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        _myClass = CLASS_WARRIOR;
        _scheduler.CancelAll();

        _scheduler.Schedule(1s, [this](TaskContext /*context*/)
            {
                switch (_myClass)
                {
                case CLASS_WARRIOR:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_MORTAL_STRIKE);
                            context.Repeat(6s);
                        });
                    break;
                case CLASS_PALADIN:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_HAMMER_OF_JUSTICE);
                            context.Repeat(6s);
                        });
                    break;
                case CLASS_HUNTER:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_FREEZING_TRAP);
                            context.Repeat(20s);
                        });
                    break;
                case CLASS_ROGUE:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_HEMORRHAGE);
                            context.Repeat(10s);
                        });
                    break;
                case CLASS_PRIEST:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_MIND_FLAY);
                            context.Repeat(5s);
                        });
                    break;
                case CLASS_SHAMAN:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_FROSTSHOCK);
                            context.Repeat(8s);
                        });
                    break;
                case CLASS_MAGE:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_FIREBALL);
                            context.Repeat(5s);
                        });
                    break;
                case CLASS_WARLOCK:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_CURSE_OF_AGONY);
                            context.Repeat(20s);
                        });
                    break;
                case CLASS_DRUID:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_MOONFIRE);
                            context.Repeat(10s);
                        });
                    break;
                case CLASS_DEATH_KNIGHT:
                    _scheduler.Schedule(0ms, [this](TaskContext context)
                        {
                            DoCastVictim(SPELL_PLAGUE_STRIKE);
                            context.Repeat(6s);
                        });
                    break;
                default:
                    break;
                }
            });
    }

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        _targetGuid = guid;
    }

    void DoAction(int32 pClass) override
    {
        _myClass = pClass;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);
        DoMeleeAttackIfReady();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Unit* target = ObjectAccessor::GetUnit(*me, _targetGuid))
            target->RemoveAurasDueToSpell(SPELL_STOLEN_SOUL);
    }

private:
    TaskScheduler _scheduler;
    ObjectGuid _targetGuid;
    uint8 _myClass = CLASS_WARRIOR;
};

void AddSC_boss_exarch_maladaar()
{
    RegisterAuchenaiCryptsCreatureAI(boss_exarch_maladaar);
    RegisterAuchenaiCryptsCreatureAI(npc_stolen_soul);
}
