# Configuring class skills

The server chooses which skills a character can learn, their prerequisites, and
their maximum levels. OpenMidgard displays the skill list sent by the server and
sends upgrade/use requests back to it.

## Class assignments and prerequisites

In the local server checkout at `C:\ragnarok\ragnarok`, the base class trees are in
`db/re/skill_tree.txt`. Put overrides or additions in `db/import/skill_tree.txt`,
which the server reads afterward. A row with the same class ID and skill ID
replaces that entry, including its prerequisites.

The usual row contains 13 comma-separated numbers:

```text
ClassID,SkillID,MaxLevel,RequiredSkill1,RequiredLevel1,RequiredSkill2,RequiredLevel2,RequiredSkill3,RequiredLevel3,RequiredSkill4,RequiredLevel4,RequiredSkill5,RequiredLevel5
```

For example, this gives Magician (class 2) Fire Bolt (skill 19), with a maximum
level of 5 and a requirement of Cold Bolt (skill 14) at level 3:

```text
2,19,5,14,3,0,0,0,0,0,0,0,0
```

Unused requirement pairs are `0,0`. To add base/job level requirements, use the
15-column form, inserting those two numbers after `MaxLevel`:

```text
2,19,5,1,5,14,3,0,0,0,0,0,0,0,0
```

That example also requires base level 1 and job level 5. The class maximum cannot
exceed the skill's global `MaxLevel` in the server skill database. Check the
existing class rows for the correct job IDs, including separate normal,
transcendent, and baby variants.

After tree edits, use the GM command `@reloadpcdb` and relog the character to
refresh the server's calculated skill list. Restarting the map server also
reloads these files.

## Costs and behavior

The server's `db/re/skill_db.yml` imports class files such as
`db/re/skill_db_magician.yml`. These define each skill's `Id`, `Name`, `MaxLevel`,
target type, SP cost, range, cooldown, and other mechanics. The class tree assigns
existing skill IDs to classes; it does not implement new skill behavior.

Use `@reloadskilldb` after changing these definitions. Use `@reloadpcdb` as well
if you also changed class trees.

## Names, descriptions, and icons in the client

OpenMidgard loads these scripts from the game folder or GRFs, using the Lua
bridge's existing lookup order: the requested path, then
`data/luafiles514/<path>`, then `data/<path>`. `.lua` is a fallback if `.lub` cannot
be loaded.

| Relative script path | What it configures |
| --- | --- |
| `lua files/skillinfoz/skillid.lub` | Symbolic names mapped to server skill IDs (`SKID`) |
| `lua files/skillinfoz/skillinfolist.lub` | Icon resource name, displayed name, displayed maximum level, and per-level SP metadata (`SKILL_INFO_LIST`) |
| `lua files/skillinfoz/skilldescript.lub` | Tooltip title and description lines (`SKILL_DESCRIPT`) |

For example, matching server skill 19:

```lua
-- skillid.lub
SKID = {
    SK_MG_FIREBOLT = 19,
    -- Keep the other skill mappings here too.
}

-- skillinfolist.lub
SKILL_INFO_LIST = {
    [SKID.SK_MG_FIREBOLT] = {
        "SK_MG_FIREBOLT", -- icon resource name
        SkillName = "Fire Bolt",
        MaxLv = 5,
        SpAmount = { 30, 30, 30, 30, 30 }
    }
}

-- skilldescript.lub
SKILL_DESCRIPT = {
    [SKID.SK_MG_FIREBOLT] = {
        "Fire Bolt",
        "Calls down bolts of fire on the target."
    }
}
```

Edit the existing entries in each file rather than replacing the complete
tables with this example. Icons use the first string in the info entry,
lowercased, under `data/texture/유저인터페이스/item/`, for example
`sk_mg_firebolt.bmp`. Restart the client after metadata changes. Upgrade
availability and the current SP cost come from server packets.

The current skill window is a flat list of learned or learnable skills, sorted
by skill ID. It does not read `skilltreeview.lub`, `jobinheritlist.lub`, or
`jobskillinfo.lub` for class tabs or a graphical tree. Editing those layout files
does not change this window's layout or grant skills to a class.

## Skill packets and effects

The client uses the targeting type, learned level, and range sent by the server.
Hotkeys use their saved skill level, capped at the learned level. A targeted
skill remains selected after clicking empty space; clicking a target or ground
cell outside range queues movement and casting. Releasing the mouse preserves
that queued cast. Shift also works while a skill is selected.

The 2008 receive profile handles destination selection (`011c`), Sense (`018c`),
production (`018d`, `01ad`, `025a`), Auto Spell (`01cd`, `0442`), and Repair Weapon
(`01fc`, `01fe`). Choices use the existing menu window, including keyboard
selection, cancellation, and pages for long lists. The customized server also
uses `01ad` for potion production and its successive forging material choices;
these use the same `01ae` response as arrow creation. Production quantities
remain controlled by the server.

Skill-unit IDs in `011f`/`01c9` are distinct from special-effect IDs. Ground
presentations now follow the unit's server ID, visibility, updates, and removal
(`0120`). Their animations repeat until the server removes them.
Classic and colored Ranger trap units use ground-unit SPR/ACT pairs, rather
than the unrelated skill-window icons. Memo, teleport, and production result
packets also display their success/failure messages.

STR playback reads the authored FPS before the frame count, includes layer zero,
and samples absolute/delta key pairs without overwriting the absolute key's
blend modes. Compound effects retain their component animations. Sprite effects
resolve full GRF resource paths, apply clip rotation, and use the bounds of the
actual animation frame instead of cropping every frame to a 384-pixel canvas.
SPR/ACT catalog paths include `data\`; stripping that archive prefix in the
shared resource loader breaks character previews and map resources as well.
Existing minimal-effect variants remain selected when `/mineffect` is enabled.
Skill damage presentation accepts an unresolved source when the target is
known, and server SP-restoration notifications show blue recovery numbers.

### Regenerating the asset catalogs

`tools/generate_rageffect_assets.py` reads installed GRFs directly. Pass them in
the same priority order as the game's `DATA.INI`, with patches first. It matches
captured texture references to unambiguous STR assets and resolves sprite
definitions to installed SPR/ACT pairs. These mappings are inferred from asset
references; they are not an official client effect table.

Download the public [captured effect metadata](https://raw.githubusercontent.com/lukasrmattos/ragnarok_effects_list/main/effects.json)
and [roBrowser sprite definitions](https://raw.githubusercontent.com/vthibault/roBrowser/master/src/DB/Effects/EffectTable.js)
to local files, then run:

```powershell
python tools/generate_rageffect_assets.py --reference effects.json --sprite-reference EffectTable.js --grf C:/ragnarok/client/patchRunerok.grf --grf C:/ragnarok/client/data.grf
```

Add `--check` to verify the generated catalogs without changing them. The
generated headers record the SHA256 of each reference input. With the installed
archives inspected during this repair, the catalogs contain 796 effect IDs
referencing 815 STR animations and 45 sprite effect definitions. Asset inspection
parsed 1,735 STR files under `data/texture/effect`, including intentional empty
animations in the minimal-effect directory.

### Owned skill presentation table

`skill_presentation.lua` is Open Midgard's editable source of truth for skills
registered there. It is read once per process in its own Lua state, without
loading any GRF skill scripts. The existing Lua runtime parses this file; no new
parser dependency is needed. Restart the client after editing it.

The skill entries include Two-Handed Sword Mastery (`3`, `SK_SM_TWOHAND`),
Traumatic Strike (`398`, `SK_SM_TRAUMATIC_STRIKE`), and Arcane Strike
(`399`, `SK_SM_LIGHTNING_STRIKE`). Mastery is a passive weapon bonus
in the running server database, so its entry explicitly has no effects or audio.
Two Hand Quicken (`817`) is a separate active skill. The shared effect `130`
definition preserves the earlier Quicken correction for all weapon types.

Each skill can define `source`, `target`, and `ground` arrays. Their entries carry
an `effect_id` and full GRF resource paths: `str` begins with
`data\texture\effect\`, while `wav` begins with `data\wav\`. Omitting `wav`
means silence. A `variants` array pairs visual and audio paths so both use the
same random variant. `offset_y` sets the vertical attachment offset, and `loop`
controls repetition; STR playback uses the animation's authored FPS.
For a procedural caster flash and camera-facing radial burst, use
`renderer = "body_burst"` with a full `texture` TGA path and optional `wav`.
Its `flash_start_color` and `flash_peak_color` are RGB integers, with
`flash_peak_ms` and `flash_end_ms` defining the color ramp and return to white.
`halo_alpha` and `halo_radius` control the sprite halo (radius in screen pixels).
`burst_delay_ms`, `burst_duration_ms`, `burst_count`, `burst_arc_degrees`,
`burst_height`, `burst_alpha`, and `burst_radii` define the wedge groups;
height and radii use world units; positive height moves up in the caster's
billboard plane. These effects cannot loop. Flash/halo state expires
automatically and does not modify shared character textures.
Shared definitions can live in the top-level `effects` table, with skill entries
referencing them using just `{ effect_id = ... }`. Define each effect once.

`server_notified = true` means that effect-notification packets already trigger
the configured effects. Skill-use packets then suppress their own effect replay,
avoiding duplicates. Direct server packets carry only an effect ID; inline skill
definitions are also indexed by that ID. `source_server_notified`,
`target_server_notified`, and `ground_server_notified` can override the default
independently. This lets a skill play a missing caster presentation locally while
the server continues to drive its target effects. Effect IDs and skill IDs are
separate namespaces: skill `398` uses caster effect `399`, without affecting
skill `399` (Arcane Strike). `passive = true` requires empty effect
arrays. `begin_effect_id` defaults to `-1` (no cast-start effect), and
`motion_type` defaults to `7` (the generic skill action).

The loader searches beside `open-midgard.ini` (including its data-directory
override), then the current working directory, then the source checkout for
development. The deployment rules copy the file beside the executable and to
the configured client deployment directory. Invalid definitions are rejected
with a `[SkillPresentation]` log message; successful loading logs the source path
and counts. Skills and effects outside the file retain their existing handling
until they are migrated. The shared resource loader is unchanged.

Traumatic Strike's caster effect `399` (`EF_BASH3D3`, Head Crush) uses a yellow
body flash for 500 ms, followed by two groups of six expanding wedges for 175 ms.
The texture is `data\texture\effect\alpha_center.tga`; its dedicated sound is
`data\wav\effect\헤드 크러쉬.wav`, stored with CP949 byte escapes in the Lua file.
The texture, sound, and procedural timings follow the
[roBrowser effect definition](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Effects/EffectTable.js).
Its dimensions are converted to native world units (multiply by five), matching
the coordinate scaling in its
[ground loader](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/Loaders/Ground.js).
The installed GRFs contain both resources. The server sends target effect `677`,
mapped to `cwound.str`, so the client does not replay that effect from the skill
damage packet. The server also sends `626` (`EF_MADNESS_RED`); its separate
renderer remains unimplemented. The caster visual and sound need an in-game
comparison with the official client.

### Coverage and play checks

Spear Quicken (`SK_KN_SQUICKEN`, skill `816`) explicitly sends
`EF_TWOHANDQUICKEN` (`130`) from this server. The gameplay log confirms that
notification immediately before a successful skill-use response. Effect `130`
now uses the shared presentation-table definition for `twohand.str`, the head
offset, and `data\wav\effect\knight_twohandquicken.wav`. The
[roBrowser effect definition](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Effects/EffectTable.js)
also identifies these assets and the head attachment for effect `130`.
The installed visual parses as 20 frames at 60 FPS with all six referenced
textures present; the sound parses as a valid 0.448-second PCM WAV. This targeted
correction still needs an in-game comparison with the official client.

Arcane Strike (`SK_SM_LIGHTNING_STRIKE`, skill `399`) sends effects `1537`,
`1538`, `1539`, and `52` on its target. The gameplay log records all four
notifications, with the first three previously reported as missing presentations.
Its presentation-table entry maps the first three to the minimal Comet
sub-explosion, bottom ring, and cast-flash STRs under `mineffect\new_comet`, matching the
[captured effect assets](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/effects.json).
These explicit effect IDs use those assets regardless of the `/mineffect` setting.
Effect `52` (`EF_WINDHIT`) now plays `_hit_fist1.wav` through `_hit_fist3.wav`,
using the same variant as its `WindHit` animation, as specified by the
[roBrowser effect definition](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Effects/EffectTable.js).
The three Comet components have no startup sound of their own. Their paths,
the paired wind-hit variants, and the shared Quicken correction now live in the
editable file; the corresponding C++ asset/audio/position exceptions were
removed. Inspection of the installed GRFs verified all seven configured STRs,
every referenced texture, and the four PCM WAVs. An updated client still needs an
in-game check.

This repair does **not** establish complete parity with the official client.
The customized database contains 455 skills. A static scan found 325 distinct
explicit effect IDs in the server's skill/status source, including conditional
and potentially unused paths. The original catalog repair left 174 of those IDs
without a registered presentation; the Arcane Strike correction resolves three
of them. Many need procedural rendering code, which is not
supplied by the GRFs. Ground examples still needing that work include Cloud Kill
(`1080`), Stealth Field (`784`), Wall of Thorns (`912`), Demonic Fire (`916`), and
Fire Insignia (`918`). Missing IDs are logged once per client run as
`[RagEffect] no presentation for effect=...`.
Unknown visible ground-unit types are also logged once as
`[SkillUnit] no presentation for unit=...`. Specialized service workflows remain
incomplete too: for example, the existing Vending shop-creation packet (`01b0`)
is still ignored because the client has no shop-creation window.

No client or server build was attempted, and these changes have not been tested
in a running updated client. Compare these cases against the official client:

| Case | Expected behavior |
| --- | --- |
| Two-Handed Sword Mastery (3) | Passive bonus; no cast visual or sound |
| Traumatic Strike (398) | One caster body flash/halo, delayed radial burst, and Head Crush WAV; server-driven `cwound.str` on the target |
| Spear Quicken (816) | One head-attached `twohand.str` effect and one `knight_twohandquicken.wav` sound per successful cast |
| Arcane Strike (399) | Three minimal Comet components and one `WindHit` animation on the target, with one corresponding `_hit_fist` sound |
| Actor skill outside range | Click and release; walk into range, then cast once |
| Ground skill outside range | Walk into range and cast at the chosen cell |
| Low-level hotkey for a higher learned skill | Send the level saved in the hotkey |
| Shift while targeting; click empty space | Preserve skill selection and casting behavior |
| Teleport / Warp Portal | Select a destination or cancel; subsequent skills still work |
| Auto Spell | Select an offered learned spell or cancel |
| Pharmacy / forging / arrow creation | Select items and successive materials; compare cancellation |
| Repair Weapon / Sense | Repair an offered item; display the returned monster information |
| Sanctuary / Magnus / Epiclesis / Psychic Wave | Ground effect persists and disappears with the server unit |
| Bolts / small Heal / buff while another actor casts | Show hit/heal/buff effects without disturbing that cast |
| Demonstration / ninja effects | Sprite assets load, rotate, and remain fully visible |
| Newer effects with multiple STR components | Show each component at the authored speed |

Also check both native and Qt menus and the existing `AutoAttack=0`/`1` behavior,
including holding and releasing the mouse. Remaining failures should be recorded
with the skill ID, chosen level, target, packet opcode, and missing-effect log.
