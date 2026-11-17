"""Regression checks for skill damage audio metadata; does not build the game."""
import re
import unittest

from generate_skill_damage_audio import render_skill_damage_audio, render_weapon_damage_audio


class SkillDamageAudioTests(unittest.TestCase):
    def classify(self, skills):
        generated = render_skill_damage_audio({skill["Id"]: skill for skill in skills})
        return {
            int(skill_id): (kind, int(element), single_hit == "true")
            for skill_id, kind, element, single_hit in re.findall(
                r"\{ (\d+), CombatDamageKind::(\w+), (-?\d+), (true|false),", generated)
        }

    def test_one_hit_per_ground_damage_packet_is_not_a_single_impact_spell(self):
        skills = [
            {"Id": 83, "Name": "Meteor Storm", "Element": "Fire", "HitCount": [{"Level": 1, "Count": 1}]},
            {"Id": 89, "Name": "Storm Gust", "Element": "Water", "HitCount": 1},
            {"Id": 2446, "Name": "Earth Strain", "Element": "Earth", "HitCount": -1},
        ]
        for skill in skills:
            skill.update(Type="Magic", TargetType="Ground")
        classified = self.classify(skills)
        self.assertEqual(classified[83], ("Magic", 3, False))
        self.assertEqual(classified[89], ("Magic", 1, False))
        self.assertEqual(classified[2446], ("Magic", 2, False))

    def test_targeted_single_impact_magic_keeps_its_dedicated_sound(self):
        classified = self.classify([
            {"Id": 375, "Name": "Soul Burn", "Type": "Magic", "TargetType": "Attack", "HitCount": 1},
        ])
        self.assertEqual(classified[375], ("Magic", 0, True))

    def test_area_and_persistent_spells_use_element_audio_on_each_notification(self):
        classified = self.classify([
            {"Id": 1, "Name": "Area", "Type": "Magic", "TargetType": "Attack", "HitCount": 1,
             "SplashArea": 2},
            {"Id": 2, "Name": "Persistent", "Type": "Magic", "TargetType": "Attack", "HitCount": 1,
             "Unit": {"Interval": 1000}},
        ])
        self.assertFalse(classified[1][2])
        self.assertFalse(classified[2][2])

    def test_level_one_bolt_does_not_change_the_skill_to_dedicated_audio(self):
        classified = self.classify([
            {"Id": 19, "Name": "Fire Bolt", "Type": "Magic", "Element": "Fire", "TargetType": "Attack",
             "HitCount": [{"Level": 1, "Count": 1}, {"Level": 2, "Count": 2}]},
        ])
        self.assertEqual(classified[19], ("Magic", 3, False))


class WeaponDamageAudioTests(unittest.TestCase):
    def classify(self, items):
        generated = render_weapon_damage_audio({item["Id"]: item for item in items})
        return [tuple(map(int, row)) for row in re.findall(
            r"\{ (\d+), (\d+), (-?\d+), (-?\d+) \}", generated)]

    def test_plain_weapons_have_a_confirmable_neutral_property(self):
        self.assertEqual(self.classify([
            {"Id": 1102, "Type": "Weapon", "SubType": "1hSword"},
            {"Id": 1159, "Type": "Weapon", "SubType": "2hSword",
             "Script": "bonus2 bAddSize,Size_Large,10;"},
            {"Id": 50401, "Type": "Weapon", "SubType": "1hSpear"},
        ]), [(1102, 1102, 2, 0), (1159, 1159, 3, 0), (50401, 50401, 4, 0)])

    def test_arrows_keep_neutral_and_elemental_properties_separate(self):
        self.assertEqual(self.classify([
            {"Id": 1750, "Type": "Ammo", "SubType": "Arrow", "Script": "bonus bAtkEle,Ele_Neutral;"},
            {"Id": 1751, "Type": "Ammo", "SubType": "Arrow", "Script": "bonus bAtkEle,Ele_Fire;"},
        ]), [(1750, 1750, -1, 0), (1751, 1751, -1, 3)])

    def test_dynamic_properties_are_not_assumed_neutral(self):
        self.assertEqual(self.classify([
            {"Id": 1201, "Type": "Weapon", "SubType": "Dagger",
             "Script": "if (getrefine() > 5) bonus bAtkEle,Ele_Fire;"},
            {"Id": 1202, "Type": "Weapon", "SubType": "Dagger",
             "Script": "bonus bAtkEle,rand(0,9);"},
        ]), [(1201, 1202, 1, -1)])

    def test_compact_ranges_preserve_gaps_and_changes_in_weapon_type(self):
        self.assertEqual(self.classify([
            {"Id": 1101, "Type": "Weapon", "SubType": "1hSword"},
            {"Id": 1102, "Type": "Weapon", "SubType": "1hSword"},
            {"Id": 1104, "Type": "Weapon", "SubType": "1hSword"},
            {"Id": 1105, "Type": "Weapon", "SubType": "2hSword"},
            {"Id": 1106, "Type": "Armor"},
        ]), [(1101, 1102, 2, 0), (1104, 1104, 2, 0), (1105, 1105, 3, 0)])


if __name__ == "__main__":
    unittest.main()
