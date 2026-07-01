#include "bot_ai.h"
#include "botdatamgr.h"
#include "botlogtraits.h"
#include "botmgr.h"
#include "bottext.h"
#include "bottraits.h"
#include "Creature.h"
#include "Group.h"
#include "Item.h"
#include "Map.h"
#include "Player.h"
#include "RaceMgr.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
//#include "WorldSession.h"

#include <array>
/*
Paladin NpcBot (reworked by Trickerer onlysuffering@gmail.com)
Complete - Around 95%
TODO:
*/

enum PaladinBaseSpells// all orignals
{
    FLASH_OF_LIGHT_1 = 19750,
    HOLY_LIGHT_1 = 635,
    LAY_ON_HANDS_1 = 633,
    REDEMPTION_1 = 7328,
    HAND_OF_FREEDOM_1 = 1044,
    SACRED_SHIELD_1 = 300366, // New Rank 1
    HOLY_SHOCK_1 = 20473,
    CLEANSE_1 = 4987,
    HAND_OF_PROTECTION_1 = 1022,
    HAND_OF_SALVATION_1 = 1038,
    HAND_OF_SACRIFICE_1 = 6940,
    SEAL_OF_VENGEANCE_1 = 31801,
    SEAL_OF_CORRUPTION_1 = 53736,
    SEAL_OF_COMMAND_1 = 20375,
    SEAL_OF_LIGHT_1 = 20165,
    SEAL_OF_RIGHTEOUSNESS_1 = 21084,
    SEAL_OF_WISDOM_1 = 20166,
    SEAL_OF_JUSTICE_1 = 20164,
    DIVINE_SACRIFICE_1 = 64205,
    HAND_OF_RECKONING_1 = 62124,
    RIGHTEOUS_DEFENSE_1 = 31789,
    DIVINE_PLEA_1 = 54428,
    REPENTANCE_1 = 20066,
    TURN_EVIL_1 = 10326,
    CRUSADER_STRIKE_1 = 35395,
    JUDGEMENT_OF_LIGHT_1 = 20271,
    JUDGEMENT_OF_WISDOM_1 = 53408,
    JUDGEMENT_OF_JUSTICE_1 = 53407,
    CONSECRATION_1 = 26573,
    HAMMER_OF_JUSTICE_1 = 853,
    DIVINE_STORM_1 = 53385,
    HAMMER_OF_WRATH_1 = 24275,
    EXORCISM_1 = 879,
    HOLY_WRATH_1 = 2812,
    AVENGING_WRATH_1 = 31884,
    RIGHTEOUS_FURY_1 = 25780,
    HOLY_SHIELD_1 = 20925,
    AVENGERS_SHIELD_1 = 31935,
    HAMMER_OF_THE_RIGHTEOUS_1 = 53595,
    SHIELD_OF_RIGHTEOUSNESS_1 = 300365, //New Rank 1
    BLESSING_OF_MIGHT_1 = 19740,
    BLESSING_OF_WISDOM_1 = 19742,
    BLESSING_OF_KINGS_1 = 20217,
    BLESSING_OF_SANCTUARY_1 = 20911,
    DEVOTION_AURA_1 = 465,
    CONCENTRATION_AURA_1 = 19746,
    FIRE_RESISTANCE_AURA_1 = 19891,
    FROST_RESISTANCE_AURA_1 = 19888,
    SHADOW_RESISTANCE_AURA_1 = 19876,
    RETRIBUTION_AURA_1 = 7294,
    CRUSADER_AURA_1 = 32223,

    DIVINE_INTERVENTION_1 = 19752,
    AURA_MASTERY_1 = 31821,
    DIVINE_FAVOR_1 = 20216,
    DIVINE_ILLUMINATION_1 = 31842,
    BEACON_OF_LIGHT_1 = 53563,

    DIVINE_PROTECTION_1 = 498,
    DIVINE_SHIELD_1 = 642,

    PURIFY_1 = 1152
};
enum PaladinPassives
{
    //Talents
    DIVINE_PURPOSE = 31872,
    JUDGEMENTS_OF_THE_PURE = 54155,
    JUDGEMENTS_OF_THE_WISE = 31878,
    SACRED_CLEANSING = 53553,//rank 3
    RECKONING1 = 20177,
    RECKONING2 = 20179,
    RECKONING3 = 20181,
    RECKONING4 = 20180,
    RECKONING5 = 20182,
    VINDICATION1 = 9452,
    VINDICATION2 = 26016,
    PURSUIT_OF_JUSTICE = 26023,//rank 2
    ART_OF_WAR = 53488,//rank 2
    IMPROVED_LAY_ON_HANDS = 20235,//rank 2
    FANATICISM = 31881,//rank 3
    RIGHTEOUS_VENGEANCE1 = 53380,//rank 1
    RIGHTEOUS_VENGEANCE2 = 53381,//rank 2
    RIGHTEOUS_VENGEANCE3 = 53382,//rank 3
    VENGEANCE1 = 20049,//rank 1
    VENGEANCE2 = 20056,//rank 2
    VENGEANCE3 = 20057,//rank 3
    SHEATH_OF_LIGHT1 = 53501,//rank 1
    SHEATH_OF_LIGHT2 = 53502,//rank 2
    SHEATH_OF_LIGHT3 = 53503,//rank 3
    ARDENT_DEFENDER = 31852,//rank 3
    ILLUMINATION = 20215,//rank 5
    INFUSION_OF_LIGHT = 53576,//rank 2
    REDOUBT1 = 20127,//rank 3
    REDOUBT2 = 20130,//rank 3
    REDOUBT3 = 20135,//rank 3
    IMPROVED_RIGHTEOUS_FURY = 20470,//rank 3
    SHIELD_OF_THE_TEMPLAR = 53711,//rank 3
    IMPROVED_DEVOTION_AURA = 20140,//rank 3
    IMPROVED_CONCENTRATION_AURA = 20256,//rank 3
    SANCTIFIED_RETRIBUTION = 31869,
    SWIFT_RETRIBUTION = 53648,//rank 3
    LIGHTS_GRACE = 31836,//rank 3
    DIVINE_GUARDIAN = 53530,//rank 3
    //COMBAT_EXPERTISE                    = 31860,//rank 3
    CRUSADE = 31868,//rank 3
    ONE_HANDED_WEAPON_SPECIALIZATION = 20198,//rank 3
    TWO_HANDED_WEAPON_SPECIALIZATION = 20113,//rank 3
    //JUDGEMENTS_OF_THE_JUST              = 53696,//rank 2
    GUARDED_BY_THE_LIGHT = 53585,//rank 2
    TOUCHED_BY_THE_LIGHT = 53592,//rank 3
    HEART_OF_THE_CRUSADER = 20337,//rank 3
    //Glyphs
    GLYPH_HOLY_LIGHT = 54937,
    GLYPH_SALVATION = 63225,
    //Innate
    JUDGEMENT_ANTI_PARRY_DODGE_PASSIVE = 60091,
    //other
    RECUCED_HOLY_LIGHT_CAST_TIME = 37189,//not a typo
    //CLEANSE_HEAL_PASSIVE                = 28787
};

enum PaladinSpecial
{
    SPECIFIC_BLESSING_WISDOM = 0x01,
    SPECIFIC_BLESSING_KINGS = 0x02,
    SPECIFIC_BLESSING_SANCTUARY = 0x04,
    SPECIFIC_BLESSING_MIGHT = 0x08,
    SPECIFIC_BLESSING_MY_BLESSING = 0x10,

    SPECIFIC_AURA_DEVOTION = 0x01,
    SPECIFIC_AURA_CONCENTRATION = 0x02,
    SPECIFIC_AURA_FIRE_RES = 0x04,
    SPECIFIC_AURA_FROST_RES = 0x08,
    SPECIFIC_AURA_SHADOW_RES = 0x10,
    SPECIFIC_AURA_RETRIBUTION = 0x20,
    SPECIFIC_AURA_CRUSADER = 0x40,
    SPECIFIC_AURA_MY_AURA = 0x80,
    SPECIFIC_AURA_ALL_AUTOUSE = (SPECIFIC_AURA_DEVOTION | SPECIFIC_AURA_CONCENTRATION | SPECIFIC_AURA_RETRIBUTION | \
        SPECIFIC_AURA_FIRE_RES | SPECIFIC_AURA_FROST_RES | SPECIFIC_AURA_SHADOW_RES),

    FLASH_OF_LIGHT_HEAL_PERIODIC = 66922,

    ENLIGHTENMENT_BUFF = 43837,
    INFUSION_OF_LIGHT_BUFF = 54149,//rank 2
    THE_ART_OF_WAR_BUFF = 59578,//rank 2
    //FORBEARANCE_AURA                    = 25771,

    LIGHTS_GRACE_BUFF = 31834,

    SEAL_OF_JUSTICE_STUN_AURA = 20170,
    JUDGEMENTS_OF_THE_JUST_AURA = 68055, //melee attack speed reduce

    //JUDGEMENT_OF_LIGHT_AURA             = 20185,
    JUDGEMENT_OF_WISDOM_AURA = 20186,
    //JUDGEMENT_OF_JUSTICE_AURA           = 20184,

    GREATER_BLESSING_OF_MIGHT_1 = 25782,
    GREATER_BLESSING_OF_WISDOM_1 = 25894,
    GREATER_BLESSING_OF_KINGS_1 = 25898,
    GREATER_BLESSING_OF_SANCTUARY_1 = 25899,
    BATTLESHOUT_1 = 6673,

    HOLY_SHOCK_HEAL_1 = 25914,
    ARDENT_DEFENDER_HEAL = 66235,
    JUDGEMENT_OF_COMMAND_DAMAGE = 20467,
    SPIRITUAL_ATTUNEMENT_ENERGIZE = 31786,
    SACRED_SHIELD_AURA_R1 = 300368, // lvl 58+
    SACRED_SHIELD_AURA_R2 = 300369, // lvl 68+
    SACRED_SHIELD_AURA_R3 = 58597,  // lvl 80
    AVENGING_WRATH_MARKER_SPELL = 61987,
    IMMUNITY_SHIELD_MARKER_SPELL = 61988,

    IMPROVED_DEVOTION_AURA_SPELL = 63514
};

static const std::vector<uint32> Paladin_spells_damage
{ AVENGERS_SHIELD_1, CONSECRATION_1, CRUSADER_STRIKE_1, DIVINE_STORM_1, EXORCISM_1, JUDGEMENT_OF_LIGHT_1,
JUDGEMENT_OF_WISDOM_1, JUDGEMENT_OF_JUSTICE_1, HAMMER_OF_THE_RIGHTEOUS_1, HAMMER_OF_WRATH_1, HOLY_SHIELD_1,
HOLY_SHOCK_1, HOLY_WRATH_1, SHIELD_OF_RIGHTEOUSNESS_1, HAND_OF_RECKONING_1 };
static const std::vector<uint32> Paladin_spells_cc{ HAMMER_OF_JUSTICE_1, HOLY_WRATH_1, REPENTANCE_1, TURN_EVIL_1 };
static const std::vector<uint32> Paladin_spells_heal{ BEACON_OF_LIGHT_1, FLASH_OF_LIGHT_1, HOLY_LIGHT_1, HOLY_SHOCK_1, LAY_ON_HANDS_1 };
static const std::vector<uint32> Paladin_spells_support
{ /*DEVOTION_AURA_1, CONCENTRATION_AURA_1, FIRE_RESISTANCE_AURA_1, FROST_RESISTANCE_AURA_1, SHADOW_RESISTANCE_AURA_1,
RETRIBUTION_AURA_1, CRUSADER_AURA_1, */AURA_MASTERY_1, AVENGING_WRATH_1, BLESSING_OF_MIGHT_1, BLESSING_OF_WISDOM_1,
BLESSING_OF_KINGS_1, BLESSING_OF_SANCTUARY_1, CLEANSE_1, DIVINE_FAVOR_1, DIVINE_ILLUMINATION_1, DIVINE_INTERVENTION_1,
DIVINE_PLEA_1, DIVINE_PROTECTION_1, DIVINE_SACRIFICE_1, DIVINE_SHIELD_1, HAND_OF_FREEDOM_1, HAND_OF_PROTECTION_1,
HAND_OF_RECKONING_1, HAND_OF_SACRIFICE_1, HAND_OF_SALVATION_1, HOLY_SHIELD_1, PURIFY_1, REDEMPTION_1,
RIGHTEOUS_DEFENSE_1, RIGHTEOUS_FURY_1, SACRED_SHIELD_1, SEAL_OF_RIGHTEOUSNESS_1, SEAL_OF_JUSTICE_1, SEAL_OF_LIGHT_1,
SEAL_OF_WISDOM_1, SEAL_OF_COMMAND_1, SEAL_OF_VENGEANCE_1, SEAL_OF_CORRUPTION_1 };

class paladin_bot : public CreatureScript
{
public:
    paladin_bot() : CreatureScript("paladin_bot") {}

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new paladin_botAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        return creature->GetBotAI()->OnGossipHello(player, 0);
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (bot_ai* ai = creature->GetBotAI())
            return ai->OnGossipSelect(player, creature, sender, action);
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, char const* code) override
    {
        if (bot_ai* ai = creature->GetBotAI())
            return ai->OnGossipSelectCode(player, creature, sender, action, code);
        return true;
    }

    struct paladin_botAI : public bot_ai
    {
        paladin_botAI(Creature* creature) : bot_ai(creature)
        {
            _botclass = BOT_CLASS_PALADIN;

            _myaura = 0;

            InitUnitFlags();
        }

        bool doCast(Unit* victim, uint32 spellId)
        {
            if (CheckBotCast(victim, spellId) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId);
        }

        void CheckBeacon(uint32 diff)
        {
            if (checkBeaconTimer > diff || !IsSpellReady(BEACON_OF_LIGHT_1, diff) ||
                !HasRole(BOT_ROLE_HEAL | BOT_ROLE_RANGED) || IsCasting() || Rand() > 15)
                return;

            checkBeaconTimer = urand(2000, 5000);

            if (FindAffectedTarget(GetSpell(BEACON_OF_LIGHT_1), me->GetGUID(), 60, 3))
                return;

            //find tank
            //stacks
            if (Group const* gr = !IAmFree() ? master->GetGroup() : GetGroup())
            {
                std::vector<Unit*> members = BotMgr::GetAllGroupMembers(gr);

                // Safety: FindAffectedTarget() can miss custom/triggered aura layouts.
                // If this paladin already has Beacon anywhere sensible in the group,
                // do not randomly move it to another tank.
                for (Unit const* member : members)
                {
                    if (member && member->IsInWorld() && me->GetMap() == member->FindMap() && member->IsAlive() &&
                        HasMyBeaconOfLightAura(member))
                        return;
                }

                std::set<Unit*> tanks;
                for (Unit* member : members)
                {
                    if (me->GetMap() == member->FindMap() && member->IsAlive() && member->IsInCombat() && IsTank(member) &&
                        (!member->getAttackers().empty() || GetHealthPCT(member) < 90) &&
                        !HasMyBeaconOfLightAura(member))
                        tanks.insert(member);
                }

                if (tanks.empty())
                    return;

                Unit* target = tanks.size() == 1 ? *tanks.begin() : Bcore::Containers::SelectRandomContainerElement(tanks);
                if (doCast(target, GetSpell(BEACON_OF_LIGHT_1)))
                    return;
            }
        }

        void CheckSacrifice(uint32 diff)
        {
            if (!IsSpellReady(DIVINE_SACRIFICE_1, diff) || IAmFree() || me->IsMounted() ||
                IsTank() || Feasting() || !CanBlock() || IsCasting() || Rand() > 25 || GetHealthPCT(me) < 60)
                return;

            Group const* gr = master->GetGroup();
            if (!gr)
            {
                if (master->IsAlive() && GetHealthPCT(master) < 75 && me->GetDistance(master) < 30 && !master->getAttackers().empty() &&
                    !master->GetAuraEffect(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELLFAMILY_PALADIN, 3837, EFFECT_0))
                {
                    if (doCast(me, GetSpell(DIVINE_SACRIFICE_1)))
                        return;
                }
            }
            else
            {
                uint8 attacked = 0;
                for (Unit const* member : BotMgr::GetAllGroupMembers(gr))
                {
                    if (me->GetMap() == member->FindMap() && member->IsAlive() &&
                        !(member->IsNPCBot() && member->ToCreature()->IsTempBot()) &&
                        me->GetDistance(member) < 30 && !member->getAttackers().empty() &&
                        !member->GetAuraEffect(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELLFAMILY_PALADIN, 3837, EFFECT_0))
                    {
                        if (++attacked > 3)
                            break;
                    }
                }
                if (attacked > 3 && doCast(me, GetSpell(DIVINE_SACRIFICE_1)))
                    return;
            }

            SetSpellCooldown(DIVINE_SACRIFICE_1, 1000); //fail
        }

        void CheckHandOfSacrifice(uint32 diff)
        {
            if (!IsSpellReady(HAND_OF_SACRIFICE_1, diff) || IAmFree() || me->IsMounted() ||
                IsTank() || Feasting() || !CanBlock() || IsCasting() || Rand() > 25 || GetHealthPCT(me) < 50)
                return;

            Group const* gr = master->GetGroup();
            if (!gr)
            {
                if (master->IsAlive() && me->GetDistance(master) < 30 && !master->getAttackers().empty() &&
                    (master->getAttackers().size() > 2 || GetHealthPCT(master) < 50) &&
                    !master->GetAuraEffect(SPELL_AURA_SPLIT_DAMAGE_PCT, SPELLFAMILY_PALADIN, 0x2000, 0x0, 0x0))
                {
                    if (doCast(master, GetSpell(HAND_OF_SACRIFICE_1)))
                        return;
                }
            }
            else
            {
                Unit* u = nullptr;
                for (Unit* member : BotMgr::GetAllGroupMembers(gr))
                {
                    if (me->GetMap() == member->FindMap() && member->IsAlive() && me->GetDistance(member) < 30 &&
                        !(member->IsNPCBot() && member->ToCreature()->IsTempBot()) &&
                        (member->getAttackers().size() > 2 || GetHealthPCT(member) < 50) &&
                        !member->GetAuraEffect(SPELL_AURA_SPLIT_DAMAGE_PCT, SPELLFAMILY_PALADIN, 0x2000, 0x0, 0x0))
                    {
                        u = member;
                        break;
                    }
                }

                if (u && doCast(u, GetSpell(HAND_OF_SACRIFICE_1)))
                    return;
            }

            SetSpellCooldown(HAND_OF_SACRIFICE_1, 2000); //fail
        }

        void ShieldGroup(uint32 diff)
        {
            if (checkShieldTimer > diff || !IsSpellReady(SACRED_SHIELD_1, diff) || me->IsMounted() || Feasting() || IsCasting() || Rand() > 50)
                return;

            // Grouped Holy healers use MaintainHolyTankSupport() for Sacred Shield so the
            // generic shield picker does not fight the tank-maintenance routine.
            if (!IAmFree() && GetSpec() == BOT_SPEC_PALADIN_HOLY && HasRole(BOT_ROLE_HEAL))
                return;

            checkShieldTimer = 3000;

            if (IsTank())
            {
                if (Rand() > 25)
                    return;
            }
            else if (!HasRole(BOT_ROLE_HEAL) && Rand() > 35)
                return;

            if (IAmFree() && (me->IsInCombat() || !me->getAttackers().empty()) && HasMySacredShieldLikeAura(me))
                return;

            if (Unit const* shielded = FindAffectedTarget(GetSpell(SACRED_SHIELD_1), me->GetGUID(), 80, 3))
                if (shielded->IsInCombat() && !shielded->getAttackers().empty())
                    return;

            Group const* gr = !IAmFree() ? master->GetGroup() : GetGroup();
            Unit* target = nullptr;
            if (!gr)
            {
                Unit* u = master;
                if (u->IsAlive() && u->IsInCombat() && (IAmFree() || IsTank(u)) && me->GetDistance(u) < 40 &&
                    !HasSacredShieldLikeAura(u))
                    target = u;

                if (!target && IsWanderer())
                {
                    std::list<Unit*> targets;
                    GetNearbyFriendlyTargetsList(targets, 40.0f);
                    std::erase_if(targets, [this](Unit const* unit) {
                        return (!unit->IsInCombat() && unit->getAttackers().empty()) || HasSacredShieldLikeAura(unit);
                        });
                    if (!targets.empty())
                        target = targets.size() == 1 ? targets.front() : Bcore::Containers::SelectRandomContainerElement(targets);
                }

                if (!target && !IAmFree())
                {
                    if (IsTank() && me->IsInCombat() && !me->getAttackers().empty() && !HasSacredShieldLikeAura(me))
                        target = me;
                    else
                    {
                        for (auto const& [_, bot] : *master->GetBotMgr()->GetBotMap())
                        {
                            u = bot;
                            if (!u || !u->IsInWorld() || me->GetMap() != u->FindMap() || !u->IsAlive() || !u->IsInCombat() ||
                                u->getAttackers().empty() || u->ToCreature()->IsTempBot() || me->GetDistance(u) > 40 ||
                                HasSacredShieldLikeAura(u))
                                continue;

                            target = u;
                            break;
                        }
                    }
                }
            }
            else
            {
                std::vector<Unit*> members = BotMgr::GetAllGroupMembers(gr);
                std::array<decltype(members), 3> member_sets{}; //tanks, players, npcbots
                for (auto i : NPCBots::index_array<size_t, std::size(member_sets)>)
                    member_sets[i].reserve(((members.size() >> 2) + 1) * (i + 1));

                for (Unit* member : members)
                {
                    if (!member->IsInWorld() || me->GetMap() != member->FindMap() || !member->IsAlive() || !member->IsInCombat() ||
                        member->getAttackers().empty() || (member->IsNPCBot() && member->ToCreature()->IsTempBot()) || me->GetDistance(member) > 40 ||
                        HasSacredShieldLikeAura(member))
                        continue;

                    if (IsTank(member))
                        member_sets[0].push_back(member);
                    else if (member->IsPlayer())
                        member_sets[1].push_back(member);
                    else
                        member_sets[2].push_back(member);
                }

                for (auto const& container : member_sets)
                {
                    if (!container.empty())
                    {
                        target = container.size() == 1 ? container.front() : Bcore::Containers::SelectRandomContainerElement(container);
                        break;
                    }
                }

                if (!target)
                {
                    uint8 hp_pct_min = 101;
                    for (auto const& container : member_sets)
                    {
                        for (Unit* member : container)
                        {
                            if (uint8 hp_pct = GetHealthPCT(member); hp_pct < hp_pct_min)
                            {
                                hp_pct_min = hp_pct;
                                target = member;
                            }
                        }
                        if (target)
                            break;
                    }
                }

                if (!target)
                {
                    uint32 attackers_count_max = 0;
                    for (auto const& container : member_sets)
                    {
                        for (Unit* member : container)
                        {
                            if (uint32 attackers_count = member->getAttackers().size(); attackers_count > attackers_count_max)
                            {
                                attackers_count_max = attackers_count;
                                target = member;
                            }
                        }
                        if (target)
                            break;
                    }
                }

                if (!target && master->IsInCombat() && !master->getAttackers().empty() && me->GetDistance(master) < 40 &&
                    !HasSacredShieldLikeAura(master))
                    target = master;
            }

            if (target && doCast(target, GetSpell(SACRED_SHIELD_1)))
                return;
        }

        void HOPGroup(uint32 diff)
        {
            if (!IsSpellReady(HAND_OF_PROTECTION_1, diff) || me->IsMounted() || Feasting() || IsCasting() || Rand() > 30)
                return;

            Group const* gr = !IAmFree() ? master->GetGroup() : GetGroup();
            if (!gr)
            {
                if (me->GetMap() == master->FindMap())
                {
                    if (HOPTarget(master))
                    {
                    }
                    if (!IAmFree() && HOPTarget(me))
                    {
                    }
                }
            }
            else
            {
                std::vector<Unit*> members = BotMgr::GetAllGroupMembers(gr);
                for (auto i : NPCBots::index_array<uint8, 2>)
                {
                    for (Unit* member : members)
                    {
                        if (!(i == 0 ? member->IsPlayer() : member->IsNPCBot()) || me->GetMap() != member->FindMap() ||
                            !member->IsAlive() || !member->IsInCombat() || me->GetDistance(member) > 30 || IsTank(member) ||
                            (member->IsNPCBot() && member->ToCreature()->IsTempBot()) ||
                            HasSacredShieldLikeAura(member))
                            continue;
                        if (HOPTarget(member))
                            return;
                    }
                }
            }
        }

        bool HOPTarget(Unit* target)
        {
            if (!target || IsTank(target))
                return false; // never Hand of Protection an active tank and dump their physical threat
            if ((target->IsPlayer() ? target->GetClass() : target->ToCreature()->GetBotClass()) == BOT_CLASS_PALADIN)
                return false; //paladins should use their own damn bubble
            if (target->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 1) || target->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 127))
                return false; //immune to physical (hop or smth is present)
            if (target->HasAuraTypeWithMiscvalue(SPELL_AURA_MECHANIC_IMMUNITY, 25))
                return false; //forbearance
            if (target->getAttackers().empty())
                return false; //HOP only saves from physical, these aoe are rare and on bosses they are ultimate anyway

            if (GetHealthPCT(target) < 15 + 5 * (uint32)target->getAttackers().size())
            {
                me->InterruptNonMeleeSpells(false);
                if (doCast(target, GetSpell(HAND_OF_PROTECTION_1)))
                {
                    if (target->IsPlayer())
                        ReportSpellCast(HAND_OF_PROTECTION_1, LocalizedNpcText(target->ToPlayer(), BOT_TEXT__ON_YOU), target->ToPlayer());

                    if (!IAmFree() && target->GetGUID() != master->GetGUID())
                        ReportSpellCast(HAND_OF_PROTECTION_1, LocalizedNpcText(master, BOT_TEXT__ON_) + target->GetName() + '!', master);
                }
                return true;
            }

            return false;
        }

        void HOFGroup(uint32 diff)
        {
            if (!IsSpellReady(HAND_OF_FREEDOM_1, diff) || me->IsMounted() || Feasting() || IsCasting() || Rand() > 20)
                return;

            Group const* gr = !IAmFree() ? master->GetGroup() : GetGroup();
            if (!gr)
            {
                if (me->GetMap() == master->FindMap())
                {
                    if (HOFTarget(master))
                    {
                    }
                    if (!IAmFree() && HOFTarget(me))
                    {
                    }
                }
            }
            else
            {
                std::vector<Unit*> members = BotMgr::GetAllGroupMembers(gr);
                for (auto i : NPCBots::index_array<uint8, 2>)
                {
                    for (Unit* member : members)
                    {
                        if (!(i == 0 ? member->IsPlayer() : member->IsNPCBot()) || me->GetMap() != member->FindMap() ||
                            !member->IsAlive() || me->GetDistance(member) > 30 || (member->IsNPCBot() && member->ToCreature()->IsTempBot()))
                            continue;
                        if (HOFTarget(member))
                            return;
                    }
                }
            }
        }

        bool HOFTarget(Unit* target)
        {
            const bool canUnstun = me->GetLevel() >= 35 && GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION;
            if (target->HasAuraType(SPELL_AURA_MECHANIC_IMMUNITY))
            {
                if (target->HasAuraTypeWithMiscvalue(SPELL_AURA_MECHANIC_IMMUNITY, 11) &&
                    target->HasAuraTypeWithMiscvalue(SPELL_AURA_MECHANIC_IMMUNITY, 7))
                    return false; //immune to root and snares
                if (canUnstun && target->HasAuraTypeWithMiscvalue(SPELL_AURA_MECHANIC_IMMUNITY, 12))
                    return false; //immune to stuns
            }

            for (auto const& [_, app] : target->GetAppliedAuras())
            {
                if (!app || app->IsPositive() || app->GetBase()->IsPassive() || app->GetBase()->GetDuration() < 2000)
                    continue;
                SpellInfo const* spellInfo = app->GetBase()->GetSpellInfo();
                if (spellInfo->Attributes & SPELL_ATTR0_DO_NOT_DISPLAY) continue;
                //if (spellInfo->AttributesEx & SPELL_ATTR1_NO_AURA_ICON) continue;
                if (spellInfo->GetSpellMechanicMaskByEffectMask(app->GetEffectMask()) &
                    ((1u << MECHANIC_SNARE) | (1u << MECHANIC_ROOT) | (!canUnstun ? 0 : (1u << MECHANIC_STUN))))
                {
                    uint32 dispel = spellInfo->Dispel;
                    uint32 spell;
                    //Hand of Freedom is level 12, Purify is 8, Cleanse is 42
                    if (!GetSpell(CLEANSE))
                        spell = (dispel == DISPEL_DISEASE || dispel == DISPEL_POISON) ?
                        GetSpell(PURIFY_1) : GetSpell(HAND_OF_FREEDOM_1);
                    else
                        spell = (dispel == DISPEL_MAGIC || dispel == DISPEL_DISEASE || dispel == DISPEL_POISON) ?
                        GetSpell(CLEANSE_1) : GetSpell(HAND_OF_FREEDOM_1);

                    if (doCast(target, spell))
                        return true;
                }
            }
            return false;
        }

        void HOSGroup(uint32 diff)
        {
            if (!IsSpellReady(HAND_OF_SALVATION_1, diff) || IsCasting() || Rand() > 40)
                return;

            //Glyph of Salvation
            if (me->GetLevel() >= 26 && me->GetVictim() && (!me->GetVictim()->CanHaveThreatList() || me->GetVictim()->IsControlledByPlayer()))
            {
                if (!me->getAttackers().empty() && GetHealthPCT(me) < std::max<int32>(80 - 5 * me->getAttackers().size(), 25))
                    if (doCast(me, GetSpell(HAND_OF_SALVATION_1)))
                        return;
            }

            if (IAmFree())
                return;

            Group const* gr = master->GetGroup();
            if (!gr)
                return;

            std::vector<Unit*> members = BotMgr::GetAllGroupMembers(gr);
            for (auto i : NPCBots::index_array<uint8, 2>)
            {
                for (Unit* member : members)
                {
                    if (!(i == 0 ? member->IsPlayer() : member->IsNPCBot()) || me->GetMap() != member->FindMap() ||
                        !member->IsInCombat() || IsTank(member) || me->GetDistance(member) > 30 ||
                        (BotDataMgr::IsTankingClass(i == 0 ? member->GetClass() : member->ToCreature()->GetBotClass()) && !me->GetMap()->IsRaid()) ||
                        (member->IsNPCBot() && member->ToCreature()->IsTempBot()) ||
                        member->HasAuraTypeWithFamilyFlags(SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE, SPELLFAMILY_PALADIN, 0x100))
                        continue;
                    if (HOSTarget(member))
                        return;
                }
            }
        }

        bool HOSTarget(Unit* target)
        {
            for (Unit* attacker : target->getAttackers())
            {
                if (attacker->CanHaveThreatList() && attacker->GetThreatMgr().GetThreatListSize() >= 3 &&
                    attacker->GetThreatMgr().GetThreat(target) > target->GetMaxHealth() / 4.f && target->GetDistance(attacker) < 15)
                {
                    if (doCast(target, GetSpell(HAND_OF_SALVATION_1)))
                        return true;
                    break; //do not try more than once on the same target
                }
            }
            return false;
        }

        bool HealTarget(Unit* target, uint32 diff) override
        {
            if (!target || !target->IsAlive() || target->GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION || me->GetDistance(target) > 40 || !target->GetMaxHealth())
                return false;
            uint8 hp = GetHealthPCT(target);
            if (hp > GetHealHpPctThreshold())
                return false;
            bool pointed = IsPointedHealTarget(target);
            if (hp > 90 && !(pointed && me->GetMap()->IsRaid()) &&
                (!target->IsInCombat() || target->getAttackers().empty() || !IsTank(target) || !me->GetMap()->IsRaid()))
                return false;
            //try to preserve heal if Divine Plea is active
            if (hp > 50 && me->GetAuraEffect(SPELL_AURA_OBS_MOD_POWER, SPELLFAMILY_PALADIN, 0x0, 0x0, 0x1))
                return false;

            int32 hps = GetHPS(target);
            int32 xphp = target->GetHealth() + hps * 2.5f;
            int32 hppctps = int32(hps * 100.f / float(target->GetMaxHealth()));
            int32 xphploss = xphp > int32(target->GetMaxHealth()) ? 0 : abs(int32(xphp - target->GetMaxHealth()));
            int32 xppct = hp + hppctps * 2.5f;
            if (xppct >= 95 && hp >= 25 && !pointed)
                return false;

            //Lay on Hands
            if (IsSpellReady(LAY_ON_HANDS_1, diff, false) && (target != me || shieldDelayTimer <= diff) &&
                (target->IsInCombat() || !target->getAttackers().empty()) && Rand() < 80 && hp <= 20 && xppct <= 0 &&
                !target->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 127))
            {
                me->InterruptNonMeleeSpells(false);
                if (doCast(target, GetSpell(LAY_ON_HANDS_1)))
                {
                    if (target->IsPlayer())
                        ReportSpellCast(LAY_ON_HANDS_1, LocalizedNpcText(target->ToPlayer(), BOT_TEXT__ON_YOU), target->ToPlayer());

                    if (!IAmFree() && target != master)
                    {
                        std::string msg = target == me ? LocalizedNpcText(master, BOT_TEXT__ON_MYSELF) : (LocalizedNpcText(master, BOT_TEXT__ON_) + target->GetName() + '!');
                        ReportSpellCast(LAY_ON_HANDS_1, msg, master);
                    }
                    return true;
                }
            }

            int32 holyShockHealThreshold = _heals[HOLY_SHOCK_1] / 2;

            //Holy Shock
            if (IsSpellReady(HOLY_SHOCK_1, diff, false) && !target->IsCharmed() && !target->isPossessed() &&
                xphploss > holyShockHealThreshold)
            {
                me->InterruptNonMeleeSpells(false);
                if (hp < 30 && IsSpellReady(DIVINE_FAVOR_1, diff, false) && !target->getAttackers().empty())
                    if (doCast(me, GetSpell(DIVINE_FAVOR_1)))
                    {
                    }
                if (doCast(target, GetSpell(HOLY_SHOCK_1)))
                    return true;
            }

            if (IsCasting()) return false;

            Unit const* u = target->GetVictim();
            bool tanking = u && IsTank(target) && u->ToCreature() && u->ToCreature()->isWorldBoss();
            int32 flashOfLightHealThreshold = _heals[FLASH_OF_LIGHT_1] / 2;

            if (IsSpellReady(DIVINE_ILLUMINATION_1, diff, false) && GetManaPCT(me) <= 50 && Rand() < 50 + 50 * tanking)
                if (doCast(me, GetSpell(DIVINE_ILLUMINATION_1)))
                {
                }

            //Holy Light
            if (IsSpellReady(HOLY_LIGHT_1, diff) && (xppct > 15 || !GetSpell(FLASH_OF_LIGHT_1)) &&
                xphploss > _heals[HOLY_LIGHT_1])
            {
                //Aura Mastery
                if (hp < 60 && _myaura == CONCENTRATION_AURA_1 && IsSpellReady(AURA_MASTERY_1, diff, false) && Rand() < 90 &&
                    ((!me->getAttackers().empty() && (*me->getAttackers().begin())->IsPlayer()) ||
                        me->GetMap()->Instanceable() || tanking))
                    if (doCast(me, GetSpell(AURA_MASTERY_1)))
                    {
                    }
                if (doCast(target, GetSpell(HOLY_LIGHT_1)))
                    return true;
            }
            //Flash of Light
            if (IsSpellReady(FLASH_OF_LIGHT_1, diff) && (tanking || xphploss > flashOfLightHealThreshold))
            {
                if (doCast(target, GetSpell(FLASH_OF_LIGHT_1)))
                    return true;
            }

            return false;
        }

        void StartAttack(Unit* u, bool force = false)
        {
            if (!bot_ai::StartAttack(u, force))
                return;
            GetInPosition(force, u);
        }

        void JustEngagedWith(Unit* u) override { bot_ai::JustEngagedWith(u); }
        void KilledUnit(Unit* u) override { bot_ai::KilledUnit(u); }
        void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override { bot_ai::EnterEvadeMode(why); }
        void MoveInLineOfSight(Unit* u) override { bot_ai::MoveInLineOfSight(u); }
        void JustDied(Unit* u) override { bot_ai::JustDied(u); }

        void BreakCC(uint32 diff) override
        {
            if (me->GetLevel() >= 35 && GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION && IsSpellReady(HAND_OF_FREEDOM_1, diff) && Rand() < 30 && me->HasAuraWithMechanic(1u << MECHANIC_STUN))
            {
                if (me->IsMounted())
                    me->RemoveAurasByType(SPELL_AURA_MOUNTED);
                if (doCast(me, GetSpell(HAND_OF_FREEDOM_1)))
                    return;
            }
            bot_ai::BreakCC(diff);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!GlobalUpdate(diff))
                return;

            DoVehicleActions(diff);
            if (!CanBotAttackOnVehicle())
                return;

            if (IsPotionReady())
            {
                if (GetManaPCT(me) < 30)
                    DrinkPotion(true);
                else if (GetHealthPCT(me) < 60)
                    DrinkPotion(false);
            }
            else if (GetManaPCT(me) < 40 && IsSpellReady(DIVINE_PLEA_1, diff) && Rand() < 30 &&
                !(GetSpec() == BOT_SPEC_PALADIN_HOLY && HasUrgentHealingNeed(65)) &&
                !me->GetAuraEffect(SPELL_AURA_OBS_MOD_POWER, SPELLFAMILY_PALADIN, 0x0, 0x80004000, 0x1))
            {
                if (doCast(me, GetSpell(DIVINE_PLEA_1)))
                    return;
            }

            CheckRacials(diff);

            HOPGroup(diff);
            CheckBeacon(diff);

            // Holy paladins maintain Beacon / Sacred Shield on tanks outside the normal
            // reactive healing threshold. In raids this runs after the existing raid-heal
            // pass so raid healing remains the first priority.
            if (!me->GetMap()->IsRaid() && MaintainHolyTankSupport(diff))
                return;

            if (me->GetMap()->IsRaid())
            {
                CureGroup(GetSpell(CLEANSE), diff);
                BuffAndHealGroup(diff);
                CheckHandOfSacrifice(diff);
                ShieldGroup(diff);
                MaintainHolyTankSupport(diff);
            }
            else
            {
                BuffAndHealGroup(diff);
                CheckHandOfSacrifice(diff);
                ShieldGroup(diff);
                CureGroup(GetSpell(CLEANSE), diff);
            }

            CheckSacrifice(diff);
            HOFGroup(diff);
            HOSGroup(diff);

            if (!me->IsInCombat())
                DoNonCombatActions(diff);

            CheckSeal(diff);
            CheckAura(diff);

            if (TryMaintainRighteousFury(diff))
                return;
            if (TryProtectionSacredShield(nullptr, diff))
                return;

            if (ProcessImmediateNonAttackTarget())
                return;

            if (!CheckAttackTarget())
                return;

            CheckRepentance(diff);
            Counter(diff);
            TurnEvil(diff);

            CheckDivineIntervention(diff);
            if (!me->IsAlive())
                return;

            if (IsCasting())
                return;

            CheckUsableItems(diff);

            DoNormalAttack(diff);
        }

        void DoNonCombatActions(uint32 diff)
        {
            if (GC_Timer > diff || me->IsMounted() || IsCasting())
                return;

            ResurrectGroup(GetSpell(REDEMPTION_1));
        }

        void CheckSeal(uint32 diff)
        {
            if (checkSealTimer > diff || GC_Timer > diff || me->IsMounted() ||
                IsCasting() || Feasting() || Rand() > 30)
                return;

            checkSealTimer = 10000;

            Unit const* victim = me->GetVictim();

            uint32 COMMAND = GetSpell(SEAL_OF_COMMAND_1);
            uint32 LIGHT = GetSpell(SEAL_OF_LIGHT_1);
            uint32 RIGHT = GetSpell(SEAL_OF_RIGHTEOUSNESS_1);
            uint32 WISDOM = GetSpell(SEAL_OF_WISDOM_1);
            uint32 JUSTICE = GetSpell(SEAL_OF_JUSTICE_1);
            uint32 VENGEANCE = (me->GetRaceMask() & sRaceMgr->GetAllianceRaceMask()) ? GetSpell(SEAL_OF_VENGEANCE_1) : GetSpell(SEAL_OF_CORRUPTION_1);

            bool const durable = victim && IsDurablePaladinTarget(victim);
            bool const multiTarget = me->getAttackers().size() > 1 || (victim && HasNearbyPaladinAOETargets(8.f, 3));
            uint32 SEAL = 0;

            if (IsMelee() && GetManaPCT(me) < 18 && WISDOM)
                SEAL = WISDOM;
            else if (GetSpec() == BOT_SPEC_PALADIN_PROTECTION || IsTank())
            {
                // Prot: Vengeance/Corruption for bosses and durable single targets, Command for trash packs,
                // Wisdom when strained, Righteousness as fallback. Justice is utility, not default tanking.
                if (GetManaPCT(me) < 25 && WISDOM)
                    SEAL = WISDOM;
                else if (durable && VENGEANCE)
                    SEAL = VENGEANCE;
                else if (multiTarget && COMMAND)
                    SEAL = COMMAND;
                else if (RIGHT)
                    SEAL = RIGHT;
                else if (JUSTICE)
                    SEAL = JUSTICE;
            }
            else if (GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION || HasRole(BOT_ROLE_DPS))
            {
                // Ret: Command for general cleave/trash, Vengeance/Corruption for durable targets.
                if (HasRole(BOT_ROLE_HEAL) && WISDOM)
                    SEAL = WISDOM;
                else if (durable && VENGEANCE)
                    SEAL = VENGEANCE;
                else if (COMMAND)
                    SEAL = COMMAND;
                else if (RIGHT)
                    SEAL = RIGHT;
            }
            else if (GetSpec() == BOT_SPEC_PALADIN_HOLY || HasRole(BOT_ROLE_HEAL))
            {
                // Holy defaults to Wisdom for longevity, Light if Wisdom is unavailable.
                SEAL = WISDOM ? WISDOM : LIGHT ? LIGHT : RIGHT;
            }

            if (SEAL && !me->HasAura(SEAL))
                if (doCast(me, SEAL))
                    return;
        }

        void CheckAura(uint32 diff)
        {
            if (checkAuraTimer > diff || GC_Timer > diff || (IAmFree() && !GetBG()) || IsCasting() ||
                /*me->GetExactDist(master) > 40 || me->IsMounted() || Feasting() || */Rand() > 20)
                return;

            checkAuraTimer = urand(3000, 6000);

            //7 paladins in group?
            uint32 DEVOTION_AURA = GetSpell(DEVOTION_AURA_1);
            uint32 CONCENTRATION_AURA = GetSpell(CONCENTRATION_AURA_1);
            uint32 FIRE_RESISTANCE_AURA = GetSpell(FIRE_RESISTANCE_AURA_1);
            uint32 FROST_RESISTANCE_AURA = GetSpell(FROST_RESISTANCE_AURA_1);
            uint32 SHADOW_RESISTANCE_AURA = GetSpell(SHADOW_RESISTANCE_AURA_1);
            uint32 RETRIBUTION_AURA = GetSpell(RETRIBUTION_AURA_1);
            //uint32 CRUSADER_AURA = GetSpell(CRUSADER_AURA_1);

            bool pureHealer = GetSpec() == BOT_SPEC_PALADIN_HOLY;
            bool isProt = GetSpec() == BOT_SPEC_PALADIN_PROTECTION;

            std::map<uint32 /*baseid*/, uint32 /*curid*/> idMap;
            uint32 mask = _getAurasMask(idMap);

            //for Aura Mastery allow every pure healer paladin to have their own C aura
            //SPECIFIC_AURA_MY_AURA check still works so no spam
            if (pureHealer)
                mask &= ~SPECIFIC_AURA_CONCENTRATION;

            //if (CRUSADER_AURA && !(mask & SPECIFIC_AURA_CRUSADER) &&
            //    (master->IsMounted() || me->IsMounted()))
            //{
            //    if (doCast(me, CRUSADER_AURA))
            //        return;
            //}

            //Has own aura or has all auras
            if (mask & SPECIFIC_AURA_MY_AURA)
                return;
            else if ((mask & SPECIFIC_AURA_ALL_AUTOUSE) == SPECIFIC_AURA_ALL_AUTOUSE)
                return;

            //TODO: priority?
            if (_myaura && GetSpell(_myaura) && (!idMap.contains(_myaura) || idMap[_myaura] < GetSpell(_myaura)))
            {
                if (doCast(me, GetSpell(_myaura)))
                    return;
            }
            if (DEVOTION_AURA &&
                (!(mask & SPECIFIC_AURA_DEVOTION) || idMap[DEVOTION_AURA_1] < DEVOTION_AURA) &&
                (!RETRIBUTION_AURA || IsTank(master) || isProt))
            {
                if (doCast(me, DEVOTION_AURA))
                    return;
            }
            if (CONCENTRATION_AURA && !(mask & SPECIFIC_AURA_CONCENTRATION) &&
                (master->GetClass() == BOT_CLASS_MAGE || master->GetClass() == BOT_CLASS_PRIEST ||
                    master->GetClass() == BOT_CLASS_WARLOCK || master->GetClass() == BOT_CLASS_DRUID ||
                    (!IAmFree() && master->GetClass() == BOT_CLASS_PALADIN) || pureHealer))
            {
                if (doCast(me, CONCENTRATION_AURA))
                    return;
            }
            if (RETRIBUTION_AURA &&
                (!(mask & SPECIFIC_AURA_RETRIBUTION) || idMap[RETRIBUTION_AURA_1] < RETRIBUTION_AURA) &&
                (BotDataMgr::IsMeleeClass(master->GetClass()) || IsMelee()))
            {
                if (doCast(me, RETRIBUTION_AURA))
                    return;
            }
            if (FIRE_RESISTANCE_AURA &&
                (!(mask & SPECIFIC_AURA_FIRE_RES) || idMap[FIRE_RESISTANCE_AURA_1] < FIRE_RESISTANCE_AURA))
            {
                if (doCast(me, FIRE_RESISTANCE_AURA))
                    return;
            }
            if (SHADOW_RESISTANCE_AURA && GetBG() &&
                (!(mask & SPECIFIC_AURA_SHADOW_RES) || idMap[SHADOW_RESISTANCE_AURA_1] < SHADOW_RESISTANCE_AURA))
            {
                if (doCast(me, SHADOW_RESISTANCE_AURA))
                    return;
            }
            if (FROST_RESISTANCE_AURA &&
                (!(mask & SPECIFIC_AURA_FROST_RES) || idMap[FROST_RESISTANCE_AURA_1] < FROST_RESISTANCE_AURA))
            {
                if (doCast(me, FROST_RESISTANCE_AURA))
                    return;
            }
            if (SHADOW_RESISTANCE_AURA &&
                (!(mask & SPECIFIC_AURA_SHADOW_RES) || idMap[SHADOW_RESISTANCE_AURA_1] < SHADOW_RESISTANCE_AURA))
            {
                if (doCast(me, SHADOW_RESISTANCE_AURA))
                    return;
            }
        }

        bool BuffTarget(Unit* target, uint32 /*diff*/) override
        {
            if ((me->IsInCombat() || !CanDoNonCombatActions()) && !master->GetMap()->IsRaid()) return false;

            if (target == me)
            {
                if (uint32 rFury = GetSpell(RIGHTEOUS_FURY_1))
                {
                    if (IsTank())
                    {
                        if (!me->HasAura(rFury) && doCast(me, rFury))
                            return true;
                    }
                    else if (me->HasAura(rFury))
                        me->RemoveAurasDueToSpell(rFury);
                }
            }

            uint32 mask = _getBlessingsMask(target);

            //already has my blessing
            if (mask & SPECIFIC_BLESSING_MY_BLESSING)
                return false;

            uint32 BLESSING_OF_WISDOM = GetSpell(BLESSING_OF_WISDOM_1);
            uint32 BLESSING_OF_KINGS = GetSpell(BLESSING_OF_KINGS_1);
            uint32 BLESSING_OF_SANCTUARY = GetSpell(BLESSING_OF_SANCTUARY_1);
            uint32 BLESSING_OF_MIGHT = GetSpell(BLESSING_OF_MIGHT_1);

            bool wisdom = (mask & SPECIFIC_BLESSING_WISDOM);
            bool kings = (mask & SPECIFIC_BLESSING_KINGS);
            bool sanctuary = (mask & SPECIFIC_BLESSING_SANCTUARY);
            bool might = (mask & SPECIFIC_BLESSING_MIGHT);

            if (IsTank(target))
            {
                // Tanks should prefer Sanctuary when available. Kings is excellent, but Sanctuary
                // is the paladin's tank-specific blessing and helps the role feel less generic.
                if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                else if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (BLESSING_OF_WISDOM && !wisdom && target->GetMaxPower(POWER_MANA) > 1 && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                else if (BLESSING_OF_MIGHT && !might && doCast(target, BLESSING_OF_MIGHT))
                    return true;

                return false;
            }

            uint8 Class = 0;
            if (target->IsPlayer())
                Class = target->GetClass();
            else if (Creature* cre = target->ToCreature())
                Class = cre->GetBotAI() ? cre->GetBotAI()->GetBotClass() : cre->GetClass();

            switch (Class)
            {
            case BOT_CLASS_BM:
            case BOT_CLASS_SPHYNX:
            case BOT_CLASS_DREADLORD:
            case BOT_CLASS_SPELLBREAKER:
            case BOT_CLASS_DARK_RANGER:
            case BOT_CLASS_NECROMANCER:
            case BOT_CLASS_SEA_WITCH:
            case BOT_CLASS_CRYPT_LORD:
                if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (BLESSING_OF_MIGHT && !might && doCast(target, BLESSING_OF_MIGHT))
                    return true;
                else if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                break;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (BLESSING_OF_WISDOM && !wisdom && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                else if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                break;
            case CLASS_DEATH_KNIGHT:
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_ROGUE:
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (BLESSING_OF_MIGHT && !might && doCast(target, BLESSING_OF_MIGHT))
                    return true;
                else if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                else if (BLESSING_OF_WISDOM && !wisdom && target->GetPowerType() == POWER_MANA && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                break;
            default:
                if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (BLESSING_OF_WISDOM && !wisdom && target->GetMaxPower(POWER_MANA) > 1 && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                else if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                else if (BLESSING_OF_MIGHT && !might && doCast(target, BLESSING_OF_MIGHT))
                    return true;
                break;
            }
            return false;
        }

        void CheckRepentance(uint32 diff)
        {
            if (Rand() > 25)
                return;

            if (IsSpellReady(REPENTANCE_1, diff))
            {
                Unit* u = FindStunTarget();
                if (u && u->GetVictim() != me && doCast(u, GetSpell(REPENTANCE_1)))
                    return;
            }
        }

        void Counter(uint32 diff)
        {
            if (Rand() > 30)
                return;

            for (const auto base_spell : { REPENTANCE_1, TURN_EVIL_1, HOLY_WRATH_1, HAMMER_OF_JUSTICE_1 })
                if (IsSpellReady(base_spell, diff, false) && !HasQueuedSpellAction(base_spell))
                    if (Unit const* target = FindCastingTarget(base_spell == HOLY_WRATH_1 ? 8.0f : CalcSpellMaxRange(base_spell), 0, base_spell))
                        if (EnqueueCounterSpellAction(target->GetGUID(), base_spell, true))
                            return;
        }

        void TurnEvil(uint32 diff)
        {
            if (!IsSpellReady(TURN_EVIL_1, diff) || IsCasting() || Rand() > 50 ||
                FindAffectedTarget(GetSpell(TURN_EVIL_1), me->GetGUID(), 50))
                return;
            Unit* target = FindUndeadCCTarget(20, TURN_EVIL_1);
            if (target &&
                (target != me->GetVictim() || GetHealthPCT(me) < 70 || target->GetVictim() == master) &&
                doCast(target, GetSpell(TURN_EVIL_1)))
                return;
            else
            {
                for (Unit* mtar : { opponent, disttarget })
                {
                    if (mtar && (mtar->GetCreatureTypeMask() & CREATURE_TYPEMASK_DEMON_OR_UNDEAD) && !CCed(mtar) &&
                        mtar->GetVictim() && !IsTank(mtar->GetVictim()) && mtar->GetVictim() != me &&
                        GetHealthPCT(me) < 90 &&
                        doCast(mtar, GetSpell(TURN_EVIL_1)))
                        return;
                }
            }
        }

        void CheckDivineIntervention(uint32 diff)
        {
            if (!IsSpellReady(DIVINE_INTERVENTION_1, diff, !IsCasting()) || IAmFree() || IsTank() ||
                GetManaPCT(me) > 10 || Rand() > 20)
                return;

            std::list<Unit*> players;

            if (master->IsAlive() && !master->getAttackers().empty() && GetHealthPCT(master) < 15 &&
                !master->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 127))
                players.push_back(master);
            if (Group const* gr = master->GetGroup())
            {
                for (GroupReference const* itr = gr->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    Player* pl = itr->GetSource();
                    if (!pl || pl == master || !pl->IsInWorld() || me->GetMap() != pl->FindMap() ||
                        !pl->IsAlive() || pl->getAttackers().empty() || GetHealthPCT(pl) > 15 ||
                        pl->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 127))
                        continue;

                    players.push_back(pl);
                }
            }

            if (players.empty())
                return;

            Unit* target = players.size() == 1 ? players.front() : Bcore::Containers::SelectRandomContainerElement(players);
            if (doCast(target, GetSpell(DIVINE_INTERVENTION_1)))
                return;
        }

        bool HasUrgentHealingNeed(uint8 hpThreshold = 55)
        {
            if (!HasRole(BOT_ROLE_HEAL) || IAmFree() || !master || !master->GetBotMgr())
                return false;

            auto needsHelp = [&](Unit const* member) -> bool
                {
                    if (!member || !member->IsAlive() || me->GetMap() != member->FindMap())
                        return false;
                    if (member->isPossessed() || member->IsCharmed())
                        return false;
                    if (member->IsNPCBot() && member->ToCreature()->IsTempBot())
                        return false;
                    if (me->GetDistance(member) > 40)
                        return false;
                    if (!member->IsInCombat() && member->getAttackers().empty())
                        return false;

                    uint8 pct = GetHealthPCT(member);
                    if (pct <= hpThreshold)
                        return true;

                    // Tanks deserve an earlier warning bell, especially in 5-man content.
                    if (IsTank(member) && pct <= hpThreshold + 15 && !member->getAttackers().empty())
                        return true;

                    return false;
                };

            if (needsHelp(master))
                return true;

            if (Group const* gr = master->GetGroup())
            {
                for (Unit const* member : BotMgr::GetAllGroupMembers(gr))
                    if (needsHelp(member))
                        return true;
            }
            else
            {
                for (auto const& [_, bot] : *master->GetBotMgr()->GetBotMap())
                    if (needsHelp(bot))
                        return true;
            }

            return false;
        }

        bool IsDurablePaladinTarget(Unit const* target) const
        {
            if (!target || !target->IsAlive())
                return false;

            if (target->IsPlayer())
                return true;

            if (Creature const* creature = target->ToCreature())
            {
                if (creature->IsDungeonBoss() || creature->isWorldBoss())
                    return true;
                if (creature->GetCreatureTemplate()->rank != CREATURE_ELITE_NORMAL)
                    return true;
            }

            return target->GetHealth() > me->GetMaxHealth() * uint32(1 + target->getAttackers().size());
        }

        bool HasNearbyPaladinAOETargets(float range, uint8 requiredCount)
        {
            std::list<Unit*> targets;
            GetNearbyTargetsList(targets, range, 0);
            return targets.size() >= requiredCount;
        }

        uint32 SelectJudgementForTarget(Unit* target, float dist, bool preferWisdom, bool allowJustice)
        {
            if (!target)
                return 0;

            if (allowJustice && GetSpell(JUDGEMENT_OF_JUSTICE_1) && target->HasAuraType(SPELL_AURA_MOD_INCREASE_SPEED) &&
                dist < CalcSpellMaxRange(JUDGEMENT_OF_JUSTICE_1))
            {
                bool canCast = true;
                for (AuraEffect const* aeff : target->GetAuraEffectsByType(SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED))
                {
                    if (aeff->GetCasterGUID() != me->GetGUID() && aeff->GetBase()->GetDuration() > 2000)
                    {
                        canCast = false;
                        break;
                    }
                }

                if (canCast)
                {
                    for (AuraEffect const* aeff : target->GetAuraEffectsByType(SPELL_AURA_MOD_INCREASE_SPEED))
                    {
                        if (!aeff->GetBase()->IsPassive() && aeff->GetBase()->GetDuration() > 2000 && aeff->GetAmount() >= 30)
                            return JUDGEMENT_OF_JUSTICE_1;
                    }
                }
            }

            if (GetSpell(JUDGEMENT_OF_WISDOM_1) && dist < CalcSpellMaxRange(JUDGEMENT_OF_WISDOM_1))
            {
                AuraEffect const* wisd = target->GetAuraEffect(JUDGEMENT_OF_WISDOM_AURA, 0);
                uint8 myManaPct = GetManaPCT(me);
                if (preferWisdom || (!wisd && myManaPct < 35) || (wisd && wisd->GetCasterGUID() == me->GetGUID() && myManaPct < 50))
                    return JUDGEMENT_OF_WISDOM_1;
            }

            if (GetSpell(JUDGEMENT_OF_LIGHT_1) && dist < CalcSpellMaxRange(JUDGEMENT_OF_LIGHT_1))
                return JUDGEMENT_OF_LIGHT_1;

            if (GetSpell(JUDGEMENT_OF_WISDOM_1) && dist < CalcSpellMaxRange(JUDGEMENT_OF_WISDOM_1))
                return JUDGEMENT_OF_WISDOM_1;
            if (GetSpell(JUDGEMENT_OF_JUSTICE_1) && dist < CalcSpellMaxRange(JUDGEMENT_OF_JUSTICE_1))
                return JUDGEMENT_OF_JUSTICE_1;

            return 0;
        }

        bool TryJudgement(Unit* target, uint32 diff, bool canHoly, float dist, bool preferWisdom = false, bool allowJustice = true)
        {
            if (!canHoly || GetSpellCooldown(JUDGEMENT_OF_LIGHT_1) > diff)
                return false;

            uint32 judgement = SelectJudgementForTarget(target, dist, preferWisdom, allowJustice);
            return judgement && doCast(target, GetSpell(judgement));
        }

        bool TryAvengingWrath(Unit* target, uint32 diff, bool canHoly, float dist)
        {
            uint8 const useChance = IsTank() ? 80 : 50;
            if (!canHoly || !IsSpellReady(AVENGING_WRATH_1, diff, false) || avDelayTimer > diff || dist > 30 || Rand() > useChance)
                return false;

            bool const execute = target && target->HasAuraState(AURA_STATE_HEALTHLESS_20_PERCENT);
            bool const durable = IsDurablePaladinTarget(target);
            bool const pressured = me->getAttackers().size() >= 3;
            bool const holyPleaOffset = HasRole(BOT_ROLE_HEAL | BOT_ROLE_RANGED) &&
                me->GetAuraEffect(SPELL_AURA_OBS_MOD_POWER, SPELLFAMILY_PALADIN, 0x0, 0x80004000, 0x1);

            bool use = false;
            if (IsTank())
                use = durable || pressured;
            else if (GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION)
                use = durable || execute || pressured;
            else if (GetSpec() == BOT_SPEC_PALADIN_HOLY)
                use = holyPleaOffset || (GetBG() && (execute || pressured));
            else
                use = durable || execute;

            if (use && doCast(me, GetSpell(AVENGING_WRATH_1)))
                return true;

            return false;
        }

        bool TryHammerOfWrath(Unit* target, uint32 diff, bool canHoly, float dist)
        {
            if (IsSpellReady(HAMMER_OF_WRATH_1, diff) && canHoly && dist < 30 &&
                target->HasAuraState(AURA_STATE_HEALTHLESS_20_PERCENT))
            {
                if (doCast(target, GetSpell(HAMMER_OF_WRATH_1)))
                    return true;
            }

            return false;
        }

        bool TryHolyWrath(Unit* target, uint32 diff)
        {
            if (!target || !IsSpellReady(HOLY_WRATH_1, diff) || Rand() > 75)
                return false;

            if ((target->GetCreatureType() == CREATURE_TYPE_UNDEAD || target->GetCreatureType() == CREATURE_TYPE_DEMON) &&
                me->GetDistance(target) < 8.5f && doCast(me, GetSpell(HOLY_WRATH_1)))
                return true;

            if (FindUndeadCCTarget(8.5f, HOLY_WRATH_1, false) && doCast(me, GetSpell(HOLY_WRATH_1)))
                return true;

            return false;
        }

        bool HasMyBeaconOfLightAura(Unit const* target) const
        {
            if (!target)
                return false;

            ObjectGuid const casterGuid = me->GetGUID();

            if (uint32 beacon = GetSpell(BEACON_OF_LIGHT_1))
                if (target->GetAura(beacon, casterGuid))
                    return true;

            return target->GetAuraEffect(SPELL_AURA_PERIODIC_TRIGGER_SPELL, SPELLFAMILY_PALADIN, 0x0, 0x1000000, 0x0, casterGuid);
        }

        bool HasMySacredShieldLikeAura(Unit const* target) const
        {
            if (!target)
                return false;

            auto const casterGuid = me->GetGUID();

            if (uint32 sacredShield = GetSpell(SACRED_SHIELD_1))
                if (target->GetAura(sacredShield, casterGuid))
                    return true;

            if (target->GetAura(SACRED_SHIELD_AURA_R1, casterGuid) ||
                target->GetAura(SACRED_SHIELD_AURA_R2, casterGuid) ||
                target->GetAura(SACRED_SHIELD_AURA_R3, casterGuid))
                return true;

            return target->GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_PALADIN, 0x0, 0x80000, 0x0, casterGuid);
        }

        bool HasSacredShieldLikeAura(Unit const* target) const
        {
            if (!target)
                return false;

            if (uint32 sacredShield = GetSpell(SACRED_SHIELD_1))
                if (target->HasAura(sacredShield))
                    return true;

            if (target->HasAura(SACRED_SHIELD_AURA_R1) ||
                target->HasAura(SACRED_SHIELD_AURA_R2) ||
                target->HasAura(SACRED_SHIELD_AURA_R3))
                return true;

            return target->GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_PALADIN, 0x0, 0x80000, 0x0);
        }

        bool TargetNeedsJudgementsOfTheJust(Unit const* target) const
        {
            if (GetSpec() != BOT_SPEC_PALADIN_PROTECTION || me->GetLevel() < 55 || !target || !target->IsAlive())
                return false;
            if (target->GetAuraEffect(JUDGEMENTS_OF_THE_JUST_AURA, EFFECT_1, me->GetGUID()))
                return false;
            if (target->IsPlayer())
                return true;

            Creature const* creature = target->ToCreature();
            if (!creature)
                return IsDurablePaladinTarget(target);

            if (creature->IsDungeonBoss() || creature->isWorldBoss())
                return true;
            if (creature->GetCreatureTemplate()->rank != CREATURE_ELITE_NORMAL)
                return true;

            return target->GetHealth() > me->GetMaxHealth() / 2;
        }

        bool TryMaintainRighteousFury(uint32 diff)
        {
            if (!IsTank() || IsCasting() || me->IsMounted() || Feasting())
                return false;

            uint32 const RIGHTEOUS_FURY = GetSpell(RIGHTEOUS_FURY_1);
            if (!RIGHTEOUS_FURY || me->HasAura(RIGHTEOUS_FURY) || !IsSpellReady(RIGHTEOUS_FURY_1, diff))
                return false;

            // Righteous Fury falling off in combat is catastrophic for a tank. Reapply it immediately.
            if (me->IsInCombat() || !me->getAttackers().empty() || GetBG() || IsWanderer())
                if (doCast(me, RIGHTEOUS_FURY))
                    return true;

            return false;
        }

        bool TryProtectionLayOnHands(uint32 diff)
        {
            if (!IsTank() || !IsSpellReady(LAY_ON_HANDS_1, diff, false) || shieldDelayTimer > diff)
                return false;
            if (!me->IsInCombat() && me->getAttackers().empty())
                return false;
            if (me->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 127))
                return false;

            uint8 const hp = GetHealthPCT(me);
            uint8 const panicPct = me->getAttackers().size() >= 3 ? 24 : 18;
            if (hp > panicPct)
                return false;

            me->InterruptNonMeleeSpells(false);
            if (doCast(me, GetSpell(LAY_ON_HANDS_1)))
            {
                if (!IAmFree())
                    ReportSpellCast(LAY_ON_HANDS_1, LocalizedNpcText(master, BOT_TEXT__ON_MYSELF), master);
                return true;
            }

            return false;
        }

        bool TryProtectionSacredShield(Unit* target, uint32 diff)
        {
            if (!IsTank() || !IsSpellReady(SACRED_SHIELD_1, diff) || IsCasting() || me->IsMounted() || Feasting())
                return false;
            if (HasSacredShieldLikeAura(me))
                return false;

            bool const tankingNow = me->IsInCombat() || !me->getAttackers().empty() || (target && target->GetVictim() == me);
            bool const meaningfulFight = tankingNow && (me->getAttackers().size() >= 2 || GetHealthPCT(me) < 95 || IsDurablePaladinTarget(target));
            if (!meaningfulFight)
                return false;

            if (doCast(me, GetSpell(SACRED_SHIELD_1)))
                return true;

            return false;
        }

        bool TryProtectionEmergencyStun(Unit* target, uint32 diff, float dist)
        {
            if (!target || !IsSpellReady(HAMMER_OF_JUSTICE_1, diff) || CCed(target) || dist >= 10)
                return false;
            if (target->GetDiminishing(DIMINISHING_STUN) > DIMINISHING_LEVEL_2)
                return false;
            if (IsImmunedToMySpellEffect(target, sSpellMgr->GetSpellInfo(HAMMER_OF_JUSTICE_1), EFFECT_0))
                return false;

            Unit* victim = target->GetVictim();
            bool const interrupt = target->IsNonMeleeSpellCast(false, false, true);
            bool const rescue = victim && victim != me && IsInBotParty(victim) && !IsTank(victim);
            bool const pressureStop = GetHealthPCT(me) < 55 && target->GetVictim() == me;
            if (!(interrupt || rescue || pressureStop))
                return false;

            if (doCast(target, GetSpell(HAMMER_OF_JUSTICE_1)))
                return true;

            return false;
        }

        bool DoPaladinDefensives(Unit* target, uint32 diff)
        {
            // Divine Shield for non-tanks / desperate solo or PvP situations.
            if (IsSpellReady(DIVINE_SHIELD_1, diff) && shieldDelayTimer <= diff && (IAmFree() || !IsTank()) &&
                Rand() < 80 && !me->getAttackers().empty() && GetHealthPCT(me) < 25)
            {
                if (doCast(me, GetSpell(DIVINE_SHIELD_1)))
                    return true;
            }

            if (TryProtectionLayOnHands(diff))
                return true;

            // Divine Protection is the tank wall. Use it before the paladin is already paste.
            if (IsSpellReady(DIVINE_PROTECTION_1, diff, false) && shieldDelayTimer <= diff && IsTank() &&
                !me->getAttackers().empty())
            {
                bool const heavyPressure = me->getAttackers().size() >= 3 || (target && target->GetVictim() == me && IsDurablePaladinTarget(target));
                uint8 const wallPct = heavyPressure ? 82 : 68;
                uint8 const hotPenalty = me->HasAuraType(SPELL_AURA_PERIODIC_HEAL) ? 12 : 0;
                if (GetHealthPCT(me) < wallPct - hotPenalty && Rand() < (heavyPressure ? 95 : 85))
                    if (doCast(me, GetSpell(DIVINE_PROTECTION_1)))
                        return true;
            }

            if (TryMaintainRighteousFury(diff))
                return true;

            if (TryProtectionSacredShield(target, diff))
                return true;

            // Divine Plea belongs here too, but Holy uses a stricter gate elsewhere so it does not
            // gleefully halve its healing while the tank is being turned into jam.
            if (IsSpellReady(DIVINE_PLEA_1, diff) && Rand() < (IsTank() ? 70 : 35) &&
                GetManaPCT(me) < (IsTank() ? 92 : GetSpec() == BOT_SPEC_PALADIN_HOLY ? 30 : 12) &&
                !(GetSpec() == BOT_SPEC_PALADIN_HOLY && HasUrgentHealingNeed(65)) &&
                !me->GetAuraEffect(SPELL_AURA_OBS_MOD_POWER, SPELLFAMILY_PALADIN, 0x0, 0x80004000, 0x1))
            {
                if (doCast(me, GetSpell(DIVINE_PLEA_1)))
                    return true;
            }

            return false;
        }

        bool DoPaladinTaunts(Unit* target, uint32 diff, bool canHoly, float dist)
        {
            if (!IsTank() || !target || !target->IsCreature() || target->IsControlledByPlayer() || CCed(target))
                return false;

            Unit* victim = target->GetVictim();
            bool const targetTaunted = target->HasAuraType(SPELL_AURA_MOD_TAUNT);

            // Hand of Reckoning: snap threat back to the paladin when a party member is getting hit.
            if (IsSpellReady(HAND_OF_RECKONING_1, diff, false) && canHoly && victim && victim != me && Rand() < 92 && dist < 30 &&
                !targetTaunted && IsInBotParty(victim) &&
                (!IsTank(victim) || GetHealthPCT(victim) < 55 ||
                    (GetHealthPCT(me) > 62 &&
                        ((IsOffTank() && !IsOffTank(victim) && IsPointedOffTankingTarget(target)) ||
                            (!IsOffTank() && IsOffTank(victim) && IsPointedTankingTarget(target))))))
            {
                if (doCast(target, GetSpell(HAND_OF_RECKONING_1)))
                    return true;
            }

            // Righteous Defense: protect the party member being hit, especially non-tanks and low-health tanks.
            if (IsSpellReady(RIGHTEOUS_DEFENSE_1, diff, false) && !IAmFree() && victim && victim != me &&
                me->GetDistance(victim) < 40 && !targetTaunted && IsInBotParty(victim) &&
                (!IsTank(victim) || GetHealthPCT(victim) < 45) && Rand() < 60 + 15 * victim->getAttackers().size())
            {
                if (doCast(victim, GetSpell(RIGHTEOUS_DEFENSE_1)))
                    return true;
            }

            // Distant pickup when current target is already on the paladin. This is what makes
            // Protection feel like a tank instead of a well-armored tunnel-vision machine.
            if (!IAmFree() && (victim == me || !victim) && GetHealthPCT(me) > 35)
            {
                if (IsSpellReady(HAND_OF_RECKONING_1, diff, false) && canHoly && Rand() < 70)
                {
                    if (Unit* tUnit = FindDistantTauntTarget())
                        if (doCast(tUnit, GetSpell(HAND_OF_RECKONING_1)))
                            return true;
                }

                if (IsSpellReady(RIGHTEOUS_DEFENSE_1, diff, false) && Rand() < 70)
                {
                    if (Unit* tUnit = FindDistantTauntTarget(40, true))
                        if (doCast(tUnit, GetSpell(RIGHTEOUS_DEFENSE_1)))
                            return true;
                }
            }

            return false;
        }

        bool MaintainHolyTankSupport(uint32 diff)
        {
            if (GetSpec() != BOT_SPEC_PALADIN_HOLY || !HasRole(BOT_ROLE_HEAL) || IAmFree())
                return false;
            if (!master || !master->GetBotMgr())
                return false;
            if (holySupportTimer > diff || GC_Timer > diff || me->IsMounted() || Feasting() || IsCasting())
                return false;

            holySupportTimer = 1250;

            static constexpr float HOLY_SUPPORT_RANGE = 40.0f;
            static constexpr float PRE_PULL_HOSTILE_SCAN_RANGE = 50.0f;

            auto isSupportTank = [&](Unit const* member) -> bool
                {
                    return member && IsTank(member);
                };

            auto isPrePullHostile = [&](Unit const* candidate) -> bool
                {
                    if (!candidate || !candidate->IsAlive())
                        return false;
                    if (candidate == me || candidate == master)
                        return false;
                    if (!candidate->ToCreature())
                        return false;
                    if (candidate->IsNPCBot() || candidate->IsPet() || candidate->IsCritter() || candidate->IsControlledByPlayer())
                        return false;

                    return me->IsValidAttackTarget(candidate);
                };

            auto hasHostileNear = [&](Unit* origin) -> bool
                {
                    if (!origin || !origin->IsInWorld() || origin->FindMap() != me->GetMap())
                        return false;

                    Unit* nearby = origin->SelectNearbyTarget(nullptr, PRE_PULL_HOSTILE_SCAN_RANGE);
                    return isPrePullHostile(nearby);
                };

            bool keepExistingBeacon = GetSpell(BEACON_OF_LIGHT_1) &&
                FindAffectedTarget(GetSpell(BEACON_OF_LIGHT_1), me->GetGUID(), 80, 3);

            auto trySupportTank = [&](Unit* member) -> bool
                {
                    if (!member || member == me || !member->IsAlive() || me->GetMap() != member->FindMap())
                        return false;
                    if (member->isPossessed() || member->IsCharmed())
                        return false;
                    if (member->IsNPCBot() && member->ToCreature()->IsTempBot())
                        return false;
                    if (me->GetDistance(member) > HOLY_SUPPORT_RANGE)
                        return false;
                    if (!isSupportTank(member))
                        return false;

                    bool const partyInCombat = me->IsInCombat() || master->IsInCombat() || master->GetBotMgr()->IsPartyInCombat(false);
                    bool const tankUnderPressure = member->IsInCombat() || !member->getAttackers().empty();
                    bool const prePull = hasHostileNear(member) || hasHostileNear(master) || hasHostileNear(me);

                    if (!partyInCombat && !tankUnderPressure && !prePull)
                        return false;

                    // Beacon first. Holy paladins should not discover the tank exists only after the pull.
                    // But Beacon is exclusive per paladin: if one valid support tank already has *my* Beacon,
                    // do not bounce it to another tank every support tick.
                    if (uint32 BEACON_OF_LIGHT = GetSpell(BEACON_OF_LIGHT_1))
                    {
                        if (!keepExistingBeacon && IsSpellReady(BEACON_OF_LIGHT_1, diff) &&
                            !HasMyBeaconOfLightAura(member))
                        {
                            if (doCast(member, BEACON_OF_LIGHT))
                            {
                                keepExistingBeacon = true;
                                checkBeaconTimer = 5000;
                                holySupportTimer = 3000;
                                return true;
                            }
                        }
                    }

                    // Sacred Shield second. Do not overwrite an existing Sacred Shield-style aura.
                    if (uint32 SACRED_SHIELD = GetSpell(SACRED_SHIELD_1))
                    {
                        if (IsSpellReady(SACRED_SHIELD_1, diff) &&
                            !HasSacredShieldLikeAura(member))
                        {
                            if (doCast(member, SACRED_SHIELD))
                            {
                                // Safety throttle: if a custom rank fails to leave the expected aura
                                // immediately, do not machine-gun Sacred Shield every support tick.
                                holySupportTimer = 6000;
                                checkShieldTimer = 6000;
                                return true;
                            }
                        }
                    }

                    return false;
                };

            if (Group const* gr = master->GetGroup())
            {
                std::vector<Unit*> members = BotMgr::GetAllGroupMembers(gr);

                for (Unit const* member : members)
                {
                    if (member && member != me && member->IsInWorld() && member->IsAlive() &&
                        me->GetMap() == member->FindMap() && me->GetDistance(member) <= HOLY_SUPPORT_RANGE &&
                        isSupportTank(member) && HasMyBeaconOfLightAura(member))
                    {
                        keepExistingBeacon = true;
                        break;
                    }
                }

                for (Unit* member : members)
                    if (trySupportTank(member))
                        return true;
            }
            else
            {
                for (auto const& [_, bot] : *master->GetBotMgr()->GetBotMap())
                {
                    if (bot && bot != me && bot->IsInWorld() && bot->IsAlive() &&
                        me->GetMap() == bot->FindMap() && me->GetDistance(bot) <= HOLY_SUPPORT_RANGE &&
                        isSupportTank(bot) && HasMyBeaconOfLightAura(bot))
                    {
                        keepExistingBeacon = true;
                        break;
                    }
                }

                if (!keepExistingBeacon && master && master->IsInWorld() && master->IsAlive() &&
                    me->GetMap() == master->FindMap() && me->GetDistance(master) <= HOLY_SUPPORT_RANGE &&
                    isSupportTank(master) && HasMyBeaconOfLightAura(master))
                    keepExistingBeacon = true;

                for (auto const& [_, bot] : *master->GetBotMgr()->GetBotMap())
                    if (trySupportTank(bot))
                        return true;

                if (trySupportTank(master))
                    return true;
            }

            return false;
        }

        bool DoHolyActions(Unit* target, uint32 diff, bool canHoly, bool /*canNormal*/, float dist)
        {
            bool const urgentHealing = HasUrgentHealingNeed(55);
            bool const allowHolyDamage = IAmFree() || IsWanderer() || GetBG() || HasRole(BOT_ROLE_DPS) ||
                !me->getAttackers().empty() || !HasRole(BOT_ROLE_HEAL);
            bool const safeToSpendMana = !HasRole(BOT_ROLE_HEAL) || !urgentHealing || !me->getAttackers().empty() || GetBG() || IsWanderer();

            if (!allowHolyDamage && urgentHealing)
                return false;

            // Holy should still execute/defend itself, especially for BG and wandering bots.
            if (safeToSpendMana && TryHammerOfWrath(target, diff, canHoly, dist))
                return true;

            // Judgement is support for Holy because it maintains Judgements of the Pure.
            if (safeToSpendMana && TryJudgement(target, diff, canHoly, dist, GetManaPCT(me) < 55, true))
                return true;

            // Holy Shock can be offensive only when healing is stable or the paladin is personally threatened.
            if (safeToSpendMana && IsSpellReady(HOLY_SHOCK_1, diff, false) && canHoly && dist < 30 && Rand() < 65 &&
                (!HasRole(BOT_ROLE_HEAL) || !urgentHealing || !me->getAttackers().empty() || GetBG() || IsWanderer()))
            {
                if (doCast(target, GetSpell(HOLY_SHOCK_1)))
                    return true;
            }

            if (safeToSpendMana && IsSpellReady(EXORCISM_1, diff) && canHoly && dist < 30 && Rand() < 45 &&
                (GetBG() || IsWanderer() || !me->getAttackers().empty() || IsDurablePaladinTarget(target) ||
                    target->GetCreatureType() == CREATURE_TYPE_UNDEAD || target->GetCreatureType() == CREATURE_TYPE_DEMON))
            {
                if (doCast(target, GetSpell(EXORCISM_1)))
                    return true;
            }

            if (safeToSpendMana && IsSpellReady(CONSECRATION_1, diff) && canHoly && GetManaPCT(me) > 55 && dist < 5 && Rand() < 35 &&
                (me->getAttackers().size() > 1 || HasNearbyPaladinAOETargets(8.f, 3)))
            {
                if (doCast(me, GetSpell(CONSECRATION_1)))
                    return true;
            }

            if (IsSpellReady(HAMMER_OF_JUSTICE_1, diff) && !CCed(target) && dist < 10 && Rand() < 35 &&
                (!me->getAttackers().empty() || target->IsNonMeleeSpellCast(false, false, true)) &&
                target->GetDiminishing(DIMINISHING_STUN) <= DIMINISHING_LEVEL_2 &&
                !IsImmunedToMySpellEffect(target, sSpellMgr->GetSpellInfo(HAMMER_OF_JUSTICE_1), EFFECT_0))
            {
                if (doCast(target, GetSpell(HAMMER_OF_JUSTICE_1)))
                    return true;
            }

            return TryHolyWrath(target, diff);
        }

        bool DoProtectionActions(Unit* target, uint32 diff, bool canHoly, bool canNormal, float dist)
        {
            bool const durable = IsDurablePaladinTarget(target);
            bool const multiTarget = me->getAttackers().size() > 1 || HasNearbyPaladinAOETargets(8.f, 3);

            // Holy Shield is mitigation and threat; tanks should maintain it deliberately.
            if (IsSpellReady(HOLY_SHIELD_1, diff) && canHoly && CanBlock() && GetManaPCT(me) > 15 &&
                (me->IsInCombat() || !me->getAttackers().empty() || (target && target->GetVictim() == me)) &&
                !me->HasAuraTypeWithMiscvalue(SPELL_AURA_SCHOOL_IMMUNITY, 127))
            {
                if (doCast(me, GetSpell(HOLY_SHIELD_1)))
                    return true;
            }

            if (TryHammerOfWrath(target, diff, canHoly, dist))
                return true;

            TryAvengingWrath(target, diff, canHoly, dist); // off-GCD-style buff; keep going if it succeeds

            // Casters and loose pull targets get shielded in the face before the normal melee cadence.
            if (IsSpellReady(AVENGERS_SHIELD_1, diff) && canHoly && CanBlock() && dist < 30 && Rand() < 90 &&
                (dist > 8 || target->IsNonMeleeSpellCast(false, false, true) || (multiTarget && me->getAttackers().size() <= 1)))
            {
                if (doCast(target, GetSpell(AVENGERS_SHIELD_1)))
                    return true;
            }

            // Keep Judgements of the Just on serious targets; it is mitigation, not decoration.
            if (TargetNeedsJudgementsOfTheJust(target) && TryJudgement(target, diff, canHoly, dist, false, true))
                return true;

            if (IsSpellReady(SHIELD_OF_RIGHTEOUSNESS_1, diff) && canHoly && CanBlock() && dist < 5 && Rand() < 98)
            {
                if (doCast(target, GetSpell(SHIELD_OF_RIGHTEOUSNESS_1)))
                    return true;
            }

            if (IsSpellReady(HAMMER_OF_THE_RIGHTEOUS_1, diff) && canHoly && dist < 5 && Rand() < 98)
            {
                Item const* weapMH = GetEquips(BOT_SLOT_MAINHAND);
                if (weapMH &&
                    (weapMH->GetTemplate()->InventoryType == INVTYPE_WEAPON ||
                        weapMH->GetTemplate()->InventoryType == INVTYPE_WEAPONMAINHAND) &&
                    doCast(target, GetSpell(HAMMER_OF_THE_RIGHTEOUS_1)))
                    return true;
            }

            if (TryJudgement(target, diff, canHoly, dist, GetManaPCT(me) < 50, true))
                return true;

            if (IsSpellReady(CONSECRATION_1, diff) && canHoly && GetManaPCT(me) > (multiTarget ? 20 : 45) && dist < 5 &&
                Rand() < (multiTarget ? 90 : 35))
            {
                if ((multiTarget || (durable && !target->isMoving())) && doCast(me, GetSpell(CONSECRATION_1)))
                    return true;
            }

            if (TryProtectionEmergencyStun(target, diff, dist))
                return true;

            if (TryHolyWrath(target, diff))
                return true;

            if (IsSpellReady(AVENGERS_SHIELD_1, diff) && canHoly && CanBlock() && dist < 30 && Rand() < 55 &&
                (multiTarget || durable))
            {
                if (doCast(target, GetSpell(AVENGERS_SHIELD_1)))
                    return true;
            }

            if (IsSpellReady(EXORCISM_1, diff) && canHoly && dist < 30 && Rand() < 35 &&
                (dist > 12 || target->GetCreatureType() == CREATURE_TYPE_UNDEAD || target->GetCreatureType() == CREATURE_TYPE_DEMON))
            {
                if (doCast(target, GetSpell(EXORCISM_1)))
                    return true;
            }

            if (IsSpellReady(CRUSADER_STRIKE_1, diff) && canNormal && dist < 5 && Rand() < 60)
            {
                if (doCast(target, GetSpell(CRUSADER_STRIKE_1)))
                    return true;
            }

            return false;
        }

        bool DoRetributionActions(Unit* target, uint32 diff, bool canHoly, bool canNormal, float dist)
        {
            bool const durable = IsDurablePaladinTarget(target);
            bool const multiTarget = HasNearbyPaladinAOETargets(8.f, 3);

            if (TryHammerOfWrath(target, diff, canHoly, dist))
                return true;

            TryAvengingWrath(target, diff, canHoly, dist); // off-GCD-style buff; keep going if it succeeds

            if (TryJudgement(target, diff, canHoly, dist, GetManaPCT(me) < 35, true))
                return true;

            if (IsSpellReady(DIVINE_STORM_1, diff) && canNormal && dist < 7 && Rand() < (multiTarget ? 95 : 75))
            {
                if (doCast(me, GetSpell(DIVINE_STORM_1)))
                    return true;
            }

            if (IsSpellReady(CRUSADER_STRIKE_1, diff) && canNormal && dist < 5 && Rand() < 90)
            {
                if (doCast(target, GetSpell(CRUSADER_STRIKE_1)))
                    return true;
            }

            if (IsSpellReady(EXORCISM_1, diff) && canHoly && dist < 30 && Rand() < 75)
            {
                bool const instant = me->GetAuraEffect(SPELL_AURA_ADD_PCT_MODIFIER, SPELLFAMILY_PALADIN, 0x0, 0x0, 0x2);
                bool const rangedOrSpecial = dist > 10 || target->GetCreatureType() == CREATURE_TYPE_UNDEAD || target->GetCreatureType() == CREATURE_TYPE_DEMON;
                if ((instant || rangedOrSpecial || GetBG()) && doCast(target, GetSpell(EXORCISM_1)))
                    return true;
            }

            if (IsSpellReady(CONSECRATION_1, diff) && canHoly && GetManaPCT(me) > 45 && dist < 5 && !target->isMoving() && Rand() < 45 &&
                (multiTarget || durable))
            {
                if (doCast(me, GetSpell(CONSECRATION_1)))
                    return true;
            }

            if (TryHolyWrath(target, diff))
                return true;

            if (IsSpellReady(HAMMER_OF_JUSTICE_1, diff) && !CCed(target) && dist < 10 && Rand() < 20 &&
                target->GetDiminishing(DIMINISHING_STUN) <= DIMINISHING_LEVEL_2 &&
                !IsImmunedToMySpellEffect(target, sSpellMgr->GetSpellInfo(HAMMER_OF_JUSTICE_1), EFFECT_0))
            {
                if (doCast(target, GetSpell(HAMMER_OF_JUSTICE_1)))
                    return true;
            }

            return false;
        }

        bool DoDefaultPaladinActions(Unit* target, uint32 diff, bool canHoly, bool canNormal, float dist)
        {
            if (TryHammerOfWrath(target, diff, canHoly, dist))
                return true;
            if (TryJudgement(target, diff, canHoly, dist, GetManaPCT(me) < 35, true))
                return true;
            if (IsSpellReady(EXORCISM_1, diff) && canHoly && dist < 30 && Rand() < 55)
                if (doCast(target, GetSpell(EXORCISM_1)))
                    return true;
            if (IsSpellReady(CRUSADER_STRIKE_1, diff) && canNormal && dist < 5 && Rand() < 70)
                if (doCast(target, GetSpell(CRUSADER_STRIKE_1)))
                    return true;

            return false;
        }

        void DoNormalAttack(uint32 diff)
        {
            Unit* mytar = opponent ? opponent : disttarget ? disttarget : nullptr;
            if (!mytar)
                return;

            StartAttack(mytar, IsMelee());

            CheckAttackState();
            if (!me->IsAlive() || !mytar->IsAlive())
                return;

            MoveBehind(mytar);

            auto [can_do_holy, can_do_normal] = CanAffectVictimBools(mytar, SPELL_SCHOOL_HOLY, SPELL_SCHOOL_NORMAL);
            float dist = me->GetDistance(mytar);

            if (DoPaladinDefensives(mytar, diff))
                return;

            if (DoPaladinTaunts(mytar, diff, can_do_holy, dist))
                return;

            if (GetSpec() == BOT_SPEC_PALADIN_HOLY)
            {
                if (DoHolyActions(mytar, diff, can_do_holy, can_do_normal, dist))
                    return;
            }
            else if (GetSpec() == BOT_SPEC_PALADIN_PROTECTION)
            {
                if (DoProtectionActions(mytar, diff, can_do_holy, can_do_normal, dist))
                    return;
            }
            else if (GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION)
            {
                if (DoRetributionActions(mytar, diff, can_do_holy, can_do_normal, dist))
                    return;
            }
            else
            {
                if (DoDefaultPaladinActions(mytar, diff, can_do_holy, can_do_normal, dist))
                    return;
            }
        }

        void ApplyClassSpellCritMultiplierAll(Unit const* /*victim*/, float& crit_chance, SpellInfo const* spellInfo, SpellSchoolMask schoolMask, WeaponAttackType /*attackType*/) const override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            uint8 lvl = me->GetLevel();

            //Sanctified Light: 6% additional critical chance for Holy Light and Holy Shock
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 30 && (baseId == HOLY_LIGHT_1 || baseId == HOLY_SHOCK_1))
                crit_chance += 6.f;
            //Holy Power: 5% additional critical chance for Holy spells
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 35 && (schoolMask & SPELL_SCHOOL_MASK_HOLY))
                crit_chance += 5.f;
            //Improved Flash of Light (id: 20251): 6% additional critical chance for Flash of Light
            if (lvl >= 70 && baseId == FLASH_OF_LIGHT_1)
                crit_chance += 6.f;
            //Glyph of Flash of Light: 5% additional critical chance for Flash of Light
            if (lvl >= 20 && baseId == FLASH_OF_LIGHT_1)
                crit_chance += 5.f;
            //Sanctified Wrath: 50% additional critical chance for Hammer of Wrath
            if ((GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION) && lvl >= 45 && baseId == HAMMER_OF_WRATH_1)
                crit_chance += 50.f;
            //Fanaticism: 18% additional critical chance for all Judgements (not shure which check is right)
            if ((GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION) && lvl >= 45 && spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT)
                crit_chance += 18.f;
            //Infusion of Light
            if (baseId == HOLY_LIGHT_1)
            {
                if (AuraEffect const* infu = me->GetAuraEffect(INFUSION_OF_LIGHT_BUFF, 0))
                    if (infu->IsAffectedOnSpell(spellInfo))
                        crit_chance += 20.f;
            }
            if (baseId == HOLY_LIGHT_1 || baseId == FLASH_OF_LIGHT_1 || baseId == HOLY_SHOCK_1)
            {
                if (AuraEffect const* favo = me->GetAuraEffect(DIVINE_FAVOR_1, 0))
                    if (favo->IsAffectedOnSpell(spellInfo))
                        crit_chance += 100.f;
            }
        }

        void ApplyClassDamageMultiplierMeleeSpell(int32& damage, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* spellInfo, WeaponAttackType /*attackType*/, bool /*iscrit*/) const override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            uint8 lvl = me->GetLevel();
            float fdamage = float(damage);

            //apply bonus damage mods
            float pctbonus = 0.0f;
            //if (iscrit)
            //{
            //}
            //Sanctity of Battle: 15% bonus damage for Exorcism and Crusader Strike
            if ((GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION) && lvl >= 25 && baseId == EXORCISM_1)
                pctbonus += 0.15f;
            //The Art of War (damage part): 10% bonus damage for Judgements, Crusader Strike and Divine Storm
            if ((GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION) && lvl >= 40 &&
                (spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT || baseId == CRUSADER_STRIKE_1 || baseId == DIVINE_STORM_1))
                pctbonus += 0.1f;
            //Judgements of the Pure (damage part): 25% bonus damage for Judgements and Seals
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 50 &&
                (spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT ||
                    spellInfo->GetSpellSpecific() == SPELL_SPECIFIC_SEAL ||
                    baseId == JUDGEMENT_OF_COMMAND_DAMAGE))
                pctbonus += 0.25f;
            //Glyph of Exorcism: 20% bonus damage for Exorcism
            if (lvl >= 50 && baseId == EXORCISM_1)
                pctbonus += 0.2f;

            damage = int32(fdamage * (1.0f + pctbonus));
        }

        void ApplyClassDamageMultiplierSpell(int32& damage, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* spellInfo, WeaponAttackType /*attackType*/, bool /*iscrit*/) const override
        {
            uint32 spellId = spellInfo->Id;
            uint8 lvl = me->GetLevel();
            float fdamage = float(damage);

            //apply bonus damage mods
            float pctbonus = 0.0f;
            //if (iscrit)
            //{
            //}

            //Judgements of the Pure (damage part): 25% bonus damage for Judgements and Seals
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 50 &&
                (spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT ||
                    spellInfo->GetSpellSpecific() == SPELL_SPECIFIC_SEAL ||
                    spellId == JUDGEMENT_OF_COMMAND_DAMAGE))
                pctbonus += 0.25f;
            //Improved Consecration (id: 38422): 10% bonus damage for Consecration
            if (lvl >= 20 && spellId == GetSpell(CONSECRATION_1))
                pctbonus += 0.1f;

            damage = int32(fdamage * (1.0f + pctbonus));
        }

        void ApplyClassDamageMultiplierHeal(Unit const* /*victim*/, float& heal, SpellInfo const* spellInfo, DamageEffectType /*damagetype*/, uint32 /*stack*/) const override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            uint8 lvl = me->GetLevel();
            float pctbonus = 0.0f;
            float flat_mod = 0.0f;

            //Divine Plea: 50% reduced healing for all spells
            if (/*lvl >= 71 && */me->GetAuraEffect(SPELL_AURA_OBS_MOD_POWER, SPELLFAMILY_PALADIN, 0x0, 0x80004000, 0x1))
                pctbonus -= 0.5f;

            //Healing Light: 12% bonus healing for Holy Light, Flash of Light and Holy Shock
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 15 && (baseId == HOLY_LIGHT_1 || baseId == FLASH_OF_LIGHT_1 || baseId == HOLY_SHOCK_1))
                pctbonus += 0.12f;
            //Glyph of Seal of Light: 5% bonus healing for all spells
            if (lvl >= 30 && me->GetAuraEffect(SPELL_AURA_ADD_PCT_MODIFIER, SPELLFAMILY_PALADIN, 0x0, 0x2000000, 0x0))
                pctbonus += 0.05f;

            heal = heal * (1.0f + pctbonus) + flat_mod;
        }

        void ApplyClassSpellCostMods(SpellInfo const* spellInfo, int32& cost) const override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            uint8 lvl = me->GetLevel();
            float fcost = float(cost);
            int32 flatbonus = 0;
            float pctbonus = 0.0f;

            //percent mods
            //Benediction: -10% mana cost for Instant spells
            if (lvl >= 10 && !spellInfo->CalcCastTime())
                pctbonus += 0.1f;
            //Blessed Hands: -30% mana cost for Hand spells
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 25 && (spellInfo->SpellFamilyFlags[0] & 0x2110))
                pctbonus += 0.3f;
            //Holy Light Cost Reduction (id: 60148): -5% mana cost for Holy Light
            if (lvl >= 30 && baseId == HOLY_LIGHT_1)
                pctbonus += 0.05f;
            //Consecration Discount (id: 37180): -15% mana cost for Consecration
            if (lvl >= 30 && baseId == CONSECRATION_1)
                pctbonus += 0.15f;
            //Glyph of Seal of Wisdom: -5% mana cost for all healing spells (for bot it is all spells)
            if (lvl >= 15 && me->GetAuraEffect(SPELL_AURA_ADD_PCT_MODIFIER, SPELLFAMILY_PALADIN, 0x0, 0x4000000, 0x0))
                pctbonus += 0.05f;
            //Glyph of Shield of Righteous: -80% mana cost for Shield of Righteous
            if (lvl >= 75 && (spellInfo->SpellFamilyFlags[1] & 0x100000))
                pctbonus += 0.8f;

            //flat mods
            //Cleanse Cost Reduced (id: 27847): -25 mana cost for Cleanse
            if (lvl >= 40 && baseId == CLEANSE_1)
                flatbonus += 25;
            //Reduced Holy Light Cost (id: 37739): -34 mana cost for Holy Light
            if (lvl >= 40 && baseId == HOLY_LIGHT_1)
                flatbonus += 34;

            //cost can be < 0
            cost = int32(fcost * (1.0f - pctbonus)) - flatbonus;
        }

        void ApplyClassSpellCastTimeMods(SpellInfo const* spellInfo, int32& casttime) const override
        {
            //casttime is in milliseconds
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            uint8 lvl = me->GetLevel();
            int32 timebonus = 0;
            //float pctbonus = 0.0f;

            //100% mods
            //Glyph of Turn Evil: -100% cast time for Turn Evil
            if (lvl >= 24 && baseId == TURN_EVIL_1)
                timebonus += casttime;
            if (baseId == FLASH_OF_LIGHT_1 || baseId == EXORCISM_1)
            {
                //The Art of War
                AuraEffect const* arto = me->GetAuraEffect(THE_ART_OF_WAR_BUFF, 0);
                //Infusion of Light
                AuraEffect const* infu = me->GetAuraEffect(INFUSION_OF_LIGHT_BUFF, 1);
                if (arto && arto->IsAffectedOnSpell(spellInfo))
                    timebonus += casttime;
                else if (infu && infu->IsAffectedOnSpell(spellInfo))
                    timebonus += casttime;
            }

            //flat mods
            //Improved Holy Light (id: 24457): -0.1 sec cast time for Holy Light
            if (lvl >= 40 && baseId == HOLY_LIGHT_1)
                timebonus += 100;
            //Recuced Holy Light Cast Time (id: 37189): -0.5 sec cast time for Holy Light (works only for healers)
            //Light's Grace: -0.5 sec cast time for Holy Light
            if (baseId == HOLY_LIGHT_1)
            {
                if (AuraEffect const* enli = me->GetAuraEffect(ENLIGHTENMENT_BUFF, 0))
                    if (enli->IsAffectedOnSpell(spellInfo))
                        timebonus += 500;
                if (AuraEffect const* grac = me->GetAuraEffect(LIGHTS_GRACE_BUFF, 0))
                    if (grac->IsAffectedOnSpell(spellInfo))
                        timebonus += 500;
            }

            casttime = std::max<int32>(casttime - timebonus, 0);
        }

        void ApplyClassSpellNotLoseCastTimeMods(SpellInfo const* spellInfo, int32& delayReduce) const override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchoolMask schools = spellInfo->GetSchoolMask();
            uint8 lvl = me->GetLevel();
            int32 reduceBonus = 0;

            if (lvl >= 10 && (baseId == HOLY_LIGHT_1 || baseId == FLASH_OF_LIGHT_1))
                reduceBonus += 70;

            delayReduce += reduceBonus;
        }

        void ApplyClassSpellCooldownMods(SpellInfo const* spellInfo, uint32& cooldown) const override
        {
            //cooldown is in milliseconds
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            uint8 lvl = me->GetLevel();
            int32 timebonus = 0;
            float pctbonus = 0.0f;

            //pct mods

            //flat mods
            //Improved Judgements: -2 sec cooldown for judgements
            //Judgment Cooldown Reduction (60153): -1 sec cooldown for judgements
            //Judgement Cooldown Reduction (61776): -1 sec cooldown for judgements
            if (spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT)
            {
                if (lvl >= 70)
                    timebonus += 4000;
                else if (lvl >= 60)
                    timebonus += 3000;
                else if (lvl >= 15)
                    timebonus += 2000;
            }
            //Sacred Duty: -60 sec cooldown for Divine Shield and Divine Protection
            if ((GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && lvl >= 35 && (baseId == DIVINE_SHIELD_1 || baseId == DIVINE_PROTECTION_1))
                timebonus += 60000;
            //Reduced Righteous Defense Cooldown (37181): -2 sec cooldown for Righteous Defense
            if (lvl >= 60 && baseId == RIGHTEOUS_DEFENSE_1)
                timebonus += 2000;
            //Paladin T9 Tank 2P Bonus part 1: -2 sec cooldown for Hand of Reckoning
            if (lvl >= 78 && baseId == HAND_OF_RECKONING_1)
                timebonus += 2000;
            //Glyph of Turn Evil: +8 sec cooldown for Turn Evil
            if (lvl >= 24 && baseId == TURN_EVIL_1)
                timebonus -= 8000;

            cooldown = std::max<int32>((float(cooldown) * (1.0f - pctbonus)) - timebonus, 0);
        }

        void ApplyClassSpellCategoryCooldownMods(SpellInfo const* spellInfo, uint32& cooldown) const override
        {
            //cooldown is in milliseconds
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            uint8 lvl = me->GetLevel();
            int32 timebonus = 0;
            float pctbonus = 0.0f;

            //pct mods
            //Purifying Power part 2: -33% cooldown for Exorcism and Holy Wrath
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 35 && (baseId == EXORCISM_1 || baseId == HOLY_WRATH_1))
                pctbonus += 0.333f;
            //Glyph of Avenging Wrath: -50% cooldown for Hammer of Wrath if Avenging Wrath is active
            if (lvl >= 70 && baseId == HAMMER_OF_WRATH_1 &&
                me->GetAuraEffect(SPELL_AURA_MOD_HEALING_DONE_PERCENT, SPELLFAMILY_PALADIN, 0x0, 0x2000, 0x0))
                pctbonus += 0.5f;

            //flat mods
            //Improved Judgements: -2 sec cooldown for judgements
            //Judgment Cooldown Reduction (60153): -1 sec cooldown for judgements
            //Judgement Cooldown Reduction (61776): -1 sec cooldown for judgements
            if (spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT)
            {
                if (lvl >= 70)
                    timebonus += 4000;
                else if (lvl >= 60)
                    timebonus += 3000;
                else if (lvl >= 15)
                    timebonus += 2000;
            }
            //Guardian's Favor part 1: -120 sec cooldown for Hand of Protection
            if (lvl >= 15 && baseId == HAND_OF_PROTECTION_1)
                timebonus += 120000;
            //Improved Hammer of Justice: -20 sec cooldown for Hammer of Justice
            if ((GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && lvl >= 25 && baseId == HAMMER_OF_JUSTICE_1)
                timebonus += 20000;
            //Judgements of the Just: -10 sec cooldown for Hammer of Justice (tanks only)
            if ((GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && lvl >= 55 && baseId == HAMMER_OF_JUSTICE_1)
                timebonus += 10000;
            //Glyph of Holy Shock: -1 sec cooldown for Holy Shock
            if (baseId == HOLY_SHOCK_1)
                timebonus += 1000;
            //Glyph of Consecration: +2 sec cooldown for Consecration
            if (lvl >= 20 && baseId == CONSECRATION_1)
                timebonus -= 2000;
            //Glyph of Holy Wrath: -15 sec cooldown for Holy Wrath
            if (lvl >= 50 && baseId == HOLY_WRATH_1)
                timebonus += 15000;
            //Improved Lay on Hands (part 2): -4 min cooldown for Lay on Hands
            if (lvl >= 20 && baseId == LAY_ON_HANDS_1)
                timebonus += 240000;
            //Glyph of Lay on Hands: -5 min cooldown for Lay on Hands (only healers)
            if (lvl >= 15 && HasRole(BOT_ROLE_HEAL) && baseId == LAY_ON_HANDS_1)
                timebonus += 300000;
            //Lay Hands (id: 28774): -4 min cooldown for Lay on Hands (only healers)
            if (lvl >= 60 && HasRole(BOT_ROLE_HEAL) && baseId == LAY_ON_HANDS_1)
                timebonus += 240000;

            cooldown = std::max<int32>((float(cooldown) * (1.0f - pctbonus)) - timebonus, 0);
        }

        void ApplyClassSpellGlobalCooldownMods(SpellInfo const* /*spellInfo*/, float& cooldown) const override
        {
            //cooldown is in milliseconds
            //uint32 spellId = spellInfo->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            //uint8 lvl = me->GetLevel();
            float timebonus = 0.0f;
            float pctbonus = 0.0f;

            ////Unrelenting Assault (part 1, special): -0.5 sec global cooldown for Overpower and Revenge (not for tanks)
            //if (lvl >= 50 && !IsTank() && (spellId == GetSpell(OVERPOWER_1) || spellId == GetSpell(REVENGE_1)))
            //    timebonus += 500.f;

            cooldown = (cooldown * (1.0f - pctbonus)) - timebonus;
        }

        void ApplyClassSpellRadiusMods(SpellInfo const* spellInfo, float& radius) const override
        {
            //uint32 spellId = spellInfo->Id;
            //uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            uint8 lvl = me->GetLevel();
            float flatbonus = 0.0f;
            float pctbonus = 0.0f;

            //pct mods
            ////Holy Reach
            //if (lvl >= 25 && ((spellInfo->SpellFamilyFlags[0] & 0x18400200) || (spellInfo->SpellFamilyFlags[2] & 0x4)))
            //    pctbonus += 0.2f;

            //flat mods
            //Increased Aura Radii (23565)
            if (lvl >= 40 && (spellInfo->SpellFamilyFlags[0] & 0x4020048))
                flatbonus += 10.f;

            radius = radius * (1.0f + pctbonus) + flatbonus;
        }

        void ApplyClassSpellRangeMods(SpellInfo const* spellInfo, float& maxrange) const override
        {
            //uint32 spellId = spellInfo->Id;
            //uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            //SpellSchool school = GetFirstSchoolInMask(spellInfo->GetSchoolMask());
            uint8 lvl = me->GetLevel();
            float flatbonus = 0.0f;
            float pctbonus = 0.0f;

            //pct mods
            //Booming Voice
            //if (lvl >= 10 && ((spellInfo->SpellFamilyFlags[0] & 0x30000) || (spellInfo->SpellFamilyFlags[1] & 0x80)))
            //    pctbonus += 1.0f;

            //flat mods
            //Enlightened Judgements: +30 yd range for Judgement of Light and Judgement of Wisdom (healers)
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 55 && (spellInfo->SpellFamilyFlags[0] & 0x800000))
                flatbonus += 30.f;

            maxrange = maxrange * (1.0f + pctbonus) + flatbonus;
        }

        void ApplyClassSpellMaxTargetsMods(SpellInfo const* spellInfo, uint32& targets) const override
        {
            uint32 bonusTargets = 0;

            //Glyph of Hammer of the Righteous: +1 target
            if (spellInfo->SpellFamilyFlags[1] & 0x40000)
                bonusTargets += 1;

            targets = targets + bonusTargets;
        }

        void ApplyClassEffectMods(SpellInfo const* spellInfo, uint8 effIndex, float& value) const override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;
            uint8 lvl = me->GetLevel();
            float pctbonus = 1.0f;

            //Improved Devotion Aura: 50% increased effect
            if (baseId == DEVOTION_AURA_1 && effIndex == EFFECT_0 && GetSpec() == BOT_SPEC_PALADIN_PROTECTION && lvl >= 25)
                pctbonus *= 1.5f;
            //Improved Devotion Aura: 6% bonus healing
            if (baseId == IMPROVED_DEVOTION_AURA_SPELL && effIndex == EFFECT_1 && GetSpec() == BOT_SPEC_PALADIN_PROTECTION && lvl >= 25)
                value += 6.f;

            value = value * pctbonus;
        }

        void ApplyClassThreatMods(SpellInfo const* spellInfo, float& threat) const override
        {
            if (!spellInfo || GetSpec() != BOT_SPEC_PALADIN_PROTECTION || !IsTank())
                return;

            uint32 const baseId = spellInfo->GetFirstRankSpell()->Id;
            bool const coreTankThreat = spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT ||
                baseId == SHIELD_OF_RIGHTEOUSNESS_1 || baseId == HAMMER_OF_THE_RIGHTEOUS_1 ||
                baseId == AVENGERS_SHIELD_1 || baseId == CONSECRATION_1 || baseId == HOLY_SHIELD_1 ||
                baseId == HOLY_WRATH_1 || baseId == HAMMER_OF_WRATH_1;

            if (!coreTankThreat)
                return;

            // Righteous Fury does the heavy lifting; this is a small Protection-only hardening layer
            // so a tank bot keeps mobs glued while still leaving the global tank-threat config meaningful.
            threat *= 1.25f;

            if (me->GetLevel() >= 55 &&
                (baseId == SHIELD_OF_RIGHTEOUSNESS_1 || baseId == HAMMER_OF_THE_RIGHTEOUS_1 || spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT))
                threat *= 1.10f;
        }

        void OnClassSpellGo(SpellInfo const* spellInfo) override
        {
            uint32 baseId = spellInfo->GetFirstRankSpell()->Id;

            if (baseId == HOLY_LIGHT_1 || baseId == FLASH_OF_LIGHT_1 || baseId == HOLY_SHOCK_1)
            {
                if (AuraEffect const* favo = me->GetAuraEffect(DIVINE_FAVOR_1, 0))
                    if (favo->IsAffectedOnSpell(spellInfo))
                        me->RemoveAurasDueToSpell(DIVINE_FAVOR_1);
            }

            if (baseId == HOLY_LIGHT_1)
            {
                if (AuraEffect const* enli = me->GetAuraEffect(ENLIGHTENMENT_BUFF, 0))
                    if (enli->IsAffectedOnSpell(spellInfo))
                        me->RemoveAurasDueToSpell(ENLIGHTENMENT_BUFF);
                if (AuraEffect const* grac = me->GetAuraEffect(LIGHTS_GRACE_BUFF, 0))
                    if (grac->IsAffectedOnSpell(spellInfo))
                        me->RemoveAurasDueToSpell(LIGHTS_GRACE_BUFF);
                if (AuraEffect const* infu = me->GetAuraEffect(INFUSION_OF_LIGHT_BUFF, 0))
                    if (infu->IsAffectedOnSpell(spellInfo))
                        me->RemoveAurasDueToSpell(INFUSION_OF_LIGHT_BUFF);
            }

            if (baseId == EXORCISM_1 || baseId == FLASH_OF_LIGHT_1)
            {
                //Infusion of Light takes priority since AoW affects Exorcism too
                AuraEffect const* infu = me->GetAuraEffect(INFUSION_OF_LIGHT_BUFF, 1);
                //The Art of War
                AuraEffect const* arto = me->GetAuraEffect(THE_ART_OF_WAR_BUFF, 0);
                if (arto && arto->IsAffectedOnSpell(spellInfo))
                    me->RemoveAurasDueToSpell(THE_ART_OF_WAR_BUFF);
                else if (infu && infu->IsAffectedOnSpell(spellInfo))
                    me->RemoveAurasDueToSpell(INFUSION_OF_LIGHT_BUFF);
            }

            if (baseId == DIVINE_SACRIFICE_1)
            {
                _sacDamage = 0;
            }
        }

        void SpellHitTarget(Unit* wtarget, SpellInfo const* spell) override
        {
            Unit* target = wtarget->ToUnit();
            if (!target)
                return;

            uint32 spellId = spell->Id;
            uint32 baseId = spell->GetFirstRankSpell()->Id;
            uint8 lvl = me->GetLevel();

            //Glyph of Beacon of Light: 30 sec increased duration
            if (baseId == BEACON_OF_LIGHT_1)
            {
                Aura* beac = target->GetAura(spellId, me->GetGUID());
                if (beac)
                {
                    uint32 dur = beac->GetDuration() + 30000;
                    beac->SetDuration(dur);
                    beac->SetMaxDuration(dur);
                }
            }
            //Judgements of the Just melee attack speed reduction part 1
            if ((GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && lvl >= 55 && spell->GetCategory() == SPELLCATEGORY_JUDGEMENT)
            {
                me->CastSpell(target, JUDGEMENTS_OF_THE_JUST_AURA, true);
            }
            //Judgements of the Just melee attack speed reduction part 2
            if ((GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && spellId == JUDGEMENTS_OF_THE_JUST_AURA)
            {
                AuraEffect* slow = target->GetAuraEffect(JUDGEMENTS_OF_THE_JUST_AURA, 1, me->GetGUID());
                if (slow)
                    slow->ChangeAmount(slow->GetAmount() - 20);
            }

            if ((GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && spellId == SEAL_OF_JUSTICE_STUN_AURA)
            {
                if (lvl >= 55)
                {
                    //Judgements of the Just: 1 sec increased duration
                    Aura* stun = target->GetAura(spellId, me->GetGUID());
                    if (stun)
                    {
                        uint32 dur = stun->GetDuration() + 1000;
                        stun->SetDuration(dur);
                        stun->SetMaxDuration(dur);
                    }
                }
            }
            if (baseId == CONSECRATION_1)
            {
                if (lvl >= 30)
                {
                    //Glyph of Consecration: 2 sec increased duration
                    Aura* cons = target->GetAura(spellId, me->GetGUID());
                    if (cons)
                    {
                        uint32 dur = cons->GetDuration() + 2000;
                        cons->SetDuration(dur);
                        cons->SetMaxDuration(dur);
                    }
                }
            }
            if ((GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION) && baseId == RETRIBUTION_AURA_1)
            {
                if (lvl >= 30)
                {
                    //Sanctified Retribution: 50% increased effect
                    AuraEffect* eff = target->GetAuraEffect(spellId, EFFECT_0, me->GetGUID());
                    if (eff)
                        eff->ChangeAmount(eff->GetAmount() * 3 / 2);
                }
            }
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && baseId == CONCENTRATION_AURA_1)
            {
                if (lvl >= 25)
                {
                    //Improved Concentration Aura: 15% increased effect (flat)
                    AuraEffect* eff = target->GetAuraEffect(spellId, EFFECT_0, me->GetGUID());
                    if (eff)
                        eff->ChangeAmount(eff->GetAmount() + 15); //base = 35, bonus = 15
                }
            }
            if (baseId == FLASH_OF_LIGHT_HEAL_PERIODIC)
            {
                if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && lvl >= 78 && !HasRole(BOT_ROLE_TANK | BOT_ROLE_DPS))
                {
                    //Paldin T9 Holy 4P Bonus: 100% increased healing from Infusion of Light (pure healers only)
                    AuraEffect* eff = target->GetAuraEffect(spellId, EFFECT_0, me->GetGUID());
                    if (eff)
                        eff->ChangeAmount(eff->GetAmount() * 2);
                }
            }
            if (baseId == BLESSING_OF_WISDOM_1)
            {
                if (lvl >= 25)
                {
                    //Improved Blessing of Wisdom: 20% increased effect
                    AuraEffect* eff = target->GetAuraEffect(spellId, EFFECT_0, me->GetGUID());
                    if (eff)
                        eff->ChangeAmount(eff->GetAmount() * 6 / 5);
                }
            }
            if (baseId == BLESSING_OF_MIGHT_1)
            {
                if (lvl >= 15)
                {
                    //Improved Blessing of Might: 25% increased effect
                    if (Aura* migh = target->GetAura(spellId, me->GetGUID()))
                        for (auto i : NPCBots::index_array<uint8, EFFECT_2>) // 2 effects
                            if (AuraEffect* eff = migh->GetEffect(i))
                                eff->ChangeAmount((eff->GetAmount() * 125) / 100);
                }
            }
            if (baseId == HAND_OF_FREEDOM_1)
            {
                //Guardian's Favor part 2 (handled separately)
                if (Aura* hof = target->GetAura(spellId, me->GetGUID()))
                {
                    uint32 dur = hof->GetDuration() + 4000;
                    hof->SetDuration(dur);
                    hof->SetMaxDuration(dur);
                }
            }
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && baseId == HAND_OF_SALVATION_1 && !IsTank(target))
            {
                //Blessed Hands (part 2)
                if (AuraEffect* hos = target->GetAuraEffect(spellId, 0, me->GetGUID()))
                {
                    hos->ChangeAmount(hos->GetAmount() * 2);
                }
            }
            if ((GetSpec() == BOT_SPEC_PALADIN_HOLY) && baseId == HAND_OF_SACRIFICE_1)
            {
                //Blessed Hands (part 3)
                if (AuraEffect* hos = target->GetAuraEffect(spellId, 0, me->GetGUID()))
                {
                    hos->ChangeAmount(hos->GetAmount() + 10);
                }
            }
            if (baseId == BLESSING_OF_KINGS_1 || baseId == BLESSING_OF_MIGHT_1 ||
                baseId == BLESSING_OF_WISDOM_1 || baseId == BLESSING_OF_SANCTUARY_1)
            {
                //Blessings duration 1h
                if (Aura* bless = target->GetAura(spellId, me->GetGUID()))
                {
                    uint32 dur = HOUR * IN_MILLISECONDS;
                    bless->SetDuration(dur);
                    bless->SetMaxDuration(dur);
                }
            }
            if (baseId == SACRED_SHIELD_1 ||
                baseId == SACRED_SHIELD_AURA_R1 || baseId == SACRED_SHIELD_AURA_R2 || baseId == SACRED_SHIELD_AURA_R3)
            {
                // Choose the expected aura base for the paladin's level
                uint32 expectedAuraBase =
                    (lvl >= 80) ? SACRED_SHIELD_AURA_R3 :
                    (lvl >= 68) ? SACRED_SHIELD_AURA_R2 :
                    SACRED_SHIELD_AURA_R1;

                // Try to grab whatever rank just landed, and fall back to our expected base
                Aura* shi = target->GetAura(spellId, me->GetGUID());
                if (!shi)
                    shi = target->GetAura(expectedAuraBase, me->GetGUID());

                if (shi)
                {
                    // Divine Guardian (part 2): +100% duration for every Sacred Shield rank
                    uint32 dur = shi->GetDuration() * 2;
                    shi->SetDuration(dur);
                    shi->SetMaxDuration(dur);

                    // And +20% absorb when the *triggered Sacred Shield aura* is what's being processed,
                    // regardless of which rank (R1/R2/R3) is active.
                    if (baseId == SACRED_SHIELD_AURA_R1 ||
                        baseId == SACRED_SHIELD_AURA_R2 ||
                        baseId == SACRED_SHIELD_AURA_R3)
                    {
                        if (AuraEffect* eff = shi->GetEffect(EFFECT_0))
                            eff->ChangeAmount(eff->GetAmount() * 6 / 5);
                    }
                }
            }

            OnSpellHitTarget(target, spell);
        }

        void SpellHit(Unit* wcaster, SpellInfo const* spell) override
        {
            Unit* caster = wcaster->ToUnit();
            if (!caster)
                return;

            uint32 baseId = spell->GetFirstRankSpell()->Id;

            //Glyph of Seal of Vengeance
            if (baseId == SEAL_OF_VENGEANCE_1 || baseId == SEAL_OF_CORRUPTION_1)
            {
                AuraEffect* sea = me->GetAuraEffect(spell->Id, 1);
                if (sea)
                    sea->ChangeAmount(sea->GetAmount() + 10);
            }

            //Aura Helper
            if (caster == me)
            {
                switch (baseId)
                {
                case DEVOTION_AURA_1:
                case CONCENTRATION_AURA_1:
                case FIRE_RESISTANCE_AURA_1:
                case FROST_RESISTANCE_AURA_1:
                case SHADOW_RESISTANCE_AURA_1:
                case RETRIBUTION_AURA_1:
                case CRUSADER_AURA_1:
                    SetAIMiscValue(BOTAI_MISC_AURA_TYPE, baseId);
                    break;
                default:
                    break;
                }
            }

            //immunity markers
            if (baseId == AVENGING_WRATH_MARKER_SPELL)
                avDelayTimer = 30000;
            if (baseId == IMMUNITY_SHIELD_MARKER_SPELL)
                shieldDelayTimer = 30000;

            OnSpellHit(caster, spell);
        }

        void DamageDealt(Unit* victim, uint32& damage, DamageEffectType damageType, SpellSchoolMask damageSchoolMask) override
        {
            bot_ai::DamageDealt(victim, damage, damageType, damageSchoolMask);
        }

        void OnBotDamageTaken(Unit* /*attacker*/, uint32 damage, CleanDamage const* /*cleanDamage*/, DamageEffectType /*damagetype*/, SpellInfo const* spellInfo) override
        {
            // Divine Sacrifice helper - calculate remaining damage amount and find if we can be one-shot'ed
            if (damage && _sacDamage < int32(me->GetMaxHealth() / 4))
            {
                if (spellInfo && spellInfo->Id == DIVINE_SACRIFICE_1)
                    _sacDamage -= int32(damage);
                else
                    _sacDamage += int32(damage);

                if (me->GetHealth() - _sacDamage < me->GetMaxHealth() / 5)
                {
                    if (me->GetAuraEffect(SPELL_AURA_SPLIT_DAMAGE_PCT, SPELLFAMILY_PALADIN, 0x0, 0x0, 0x4, me->GetGUID()))
                    {
                        _sacDamage = me->GetMaxHealth();
                        me->RemoveAurasDueToSpell(DIVINE_SACRIFICE_1, me->GetGUID());
                    }
                }
            }
        }

        void DamageTaken(Unit* u, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellSchoolMask /*schoolMask*/) override
        {
            if (!u)
                return;
            if (!u->IsInCombat() && !me->IsInCombat())
                return;
            OnOwnerDamagedBy(u);
        }

        //healer may be nullptr
        void HealReceived(Unit* healer, uint32& heal) override
        {
            //Spiritual Attunement (double the effect on bots)
            if (heal && (GetSpec() == BOT_SPEC_PALADIN_PROTECTION) && me->GetLevel() >= 40 && healer != me && GetLostHP(me))
            {
                if (int32 basepoints = int32(CalculatePct(std::min<int32>(heal, GetLostHP(me)), 20)))
                {
                    //CastSpellExtraArgs args(true);
                    //args.AddSpellBP0(basepoints);
                    //me->CastSpell(me, SPIRITUAL_ATTUNEMENT_ENERGIZE, args);
                    me->CastCustomSpell(me, SPIRITUAL_ATTUNEMENT_ENERGIZE, &basepoints, nullptr, nullptr, true);
                }
            }

            //bot_ai::HealReceived(healer, heal);
        }

        void OwnerAttackedBy(Unit* u) override
        {
            if (!u)
                return;
            OnOwnerDamagedBy(u);
        }

        float GetSpellAttackRange(bool longRange) const override
        {
            return longRange ? CalcSpellMaxRange(GetSpell(EXORCISM_1) ? EXORCISM_1 : JUDGEMENT_OF_LIGHT_1) : 10.f;
        }

        uint32 GetAIMiscValue(uint32 data) const override
        {
            switch (data)
            {
            case BOTAI_MISC_AURA_TYPE:
                return _myaura;
            default:
                return 0;
            }
        }

        void SetAIMiscValue(uint32 data, uint32 value) override
        {
            switch (data)
            {
            case BOTAI_MISC_AURA_TYPE:
                _myaura = value;
                break;
            default:
                break;
            }

            bot_ai::SetAIMiscValue(data, value);
        }

        void Reset() override
        {
            checkAuraTimer = 0;
            checkSealTimer = 0;
            checkShieldTimer = 0;
            checkBeaconTimer = 0;
            holySupportTimer = 0;
            avDelayTimer = 0;
            shieldDelayTimer = 0;
            _sacDamage = 0;

            CLEANSE = 0;

            DefaultInit();
        }

        void ReduceCD(uint32 diff) override
        {
            if (checkAuraTimer > diff)              checkAuraTimer -= diff;
            if (checkSealTimer > diff)              checkSealTimer -= diff;
            if (checkShieldTimer > diff)            checkShieldTimer -= diff;
            if (checkBeaconTimer > diff)            checkBeaconTimer -= diff;
            if (holySupportTimer > diff)            holySupportTimer -= diff;
            if (avDelayTimer > diff)                avDelayTimer -= diff;
            if (shieldDelayTimer > diff)            shieldDelayTimer -= diff;
        }

        void InitPowers() override
        {
            me->SetPowerType(POWER_MANA);
        }

        void InitSpells() override
        {
            uint8 lvl = me->GetLevel();
            bool isHoly = GetSpec() == BOT_SPEC_PALADIN_HOLY;
            bool isProt = GetSpec() == BOT_SPEC_PALADIN_PROTECTION;
            bool isRetr = GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION;

            InitSpellMap(FLASH_OF_LIGHT_1);
            InitSpellMap(HOLY_LIGHT_1);
            InitSpellMap(LAY_ON_HANDS_1);
            InitSpellMap(SACRED_SHIELD_1);
            InitSpellMap(REDEMPTION_1);
            InitSpellMap(HAMMER_OF_JUSTICE_1);
            InitSpellMap(TURN_EVIL_1);
            InitSpellMap(HOLY_WRATH_1);
            InitSpellMap(EXORCISM_1);
            InitSpellMap(JUDGEMENT_OF_LIGHT_1);
            InitSpellMap(JUDGEMENT_OF_WISDOM_1);
            InitSpellMap(JUDGEMENT_OF_JUSTICE_1);
            InitSpellMap(CONSECRATION_1);
            InitSpellMap(HAMMER_OF_WRATH_1);
            InitSpellMap(AVENGING_WRATH_1);
            InitSpellMap(RIGHTEOUS_FURY_1);
            InitSpellMap(SHIELD_OF_RIGHTEOUSNESS_1);
            InitSpellMap(BLESSING_OF_MIGHT_1);
            InitSpellMap(BLESSING_OF_WISDOM_1);
            InitSpellMap(BLESSING_OF_KINGS_1);
            InitSpellMap(DEVOTION_AURA_1);
            InitSpellMap(CONCENTRATION_AURA_1);
            InitSpellMap(FIRE_RESISTANCE_AURA_1);
            InitSpellMap(FROST_RESISTANCE_AURA_1);
            InitSpellMap(SHADOW_RESISTANCE_AURA_1);
            InitSpellMap(RETRIBUTION_AURA_1);
            InitSpellMap(CRUSADER_AURA_1);
            InitSpellMap(DIVINE_PLEA_1);
            InitSpellMap(HAND_OF_PROTECTION_1);
            InitSpellMap(HAND_OF_FREEDOM_1);
            InitSpellMap(HAND_OF_SALVATION_1);
            InitSpellMap(HAND_OF_SACRIFICE_1);
            InitSpellMap(HAND_OF_RECKONING_1);
            InitSpellMap(RIGHTEOUS_DEFENSE_1);
            //InitSpellMap(PURIFY_1);
            //InitSpellMap(CLEANSE_1);
            InitSpellMap(SEAL_OF_LIGHT_1);
            InitSpellMap(SEAL_OF_RIGHTEOUSNESS_1);
            InitSpellMap(SEAL_OF_WISDOM_1);
            InitSpellMap(SEAL_OF_JUSTICE_1);
            InitSpellMap((me->GetRaceMask() & sRaceMgr->GetAllianceRaceMask()) ? SEAL_OF_VENGEANCE_1 : SEAL_OF_CORRUPTION_1);
            InitSpellMap(DIVINE_INTERVENTION_1);
            InitSpellMap(DIVINE_PROTECTION_1);
            InitSpellMap(DIVINE_SHIELD_1);

            /*Talent*/lvl >= (isHoly ? 20 : 70) ? InitSpellMap(AURA_MASTERY_1) : RemoveSpell(AURA_MASTERY_1);
            /*Talent*/lvl >= 30 && isHoly ? InitSpellMap(DIVINE_FAVOR_1) : RemoveSpell(DIVINE_FAVOR_1);
            /*Talent*/lvl >= 40 && isHoly ? InitSpellMap(HOLY_SHOCK_1) : RemoveSpell(HOLY_SHOCK_1);
            /*Talent*/lvl >= 50 && isHoly ? InitSpellMap(DIVINE_ILLUMINATION_1) : RemoveSpell(DIVINE_ILLUMINATION_1);
            /*Talent*/lvl >= 60 && isHoly ? InitSpellMap(BEACON_OF_LIGHT_1) : RemoveSpell(BEACON_OF_LIGHT_1);

            /*Talent*/lvl >= (isProt ? 20 : isHoly ? 70 : 99) ? InitSpellMap(DIVINE_SACRIFICE_1) : RemoveSpell(DIVINE_SACRIFICE_1);
            /*Talent*/lvl >= 30 && isProt ? InitSpellMap(BLESSING_OF_SANCTUARY_1) : RemoveSpell(BLESSING_OF_SANCTUARY_1);
            /*Talent*/lvl >= 40 && isProt ? InitSpellMap(HOLY_SHIELD_1) : RemoveSpell(HOLY_SHIELD_1);
            /*Talent*/lvl >= 45 && isProt ? InitSpellMap(AVENGERS_SHIELD_1) : RemoveSpell(AVENGERS_SHIELD_1);
            /*Talent*/lvl >= 50 && isProt ? InitSpellMap(HAMMER_OF_THE_RIGHTEOUS_1) : RemoveSpell(HAMMER_OF_THE_RIGHTEOUS_1);

            /*Talent*/lvl >= 20 && isRetr ? InitSpellMap(SEAL_OF_COMMAND_1) : RemoveSpell(SEAL_OF_COMMAND_1);
            /*Talent*/lvl >= 40 && isRetr ? InitSpellMap(REPENTANCE_1) : RemoveSpell(REPENTANCE_1);
            /*Talent*/lvl >= 10 && isRetr ? InitSpellMap(CRUSADER_STRIKE_1) : RemoveSpell(CRUSADER_STRIKE_1);
            /*Talent*/lvl >= 50 && isRetr ? InitSpellMap(DIVINE_STORM_1) : RemoveSpell(DIVINE_STORM_1);

            CLEANSE = InitSpell(me, CLEANSE_1) ? CLEANSE_1 : PURIFY_1;
            RemoveSpell(CLEANSE_1);
            RemoveSpell(PURIFY_1);
            InitSpellMap(CLEANSE);
        }

        void ApplyClassPassives() const override
        {
            uint8 level = master->GetLevel();
            bool isHoly = GetSpec() == BOT_SPEC_PALADIN_HOLY;
            bool isProt = GetSpec() == BOT_SPEC_PALADIN_PROTECTION;
            bool isRetr = GetSpec() == BOT_SPEC_PALADIN_RETRIBUTION;

            RefreshAura(ILLUMINATION, isHoly && level >= 20 ? 1 : 0);
            RefreshAura(IMPROVED_LAY_ON_HANDS, isHoly && level >= 20 ? 1 : 0);
            RefreshAura(IMPROVED_CONCENTRATION_AURA, isHoly && level >= 25 ? 1 : 0);
            RefreshAura(LIGHTS_GRACE, isHoly && level >= 40 ? 1 : 0);
            RefreshAura(SACRED_CLEANSING, isHoly && level >= 45 ? 1 : 0);
            RefreshAura(JUDGEMENTS_OF_THE_PURE, isHoly && level >= 50 ? 1 : 0);
            RefreshAura(INFUSION_OF_LIGHT, isHoly && level >= 55 ? 1 : 0);
            RefreshAura(RECUCED_HOLY_LIGHT_CAST_TIME, isHoly && level >= 60 ? 1 : 0); //

            RefreshAura(IMPROVED_RIGHTEOUS_FURY, isProt && level >= 20 ? 1 : 0);
            RefreshAura(IMPROVED_DEVOTION_AURA, isProt && level >= 25 ? 1 : 0);
            RefreshAura(DIVINE_GUARDIAN, isProt && level >= 25 ? 1 : 0);
            RefreshAura(RECKONING5, isProt && level >= 50 ? 1 : 0);
            RefreshAura(RECKONING4, isProt && level >= 45 && level < 50 ? 1 : 0);
            RefreshAura(RECKONING3, isProt && level >= 40 && level < 45 ? 1 : 0);
            RefreshAura(RECKONING2, isProt && level >= 35 && level < 40 ? 1 : 0);
            RefreshAura(RECKONING1, isProt && level >= 30 && level < 35 ? 1 : 0);
            RefreshAura(ONE_HANDED_WEAPON_SPECIALIZATION, isProt && level >= 35 ? 1 : 0);
            RefreshAura(ARDENT_DEFENDER, isProt && level >= 40 ? 1 : 0);
            //RefreshAura(COMBAT_EXPERTISE, isProt && level >= 45 ? 1 : 0);
            RefreshAura(REDOUBT3, isProt && level >= 55 ? 1 : 0);
            RefreshAura(REDOUBT2, isProt && level >= 50 && level < 55 ? 1 : 0);
            RefreshAura(REDOUBT1, isProt && level >= 45 && level < 50 ? 1 : 0);
            RefreshAura(GUARDED_BY_THE_LIGHT, isProt && level >= 50 ? 1 : 0);
            RefreshAura(TOUCHED_BY_THE_LIGHT, isProt && level >= 50 ? 1 : 0);
            RefreshAura(SHIELD_OF_THE_TEMPLAR, isProt && level >= 55 ? 1 : 0);
            //RefreshAura(JUDGEMENTS_OF_THE_JUST, isProt && level >= 55 ? 1 : 0);

            RefreshAura(HEART_OF_THE_CRUSADER, isRetr && level >= 15 ? 1 : 0);
            RefreshAura(PURSUIT_OF_JUSTICE, isRetr && level >= 20 ? 1 : 0);
            RefreshAura(FANATICISM, isRetr && level >= 20 ? 1 : 0);
            RefreshAura(VINDICATION2, isRetr && level >= 25 ? 1 : 0);
            RefreshAura(VINDICATION1, isRetr && level >= 20 && level < 25 ? 1 : 0);
            RefreshAura(CRUSADE, isRetr && level >= 25 ? 1 : 0);
            RefreshAura(TWO_HANDED_WEAPON_SPECIALIZATION, isRetr && level >= 30 ? 1 : 0);
            RefreshAura(SANCTIFIED_RETRIBUTION, !IAmFree() && isRetr && level >= 30 ? 1 : 0);
            RefreshAura(VENGEANCE3, isRetr && level >= 40 ? 1 : 0);
            RefreshAura(VENGEANCE2, isRetr && level >= 37 && level < 40 ? 1 : 0);
            RefreshAura(VENGEANCE1, isRetr && level >= 35 && level < 37 ? 1 : 0);
            RefreshAura(DIVINE_PURPOSE, isRetr && level >= 35 ? 1 : 0);
            RefreshAura(JUDGEMENTS_OF_THE_WISE, isRetr && level >= 40 ? 1 : 0);
            RefreshAura(ART_OF_WAR, isRetr && level >= 40 ? 1 : 0);
            RefreshAura(SWIFT_RETRIBUTION, !IAmFree() && isRetr && level >= 50 ? 1 : 0);
            RefreshAura(SHEATH_OF_LIGHT3, isRetr && level >= 60 ? 1 : 0);
            RefreshAura(SHEATH_OF_LIGHT2, isRetr && level >= 55 && level < 60 ? 1 : 0);
            RefreshAura(SHEATH_OF_LIGHT1, isRetr && level >= 50 && level < 55 ? 1 : 0);
            RefreshAura(RIGHTEOUS_VENGEANCE3, isRetr && level >= 60 ? 1 : 0);
            RefreshAura(RIGHTEOUS_VENGEANCE2, isRetr && level >= 57 && level < 60 ? 1 : 0);
            RefreshAura(RIGHTEOUS_VENGEANCE1, isRetr && level >= 55 && level < 57 ? 1 : 0);

            RefreshAura(GLYPH_HOLY_LIGHT, level >= 15 ? 1 : 0);
            RefreshAura(GLYPH_SALVATION, level >= 26 ? 1 : 0);

            RefreshAura(JUDGEMENT_ANTI_PARRY_DODGE_PASSIVE);

            //RefreshAura(CLEANSE_HEAL_PASSIVE, level >= 58 ? 1 : 0);
        }

        bool CanUseManually(uint32 basespell) const override
        {
            switch (basespell)
            {
            case FLASH_OF_LIGHT_1:
            case HOLY_LIGHT_1:
            case LAY_ON_HANDS_1:
            case HAND_OF_FREEDOM_1:
            case SACRED_SHIELD_1:
            case CLEANSE_1:
            case HAND_OF_PROTECTION_1:
            case HAND_OF_SALVATION_1:
            case HAND_OF_SACRIFICE_1:
                //case SEAL_OF_COMMAND_1:
                //case SEAL_OF_LIGHT_1:
                //case SEAL_OF_RIGHTEOUSNESS_1:
                //case SEAL_OF_WISDOM_1:
                //case SEAL_OF_JUSTICE_1:
            case DIVINE_PLEA_1:
            case AVENGING_WRATH_1:
            case BLESSING_OF_MIGHT_1:
            case BLESSING_OF_WISDOM_1:
            case BLESSING_OF_KINGS_1:
            case BLESSING_OF_SANCTUARY_1:
                return true;
            case HOLY_SHOCK_1:
                return HasRole(BOT_ROLE_HEAL);
            case DEVOTION_AURA_1:
            case CONCENTRATION_AURA_1:
            case FIRE_RESISTANCE_AURA_1:
            case FROST_RESISTANCE_AURA_1:
            case SHADOW_RESISTANCE_AURA_1:
            case RETRIBUTION_AURA_1:
            case CRUSADER_AURA_1:
                return _myaura != basespell;
            case PURIFY_1:
                return !GetSpell(CLEANSE_1);
            default:
                return false;
            }
        }

        bool HasAbilitiesSpecifics() const override { return true; }
        void FillAbilitiesSpecifics(Player const* player, std::list<std::string>& specList) override
        {
            uint32 textId;
            switch (_myaura)
            {
            case DEVOTION_AURA_1:          textId = BOT_TEXT_DEVOTION;         break;
            case CONCENTRATION_AURA_1:     textId = BOT_TEXT_CONCENTRATION;    break;
            case FIRE_RESISTANCE_AURA_1:   textId = BOT_TEXT_FIRERESISTANCE;   break;
            case FROST_RESISTANCE_AURA_1:  textId = BOT_TEXT_FROSTRESISTANCE;  break;
            case SHADOW_RESISTANCE_AURA_1: textId = BOT_TEXT_SHADOWRESISTANCE; break;
            case RETRIBUTION_AURA_1:       textId = BOT_TEXT_RETRIBUTION;      break;
            case CRUSADER_AURA_1:          textId = BOT_TEXT_CRUSADER;         break;
            default:                       textId = BOT_TEXT_NOAURA;           break;
            }
            specList.push_back(LocalizedNpcText(player, BOT_TEXT_AURA) + ": " + LocalizedNpcText(player, textId));
        }

        std::vector<uint32> const* GetDamagingSpellsList() const override
        {
            return &Paladin_spells_damage;
        }
        std::vector<uint32> const* GetCCSpellsList() const override
        {
            return &Paladin_spells_cc;
        }
        std::vector<uint32> const* GetHealingSpellsList() const override
        {
            return &Paladin_spells_heal;
        }
        std::vector<uint32> const* GetSupportSpellsList() const override
        {
            return &Paladin_spells_support;
        }

        void InitHeals() override
        {
            SpellInfo const* spellInfo;
            if (InitSpell(me, HOLY_SHOCK_HEAL_1))
            {
                spellInfo = sSpellMgr->GetSpellInfo(InitSpell(me, HOLY_SHOCK_HEAL_1));
                _heals[HOLY_SHOCK_1] = me->SpellHealingBonusDone(me, spellInfo, spellInfo->Effects[0].CalcValue(me), HEAL, 0);
            }
            else
                _heals[HOLY_SHOCK_1] = 0;

            if (InitSpell(me, HOLY_LIGHT_1))
            {
                spellInfo = sSpellMgr->GetSpellInfo(InitSpell(me, HOLY_LIGHT_1));
                _heals[HOLY_LIGHT_1] = me->SpellHealingBonusDone(me, spellInfo, spellInfo->Effects[0].CalcValue(me), HEAL, 0);
            }
            else
                _heals[HOLY_LIGHT_1] = 0;

            if (InitSpell(me, FLASH_OF_LIGHT_1))
            {
                spellInfo = sSpellMgr->GetSpellInfo(InitSpell(me, FLASH_OF_LIGHT_1));
                _heals[FLASH_OF_LIGHT_1] = me->SpellHealingBonusDone(me, spellInfo, spellInfo->Effects[0].CalcValue(me), HEAL, 0);
            }
            else
                _heals[FLASH_OF_LIGHT_1] = 0;
        }

    private:
        //Spells
        uint32 CLEANSE;
        //Timers
        /*misc*/uint32 checkAuraTimer, checkSealTimer, checkShieldTimer, checkBeaconTimer, holySupportTimer, avDelayTimer, shieldDelayTimer;
        //Special
        /*misc*/uint32 _myaura;
        /*misc*/int32 _sacDamage;

        using HealMap = std::unordered_map<uint32 /*baseId*/, int32 /*amount*/>;
        HealMap _heals;

        //uint32 _getBlessingsMask(Unit const*) const
        //Scans target for auras which are related to paladin's blessings
        //(even if aura is just incompatible with one)
        //returns applied blessings mask
        //used for finding out which blessings target lacks
        uint32 _getBlessingsMask(Unit const* target) const
        {
            uint32 mask = 0;

            for (auto const& [_, auraApp] : target->GetAppliedAuras())
            {
                bool blessing = true;
                switch (auraApp->GetBase()->GetSpellInfo()->GetFirstRankSpell()->Id)
                {
                case BLESSING_OF_WISDOM_1:
                case GREATER_BLESSING_OF_WISDOM_1:
                    mask |= SPECIFIC_BLESSING_WISDOM;
                    break;
                case BLESSING_OF_KINGS_1:
                case GREATER_BLESSING_OF_KINGS_1:
                    mask |= SPECIFIC_BLESSING_KINGS;
                    break;
                case BLESSING_OF_SANCTUARY_1:
                case GREATER_BLESSING_OF_SANCTUARY_1:
                    mask |= SPECIFIC_BLESSING_SANCTUARY;
                    break;
                case BLESSING_OF_MIGHT_1:
                case GREATER_BLESSING_OF_MIGHT_1:
                case BATTLESHOUT_1:
                    mask |= SPECIFIC_BLESSING_MIGHT;
                    break;
                default:
                    blessing = false; //next aura
                    break;
                }

                if (blessing && auraApp->GetBase()->GetCasterGUID() == me->GetGUID())
                    mask |= SPECIFIC_BLESSING_MY_BLESSING;
            }

            return mask;
        }
        //uint32 _getAurasMask(Unit const*) const
        //Scans target for paladin's auras
        //returns applied auras mask
        //used for finding out which auras target lacks
        uint32 _getAurasMask(std::map<uint32 /*type*/, uint32 /*curId*/>& idMap) const
        {
            uint32 mask = 0;

            for (auto const& [spellId, auraApp] : me->GetAppliedAuras())
            {
                bool isAura = true;
                uint32 baseId = auraApp->GetBase()->GetSpellInfo()->GetFirstRankSpell()->Id;
                switch (baseId)
                {
                case DEVOTION_AURA_1:
                    mask |= SPECIFIC_AURA_DEVOTION;
                    break;
                case CONCENTRATION_AURA_1:
                    mask |= SPECIFIC_AURA_CONCENTRATION;
                    break;
                case FIRE_RESISTANCE_AURA_1:
                    mask |= SPECIFIC_AURA_FIRE_RES;
                    break;
                case FROST_RESISTANCE_AURA_1:
                    mask |= SPECIFIC_AURA_FROST_RES;
                    break;
                case SHADOW_RESISTANCE_AURA_1:
                    mask |= SPECIFIC_AURA_SHADOW_RES;
                    break;
                case RETRIBUTION_AURA_1:
                    mask |= SPECIFIC_AURA_RETRIBUTION;
                    break;
                case CRUSADER_AURA_1:
                    mask |= SPECIFIC_AURA_CRUSADER;
                    break;
                default:
                    isAura = false; //next aura
                    break;
                }

                if (isAura)
                {
                    idMap[baseId] = spellId;
                    if (auraApp->GetBase()->GetCasterGUID() == me->GetGUID())
                        mask |= SPECIFIC_AURA_MY_AURA;
                }
            }

            return mask;
        }
    };
};

void AddSC_paladin_bot()
{
    new paladin_bot();
}
