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

Multi-hit skill damage numbers appear 160 ms apart by default, matching the
bolt-effect timing. White numbers use the `[WhiteDamageNumbers]` settings in
`open-midgard.ini`: `RightDisplacement` (default `16` screen pixels),
`MaxTravelHeight` (`48`), `MaxTravelWidth` (`32` additional pixels right), and
`DelayBetweenNumbersMs` (`160` ms). White numbers rise in an arc and move right
in screen space; their delay also sets minimum spacing for repeated numbers on
the same actor. Default damage-skill hit sounds use this interval too and follow
any additional queue delay applied to white numbers. Restart after editing the
settings. Bolt effects retain their
160 ms timing when number spacing is changed.
A larger yellow total appears 320 ms after the final displayed hit in the packet
by default. Its `[YellowDamageNumbers]` settings in `open-midgard.ini` are
`DelayAfterLastHitMs` (default `320`), `StartingHeightDisplacement` (`0` screen
pixels above the target), and `MaxTravelHeight` (`48` additional pixels upward).
The total starts centered on the target, fades in over 160 ms, rises in screen
space, and fades out during the final 720 ms of its 2880 ms animation. Delayed
totals capture the target's position when they appear, or its last position if
it disappears first. Restart after editing these settings.
Individual hits retain their
existing damage, critical, or recovery color. Totals preserve the packet's exact
damage, including division remainders, and can display more than six digits.
Single hits, misses, and recovery amounts do not add a damage total. Delayed
numbers retain their scheduled animation timestamps if a frame arrives late;
totals from successive casts replace the previous visible total when they start.
Floating hit numbers and totals finish at their captured position if the target
dies or disappears before the sequence ends.

Buff and debuff icons appear at the right edge in both the native and Qt UIs,
independently of minimap visibility. Names and images come from
`lua files/stateicon/efstids.lub`, `stateiconimginfo.lub`, and `stateiconinfo.lub`,
using the Lua bridge's existing loose-file and GRF lookup order. Native icons
wrap into columns when the screen fills. Hover shows the status name and any
server-provided remaining time; `0196` has no timer and remains active until the
server removes it. After-cast delay (`46`, `EFST_POSTDELAY`) is excluded from the
buff list for both status packet formats. Its legacy `0196` notification has no
duration or expiry removal and would otherwise leave a permanent "Status 46"
tile after skills such as Magnum Break. Spirit Growth is skill `819`, status
`1353`; status IDs are separate from skill and effect IDs.
The native path logs each image lookup as `[StatusIcon]`, and local activation
and removal packets are logged as `[GameMode] self status icon`.

`status_icons.lua` provides editable names and full GRF image paths for individual
buffs/debuffs. Its `icons` table is keyed by the server status ID. Initially it
contains only Spirit Growth (`1353`), with a dummy path that should be replaced:

```lua
[1353] = {
    name = "Spirit Growth",
    icon = [[data\texture\effect\custom\spirit_growth_placeholder.tga]],
},
```

Put the image in your GRF at the chosen path and update `icon` to match. Long Lua
strings (`[[...]]`) preserve backslashes. Configured paths take priority in both
the native and Qt UI; a missing configured image displays the text tile. Other
statuses retain automatic lookup. The loader searches beside `open-midgard.ini`,
then the current working directory, then the source checkout for development.
Deployment copies the file beside the executable and into the configured client
directory. Restart the client after editing; the file and images are cached.
The `[StatusIcon]` log records which configuration file was loaded.
Native TGA icons use the client's own decoder through the shared UI pixel loader,
matching the Qt icon path's decoder, with alpha and RLE compression support.

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

Each skill can define `casting`, `source`, `target`, and `ground` arrays. Their
entries carry an `effect_id` and full GRF resource paths: `str` begins with
`data\texture\effect\`, while `wav` begins with `data\wav\`. Omitting `wav`
silences that effect. Damage skills also play per-hit audio unless disabled with
`silence_hit_sound = false`, as described below. A `variants` array pairs visual
and audio paths so both use the same random variant. `offset_y` sets the vertical
attachment offset, and `loop` controls repetition; STR playback uses the
animation's authored FPS.

STR entries also accept `hue_shift_degrees`, an integer from `0` to `360`.
Omitting it, or using `0`/`360`, preserves the authored colors. The client shifts
both texture RGB and animated layer RGB while preserving alpha and brightness
range; black/magenta masking happens before recoloring. Recolored textures use
separate managed cache entries, so shared textures and GRF assets stay unchanged.
This field supports `str` and `directional_str` renderers. Restart the client
after editing it, as with other presentation settings.

Gloria Domini's Petitio effect (`50021`) uses `hue_shift_degrees = 180` to turn
its yellow/gold artwork blue. Its bottom effect (`50022`) keeps its original
colors; no other effect enables the hue shift.

`ground` plays effects at the terrain height of the cell reported by a successful
ground-skill notification after casting, or by a position-damage notification.
These effects stay at that world position when the caster moves. Ordinary
actor-damage notifications play `target` effects on their victims; they do not
repeat the ground visual. Use an unused `effect_id` for each new inline definition,
or reference an existing definition by ID without repeating its `str` field.

Genesis Ray (`2321`, `SK_PA_GENESISRAY`) uses this configuration alongside its
existing `casting` and `target` arrays:

```lua
ground = {
    {
        effect_id = 50018,
        str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis\new_rayofgenesis.str]],
    },
},
```

The clip plays once at the selected cell when casting completes, including casts
with no victims. Future ground-targeted skills can use the same array format with
their own ID and resource path. Ground effects support `wav`, `offset_y`,
`bottom_effect`, and `delay_ms` like the other attachments. If the skill sets
`server_notified = true` but its ground effect needs local playback, set
`ground_server_notified = false` on that skill.

`casting` attaches all its effects to the caster when a cast with a positive
duration starts. STR clips automatically loop for that duration. The group stops
on successful use, cancellation, expiry, death, or actor removal; it does not
launch for instant skills. Inline definitions and shared effect references use
the same format as the other arrays:

```lua
casting = {
    { effect_id = 50017 }, -- A shared definition from the effects table.
    {
        effect_id = 50019,
        str = [[data\texture\effect\competentia\competentia_loop_bottom\competentia_loop_bottom.str]],
        bottom_effect = true,
        bottom_offset_px = 25,
    },
},
```

A nonempty `casting` array replaces `begin_effect_id`; existing skills using
only `begin_effect_id` keep their single casting effect. `server_notified` and
the per-attachment flags apply to completion effects, while `casting` is always
driven locally by the cast-start notification. Passive skills cannot have
casting effects. Configure `source`, `target`, and `ground` for successful use.

Directional effects use `renderer = "directional_str"`, an optional `wav`, and a
`directions` array of one to eight clips. Each clip defines a `str` path and
`origin_x`, `origin_y`, `end_x`, and `end_y` in authored screen pixels relative to
the STR's `(320, 320)` anchor. Clips must share the same authored FPS and frame
count. The renderer chooses the clip closest to the projected caster-to-target
direction, maps its origin to the caster and its end to the target, and preserves
the authored width. Camera turns change the direction clip while preserving the
animation phase and sound. Standard effect flags, including `offset_y`, `loop`,
`bottom_effect`, `delay_ms`, and `once_per_hit`, still apply.

Dragon Breath (`5004`, `SK_CM_DRAGONBREATH`) uses the four installed
`new_dragonbreath_water_*_clock.str` clips for blue fire from caster to target,
with `data\wav\dragon_breath.wav`. Its source effect `1310` is server-notified:
the following skill damage packet supplies the existing effect's target instead
of playing a second copy. Playback waits up to 250 ms for that target; standalone
effect notifications fall back to the caster's facing after that wait.

Set `once_per_hit = true` on an effect definition to play it once for each hit
reported by a skill damage packet. Omitted or false plays it once for the skill
notification. This works for inline and shared `effects` definitions in `source`,
`target`, and `ground` arrays, including variants and procedural renderers. Each
playback is a separate effect instance, with its own animation. A configured WAV
plays only with the first instance, even for per-hit effects; variants are selected
independently for each hit. Playbacks are staggered by the
existing skill-hit interval of `160` ms plus any configured `delay_ms`. `loop`
still controls the lifetime of each instance rather than the number of hits.

Set `delay_ms` on the same effect definition to delay its visual and first optional WAV.
It accepts a nonnegative whole number of milliseconds and defaults to `0`. It
works for inline and shared definitions, all attachments and renderers, and
effects started directly by server notifications. With `once_per_hit = true`,
the configured delay applies before each playback and accumulates across hits:
`delay_ms = 250` starts the first three playbacks at `250`, `660`, and `1070` ms;
`delay_ms = 750` starts them at `750`, `1660`, and `2570` ms. The first playback
waits `delay_ms`; subsequent playbacks are `160 + delay_ms` apart. With no delay
or `delay_ms = 0`, the original `160` ms spacing is preserved. For server-notified
effects, the first playback's delay starts when its effect notification arrives;
later playbacks are scheduled from the damage notification.

For `server_notified` attachments, the server's effect notification supplies the
first playback; the damage packet schedules only hits two onward. Unmarked
effects continue to play only from their server notifications. This assumes the
server sends each configured effect once per skill use. Packets without a hit
count, including successful no-damage and ground notifications, use one hit and
add no per-hit replays for server-notified effects. A zero hit count also uses one
hit.

For example, Hundred Spear's hit effect can repeat with a 250 ms delay while
components without `once_per_hit` remain single-playback effects:

```lua
{
    effect_id = 2037,
    str = [[data\texture\effect\servant_demolition\servant_demolition_hit\servant_demolition_hit.str]],
    once_per_hit = true,
    delay_ms = 250,
    bottom_effect = true,
    bottom_offset_px = 25,
},
```

Set `bottom_effect = true` on an effect definition to draw its visual behind
actor sprites. `bottom_offset_px` sets an additional downward shift in screen
pixels, defaulting to `4`; use `0` for the draw order alone. The value must be a
finite number between `0` and `10000`, and fractional pixels are allowed. The
shift is independent of camera zoom and adds to the existing world-space
`offset_y`. These fields work for inline effects and shared `effects` definitions,
including variants and procedural renderers. Omitted or false `bottom_effect`
uses the normal draw order and ignores `bottom_offset_px`.

```lua
{
    effect_id = 2026,
    str = [[data\texture\effect\4ig_explosionblaster\explosionblaster_bottom\explosionblaster_bottom.str]],
    bottom_effect = true,
    bottom_offset_px = 6,
},
```

Bottom effects draw after terrain and contact shadows, before actor sprites,
with their authored blend modes. Flat STR artwork keeps its original overlay
depth so terrain cannot cut through the image; actor sprites cover it through
draw order. Procedural effects retain their usual world depth testing.
Restart the client after changing the configuration.

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
For a brief textured barrier, use `renderer = "guard_sphere"` with a full
`texture` TGA path and optional `wav`. `radius` sets the horizontal radius,
`height` the full vertical height, and `offset_y` the center's attachment offset,
all in world units. `color` is an RGB integer and `alpha` ranges from 0 to 1.
`duration_ms`, `fade_in_ms`, and `fade_out_ms` define its lifetime and fades.
The fades must be positive and fit within the duration. These effects cannot
loop; the texture repeats across six columns and three curved bands. The barrier
uses `SRCALPHA` / `ONE` blending so texture transparency and lifetime fades both
control its additive glow.
Shared definitions can live in the top-level `effects` table, with skill entries
referencing them using just `{ effect_id = ... }`. Define each effect once.

For audio without replacing a skill's existing visuals, add its **skill ID** to
the separate top-level `sounds` table. Provoke is configured this way:

```lua
sounds = {
    [6] = { -- SK_SM_PROVOKE
        wav = [[data\wav\effect\swordman_provoke.wav]],
        trigger = "no_damage",
    },
},
```

Damage skills automatically play one hit WAV per reported hit, alongside any
completion sound configured in this file. Player casters use the same sound
selection as normal attacks, including equipped weapons and the normal
player-target hit sound.
Non-player or unresolved casters use `data\wav\effect\ef_hit2.wav`. Hit sounds use
`[WhiteDamageNumbers] DelayBetweenNumbersMs` from `open-midgard.ini` (default
`160` ms), including when damage numbers are red or critical. White-number queue
delays also postpone their hit sounds. Misses, recovery amounts, passive skills,
and animation-only damage packets do not produce default hit audio. Audio remains
independent of the effects visibility setting.

A `sounds` entry or a WAV in the skill's `source`, `target`, or `ground` effects
plays alongside the per-hit audio. This includes shared effect references,
variants, and procedural renderers. Each configured WAV plays once per skill
notification, even if its effect has `once_per_hit = true`; later visual copies
are silent.
Server-notified effects supply the first WAV themselves. Casting WAVs are a
separate stage and do not suppress completion hit sounds.

Set `silence_hit_sound = false` on the skill's entry in the `skills` table to
disable only its per-hit WAVs. Omit this field or set it to `true` to keep them
enabled. The flag must be a boolean and does not change configured completion
or casting WAVs. For example, add this field alongside a skill's existing
name and effect arrays:

```lua
silence_hit_sound = false, -- Disable per-hit audio; keep configured WAVs.
```

`wav` must be a full GRF path. `trigger` selects `"no_damage"` (the default,
successful buff/debuff and other non-damaging skill notifications), `"damage"`
(actor or position damage notifications), or `"ground"` (ground notifications,
including position damage notifications). This selects one packet path when a
skill sends several notification types. No-damage failures do not play audio.
Sounds play at the target or ground position, falling back to the caster for a
no-damage notification without a resolved target, with the usual sound settings
and distance attenuation. A matching ground or position sound replaces the
existing inferred ground sound. Effect-specific `wav` fields still play with
their effects, so avoid configuring the same sound in both places. Passive skill
entries cannot have a sound. The installed GRF contains Provoke's WAV; its current
visual remains handled by the existing skill/effect path.

The server can also request one-shot audio with `0x01D3` (`ZC_SOUND`, 35 bytes):
a 24-byte WAV filename relative to `data\wav`, an action byte, a 32-bit term,
and the actor ID. The client frames this packet and plays action `0` at that
actor's position using the same audio settings and distance attenuation. Avoid
adding a `sounds` entry for audio already requested by the server.

Magnum Break (`942`, `SK_SM_MAGNUM`) uses attack motion `2`. The server sends
its caster effects (`1385`, `1359`, `1465`) and `magnumbr.wav`, then damage
notifications for nearby enemies and a successful self-use notification even
when none are hit. The self-use notification drives the skill name and caster
animation; enemy damage notifications display their damage numbers. Previously,
the unregistered sound packet broke receive alignment, losing those subsequent
notifications. Magnum Break therefore has a server-notified skill entry and no
duplicate Lua sound entry. The `[GameMode] sound notify` log records the WAV,
actor resolution, and playback result.

Ignition Break (`943`, `SK_CM_IGNITIONBREAK`) also uses attack motion `2`.
Servers using the older successful-use path send a self-targeted skill damage
packet with the animation-only marker `-30000`, including when no enemies are
hit. The client plays the configured caster motion for this marker and skips
hit reactions, impact effects, damage numbers, and damage-triggered audio.
Real damage packets for nearby enemies retain their normal presentation. This
marker is handled for all skills in both actor and position damage packets.

`server_notified = true` means that effect-notification packets already trigger
the configured effects. Skill-use packets suppress the first playback, and only
schedule additional hits for effects marked `once_per_hit = true`. Direct server
packets carry only an effect ID; inline skill
definitions are also indexed by that ID. `source_server_notified`,
`target_server_notified`, and `ground_server_notified` can override the default
independently. This lets a skill play a missing caster presentation locally while
the server continues to drive its target effects. Effect IDs and skill IDs are
separate namespaces: skill `398` uses caster effect `399`, without affecting
skill `399` (Arcane Strike). `passive = true` requires empty effect
arrays. `begin_effect_id` defaults to `-1` (no cast-start effect), and
`motion_type` defaults to `7` (the generic skill action).

Set `cast_motion_type` for the caster animation when a positive-duration cast
starts, and `skill_motion_type` for the caster animation when successful use is
reported. The fields are independent: omitting either (or setting it to `-1`)
preserves that stage's existing `motion_type` behavior. Explicit stage settings
bypass the legacy cast-pose substitutions and skill-motion fallback.

```lua
[71] = { -- SK_CR_SWORDSTOPLOWSHARES
    name = "Swords to Plowshares",
    cast_motion_type = 1,
    skill_motion_type = 1,
    -- casting / source / target / ground effects go here.
},
```

Both fields accept these motion IDs:

| ID | Animation |
| --- | --- |
| `0` | Idle (no skill animation) |
| `1` | Skill invocation: hold the initial skill pose at either stage |
| `2` | Primary attack (uses the equipped weapon) |
| `5` | Pick up |
| `7` | Generic skill |
| `8` | Casting |
| `9` | Secondary attack |
| `16` | Dance (currently uses the generic skill animation) |
| `17` | Sing (currently uses the generic skill animation) |

Motion `1` holds the first frame of the player's job-specific skill action (ACT
band `96`). This is the hand-raising invocation pose for the male Paladin and
Lord Knight. It holds until casting ends, then successful use restarts and holds
the pose for the skill's motion duration (450 ms for a non-damaging use packet).
This avoids later attack-like frames in some jobs' skill actions. Use motion `7`
to play the full action. Copy the two motion fields in the example to any other
skill entry to get the same behavior; no skill-specific C++ handling is needed.
Legacy `motion_type` behavior is unchanged. The player action layout follows the
[roBrowser player action mapping](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/Renderer/Entity/EntityAction.js).

Unsupported IDs reject the presentation file with a log message. Other cast
animations play across the cast duration and hold their final frame until use,
cancellation, or expiry. Successful-use motions return to idle after their pose
duration or animation ends; attacks retain the existing weapon and attack timing.
Instant skills skip `cast_motion_type` and play only `skill_motion_type`.
These settings control the caster; effect
attachments remain configured by the four effect arrays.

Swords to Plowshares (`71`, `SK_CR_SWORDSTOPLOWSHARES`) uses the installed
Increasing Activity cast clip (`2045`, with `cd_reparatio1.wav`) and Competentia
ground glow (`2047`) through its `casting` array. Both attach to the caster and
stop when casting ends. Successful use plays the Increasing Activity target clip
(`2046`, with `cd_reparatio2.wav`) on the recipient. These internal effect IDs
leave the standard effect catalog unchanged. The server sends the skill-use
notification and a separate Heal notification for the recovered amount, rather than special-effect
notifications, so this entry leaves `server_notified` false. Failed or cancelled
casts do not emit its target effects. The accompanying Heal notification retains
its existing recovery number and Heal presentation. Its source ID is zero, so
it does not restart the caster's motion or replace the invocation pose.

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
damage packet. The server also sends `626` (`EF_MADNESS_RED`), now handled by
the shared, fading red sprite-glow renderer. The caster visual and sound need an in-game
comparison with the official client.

### Coverage and play checks

Passive HP/SP recovery boosts (`SK_SM_RECOVERY`, skill `4`, and
`SK_SM_SRECOVERY`, skill `9`) arrive through the `013d` recovery packet.
They use effects `331` (`EF_HPTIME`) and `332` (`EF_SPTIME`), independently of
skill-use presentation entries. To replace either recovery presentation, add
`[331] = { str = [[data\texture\effect\recovery.str]] }` for HP and/or
`[332] = { str = [[data\texture\effect\recovery.str]] }` for SP inside the existing
top-level `effects` table in `skill_presentation.lua`. The file includes
commented examples with the corresponding recovery WAVs. `recovery.str` is an
installed Priest Recovery animation used as an example; choose the STR you want.
Each override owns the complete visual and sound, so omitting `wav` makes that
boost silent. `offset_y` and paired `variants` also work. Restart the client to
reload changes. These entries use effect IDs `331`/`332`; passive skill entries
`4`/`9` do not control recovery packets.

Without a Lua override, both effects emit twelve small, rising
`data\texture\effect\pok1.tga` sparkles attached to the actor, with pale green
HP tint or pale blue SP tint, additive blending, twinkling, and fades. The
texture, particle count, spacing, color, and motion ranges follow the
[roBrowser effect definition](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Effects/EffectTable.js).
Each sparkle lasts 500 ms, starting 10 ms after the previous one; the effect
finishes after 610 ms. HP recovery plays `data\wav\_heal_effect.wav`; SP recovery
plays the absorption WAV whose CP949 filename bytes are `c8 ed b1 e2`.
The installed GRFs contain all three assets. The former expanding rings,
cylinders, and Priest Recovery WAV were unrelated to passive regeneration.
HP/SP updates and recovery numbers still use the existing packet handling.
The visual scale and timing still need an in-game comparison.

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

Sworn Protector (`255`, `SK_CR_SWORNPROTECTOR`) plays `devotion.str` on a successfully
protected ally. The server's `0x01CF` Devotion list separately owns up to five
blue links with a soft halo, bright white core, and travelling cross-shaped
glints from the caster to allies. Their width pulses every 1.8 seconds from the
existing thickness to 50% wider, and rounded fades soften both character ends.
Each packet replaces the complete target list: retained links preserve their
instance, added targets get a link, and
removed targets cancel immediately. The links follow both actors, respect the
packet's cell range and actor visibility, and stop when either actor is removed.
Disabling effects hides active links; re-enabling effects restores them. The
skill entry uses motion type `1` during casting and successful use.
Redirected damage arrives as a self-targeted normal/endure damage notification
on a caster with an active link. It displays damage numbers while preserving
the caster's movement, attack, or cast; it starts no attack or hit reaction.
Protected allies also keep their current action when hit while their link is
active. Normal and skill hits retain sounds, damage numbers, and impact visuals
without starting the ally's damage motion, including while effects are disabled.
Removing the link immediately restores normal hit reactions.

Counterstrike (`146`, `SK_SM_AUTOBERSERK`) is an active self buff in this server,
which sends `EF_MAXPAIN` (`1032`) on the caster. The gameplay log confirms that
notification and the client's former "no presentation" warning. Its owned skill
entry uses `server_notified = true`, so the successful-use packet drives the
caster motion and skill name without launching a second barrier or legacy GRF
skill effects. The shared effect entry uses `guard_sphere` with the installed
128-by-128 `guardK2.tga` and `effect\kyrie_guard.wav`, matching the assets in the
[captured effect reference](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/effects.json)
and its [effect 1032 clip](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/videos/effect_1032.webm).
No installed STR references this texture. The procedural barrier fades in over
100 ms, holds, then fades out over 200 ms, ending after 700 ms. It follows the
caster, uses world depth, respects delayed starts and cancellation, and plays
the guard WAV once when it starts. Its alpha-controlled additive pass preserves
the cross texture's transparent background and the configured fades; using the
`ONE` / `ONE` emissive pass instead produces a solid yellow sphere. Its geometry,
scale, tint, and timing are an approximation of the capture and still need
comparison in a running client.

Parry (`356`, `SK_CM_PARRY`) sends a successful no-damage skill notification on
activation and on each blocked attack, without a separate effect notification.
Its locally played target effect (`2042`) uses the same guard sphere, texture,
sound, and timing as Counterstrike, with `color = 0xFFFFFF` for a white barrier.
The target attachment also allows blocks during another cast to show the effect.
Only activation plays the character's skill animation: a per-actor cast
acknowledgement identifies the next Parry response as a manual cast. Automatic
blocks play the barrier while preserving the current character action and cast
effects. The acknowledgement expires after the cast time plus two seconds and
is cleared on use, failure, cancellation, or another cast.

Millenium Shields (`STATUS_MILLENIUMSHIELDS`, wire status `723` / `EFST_KINGS_GRACE`)
keeps one `holy_shield\holy_shield\holy_shield.str` loop attached to the affected
actor while active. The visual is configured in `effects[50016]` with `loop = true`.
Both `0196` and `043f` status packets drive it; timed statuses expire locally,
while legacy and infinite timers wait for removal. Refreshes preserve the loop's
animation phase. Removal, death, or actor deletion cancels it immediately;
disabling effects hides it, and re-enabling effects restores an active loop.
Statuses received before local actor creation or across map changes are restored
from the active status icons. The installed STR contains 80 frames at 60 FPS;
all 35 texture references are present. No sound is configured.

Bleeding (`STATUS_BLEEDING`) sends effect `548` (`EF_RED_HIT`) to the affected
actor every 500 ms in this server's status timer. It is a brief red sprite flash;
the [captured effect reference](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/videos/effect_548.webm)
uses no STR, texture, or WAV. The client now handles each notification with a
240 ms additive red flash that fades automatically. It follows the target's
current sprite and alpha, keeps depth order with other actors, and leaves shared
sprite textures intact. The server controls repetition, so no local status
timer or skill-specific duplicate is launched. Tint and timing still need an
in-game comparison with the capture.

The Knight buffs below now have owned skill entries with `server_notified = true`.
Gameplay logs confirm these effect notifications immediately before their
successful skill-use responses. The response supplies the skill name and motion;
it does not replay the server's effects or sounds.

| Skill | Server effects | Client presentation |
| --- | --- | --- |
| Adrenaline Rush (`835`, `SK_KN_ADRENALINERUSH`) | `750` (`EF_CONCENTRATION2`) | `concentration.str` and `effect\ac_concentration.wav`, configured in the shared Lua effects table |
| Fury (`876`, `SK_KN_FURY`) | `847` (`EF_DANCE1`), then `626` (`EF_MADNESS_RED`) twice | Brief red sprite glow and staggered white/red rising rays using `ac_center2.tga`; the existing server sound notification plays `SK_KN_FURY.wav` |
| Vigor (`2016`, `SK_KN_VIGOR`) | `538` (`EF_GREENBODY`) and `841` (`EF_BEGINSPELL_N4`) | Special effects and their casting WAV are suppressed; normal skill motion, name, status icon, and buff remain |
| Stoneskin (`2015`, `SK_KN_STONESKIN`) | `457` (`EF_JUMPKICK`) | Brief attack motion and the installed Taekwon flying-kick WAV (CP949 filename bytes `74 5f b3 af b6 f3 c2 f7 b1 e2`) |

The [roBrowser effect table](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Effects/EffectTable.js)
identifies Adrenaline Rush's STR and Stoneskin's sound. The
[captured effect metadata](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/effects.json)
and clips for effects `538`, `841`, and `847` guide the procedural visuals.
The installed GRFs contain the STR, all its referenced textures, the complete
27-frame cone sequence, the ray texture, and the sounds. The STR contains
50 frames at 60 FPS. Procedural glow, cone, and ray sizes/timing are approximations
that still need comparison in a running client. The capture of `626` reports
no standalone visible change, so its fading red glow is an approximation,
not a confirmed reconstruction of the official conditional effect.

Vigor's cast acknowledgement suppresses its two special-effect notifications
until the skill-use response. Cancellation, failure, another cast, or a timeout
also clears the suppression. This includes instant casts, whose effect packets
arrive before the skill-use response. The `server_notified` skill entry has no
local effects or audio, preventing the legacy skill presentation from replaying
them. The native green-glow and cone handlers remain available for other uses;
Acid Terror also sends `EF_GREENBODY` and retains its presentation.

Glows follow the actor's cached sprite, motion, transparency, and depth order.
Fury's repeated `626` notifications refresh one glow instead of accumulating
overlays. Green glow ends after 912 ms; red glow and the cone end after 1,512 ms.
Cone/ray texture alpha and lifetime fades use `SRCALPHA` / `ONE` blending.
All native handlers honor delayed starts and cancellation. The bleeding flash
retains its original immediate 240 ms fade through the shared draw path.
Stoneskin's server effect is a motion/sound effect; it does not reference a stone
texture or STR. Its successful-use response selects attack motion, and standalone
`457` notifications animate idle actors without restarting an existing attack.

Frenzy (`359`, `SK_CM_FRENZY`) maps `STATUS_FRENZY` to status notification
`107` (`EFST_BERSERK`) in the local server. The client now applies that status
to the affected actor as well as updating the local player's icon. While active,
the actor's current sprite has a steady red additive glow and a small red halo.
The glow follows movement, motion, transparency, and depth order, respects the
effect visibility setting, and uses separate state from brief hit/buff flashes.
It remains active until a removal notification arrives; duration-bearing `043f`
packets also provide an expiry. Legacy `0196` activations have no guessed timeout.
Repeated activations refresh one glow, and death clears it. Area notifications
apply it to other visible actors without adding their statuses to the local UI.
No build was attempted; compare its brightness and halo size in a running client.

Blood Sport (`2001`, `SK_CM_AURABLADE`) maps `STATUS_BLOODSPORT` to wire status
`103` (`EFST_AURABLADE`). Its red mist is server effect `871` (`EF_CIRCLEPOWER5`),
which previously had no client presentation. The native handler uses the installed
`freeze_a.bmp` texture for red wisps that rise around and follow the actor, with
additive alpha fades and no sound. Status activation starts the mist immediately;
the server's five-second effect notifications refresh the existing aura without
stacking layers or restarting its animation. A notification received before the
status is adopted by the status aura. Standalone `871` notifications last five
seconds and can be refreshed by later notifications.

The status aura ends on removal, timed expiry, death, or actor deletion. Legacy
`0196` activations wait for removal; `043f` supplies the remaining duration.
Bootstrap/map changes restore the local player's remaining status, and the
effect visibility setting hides/restores the mist while the status stays active.
Blood Sport's configured cast effects `2033`/`2034` remain separate from this aura.
No build or in-game test was attempted; compare the mist's size and opacity in a
running updated client.

Rend (`953`, `SK_CM_REND`) is the automatic attack triggered by Release the Beast.
The server's damage sender previously replaced its skill ID with `413`, causing
the client to display Tornado Kick and use that skill's presentation. The sender
now preserves `953`. Rend's damage calculator sends target effects `2031` and
`2032`, matching its configured Massive Flame Blaster and Axe Boomerang STRs,
instead of the older Cart Cannon effects `1361` and `1359`. Those older IDs keep
their existing meanings for other skills. Rend uses attack motion, and
`server_notified = true` prevents its damage packets from replaying the target
effects. Configured presentation names take priority over GRF labels in the skill
UI and actor shouts. Rend is the exception: its UI name remains "Rend", but its
automatic hits do not create or refresh an overhead skill-name shout. Its attack
motion and effects still run. Both the client and server changes must be deployed
together.
No build or in-game test was attempted.

Sonic Wave (`2002`, `SK_KN_SONICWAVE`) now uses an owned Lua skill entry with
attack motion and locally launched caster effect `1678`. The local server sends
skill-damage notifications for this skill, without a separate special-effect
notification. Skill ID `2002` is distinct from effect ID `2002`, which remains
Spirit Growth's configured effect. The installed GRF has no Sonic Wave STR;
the new `sonic_wave` renderer uses its `new_sonicwave` core and trail textures,
matching the assets in the
[captured effect metadata](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/effects.json)
and the blue crescents in the
[effect 1678 clip](https://github.com/lukasrmattos/ragnarok_effects_list/blob/main/videos/effect_1678.webm).
The older effect `832` is a different presentation and is not substituted here.

The renderer emits three 360 ms crescents, 336 ms apart, for a total lifetime of
1,032 ms. Each travels from the caster's position to the target position supplied
by the skill packet. The positions and direction are captured when the effect
starts, so subsequent caster movement or removal does not move the wave's path.
Direct effect notifications without a target use a short path in the caster's
facing direction. The effect respects delayed starts, cancellation, world depth,
and the effect visibility setting. Culling includes the whole path. Core and
trail alpha fade separately with `SRCALPHA` / `ONE` blending; the captured modern
effect is silent, so this entry omits `wav`.

The Lua entry exposes `texture`, `trail_texture`, `radius`, `height`, `offset_y`,
`wave_count`, `wave_interval_ms`, and `wave_duration_ms`; an optional `wav` plays
once when the effect starts. Counts are limited to 1–8, intervals to 0–5,000 ms,
and durations to 1–5,000 ms. This renderer cannot loop. The geometry and timing
approximate the capture and still need comparison in a running updated client.
No build was attempted.

Spear Cannon (`59`, `SK_KN_SPEARCANNON`, the server's renamed Spear Boomerang)
receives its presentation from damage notifications, including follow-up attacks
without a cast acknowledgement. Its Lua entry uses `motion_type = 1` and locally
plays caster effect `151` (`spearboomerang.str`) with
`knight_spear_boomerang.wav`; the server does not notify these effects separately.
The world projectile renderer uses the installed `data\sprite\이펙트\창.spr` /
`.act` pair, sharing the effect frame cache. It rotates the spear toward the
projected target, scales it to 150% sprite size with camera depth, fades it over a 180 ms flight,
and resolves the target's current position when the spear is released. Flight
starts late enough to end at the attack's impact time when that time exceeds
180 ms. The existing skill-presentation guard prevents duplicate launches.
The separate `sounds[59]` entry plays `ef_fireball.wav` at the damage position;
the reference hit effect `80` is audio only, with no separate STR. These asset
assignments follow the [Spear Boomerang skill reference](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Skills/SkillEffect.js)
and [effect reference](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/DB/Effects/EffectTable.js).

Shield Chain (`860`, `SK_PA_SHIELDCHAIN`) starts one spinning shield on successful
use. Its separate chain renderer uses the same masked shield texture and size as
Shield Boomerang, whose implementation is unchanged. The server reports the
first hit as `946` (`SK_PA_SHIELDCHAIN_ATK`) after 150 ms, followed by bounces at
200 ms intervals. The first damage packet confirms the initial destination;
later packets queue target-to-target legs without replaying the caster's motion
or skill-name shout. The first leg takes 280 ms, later legs take 200 ms, and after
300 ms without another hit the shield finishes its queued legs and returns to
the caster's current position over 280 ms. Rotation stays continuous and only
release/catch fade. Each caster has an independent chain; recasting replaces that
caster's previous chain. Missing targets retain their notified positions, caster
removal cancels the chain, and the visual respects the effect visibility setting.
The existing server notifications retain ownership of `shieldchain.wav`.
Successful use also plays `absolute_zephyr\absolute_zephyr_bottom\absolute_zephyr_bottom.str`
on the caster as a bottom effect with a 25-pixel offset. Each `946` damage
notification plays `absolute_zephyr\absolute_zephyr_hit\absolute_zephyr_hit.str`
on its target. These reuse the registered effect definitions `2076` and `2075`.

Shield Slam (`947`, `SK_PA_SHIELDSLAM`) uses the same masked
`data\texture\effect\shield_boomerang.bmp` asset at 150% size (96 pixels).
Its separate renderer drops the rotating shield from 48 world units above the
target over 560 ms, accelerating into impact and fading out over another 100 ms.
Rotation keeps its 280 ms period, completing two turns during the fall.
Release is delayed when the caster's attack impact time exceeds 560 ms so the
shield lands at that impact time. The shield follows a moving target, retains
the last target position after removal, scales with camera depth, and respects
the effect visibility setting. Actor and position damage notifications use the
same presentation guard to avoid duplicate launches. Shield Chain and Shield
Boomerang retain their existing behavior.

Rapid Smiting (`480`, `SK_PA_RAPIDSMITING`) creates shields around the target using
`data\texture\effect\shield_boomerang.bmp`. The damage notification supplies the
shield count (five on the current server, capped at 32 for rendering). All shields
appear together over 60 ms on a camera-facing circle, then simultaneously sweep
across the target over 440 ms, traveling 160 pixels on either side of the target
before camera scaling. The 72-pixel shields
spin every 160 ms and leave three cyan/gold afterimages. All paths cross the
target's center at the same moment, accompanied by one combined gold ring, flash,
and radial sparks. They track the target until impact, then finish around that
impact position, retaining it if the actor is removed. A shared start time and
delay keep every shield synchronized, aligning the crossing with the caster's
attack timing when it exceeds the 280 ms appearance/approach time. Actor and
position notifications share the existing presentation guard. The effect
respects visibility, camera depth, and culling around the target. Other shield
skills retain their existing presentations.

Grand Cross (`254`, `SK_CR_GRANDCROSS`) keeps the same casting effects as Genesis
Ray (`2057`/`2058`). On completion, the server sends `EF_GRANDCROSS` (`226`);
the client draws its native ground geometry once for that notification. The
installed GRFs have no matching STR. The renderer reuses
`data\texture\effect\ring_blue.tga` for a nine-cell cross with smooth concave
corners, a faint blue floor glow, a feathered rim, and tall translucent curtains
of streaked light. The curtains have an icy blue base and fade to transparent at
their tops. A second cross at 55% size has shorter, softer curtains inside the
outer cross. Both rotate together through 45 degrees during the effect, using
the same clock and transform. Twenty-four pale blue `pok1.tga` sparkles appear
in staggered groups, rise and drift with the rotation, then twinkle and fade;
no particles outlive the cross. The aura stays on the caster's floor plane at the
completion position when the caster moves or disappears. It uses overlay depth
in the bottom-effect pass: terrain cannot cut holes through the surface, and
actors draw over it.
It appears over 100 ms and fades out over the last 450 ms of its one-second
lifetime, with one `data\wav\effect\cru_grand cross.wav` playback. Effects visibility
and cancellation are respected; skill-use and damage packets do not launch copies.

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
| Counterstrike / Auto Berserk (146) | One brief orange cross barrier around the caster and one guard WAV; no duplicate effect on the successful-use packet or lasting barrier after the buff is applied |
| HP Recovery (4) / SP Recovery (9), below maximum HP/SP | Each boost shows its recovery amount and one brief group of rising green/blue sparkles, with its corresponding recovery sound |
| Magnum Break (942), with no nearby enemies | One caster animation and "Magnum Break!!" message, one server-requested `magnumbr.wav`, and the server's caster effects |
| Magnum Break (942), with nearby enemies | Same caster presentation, plus damage numbers on every hit enemy; following packets stay aligned |
| Magnum Break (942), after-cast delay notification | No "Status 46" buff tile in either UI; actual buffs still display normally |
| Traumatic Strike (398) | One caster body flash/halo, delayed radial burst, and Head Crush WAV; server-driven `cwound.str` on the target |
| Bleeding from Traumatic Strike (398) / Spear Stab (58) | Each server effect `548` notification briefly flashes the affected target red; normal sprite color returns between the 500 ms ticks and after bleeding ends |
| Spear Quicken (816) | One head-attached `twohand.str` effect and one `knight_twohandquicken.wav` sound per successful cast |
| Spear Cannon (59), direct use or follow-up attack | One caster burst and spear WAV; a correctly oriented spear at 150% sprite size flies toward the target over 180 ms and fades; one `ef_fireball.wav` at the damage position |
| Shield Boomerang (251), near/far targets, moving targets/caster, camera rotation/zoom, repeated casts, caster removal, and effects visibility | One spinning shield uses its full 64-pixel size, reaches the target in 280 ms, and returns to the caster's current position over another 280 ms; rotation continues at the same rate through impact, with fading only at release and catch; removing the caster cancels the return; the magenta background stays transparent; one Shield Boomerang WAV plays with the damage presentation |
| Shield Chain (860/946), one/multiple targets, moving/deleted targets, moving/removed caster, simultaneous casters, recasting, delayed packets, and effects visibility | One continuously spinning shield flies caster to first target, then target to target in notification order, and returns to the caster after the final bounce; the first damage packet does not duplicate the first leg, follow-up hits do not replay the caster, and Shield Boomerang is unchanged |
| Shield Slam (947), moving/removed targets, repeated casts, camera rotation/zoom, and effects visibility | One 150%-size spinning shield falls over 560 ms, accelerates into the target, and briefly fades; rotation keeps its existing speed, longer attack motions delay release to align impact, and Shield Chain and Shield Boomerang are unchanged |
| Rapid Smiting (480), varied hit counts, moving/removed targets, camera rotation/zoom, and effects visibility | Shields appear together around the target, simultaneously pass through their center, and exit on opposite sides with spinning cyan/gold afterimages and one combined impact burst; the count follows the packet and other shield skills are unchanged |
| Sonic Wave (2002), near/far targets and varied camera angles | Attack motion and three blue crescents travel toward the target and fade within 1,032 ms; moving or removing the caster preserves their path, and Spirit Growth retains its own effect |
| Multi-hit skill, including actor and position damage notifications | Individual hit numbers appear 160 ms apart by default; white numbers use the configured spacing, and one larger yellow total follows the last displayed hit by `DelayAfterLastHitMs` (default 320 ms), without losing division remainders or truncating totals above 999,999 |
| Hundred Spear (2312), one hit and multiple hits | Effects marked `once_per_hit` play once per reported hit, counting the server's first playback; with `delay_ms = 750`, the first waits 750 ms and repeats are 910 ms apart; unmarked components play once |
| Damage skills with hit audio enabled, player and non-player casters, including Pierce | Players use normal-attack hit audio; non-players use `ef_hit2.wav`; one sound per hit uses the configured white-number spacing, including any white-number queue delay; misses, recovery, and animation-only packets remain silent |
| Damage skills with a configured completion WAV, inline/shared/variant/procedural effects and `sounds` entries | Per-hit audio also plays; each configured WAV plays once per skill notification, including multi-hit skills and server-notified effects |
| Skill entry with `silence_hit_sound` omitted, `true`, or `false` | Omitted and `true` enable per-hit audio; `false` disables only per-hit audio and keeps configured WAVs |
| Mixed per-hit and single-playback effects, with server notification enabled/disabled | Only `once_per_hit = true` visuals repeat; locally played effects include hit one, server-notified effects add only hits two onward, and only the first playback emits its configured WAV |
| Effect with `delay_ms = 250`, single playback and per-hit replay, with server notification enabled/disabled | Visual and first WAV start 250 ms later; per-hit visuals start at 250, 660, and 1070 ms with later copies silent, and omitting the delay or setting it to zero preserves the original visual timing |
| White damage numbers, with varied camera angles and attacker positions | Numbers start `RightDisplacement` pixels right of the actor, rise at most `MaxTravelHeight` pixels, and travel an additional `MaxTravelWidth` pixels right; zero values disable the corresponding motion, and spacing is tracked separately for each target |
| Yellow totals, with varied delays and heights | Totals fade in after the configured delay, start `StartingHeightDisplacement` pixels above the target, and rise an additional `MaxTravelHeight` pixels while fading out; zero delay starts with the last hit, and zero travel holds the starting height |
| Adrenaline Rush (835) | One concentration burst and concentration WAV on the caster, with no duplicate burst from the skill response |
| Fury (876) | Brief red sprite glow and white/red rising rays; duplicate `626` notifications do not intensify the glow; one server-requested Fury WAV |
| Frenzy (359), activation, movement/attacks, refresh, and removal | Red glow and halo persist while status `107` is active, including after brief hit/buff flashes; toggle-off, expiry, and death end the glow; remote actors have their own glow and no local status icon |
| Blood Sport (2001), activation, movement/attacks, refresh, expiry, and effects visibility | Red mist starts with status `103` and follows the actor for its full duration; five-second `871` packets do not stack or restart it; removal and death end it, and re-enabling effects restores an active aura |
| Millenium Shields (5013), activation, movement/attacks, refresh, removal, and effects visibility | One Holy Shield STR repeats while status `723` is active; refreshes preserve its phase, removal/death/deletion cancel it, and re-enabling effects restores an active loop |
| Rend (953), automatic hits while Release the Beast is active | Attack motion without an overhead Rend shout, one target effect each for `2031` and `2032`, and no Tornado Kick presentation or old Cart Cannon effects |
| Vigor (2016) | Buff and status icon apply with normal skill motion/name; no green glow, cone, or casting WAV; Acid Terror still shows its green glow |
| Stoneskin (2015) | Brief attack motion and flying-kick WAV for server effect `457`; no duplicate motion restart |
| Arcane Strike (399) | Three minimal Comet components and one `WindHit` animation on the target, with one corresponding `_hit_fist` sound |
| Actor skill outside range | Click and release; walk into range, then cast once |
| Ground skill outside range | Walk into range and cast at the chosen cell |
| Genesis Ray (2321), casting at empty ground or several enemies, cancellation, and movement after casting | Casting effects stop at completion; the main STR plays once at the selected cell even with no victims, stays there when the caster moves, and does not start for cancelled casts; each damaged enemy retains its target effect |
| Grand Cross (254), empty area/enemies, movement or caster removal after completion, repeated casts, camera rotation/zoom, slopes, cancellation, and effects visibility | Genesis Ray's casting effects stop at completion; two nested crosses with curved corners and translucent light curtains rotate together through 45 degrees with sparse rising sparkles, remain whole on slopes and beneath actors at the cast position, and expire together after one second with one Grand Cross WAV; no completion cross for cancelled casts or duplicates from damage packets |
| Sworn Protector (255), up to five allies, recasting, replacing/removing targets, movement, hiding, actor deletion, camera zoom, and effects visibility | Motion 1 and one target STR on successful use; blue links with a white core and travelling glints follow both endpoints, pulse between the existing width and 50% wider with soft rounded ends even at short distances, repeated/reordered lists do not duplicate them, removed links disappear, and re-enabling effects restores active links |
| Sworn Protector (255), redirected normal/skill damage while idle, walking, attacking, or casting, including effects disabled and link removal | Transferred damage shows numbers without starting attack/hit animations or interrupting the caster's action; direct enemy hits still play their normal hit reaction, and cancelled links do not suppress later damage animations |
| Sworn Protector (255), protected ally hit by normal, critical, or multi-hit attacks and skills while idle, walking, attacking, or casting | Damage visuals and sounds still play, but the ally keeps its action without a damage animation while linked; effects disabled preserves protection, and removing/replacing the ally or deleting the caster restores normal hit reactions |
| Low-level hotkey for a higher learned skill | Send the level saved in the hotkey |
| Shift while targeting; click empty space | Preserve skill selection and casting behavior |
| Teleport / Warp Portal | Select a destination or cancel; subsequent skills still work |
| Auto Spell | Select an offered learned spell or cancel |
| Pharmacy / forging / arrow creation | Select items and successive materials; compare cancellation |
| Repair Weapon / Sense | Repair an offered item; display the returned monster information |
| Sanctuary / Magnus / Epiclesis / Psychic Wave | Ground effect persists and disappears with the server unit |
| Gospel (369 / custom 951), activation beside walls/props and map edges, walking, camera rotation/zoom, slopes, repeated activation, and expiry | Both skill IDs use motion 1 to hold the caster's invocation pose; one complete area of upright glowing crosses follows the caster at the caster's floor height, including blocked cells omitted by the server (custom 951: 5x5; native 369: 33-cell cross); crosses stay visible over walls/props and sort in front of or behind actors by depth; unit updates do not duplicate or restart the aura, and removal of its last server unit removes the entire visual |
| Hold the Line (950), activation, walking, walls/map edges, repeated activation, toggle-off, expiry, and actor removal | The existing cast effects still play; dense fog consistently covers every cell of the full 5x5 square around the caster, including cells omitted by server path checks; overlapping fog textures drift gently at steady opacity; moving unit updates do not duplicate or restart the fog, and removal of its last server unit or the caster removes the visual |
| Bolts / small Heal / buff while another actor casts | Show hit/heal/buff effects without disturbing that cast |
| Demonstration / ninja effects | Sprite assets load, rotate, and remain fully visible |
| Newer effects with multiple STR components | Show each component at the authored speed |

Also check both native and Qt menus and the existing `AutoAttack=0`/`1` behavior,
including holding and releasing the mouse. Remaining failures should be recorded
with the skill ID, chosen level, target, packet opcode, and missing-effect log.
