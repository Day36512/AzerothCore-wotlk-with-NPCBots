/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
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

// This is where scripts' loading functions should be declared:
// void MyExampleScript()

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()

void AddSC_mage_custom_spells();
void AddSC_item_custom_spells();
void AddSC_custom_commandscript();
void AddSC_CustomAreaAuras();
void AddSC_spread_command();
void AddSC_spell_npc_bot_smoke_flare_variants();
void AddSC_twins_director();
void AddSC_no_xp_with_aura();
void AddSC_spell_npc_bot_square_formation();
void AddSC_spell_npc_bot_group_flares();
void Addmod_glyph_unlocksScripts();
void AddSC_spell_remove_banish_instance_only();
void AddSC_mod_speed_commands();
void AddSC_invisible_stasis_bunny();
void AddSC_mod_timecycle();

void AddCustomScripts()
{
    AddSC_item_custom_spells();
    AddSC_mage_custom_spells();
    AddSC_custom_commandscript();
    AddSC_CustomAreaAuras();
    AddSC_spread_command();
    AddSC_spell_npc_bot_smoke_flare_variants();
    AddSC_twins_director();
    AddSC_no_xp_with_aura();
    AddSC_spell_npc_bot_square_formation();
    AddSC_spell_npc_bot_group_flares();
    Addmod_glyph_unlocksScripts();
    AddSC_spell_remove_banish_instance_only();
    AddSC_mod_speed_commands();
    AddSC_invisible_stasis_bunny();
    AddSC_mod_timecycle();
}
