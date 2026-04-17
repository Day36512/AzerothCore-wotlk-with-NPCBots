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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameObjectScript.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Config.h"
#include "blackwing_lair.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"

 //Dinkle custom
#include "bot_ai.h"

#include <limits>
#include <list>
#include <vector>

enum Emotes
{
    EMOTE_FRENZY = 0,
    EMOTE_SHIMMER = 1,
};

enum Spells
{
    // Other spells
    SPELL_INCINERATE = 23308, // Incinerate 23308, 23309
    SPELL_TIMELAPSE = 23310, // Time lapse 23310, 23311 (old threat mod removed in 2.0.1)
    SPELL_CORROSIVEACID = 23313, // Corrosive Acid 23313, 23314
    SPELL_IGNITEFLESH = 23315, // Ignite Flesh 23315, 23316
    SPELL_FROSTBURN = 23187, // Frost burn 23187, 23189

    // Brood Affliction
    SPELL_BROODAF_BLUE = 23153,
    SPELL_BROODAF_BLACK = 23154,
    SPELL_BROODAF_RED = 23155, // 23168 on death
    SPELL_BROODAF_BRONZE = 23170,
    SPELL_BROODAF_GREEN = 23169,
    SPELL_CHROMATIC_MUT_1 = 23174,

    SPELL_ELEMENTAL_SHIELD = 22276,
    SPELL_FRENZY = 23128,
    SPELL_ENRAGE = 23537
};

enum Events
{
    EVENT_SHIMMER = 1,
    EVENT_BREATH = 2,
    EVENT_AFFLICTION = 3,
    EVENT_FRENZY = 4
};

enum Misc
{
    GUID_LEVER_USER = 0
};

Position const homePos = { -7491.1587f, -1069.718f, 476.59094f, 476.59094f };

struct boss_chromaggus : public BossAI
{
    boss_chromaggus(Creature* creature) : BossAI(creature, DATA_CHROMAGGUS)
    {
        Initialize();

        // Select the 2 breaths that we are going to use until despawned so we don't end up casting 2 of the same breath.
        _breathSpells = { SPELL_INCINERATE, SPELL_TIMELAPSE, SPELL_CORROSIVEACID, SPELL_IGNITEFLESH, SPELL_FROSTBURN };
        Acore::Containers::RandomResize(_breathSpells, 2);

        // Hack fix: This is here to prevent him from being pulled from the floor underneath, remove it once maps are fixed.
        creature->SetImmuneToAll(true);
    }

    //Dinkle custom start
    static float GetBotTankThreatBonus()
    {
        static float sBonus = []() -> float
            {
                float v = sConfigMgr->GetOption<float>("NPCBots.BossEngage.BotTankThreatBonus", 120000.0f);
                if (v < 0.0f)
                    v = 0.0f;
                return v;
            }();
        return sBonus;
    }
    //Dinkle custom end

    void Initialize()
    {
        Enraged = false;
    }

    void Reset() override
    {
        _Reset();
        Initialize();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        events.ScheduleEvent(EVENT_SHIMMER, 1s);
        events.ScheduleEvent(EVENT_BREATH, 30s);
        events.ScheduleEvent(EVENT_BREATH, 60s);
        events.ScheduleEvent(EVENT_AFFLICTION, 10s);
        events.ScheduleEvent(EVENT_FRENZY, 15s);

        //Dinkle custom
        BoostThreatToBotTanksOnEngage(100.0f);
    }

    bool CanAIAttack(Unit const* victim) const override
    {
        return !victim->HasAura(SPELL_TIMELAPSE);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == GUID_LEVER_USER)
        {
            _playerGUID = guid;

            // Hack fix: allow pull now
            me->SetImmuneToAll(false);
        }
    }

    void PathEndReached(uint32 /*pathId*/) override
    {
        if (Unit* player = ObjectAccessor::GetUnit(*me, _playerGUID))
            me->SetInCombatWith(player);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_SHIMMER:
            {
                DoCast(me, SPELL_ELEMENTAL_SHIELD);
                Talk(EMOTE_SHIMMER);
                events.ScheduleEvent(EVENT_SHIMMER, 17s, 25s);
                break;
            }
            case EVENT_BREATH:
            {
                DoCastVictim(_breathSpells.front());
                _breathSpells.reverse();
                events.ScheduleEvent(EVENT_BREATH, 60s);
                break;
            }
            case EVENT_AFFLICTION:
            {
                uint32 afflictionSpellID = RAND(SPELL_BROODAF_BLUE, SPELL_BROODAF_BLACK, SPELL_BROODAF_RED, SPELL_BROODAF_BRONZE, SPELL_BROODAF_GREEN);

                std::vector<Player*> playerTargets;
                Map::PlayerList const& players = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                {
                    if (Player* player = itr->GetSource()->ToPlayer())
                    {
                        if (!player->IsGameMaster() && !player->IsSpectator() && player->IsAlive())
                            playerTargets.push_back(player);
                    }
                }

                if (playerTargets.size() > 12)
                    Acore::Containers::RandomResize(playerTargets, 12);

                for (Player* player : playerTargets)
                {
                    DoCast(player, afflictionSpellID, true);

                    if (player->HasAllAuras(SPELL_BROODAF_BLUE, SPELL_BROODAF_BLACK, SPELL_BROODAF_RED, SPELL_BROODAF_BRONZE, SPELL_BROODAF_GREEN))
                        DoCast(player, SPELL_CHROMATIC_MUT_1);
                }

                events.ScheduleEvent(EVENT_AFFLICTION, 10s);
                break;
            }
            case EVENT_FRENZY:
            {
                DoCast(me, SPELL_FRENZY);
                events.ScheduleEvent(EVENT_FRENZY, 10s, 15s);
                break;
            }
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

        if (!Enraged && HealthBelowPct(20))
        {
            DoCast(me, SPELL_ENRAGE);
            Enraged = true;
        }

        DoMeleeAttackIfReady();
    }

private:
    std::list<uint32> _breathSpells;
    bool Enraged;
    ObjectGuid _playerGUID;

    //Dinkle custom start
    void CollectNearbyNPCBotTanks(float range, std::vector<Creature*>& out)
    {
        out.clear();

        std::list<Unit*> units;
        Acore::AnyUnitInObjectRangeCheck check(me, range);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, units, check);
        Cell::VisitObjects(me, searcher, range);

        for (Unit* u : units)
        {
            if (!u || u->GetTypeId() != TYPEID_UNIT)
                continue;

            Creature* c = u->ToCreature();
            if (!c || !c->IsAlive())
                continue;

            if (!c->IsNPCBot() || !me->IsHostileTo(c))
                continue;

            if (bot_ai* bai = c->GetBotAI())
            {
                uint32 const roles = bai->GetBotRoles();
                bool const isTankRole = (roles & (BOT_ROLE_TANK | BOT_ROLE_TANK_OFF)) != 0;
                if (bai->IsTank() || isTankRole)
                    out.push_back(c);
            }
        }
    }

    void BoostThreatToBotTanksOnEngage(float searchRange)
    {
        float const bonus = GetBotTankThreatBonus();
        if (bonus <= 0.0f)
            return;

        std::vector<Creature*> tanks;
        tanks.reserve(16);
        CollectNearbyNPCBotTanks(searchRange, tanks);

        if (tanks.empty())
            return;

        Creature* primary = nullptr;
        float bestDist = std::numeric_limits<float>::max();

        for (Creature* c : tanks)
        {
            float d = me->GetDistance(c);
            if (d < bestDist)
            {
                bestDist = d;
                primary = c;
            }
        }

        for (Creature* c : tanks)
        {
            float amount = (c == primary) ? bonus * 1.05f : bonus;
            me->GetThreatMgr().AddThreat(c, amount);
        }

        if (!me->GetVictim() && primary)
            AttackStart(primary);
    }
    //Dinkle custom end
};

struct go_chromaggus_lever : public GameObjectAI
{
    go_chromaggus_lever(GameObject* go) : GameObjectAI(go), _instance(go->GetInstanceScript()) {}

    bool GossipHello(Player* player, bool reportUse) override
    {
        if (reportUse && _instance)
        {
            if (_instance->GetBossState(DATA_CHROMAGGUS) != DONE &&
                _instance->GetBossState(DATA_CHROMAGGUS) != IN_PROGRESS)
            {
                if (Creature* creature = _instance->GetCreature(DATA_CHROMAGGUS))
                {
                    creature->SetHomePosition(homePos);
                    creature->GetMotionMaster()->MoveWaypoint(creature->GetEntry() * 10, false);
                    creature->AI()->SetGUID(player->GetGUID(), GUID_LEVER_USER);
                }

                if (GameObject* go = _instance->GetGameObject(DATA_GO_CHROMAGGUS_DOOR))
                    _instance->HandleGameObject(ObjectGuid::Empty, true, go);
            }

            me->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE | GO_FLAG_IN_USE);
            me->SetGoState(GO_STATE_ACTIVE);
        }

        return true;
    }

private:
    InstanceScript* _instance;
};

enum ElementalShieldSpells
{
    SPELL_FIRE_ELEMENTAL_SHIELD = 22277,
    SPELL_FROST_ELEMENTAL_SHIELD = 22278,
    SPELL_SHADOW_ELEMENTAL_SHIELD = 22279,
    SPELL_NATURE_ELEMENTAL_SHIELD = 22280,
    SPELL_ARCANE_ELEMENTAL_SHIELD = 22281,

    SPELL_RED_BROOD_POWER = 22283,
    SPELL_BLUE_BROOD_POWER = 22285,
    SPELL_BRONZE_BROOD_POWER = 22286,
    SPELL_BLACK_BROOD_POWER = 22287,
    SPELL_GREEN_BROOD_POWER = 22288
};

class spell_gen_elemental_shield : public SpellScript
{
    PrepareSpellScript(spell_gen_elemental_shield);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_FIRE_ELEMENTAL_SHIELD,
                SPELL_FROST_ELEMENTAL_SHIELD,
                SPELL_SHADOW_ELEMENTAL_SHIELD,
                SPELL_NATURE_ELEMENTAL_SHIELD,
                SPELL_ARCANE_ELEMENTAL_SHIELD
            });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            for (uint32 spell = SPELL_FIRE_ELEMENTAL_SHIELD; spell <= SPELL_ARCANE_ELEMENTAL_SHIELD; ++spell)
                caster->RemoveAurasDueToSpell(spell);

            caster->CastSpell(caster, SPELL_FIRE_ELEMENTAL_SHIELD + urand(0, 4), true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_gen_elemental_shield::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_gen_brood_power : public SpellScript
{
    PrepareSpellScript(spell_gen_brood_power);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_RED_BROOD_POWER,
                SPELL_BLUE_BROOD_POWER,
                SPELL_BRONZE_BROOD_POWER,
                SPELL_BLACK_BROOD_POWER,
                SPELL_GREEN_BROOD_POWER
            });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            for (uint32 spell = SPELL_RED_BROOD_POWER; spell <= SPELL_GREEN_BROOD_POWER; ++spell)
                caster->RemoveAurasDueToSpell(spell);

            caster->CastSpell(caster, RAND(SPELL_RED_BROOD_POWER, SPELL_BLUE_BROOD_POWER, SPELL_BRONZE_BROOD_POWER, SPELL_BLACK_BROOD_POWER, SPELL_GREEN_BROOD_POWER), true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_gen_brood_power::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

//Dinkle custom start
// Spells: Frost Burn (23187 rank 1, 23189 rank 2)
class chromaggus_spell_frostburn_custom : public SpellScript
{
    PrepareSpellScript(chromaggus_spell_frostburn_custom);

    static float GetMultiplier()
    {
        static float sMult = []() -> float
            {
                float v = sConfigMgr->GetOption<float>("Chromaggus.Frostburn.DamageMultiplier", 1.0f);
                if (v < 0.0f)
                    v = 0.0f;
                return v;
            }();
        return sMult;
    }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ 23187u, 23189u });
    }

    void HandleHit()
    {
        float mult = GetMultiplier();
        if (mult == 1.0f)
            return;

        int32 dmg = GetHitDamage();
        if (dmg <= 0)
            return;

        double scaled = static_cast<double>(dmg) * static_cast<double>(mult);
        if (scaled < 0.0)
            scaled = 0.0;

        if (scaled > static_cast<double>(std::numeric_limits<int32>::max()))
            SetHitDamage(std::numeric_limits<int32>::max());
        else
            SetHitDamage(static_cast<int32>(scaled + 0.5));
    }

    void Register() override
    {
        OnHit += SpellHitFn(chromaggus_spell_frostburn_custom::HandleHit);
    }
};
//Dinkle custom end

void AddSC_boss_chromaggus()
{
    RegisterBlackwingLairCreatureAI(boss_chromaggus);
    RegisterBlackwingLairGameObjectAI(go_chromaggus_lever);
    RegisterSpellScript(spell_gen_elemental_shield);
    RegisterSpellScript(spell_gen_brood_power);
    //Dinkle custom
    RegisterSpellScript(chromaggus_spell_frostburn_custom);
}
