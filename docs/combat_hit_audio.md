# Combat hit audio

`src/audio/CombatHitAudio.h` owns the weapon/element recipes. Normal attacks
select their sounds when each queued hit lands. Skill audio uses the existing
damage-number schedule, independently of effect visibility. Instrument and
neutral monster variations are selected anew for each hit.

Physical hits play their weapon sound and add an elemental layer only for a
non-neutral property. Magic plays only its element. Neutral non-player physical
hits use one of `_enemy_hit1.wav` through `_enemy_hit3.wav`; ranged hits add
`attack_bow.wav`. The target's class or armor does not affect this selection.

A targeted magic skill with one impact at every level keeps its dedicated sound in
`skill_presentation.lua` (`sounds[skillId]`, trigger `damage`, or `hit_wav`).
Its authored effect audio can also supply the dedicated sound. Soul Burn keeps
`effect\lg_rayofgenesis.wav`. A level-one bolt remains an elemental bolt: skills
that gain additional hits at higher levels use the elemental recipe at every
level. Ground, area, and persistent magic also use the elemental recipe on every
damage notification, even when each packet reports just one hit. A packet
reporting multiple hits always uses the elemental recipe.

Only the generic damage impact is replaced. Skill sounds, effect audio, ACT
events, configured damage/ground sounds, and explicitly authored `hit_wav` audio
keep their existing triggers and timing. The replacement hit layer plays alongside
that skill audio, with identical `hit_wav`/impact paths played only once per hit.
Unlisted weapon families and elements have no guessed fallback.

## Additional neutral weapon impacts

Player hits also play the following impact WAV alongside the existing audio,
only when the damage is physical and its confirmed property is neutral:

| Weapon | Additional WAV, relative to `data\wav\` |
| --- | --- |
| Dagger, katar | `_hit_dagger.wav` |
| One-handed/two-handed sword | `_hit_sword.wav` |
| One-handed/two-handed axe | `_hit_axe.wav` |
| One-handed/two-handed staff | `_enemy_hit4.wav` |
| Mace | `_hit_mace.wav` |
| Book | `_attack_book.wav` (already present; plays once) |
| Instrument | `_enemy_hit1.wav` (installed outside `effect\`) |
| Knuckle | Random choice of `_hit_fist1.wav` through `_hit_fist4.wav` |
| Bow | `_hit_arrow.wav` |

The existing normal-attack and skill hit schedulers play this layer once per
successful hit, including each hit of a multi-hit skill. Knuckles choose again
at each hit. Magic, non-neutral physical damage, non-player attacks, and unlisted
weapon types gain no extra sound. Existing sounds and timing are preserved.

The extra layer requires a confirmed element. Extended packets supply it per
hand; legacy skill packets can use a fixed property from the skill metadata.
Legacy normal attacks and skills that inherit an unknown weapon/arrow property
do not gain this layer, since assuming neutral could misclassify elemental hits.

The requested instrument `effect\_enemy_hit1.wav` is absent from the installed
assets. The mapping uses the existing `data\wav\_enemy_hit1.wav` instead.

## Server properties

The paired server changes are preserved in `rathena_damage_audio.patch`. They
extend the normal and skill damage notifications rather than sending another
damage event. The server captures each hand's element during damage calculation,
before ammunition consumption or status expiry. Bow and instrument properties
come from the arrow, including neutral arrows, and the range flag comes from
the actual damage calculation.

The custom packets are enabled for the paired `PACKETVER == 20080910` server
through `ENABLE_OPENMIDGARD_DAMAGE_AUDIO` in `clif.cpp`. That server must be used
with the updated OpenMidgard client; stock clients do not understand these
opcodes. Other configured server packet versions retain their existing packets.

Both packets are 38 bytes. `0x7F10` has the first 33 bytes of `0x02E1` and
`0x7F11` has the first 33 bytes of `0x01DE`, with these additional fields:

| Offset | Size | Meaning |
| --- | --- | --- |
| 33 | 1 | Flags: physical `1`, magic `2`, long range `4`, player `8` |
| 34 | 1 | Right hand/skill/arrow element (`ELE_*`) |
| 35 | 1 | Right hand weapon (`W_*`) |
| 36 | 1 | Left hand element |
| 37 | 1 | Left hand weapon |

The client also accepts legacy notifications. Fixed skill properties and skill
classification come from `SkillDamageAudio.generated.inc`. Legacy packets cannot
report weapon enchants, another player's arrow element, or the actual range
classification; full accuracy requires the paired server changes.

Refresh the skill snapshot after changing the server database (requires PyYAML):

```powershell
python tools/generate_skill_damage_audio.py --server D:/ragnarok/ragnarok
```

## Verification

No build or compilation was performed for this change. Static checks cover
patch whitespace, server patch application, generated database consistency, and
the existence and WAV headers of every mapped sound in the installed assets.
Four Python regression tests cover one-hit ground notifications, targeted
single-impact magic, persistent/area spells, and level-one bolts:

```powershell
python -m unittest discover -s tools -p test_skill_damage_audio.py -v
```

Gameplay checks remain. These expectations describe the damage impact layer;
each skill's own audio continues to play alongside it:

| Scenario | Expected audio |
| --- | --- |
| Neutral sword normal attack and physical skill | Existing sword sound + `_hit_sword.wav`, on each successful hit |
| Fire dagger physical attack/skill | Dagger + fire on each hit |
| Each listed weapon type | Its exact mapped WAV; instrument varies between the three sign WAVs |
| One-handed/two-handed spear | `_attack_spear.wav` / `attack_spear.wav` respectively |
| Each specified magic element | Element only for a skill with multiple hits |
| Storm Gust, Meteor Storm, Earth Strain | Water, fire, or earth on each damage tick, including packets with one hit |
| Level-one versus higher-level bolt | Same elemental sound, once per actual hit |
| Soul Burn | Dedicated sound, without neutral or weapon hit audio |
| Bow/instrument with fire, water, and neutral arrows | Weapon + matching non-neutral arrow element; neutral arrow has weapon only |
| Neutral monster melee | One randomized enemy hit WAV |
| Neutral monster ranged attack/physical skill | One randomized enemy hit WAV + bow |
| Dual wield with different weapon types/properties | Each hand's sound/property at its own queued impact |
| Miss, lucky dodge, animation-only packet | No damage impact audio |
| Effects disabled | Same damage audio timing and selection |
| Neutral knuckle multi-hit skill | Existing fist sound + one of the four `_hit_fist*.wav` files per hit |
| Magic or non-neutral physical hit with a listed weapon | Existing audio, without the additional neutral weapon impact |
