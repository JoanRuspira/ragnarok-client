-- Open Midgard's source of truth for the skills migrated here.
-- Paths are full GRF keys; variants pair each STR with its WAV.
-- Damage skills use normal player attack hit audio,
-- or data\wav\effect\ef_hit2.wav for non-player casters, once per hit.
-- Hit audio follows WhiteDamageNumbers.DelayBetweenNumbersMs in open-midgard.ini.
-- A skills entry can set hit_wav to replace its default per-hit audio.
-- Completion WAVs play alongside hit audio, once even for per-hit visuals.
-- Keep per-hit audio enabled for every skill, including those with completion WAVs.
-- Casting WAVs belong to the casting stage; omit wav and loop_wav for a silent effect.
-- loop_wav repeats independently of visual timing while the effect is looping.
-- source/target/ground select the attachment. server_notified means the server
-- already sends each effect_id: skill-use packets must not launch them again.
-- Per-attachment *_server_notified overrides allow locally played caster effects.
-- source_once_per_cast = true plays the caster's motion, shout, and source effects once per cast.
-- casting effects attach to the caster for the cast's duration and stop together.
-- casting_ground effects share that lifetime but stay at the targeted ground cell.
-- Add casting_ground = { { effect_id = 50048 } } to reuse the shared ground circle.
-- Nonempty casting/casting_ground arrays replace the legacy begin_effect_id.
-- ground effects play at the notified ground cell after casting and stay there.
-- cast_motion_type and skill_motion_type independently select each stage's motion.
-- Omit either to preserve that stage's legacy motion_type behavior.
-- Motion 1 holds the job's skill invocation pose at either stage; 7 plays the full action.
-- Effect entries support bottom_effect = true to draw behind actors, with
-- bottom_offset_px = 4 (default) shifting the visual down in screen pixels.
-- Bottom STRs can set draw_over_actors = true to keep the offset and overlay actors.
-- once_per_hit = true repeats an effect at the damage packet's hit cadence.
-- For server-notified effects, the server supplies the first playback.
-- delay_ms = 0 (default) delays the visual and its first WAV; per-hit visual delays accumulate.
-- STR effects can set hue_shift_degrees = 0..360; omitted or 0 keeps original colors.
-- saturation_percent = 100 (default) keeps STR colors; 0 gives a white/gray glow.
-- ground_plane = true lays an STR flat on the ground; rotation_speed is radians per second.
-- play_count = 1 (default) sets total STR playbacks; use 2 to play twice, then stop.
-- replay_wav = true repeats an STR's WAV on each additional play_count playback.
-- ignore_server_wav = true ignores server sound packets for that effect's WAV.
-- additional_strs adds simultaneous STR layers with the same attachment, styling, and lifetime.

-- Ground casting circles share their geometry and timing; color is an optional 0xRRGGBB tint.
local function ground_casting_circle(color)
    return {
        renderer = "ground_circle",
        texture = [[data\texture\effect\magic_target.tga]],
        radius_cells = 4.5, -- A 9-cell-wide circle; matches Earth Strain's server radius 4.
        rotation_speed = 1.0, -- Radians per second; 0 holds still, negative reverses direction.
        color = color, -- Omitted uses the original texture colors.
        bottom_effect = true,
        bottom_offset_px = 0,
    }
end

local tetra_vortex_presentation = {
    name = "Tetra Vortex",
    cast_motion_type = 1,
    skill_motion_type = 2,
    casting = {
        { effect_id = 50080 }, -- White casting clip with begin-spell sound.
    },
}

return {
    version = 1,
    -- Skill IDs here add audio while preserving the skill's existing visuals.
    -- trigger selects "no_damage" (default), "damage", "ground", or "ground_cast".
    -- ground_cast plays only on cast completion, without restarting on damage ticks.
    -- play_count (default 1), interval_ms, and delay_ms control sound-only playback.
    -- ignore_server_wav = true prevents matching server sound packets from adding copies.
    sounds = {
        [2222] = { -- SK_SO_SUMMONELEMENTALSPHERES
            wav = [[data\wav\effect\ef_napalmbeat.wav]],
            trigger = "no_damage",
        },
        [90] = { -- SK_WZ_STALAGMITE
            wav = [[data\wav\effect\lg_earthdrive.wav]],
            trigger = "damage",
        },
        [927] = { -- Tetra Vortex: four earth spheres.
            wav = [[data\wav\effect\lg_earthdrive.wav]],
            trigger = "no_damage",
        },
        [928] = { -- Tetra Vortex: four wind spheres.
            wav = [[data\wav\effect\poemofnetherworld.wav]],
            trigger = "no_damage",
        },
        [929] = { -- Tetra Vortex: four water spheres.
            wav = [[data\wav\effect\iq_first_faith_power.wav]],
            trigger = "no_damage",
        },
        [895] = { -- Tetra Vortex: four fire spheres.
            wav = [[data\wav\effect\sr_raisingdragon.wav]],
            trigger = "no_damage",
        },
        [930] = { -- Tetra Vortex: one sphere of each element.
            wav = [[data\wav\effect\lg_kings_grace.wav]],
            trigger = "no_damage",
        },
        [2449] = { -- SK_WZ_PSYCHICWAVE: five sound plays without repeating the visuals.
            wav = [[data\wav\effect\sage_spell breake.wav]],
            trigger = "ground_cast",
            play_count = 5,
            interval_ms = 750, -- The WAV lasts about 1407 ms; keep each playback distinct.
            ignore_server_wav = true,
        },
        [2211] = { -- SK_WZ_CRIMSONROCK
            wav = [[data\wav\crimson_r.wav]],
            trigger = "damage",
        },
        [884] = { -- SK_WZ_THUNDERSTORM
            wav = [[data\wav\effect\ef_thunderstorm.wav]],
            trigger = "damage",
        },
        [85] = { -- SK_WZ_LORDOFVERMILION
            wav = [[data\wav\effect\ag_tornado_storm.wav]],
            trigger = "ground",
        },
        [2446] = { -- SK_WZ_EARTHSTRAIN
            wav = [[data\wav\effect\em_terra_drive.wav]],
            trigger = "ground",
        },
        [366] = { -- SK_SO_MYSTICALAMPLIFICATION
            wav = [[data\wav\effect\lg_prestige.wav]],
            trigger = "no_damage",
        },
        [5012] = { -- SK_SO_VOIDMAGEPRODIGY
            wav = [[data\wav\effect\dk_dragonic_aura2.wav]],
            trigger = "no_damage",
        },
        [2206] = { -- SK_SO_HINDSIGHT
            wav = [[data\wav\effect\cd_argutus_vita.wav]],
            trigger = "no_damage",
        },
        [403] = { -- SK_WZ_FORESIGHT
            wav = [[data\wav\effect\bo_fairy_dusty.wav]],
            trigger = "no_damage",
        },
        [933] = { -- SK_SO_CLIMAX
            wav = [[data\wav\effect\bo_advance_protection.wav]],
            trigger = "no_damage",
        },
        [6] = { -- SK_SM_PROVOKE
            wav = [[data\wav\effect\swordman_provoke.wav]],
            trigger = "no_damage",
        },
        [402] = { -- SK_SA_MINDGAMES
            wav = [[data\wav\effect\swordman_provoke.wav]],
            trigger = "no_damage",
        },
        [2201] = { -- SK_SA_WHITEIMPRISON
            wav = [[data\wav\effect\wl_whiteimprison.wav]],
            trigger = "no_damage",
        },
        [288] = { -- SK_PF_LANDPROTECTOR
            wav = [[data\wav\effect\ef_beginspell.wav]],
            trigger = "casting",
        },
        [894] = { -- SK_PF_STASIS
            wav = [[data\wav\effect\ef_beginspell.wav]],
            trigger = "casting",
        },
        [59] = { -- SK_KN_SPEARCANNON / Spear Boomerang impact
            wav = [[data\wav\effect\ef_fireball.wav]],
            trigger = "damage",
        },
        [72] = { -- SK_CR_BINDINGOATH: sound only; no custom visual effect.
            wav = [[data\wav\effect\cd_benedictum.wav]],
            trigger = "no_damage",
        },
        [253] = { -- SK_CR_HOLYCROSS
            wav = [[data\wav\cru_holy cross.wav]],
            trigger = "damage",
        },
        [251] = { -- SK_CR_SHIELDBOOMERANG
            wav = [[data\wav\effect\cru_shield boomerang.wav]],
            trigger = "damage",
        },
        [946] = { -- SK_PA_SHIELDCHAIN_ATK: each target hit in the chain.
            wav = [[data\wav\effect\cru_shield boomerang.wav]],
            trigger = "damage",
        },
        [839] = { -- SK_MG_EARTHBOLT
            wav = [[data\wav\effect\bo_acidified_zone_ground.wav]],
            trigger = "damage",
        },
        [14] = { -- SK_MG_COLDBOLT
            wav = [[data\wav\effect\wizard_icewall.wav]],
            trigger = "damage",
        },
        [19] = { -- SK_MG_FIREBOLT
            wav = [[data\wav\effect\ef_fireball.wav]],
            trigger = "damage",
        },
        [20] = { -- SK_MG_LIGHTNINGBOLT
            wav = [[data\wav\chainlight.wav]],
            trigger = "damage",
        },
        [157] = { -- SK_MG_ENERGYCOAT
            wav = [[data\wav\effect\ag_deadly_projection.wav]],
            trigger = "no_damage",
        },
        [15] = { -- SK_MG_FROSTDIVER
            wav = [[data\wav\effect\ef_frostdiver2.wav]],
            trigger = "damage",
        },
    },
    skills = {
        [3] = {
            name = "Two-Handed Sword Mastery", -- SK_SM_TWOHAND
            passive = true, -- A passive weapon bonus, with no cast visual or sound.
        },
        [13] = { -- SK_MG_SOULSTRIKE
            name = "Null Strike",
            hit_wav = [[data\wav\effect\ef_soulstrike.wav]],
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 2101,
                    -- Keep the requested filename's CP949 bytes intact.
                    str = [[data\texture\effect\]] .. "\188\250\189\196\192\252\176\1794.str",
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                },
            },
        },
        [340] = { -- SK_MG_DARKSTRIKE
            name = "Dark Strike",
            hit_wav = [[data\wav\effect\ef_soulstrike.wav]],
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 2102,
                    -- Keep the requested filename's CP949 bytes intact.
                    str = [[data\texture\effect\]] .. "\188\250\189\196\192\252\176\1795.str",
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                },
            },
        },
        [14] = { -- SK_MG_COLDBOLT
            name = "Cold Bolt",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 2091,
                    str = [[data\texture\effect\mineffect\new_tetravortex\new_tetravortex_water_bottom\new_tetravortex_water_bottom.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            target = {
                {
                    effect_id = 2092,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_b_bottom\acidified_zone_water_b_bottom.str]],
                    bottom_effect = true,
                    delay_ms = 100, -- Add 100 ms between iterations for a 260 ms cadence.
                },
                {
                    effect_id = 2093,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_water\acidified_zone_water_water.str]],
                    once_per_hit = true,
                    delay_ms = 100,
                },
                {
                    effect_id = 2094,
                    str = [[data\texture\effect\new_jackfrost\new_jackfrost\new_jackfrost_01.str]],
                    delay_ms = 100,
                },
            },
        },
        [15] = { -- SK_MG_FROSTDIVER
            name = "Frost Diver",
            source = {
                {
                    effect_id = 50023,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_b_bottom\acidified_zone_water_b_bottom.str]],
                    bottom_effect = true,
                },
                {
                    effect_id = 50024,
                    str = [[data\texture\effect\adv_protection\adv_protection_bottom\adv_protection_bottom.str]],
                    bottom_effect = true,
                },
            },
            target = { -- Freeze status also loops these effects for any affected actor.
                {
                    effect_id = 50025,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_water\acidified_zone_water_water.str]],
                },
                {
                    effect_id = 50026,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_cast\acidified_zone_water_cast.str]],
                },
            },
        },
        [2204] = { -- SK_MG_ICEWALL
            name = "Jack Frost",
            source = { -- Caster presentation is deduplicated across area-damage targets.
                {
                    effect_id = 50030, -- Separate from Frost Diver's silent shared effect 50023.
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_b_bottom\acidified_zone_water_b_bottom.str]],
                    wav = [[data\wav\effect\ef_frostdiver2.wav]],
                    bottom_effect = true,
                },
                { effect_id = 50024 },
            },
            target = {
                {
                    effect_id = 50027,
                    str = [[data\texture\effect\new_jackfrost\new_jackfrost_magicrune\new_jackfrost_frost.str]],
                },
                {
                    effect_id = 50028,
                    str = [[data\texture\effect\new_jackfrost\new_jackfrost_cast\new_jackfrost_cast.str]],
                },
            },
        },
        [16] = { -- SK_MG_STONECURSE
            name = "Stone Curse",
            source = { -- Instant cast; the client presents its cast acknowledgement.
                {
                    effect_id = 2256,
                    str = [[data\texture\effect\wooden_fairy\wooden_cast_bottom\wooden_cast_bottom.str]],
                    bottom_effect = true,
                },
            },
            target = {
                {
                    effect_id = 2088,
                    str = [[data\texture\effect\terremotus\terremotus\terremotus.str]],
                    once_per_hit = false,
                },
            },
        },
        [76] = { -- SK_SA_SILENCE
            name = "Silence",
            source = {
                { effect_id = 2023 },
                {
                    effect_id = 50104,
                    str = [[data\texture\effect\new_fatal_menace\new_fatal_menace_cast\new_fatal_menace_cast.str]],
                    hue_shift_degrees = 300,
                },
            },
            target = {
                {
                    effect_id = 193,
                    str = [[data\texture\effect\silence.str]],
                    wav = [[data\wav\_silence.wav]],
                },
                { effect_id = 50024 },
            },
        },
        [288] = { -- SK_PF_LANDPROTECTOR
            name = "Land Protector",
            casting = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
            source = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
        },
        [894] = { -- SK_PF_STASIS
            name = "Stasis",
            casting = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
            source = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
            ground = {
                {
                    effect_id = 50107,
                    str = [[data\texture\effect\ardor\ardor_cast_bottom\ardor_cast_bottom.str]],
                    hue_shift_degrees = 210,
                    bottom_effect = true,
                    bottom_offset_px = 0,
                    play_count = 15, -- Forty frames at 60 FPS, repeated for ten seconds.
                },
                {
                    effect_id = 50108,
                    str = [[data\texture\effect\absolute_zephyr\absolute_zephyr\absolute_zephyr.str]],
                    hue_shift_degrees = 250,
                    play_count = 20, -- Thirty frames at 60 FPS, repeated for ten seconds.
                },
            },
        },
        [289] = { -- SK_SA_DISPELL
            name = "Dispel",
            source = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
            target = {
                {
                    effect_id = 50105,
                    str = [[data\texture\effect\]] .. "\184\197\193\247\183\206\181\229.str",
                    wav = [[data\wav\effect\sage_magic rod.wav]],
                },
            },
        },
        [402] = { -- SK_SA_MINDGAMES
            name = "Mind Games",
            source = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
            target_server_notified = true,
            target = {
                {
                    effect_id = 507,
                    str = [[data\texture\effect\mapae.str]],
                },
            },
        },
        [2201] = { -- SK_SA_WHITEIMPRISON
            name = "Magic Cell",
            source = {
                { effect_id = 2023 },
                { effect_id = 50104 },
            },
            target = {
                {
                    effect_id = 234,
                    str = [[data\texture\effect\spell.str]],
                },
            },
        },
        [92] = { -- SK_MG_QUAGMIRE
            name = "Quagmire",
            source = {
                { effect_id = 2256 }, -- Shared Wooden Fairy caster bottom effect.
            },
        },
        [19] = { -- SK_MG_FIREBOLT
            name = "Fire Bolt",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 2095,
                    str = [[data\texture\effect\mineffect\new_tetravortex\new_tetravortex_fire_bottom\new_tetravortex_fire_bottom.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            target = {
                {
                    effect_id = 2096,
                    str = [[data\texture\effect\acidified_zone_fire\acidified_zone_fire_water\acidified_zone_fire_water.str]],
                    once_per_hit = true,
                    delay_ms = 100, -- Match Cold Bolt's 260 ms cadence.
                },
                {
                    effect_id = 2097,
                    str = [[data\texture\effect\acidified_zone_fire\acidified_zone_fire_cast\acidified_zone_fire_cast.str]],
                    once_per_hit = true,
                    delay_ms = 100,
                },
                {
                    effect_id = 2098,
                    str = [[data\texture\effect\acidified_zone_fire\acidified_zone_fire_b_bottom\acidified_zone_fire_b_bottom.str]],
                    bottom_effect = true,
                    delay_ms = 100,
                },
            },
        },
        [20] = { -- SK_MG_LIGHTNINGBOLT
            name = "Lightning Bolt",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 2086,
                    str = [[data\texture\effect\mineffect\new_tetravortex\new_tetravortex_wind_bottom\new_tetravortex_wind_bottom.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            target = {
                {
                    effect_id = 29,
                    str = [[data\texture\effect\lightning.str]],
                    once_per_hit = false,
                },
                {
                    effect_id = 50029,
                    str = [[data\texture\effect\acidified_zone_wind\acidified_zone_wind_b_bottom\acidified_zone_wind_b_bottom.str]],
                    bottom_effect = true,
                    once_per_hit = false,
                },
            },
        },
        [157] = { -- SK_MG_ENERGYCOAT
            name = "Energy Coat",
            source = {
                {
                    effect_id = 2099,
                    str = [[data\texture\effect\venom_swamp\venom_swamp_cast_bottom\venom_swamp_cast_bottom.str]],
                    bottom_effect = true,
                },
                {
                    effect_id = 2100,
                    str = [[data\texture\effect\venom_swamp\venom_swamp_cast\venom_swamp_cast.str]],
                },
            },
        },
        [839] = { -- SK_MG_EARTHBOLT
            name = "Earth Bolt",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 2087,
                    str = [[data\texture\effect\mineffect\new_tetravortex\new_tetravortex_earth_bottom\new_tetravortex_earth_bottom.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            target = {
                {
                    effect_id = 1760,
                    -- The installed GRF spells the 01b filename "vlolentquake".
                    str = [[data\texture\effect\violentquake\violentquake\vlolentquake_01_b.str]],
                    once_per_hit = false,
                },
                {
                    effect_id = 2089,
                    str = [[data\texture\effect\acidified_zone_ground\acidified_zone_ground_b_bottom\acidified_zone_ground_b_bottom.str]],
                    bottom_effect = true,
                    once_per_hit = false,
                },
                {
                    effect_id = 2090,
                    str = [[data\texture\effect\terremotus\terremotus_earth_care\terremotus_earth_care.str]],
                    once_per_hit = false,
                },
            },
        },
        [83] = { -- SK_WZ_METEORSTORM
            name = "Meteor Storm",
            cast_motion_type = 1,
            skill_motion_type = 2,
            source_once_per_cast = true, -- Later meteors only play their ground/target effects.
            casting = {
                {
                    effect_id = 50041,
                    str = [[data\texture\effect\tetravortex\tetra_01_fire_ground\tetra_01_fire_grd.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            casting_ground = {
                { effect_id = 50053 }, -- Red.
            },
            ground = { -- Keep the existing random meteor completion visual.
                {
                    effect_id = 50042,
                    variants = {
                        { str = [[data\texture\effect\meteor1.str]] },
                        { str = [[data\texture\effect\meteor2.str]] },
                        { str = [[data\texture\effect\meteor3.str]] },
                    },
                },
            },
        },
        [2211] = { -- SK_WZ_CRIMSONROCK
            name = "Crimson Rock",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50041 }, -- Shared Meteor Storm bottom casting effect.
            },
            target = {
                {
                    effect_id = 50046,
                    str = [[data\texture\effect\new_crimsonrock\new_crimsonrock\new_crimsonrock.str]],
                },
                {
                    effect_id = 50047,
                    str = [[data\texture\effect\new_crimsonrock\new_crimsonrock_bottom\new_crimsonrock_bottom.str]],
                    bottom_effect = true,
                },
            },
        },
        [85] = { -- SK_WZ_LORDOFVERMILION
            name = "Lord of Vermilion",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 50038,
                    str = [[data\texture\effect\tetravortex\tetra_02_wind_ground\tetra_02_wind_grd.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            casting_ground = {
                { effect_id = 50051 }, -- Cyan.
            },
            ground = {
                {
                    effect_id = 50036,
                    str = [[data\texture\effect\new_tornadostorm\new_tornadostorm\new_tornadostorm.str]],
                    play_count = 8, -- Eight 30-frame clips replace the 240-frame lord.str.
                },
                {
                    effect_id = 50037,
                    str = [[data\texture\effect\new_tornadostorm\new_tornadostorm_bottom\new_tornadostorm_bottom.str]],
                    bottom_effect = true,
                    play_count = 8,
                },
                {
                    effect_id = 50082,
                    str = [[data\texture\effect\conflagration\conflagration\conflagration.str]],
                    play_count = 4,
                },
                {
                    effect_id = 50083,
                    str = [[data\texture\effect\astralstrike\astralstrike_cast\astralstrike_cast.str]],
                    play_count = 2,
                },
            },
        },
        [884] = { -- SK_WZ_THUNDERSTORM (Thunderbolt)
            name = "Thunderbolt",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50038 }, -- Shared Lord of Vermilion bottom casting effect.
            },
            target = {
                {
                    effect_id = 50064,
                    str = [[data\texture\effect\thunderstorm.str]],
                },
            },
        },
        [89] = { -- SK_WZ_STORMGUST
            name = "Storm Gust",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 50039,
                    str = [[data\texture\effect\tetravortex\tetra_04_water_ground\tetra_04_water_grd.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            casting_ground = {
                { effect_id = 50052 }, -- Deep blue.
            },
            ground = { -- Keep the existing completion visual at the selected cell.
                {
                    effect_id = 50040,
                    str = [[data\texture\effect\storm gust.str]],
                },
            },
        },
        [845] = { -- SK_WZ_EXTREMEVACUUM
            name = "Extreme Vacuum",
            ground_server_notified = false, -- Local playback owns the fixed two-pass presentation.
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            casting_ground = {
                { effect_id = 50048 }, -- Original untinted ground casting circle.
            },
            ground = {
                {
                    effect_id = 50055,
                    str = [[data\texture\effect\new_tigercannon\new_tigercannon.str]],
                    wav = [[data\wav\extremevcm.wav]],
                    play_count = 2,
                    replay_wav = true,
                    ignore_server_wav = true,
                },
                {
                    effect_id = 50056,
                    str = [[data\texture\effect\destructive_hurricane\destructive_hurricane_bottom\destructive_hurricane_bottom.str]],
                    play_count = 2,
                    bottom_effect = true,
                },
                {
                    effect_id = 50061,
                    str = [[data\texture\effect\diluvio\diluvio_ice_bottom\diluvio_ice_bottom.str]],
                    play_count = 2,
                    bottom_effect = true,
                },
            },
        },
        [846] = { -- SK_WZ_REALITYBREAKER
            name = "Reality Breaker",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50060 }, -- Shared Shadow Bomb purple bottom casting effect.
            },
            casting_ground = {
                { effect_id = 50054 }, -- Purple.
            },
            ground = { -- Additional local visual alongside the server's effects.
                {
                    effect_id = 50057,
                    str = [[data\texture\effect\4abc_abyss_slayer\abyss_slayer\abyss_slayer.str]],
                    play_count = 2,
                },
                {
                    effect_id = 50074,
                    str = [[data\texture\effect\abyss_strike\abyss_strike_ground\abyss_strike_ground.str]],
                    bottom_effect = true,
                    play_count = 2,
                },
            },
        },
        [838] = { -- SK_WZ_ICEBERG
            name = "Iceberg",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50039 }, -- Shared Storm Gust bottom casting effect.
            },
            target = {
                {
                    effect_id = 636, -- EF_HYOUSYOURAKU / Falling Ice Pillar.
                    str = [[data\texture\effect\icy.str]],
                    wav = [[data\wav\effect\wl_jackfrost.wav]],
                },
            },
        },
        [90] = { -- SK_WZ_STALAGMITE
            name = "Stalagmite",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50033 }, -- Shared Earth Strain bottom casting effect.
            },
            target = {
                {
                    effect_id = 50043,
                    str = [[data\texture\effect\violentquake\violentquake\violentquake_01.str]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                    draw_over_actors = true,
                },
                {
                    effect_id = 50044,
                    str = [[data\texture\effect\violentquake\violentquake\violentquake_02.str]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                    draw_over_actors = true,
                },
                {
                    effect_id = 50045,
                    str = [[data\texture\effect\violentquake\violentquake\violentquake_03.str]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                    draw_over_actors = true,
                },
                { effect_id = 2090 }, -- Shared Terremotus Earth Care target effect.
            },
        },
        [365] = { -- SK_WZ_MAGICCRASHER
            name = "Magic Crasher",
            hit_wav = [[data\wav\effect\bo_acidified_zone_water.wav]],
            cast_motion_type = 1,
            skill_motion_type = 2,
            target = {
                {
                    effect_id = 1380,
                    str = [[data\texture\effect\new_dragonbreath\new_dragonbreath_01_clock\new_dragonbreath_01_clock.str]],
                },
                {
                    effect_id = 1637,
                    str = [[data\texture\effect\new_chainlightning\new_chainlightning_lockon\new_chainlightning_lockon.str]],
                },
            },
        },
        [403] = { -- SK_WZ_FORESIGHT
            name = "Foresight",
            cast_motion_type = 1,
            skill_motion_type = 2,
            source = {
                {
                    effect_id = 50065,
                    str = [[data\texture\effect\4ig_judge\judge\judge.str]],
                },
                { effect_id = 1539 }, -- Shared Judge cast clip.
            },
        },
        [2446] = { -- SK_WZ_EARTHSTRAIN
            name = "Earth Strain",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 50033,
                    str = [[data\texture\effect\tetravortex\tetra_03_earth_ground\tetra_03_earth_grd.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                },
            },
            casting_ground = {
                { effect_id = 50050 }, -- Brown.
            },
            ground = {
                {
                    effect_id = 50034,
                    str = [[data\texture\effect\strantumtremor\strantumtremor_bottom\strantumtremor_bottom.str]],
                    bottom_effect = true,
                    play_count = 2,
                },
                {
                    effect_id = 50035,
                    str = [[data\texture\effect\strantumtremor\strantumtremor\strantumtremor.str]],
                    play_count = 2,
                },
            },
        },
        -- Cast starts use 2217; completion packets use the elemental subskill IDs.
        [2217] = tetra_vortex_presentation, -- SK_SO_TETRAVORTEX
        [895] = tetra_vortex_presentation,  -- SK_SO_TETRAVORTEX_FIRE
        [927] = tetra_vortex_presentation,  -- SK_SO_TETRAVORTEX_GROUND
        [928] = tetra_vortex_presentation,  -- SK_SO_TETRAVORTEX_WIND
        [929] = tetra_vortex_presentation,  -- SK_SO_TETRAVORTEX_WATER
        [930] = tetra_vortex_presentation,  -- SK_SO_TETRAVORTEX_NEUTRAL
        [2222] = { -- SK_SO_SUMMONELEMENTALSPHERES
            name = "Summon Elemental Spheres",
            cast_motion_type = 1,
            skill_motion_type = 2,
            source_once_per_cast = true,
            source = {
                { effect_id = 2069 }, -- Guardian Shield bottom animation on the caster.
            },
        },
        [2213] = { -- SK_SO_COMET
            name = "Cataclysm",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            casting_ground = {
                {
                    effect_id = 50066,
                    str = [[data\texture\effect\new_comet\new_comet_cast_magicrune\new_comet_cast_magicrune.str]],
                    ground_plane = true,
                    rotation_speed = 1.0,
                    bottom_effect = true,
                    bottom_offset_px = 0,
                },
            },
        },
        [2449] = { -- SK_WZ_PSYCHICWAVE
            name = "Psychic Wave",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            casting_ground = {
                {
                    effect_id = 50049,
                    str = [[data\texture\effect\new_psychicwave\new_psychicwave_cast\new_psychicwave_cast.str]],
                },
            },
        },
        [882] = { -- SK_SO_SHADOWBOMB
            name = "Shadow Bomb",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                {
                    effect_id = 50060,
                    str = [[data\texture\effect\tetravortex\tetra_04_water_ground\tetra_04_water_grd.str]],
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                    bottom_effect = true,
                    hue_shift_degrees = 80, -- Water blue/cyan to purple.
                },
            },
            target = {
                {
                    effect_id = 50058,
                    str = [[data\texture\effect\abyss_strike\abyss_strike\abyss_strike.str]],
                    wav = [[data\wav\effect\ag_crimson_arrow1.wav]],
                },
                {
                    effect_id = 50059,
                    str = [[data\texture\effect\abyss_strike\abyss_strike_ground\abyss_strike_ground.str]],
                    bottom_effect = true,
                },
            },
        },
        [883] = { -- SK_SO_PHANTOMSPEAR
            name = "Phantom Spear",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            target = {
                {
                    effect_id = 50062,
                    str = [[data\texture\effect\new_varetyrspear\new_varetyrspear_bottom\new_varetyrspear_bottom.str]],
                    wav = [[data\wav\effect\ag_crystal_impact.wav]],
                    bottom_effect = true,
                },
            },
        },
        [885] = { -- SK_SO_DOOM
            name = "Doom",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50081 }, -- Purple casting clip with begin-spell sound.
            },
            casting_ground = {
                { effect_id = 50054 }, -- Purple.
            },
            ground = {
                {
                    effect_id = 565,
                    str = [[data\texture\effect\moonlight_1.str]],
                    play_count = 2,
                    bottom_effect = true,
                    bottom_order = -1,
                },
                {
                    effect_id = 50072,
                    str = [[data\texture\effect\abyss_strike\abyss_strike_ground\abyss_strike_ground.str]],
                    play_count = 2,
                },
                {
                    effect_id = 50073,
                    str = [[data\texture\effect\lightning_land\lightning_land_bottom\lightning_land_bottom.str]],
                    play_count = 2,
                    bottom_effect = true,
                },
                {
                    effect_id = 1352,
                    str = [[data\texture\effect\new_armscannon\new_armscannon_07_clock_down\new_armscannon_07_clock_down.str]],
                    play_count = 2,
                },
            },
        },
        [887] = { -- SK_SO_DIAMONDDUST
            name = "Diamond Dust",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            casting_target = {
                {
                    effect_id = 50070,
                    str = [[data\texture\effect\new_diamonddust\new_diamonddust_cast\new_diamonddust_cast.str]],
                    bottom_effect = true,
                },
            },
            target = {
                {
                    effect_id = 50068,
                    str = [[data\texture\effect\new_diamonddust\new_diamonddust\new_diamonddust.str]],
                },
                {
                    effect_id = 50069,
                    str = [[data\texture\effect\new_diamonddust\new_diamonddust_bottom\new_diamonddust_bottom.str]],
                    bottom_effect = true,
                },
            },
        },
        [888] = { -- SK_SO_ASTRALSTRIKE
            name = "Astral Strike",
            cast_motion_type = 1,
            skill_motion_type = 2,
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            target = {
                {
                    effect_id = 1373,
                    str = [[data\texture\effect\new_banishingpoint\new_banishingpoint_01\new_banishingpoint_01.str]],
                },
                {
                    effect_id = 1374,
                    str = [[data\texture\effect\new_banishingpoint\new_banishingpoint_05\new_banishingpoint_05.str]],
                },
            },
        },
        [2206] = { -- SK_SO_HINDSIGHT
            name = "Hindsight",
            casting = {
                { effect_id = 50080 }, -- White casting clip with begin-spell sound.
            },
            cast_motion_type = 1,
            skill_motion_type = 2,
            source = {
                {
                    effect_id = 50067,
                    str = [[data\texture\effect\4ig_firstfaithpower\firstfaithpower\firstfaithpower.str]],
                },
                { effect_id = 1983 }, -- Shared thirdexorflame_cast.str.
            },
        },
        [933] = { -- SK_SO_CLIMAX
            name = "Climax",
            cast_motion_type = 1,
            skill_motion_type = 2,
        },
        [5012] = { -- SK_SO_VOIDMAGEPRODIGY
            name = "Voidmage Prodigy",
            cast_motion_type = 1,
            skill_motion_type = 2,
            source = {
                {
                    effect_id = 50075,
                    str = [[data\texture\effect\destructive_hurricane\destructive_hurricane\destructive_hurricane.str]],
                    hue_shift_degrees = 80,
                },
            },
        },
        [2202] = { -- SK_MG_VOIDEXPANSION
            name = "Void Expansion",
            cast_motion_type = 1,
            skill_motion_type = 2,
            source = { -- Instant cast: play on the caster when the skill is used.
                {
                    effect_id = 50032, -- Own copy of Null Strike's casting visual with Void Expansion's WAV.
                    -- Keep the requested filename's CP949 bytes intact.
                    str = [[data\texture\effect\]] .. "\188\250\189\196\192\252\176\1794.str",
                    wav = [[data\wav\effect\ag_climax.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                },
            },
            target = {
                {
                    effect_id = 2104,
                    str = [[data\texture\effect\test_effect\00\00.str]],
                    once_per_hit = false,
                },
                {
                    effect_id = 1983,
                    str = [[data\texture\effect\4ig_thirdexorflame\thirdexorflame_cast\thirdexorflame_cast.str]],
                    once_per_hit = false,
                },
            },
        },
        [844] = { -- SK_WZ_ICONOFSIN
            name = "Icon of Sin",
            cast_motion_type = 1,
            skill_motion_type = 2,
            source = { -- Instant cast: play on the caster when the skill is used.
                {
                    effect_id = 50031, -- Own copy of Dark Strike's casting visual with Icon of Sin's WAV.
                    -- Keep the requested filename's CP949 bytes intact.
                    str = [[data\texture\effect\]] .. "\188\250\189\196\192\252\176\1795.str",
                    wav = [[data\wav\effect\assasin_poisonreact.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 35,
                },
            },
            target = {
                {
                    effect_id = 2103,
                    str = [[data\texture\effect\tetravortex\tetra_ball_00_core_ground\tetra_ball_00_core_grd.str]],
                    once_per_hit = false,
                },
            },
        },
        [369] = { -- SK_PA_GOSPEL
            name = "Gospel",
            cast_motion_type = 1,
            skill_motion_type = 1,
            server_notified = true, -- The server supplies effects and moving ground units.
        },
        [251] = { -- SK_CR_SHIELDBOOMERANG
            name = "Shield Boomerang",
            cast_motion_type = 1,
            skill_motion_type = 1,
            motion_type = 1,
            -- The world projectile renderer supplies the spinning shield.
            target = {
                {
                    effect_id = 2075,
                    str = [[data\texture\effect\absolute_zephyr\absolute_zephyr_hit\absolute_zephyr_hit.str]],
                },
            },
            source = {
                {
                    effect_id = 2076,
                    str = [[data\texture\effect\new_banishingpoint\new_banishingpoint_target_bottom\new_banishingpoint_target_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [860] = { -- SK_PA_SHIELDCHAIN
            name = "Shield Chain",
            cast_motion_type = 1,
            skill_motion_type = 1,
            -- The chain renderer follows the server's SK_PA_SHIELDCHAIN_ATK hits.
            source = {
                { effect_id = 2076 }, -- absolute_zephyr_bottom.str; bottom effect, offset 25 px.
            },
        },
        [946] = { -- SK_PA_SHIELDCHAIN_ATK
            name = "Shield Chain",
            -- Follow-up damage advances the shield without replaying the caster.
            target = {
                { effect_id = 2075 }, -- absolute_zephyr_hit.str on every damaged target.
            },
        },
        [947] = { -- SK_PA_SHIELDSLAM
            name = "Shield Slam",
            cast_motion_type = 1,
            skill_motion_type = 1,
            -- The world renderer drops a spinning shield onto the target.
            target = {
                {
                    effect_id = 2078,
                    str = [[data\texture\effect\new_axe_stomp\new_axe_stomp_bottom\new_axe_stomp_bottom.str]],
                    wav = [[data\wav\shieldchain.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [480] = { -- SK_PA_RAPIDSMITING
            name = "Rapid Smiting",
            cast_motion_type = 1,
            skill_motion_type = 1,
            target = {
                {
                    effect_id = 2079,
                    str = [[data\texture\effect\new_axetornado\new_axetornado_bottom\new_axetornado_bottom.str]],
                    wav = [[data\wav\effect\cru_shield boomerang.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
            -- Shields appear around the target and sweep through them simultaneously.
        },
        [255] = { 
            name = "Sworn Protector",
            begin_effect_id = 2055,
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2055 },
                {
                    effect_id = 2060,
                    str = [[data\texture\effect\ultimate_sacrifice\ultimatesacrifice_bottom\ultimatesacrifice_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                },
            },
            target = {
                {
                    effect_id = 2061,
                    str = [[data\texture\effect\ultimate_sacrifice\ultimatesacrifice\ultimatesacrifice.str]],
                    wav = [[data\wav\effect\cd_religio.wav]],
                },
            },
        },
        [950] = { -- SK_PA_HOLDTHELINE
            name = "Hold the Line",
            motion_type = 1,
            cast_motion_type = 1,
            skill_motion_type = 1,
            server_notified = true,
        },
        [951] = { -- SK_PA_GOASPEL: the custom Gospel used by the Paladin skill tree.
            name = "Gospel",
            cast_motion_type = 1,
            skill_motion_type = 1,
            server_notified = true, -- The server supplies effects and moving ground units.
        },
        [72] = { -- SK_CR_BINDINGOATH
            name = "Binding Oath",
            skill_motion_type = 1,
        },
        [948] = { -- SK_CR_TAUNT
            name = "Taunt",
            cast_motion_type = 1,
            skill_motion_type = 1,
            target = {
                {
                    effect_id = 2071,
                    str = [[data\texture\effect\provoke.str]], -- SK_SM_PROVOKE's visual.
                    wav = [[data\wav\effect\swordman_provoke.wav]],
                },
            },
            source = {
                {
                    effect_id = 2063,
                    str = [[data\texture\effect\new_peintbomb\new_peintbomb\new_peintbomb.str]],
                },
                {
                    effect_id = 2064,
                    str = [[data\texture\effect\new_peintbomb\new_peintbomb_bottom\new_peintbomb_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [949] = { -- SK_CR_RETALIATION
            name = "Retaliation",
            source = {
                {
                    effect_id = 2073,
                    str = [[data\texture\effect\new_peintbomb\new_peintbomb\new_peintbomb.str]],
                    wav = [[data\wav\effect\ig_overslash.wav]],
                },
                {
                    effect_id = 2074,
                    str = [[data\texture\effect\petitio\petitio_bottom\petitio_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [861] = { -- SK_PA_FORTIFY
            name = "Fortify",
            cast_motion_type = 1,
            skill_motion_type = 1,
            source = {
                {
                    effect_id = 2068,
                    str = [[data\texture\effect\guardianshield\guardianshield\guardianshield.str]],
                    wav = [[data\wav\effect\cd_presens_acies.wav]],
                },
                {
                    effect_id = 2069,
                    str = [[data\texture\effect\guardianshield\guardianshield_bottom\guardianshield_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [2318] = { -- SK_CR_VANGUARDFORCE
            name = "Vanguard Force",
            cast_motion_type = 1,
            skill_motion_type = 1,
            source = {
                {
                    effect_id = 2065,
                    wav = [[data\wav\effect\lg_kings_grace.wav]],
                    str = [[data\texture\effect\strip_armor.str]],
                },
                {
                    effect_id = 2066,
                    str = [[data\texture\effect\benedictum\benedictum_cast\benedictum_cast.str]],
                },
                {
                    effect_id = 2067,
                    str = [[data\texture\effect\guard_stance\guard_stance_cast_bottom\guard_stance_cast_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [254] = { -- SK_CR_GRANDCROSS
            name = "Grand Cross",
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2057 },
                { effect_id = 2058 },
            },
            source = {
                {
                    effect_id = 2081,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_cast\acidified_zone_water_cast.str]],
                },
            },
        },
        [367] = { -- SK_PA_GLORIADOMINI
            name = "Gloria Domini",
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2057 },
                { effect_id = 2058 },
            },
            target = {
                {
                    effect_id = 2082,
                    str = [[data\texture\effect\petitio\petitio\petitio.str]],
                    wav = [[data\wav\effect\lg_shieldpress.wav]],
                    hue_shift_degrees = 180, -- Yellow/gold to blue; 0 restores the original.
                },
                {
                    effect_id = 2083,
                    str = [[data\texture\effect\petitio\petitio_bottom\petitio_bottom.str]],
                    wav = [[data\wav\effect\abc_abyss_strike.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    hue_shift_degrees = 180, -- Yellow/gold to blue; 0 restores the original.
                },
            },
        },
        [2321] = { -- SK_PA_GENESISRAY
            name = "Genesis Ray",
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2057 },
                {
                    effect_id = 2058,
                    str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis_cast_bottom\new_rayofgenesis_cast_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 15,
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                },
            },
            -- Cast completion supplies the ground cell; damage supplies each victim.
            ground = {
                {
                    effect_id = 2080,
                    str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis\new_rayofgenesis.str]],
                    wav = [[data\wav\effect\lg_hesperuslit.wav]],
                },
            },
            target = {
                {
                    effect_id = 2059,
                    str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis_hit\new_rayofgenesis_hit.str]],
                    wav = [[data\wav\effect\lg_shieldspell.wav]],
                },
            },
        },
        [71] = { -- SK_CR_SWORDSTOPLOWSHARES
            name = "Swords to Plowshares",
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2045 },
                {
                    effect_id = 2047,
                    str = [[data\texture\effect\competentia\competentia_loop_bottom\competentia_loop_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                },
            },
            -- The server sends successful skill use, followed by Heal for the amount.
            -- It does not send separate effect notifications for this skill.
            target = {
                {
                    effect_id = 2046,
                    str = [[data\texture\effect\increasing_activity\increasing_activity\increasing_activity.str]],
                    wav = [[data\wav\effect\cd_mediale_votum.wav]],
                },
            },
        },
        [35] = { -- 
            name = "Divine Light",
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2048 },
                { effect_id = 2055 },
                {
                    effect_id = 2049,
                    str = [[data\texture\effect\competentia\competentia_loop_bottom\competentia_loop_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                },
            },
            -- The server sends successful skill use, followed by Heal for the amount.
            -- It does not send separate effect notifications for this skill.
            target = {
                {
                    effect_id = 2050,
                    str = [[data\texture\effect\increasing_activity\increasing_activity\increasing_activity.str]],
                },
                {
                    effect_id = 2056,
                    str = [[data\texture\effect\wooden_fairy\fairy_dusty\fairy_dusty.str]],
                    wav = [[data\wav\effect\cd_competentia.wav]],
                },
            },
        },
        [2322] = { -- 
            name = "King's Grace",
            cast_motion_type = 1,
            skill_motion_type = 1,
            casting = {
                { effect_id = 2051 },
                {
                    effect_id = 2052,
                    str = [[data\texture\effect\crossrain\cross_rain_bottom\cross_rain_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    wav = [[data\wav\effect\ef_beginspell.wav]],
                },
                { effect_id = 2055 },
            },
            -- The server sends successful skill use, followed by Heal for the amount.
            -- It does not send separate effect notifications for this skill.
            target = {
                {
                    effect_id = 2053,
                    str = [[data\texture\effect\increasing_activity\increasing_activity\increasing_activity.str]],
                    wav = [[data\wav\effect\cd_dilectio_heal.wav]],
                },
            },
        },
        [398] = {
            name = "Traumatic Strike", 
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2003,
                    str = [[data\texture\effect\new_overbrand\new_overbrand_hit\new_overbrand_hit.str]],
                },
                {
                    effect_id = 2004,
                    str = [[data\texture\effect\acidified_zone_fire\acidified_zone_fire_b_bottom\acidified_zone_fire_b_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [942] = {
            name = "Magnum Break", -- SK_SM_MAGNUM
            motion_type = 2,
            server_notified = true, -- The server sends the caster effects and magnumbr.wav.
        },
        [5] = {
            name = "Bash",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2000,
                    str = [[data\texture\effect\wooden_warrior\throwrock\throwrock_hit\throwrock_hit.str]],
                },
                {
                    effect_id = 2001,
                    str = [[data\texture\effect\acidified_zone_ground\acidified_zone_ground_b_bottom\acidified_zone_ground_b_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [813] = {
            name = "Spear Stance",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2008,
                    str = [[data\texture\effect\servant_demolition\servant_demolition_collect\servant_demolition_collect.str]],
                    wav = [[data\wav\effect\sr_powervelocity.wav]],
                }
            },
        },
        [58] = {
            name = "Spear Stab", 
            motion_type = 2,
            server_notified = true, 
            target = {         
                { 
                    effect_id = 2005, 
                    str = [[data\texture\effect\servant_phantom\servant_phantom_down\servant_phantom_down.str]],
                },
                { 
                    effect_id = 2006, 
                    str = [[data\texture\effect\servantweapon\new_servantweapon\new_servantweapon_hit\new_servantweapon_hit.str]] 
                },
                { 
                    effect_id = 2007,
                    str = [[data\texture\effect\servantweapon\new_servantweapon\new_servantweapon_hit_bottom\new_servantweapon_hit_bottom.str]], 
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [59] = {
            name = "Spear Cannon", -- SK_KN_SPEARCANNON / Spear Boomerang
            motion_type = 1,
            target = {
                {
                    effect_id = 2077,
                    str = [[data\texture\effect\new_hit\new_criticalattack\new_criticalattack.str]],
                },
            },
            source = {
                {
                    effect_id = 151,
                    str = [[data\texture\effect\new_soulexpansion\new_soulexpansion_cast_bottom\new_soulexpansion_cast_bottom.str]],
                    wav = [[data\wav\effect\knight_spear_boomerang.wav]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [399] = {
            name = "Arcane Strike", 
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 1537,
                    str = [[data\texture\effect\new_comet\new_comet_sub_explosion\new_comet_sub_explosion.str]],
                },
                {
                    effect_id = 1538,
                    str = [[data\texture\effect\new_comet\new_comet_sub_explosion_bottom\new_comet_sub_explosion_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
                {
                    effect_id = 1539,
                    str = [[data\texture\effect\4ig_judge\judge_cast\judge_cast.str]],
                },
            },
        },
        [819] = {
            name = "Spirit Growth",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2002,
                    str = [[data\texture\effect\new_comet\new_comet_cast\new_comet_cast.str]],
                    wav = [[data\wav\ag_climax.wav]],
                }
            },
        },
        [368] = {
            name = "Reckoning",
            motion_type = 2,
            target = {
                {
                    effect_id = 2084,
                    str = [[data\texture\effect\attack_stance\attack_stance_cast\attack_stance_cast.str]],
                },
                {
                    effect_id = 2085,
                    str = [[data\texture\effect\ardor\ardor_cast_bottom\ardor_cast_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [835] = {
            name = "Adrenaline Rush",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2009,
                    str = [[data\texture\effect\judgement_cross\judgement_cross_cast\judgement_cross_cast.str]],
                    wav = [[data\wav\effect\rush.wav]],
                },
                {
                    effect_id = 2030,
                    str = [[data\texture\effect\adv_protection\adv_protection_cast_bottom\adv_protection_cast_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [876] = {
            name = "Fury",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2010,
                    str = [[data\texture\effect\madness_crusher\madness_crusher\madness_crusher.str]],
                },
                {
                    effect_id = 2054,
                    str = [[data\texture\effect\madness_crusher\madness_crusher_bottom\madness_crusher_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [61] = {
            name = "Storm Slash",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2011,
                    str = [[data\texture\effect\weakness.str]],
                }
            },
        },  
        [62] = {
            name = "Bowling Bash",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2012,
                    str = [[data\texture\effect\ultimate_sacrifice\ultimatesacrifice_bottom\ultimatesacrifice_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
                {
                    effect_id = 2013,
                    str = [[data\texture\effect\bowling.str]],
                }
            },
        },   
        [355] = {
            name = "Enchant Blade",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2014,
                    str = [[data\texture\effect\new_battle_w\new_battle_w\new_battle_w.str]],
                    wav = [[data\wav\effect\enchantblade.wav]],
                },
                {
                    effect_id = 2023,
                    str = [[data\texture\effect\new_battle_w\new_battle_w_bottom\new_battle_w_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },     
        [2016] = {
            name = "Vigor",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2015,
                    str = [[data\texture\effect\new_reparatio\new_reparatio_target\new_reparatio_target.str]],
                    wav = [[data\wav\effect\wm_frigg_song.wav]],
                },
            },
        },
        [2015] = {
            name = "Stone Skin",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2017,
                    str = [[data\texture\effect\holy_shield\holy_shield_cast\holy_shield_cast.str]],
                    wav = [[data\wav\effect\ig_holy_shield1.wav]],
                },
            },
        },
        [57] = {
            name = "Brandish Spear",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2018,
                    str = [[data\texture\effect\brandish.str]],
                },
                {
                    effect_id = 2019,
                    str = [[data\texture\effect\brandish2.str]],
                },
            },
        },
        [56] = {
            name = "Pierce",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2020,
                    str = [[data\texture\effect\effligo\effligo\effligo.str]],
                },
                {
                    effect_id = 2029,
                    str = [[data\texture\effect\effligo\effligo_bottom\effligo_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [2005] = {
            name = "Wind Cutter",
            motion_type = 2,
            server_notified = true,
            source = {
                {
                    effect_id = 2021,
                    str = [[data\texture\effect\new_windcutter\new_windcutter_bottom_01clock\new_windcutter_bottom_01clock.str]],
                    wav = [[data\wav\effect\wind_cutter.wav]],
                }
            },
            target = {
                {
                    effect_id = 2022,
                    str = [[data\texture\effect\new_windcutter\new_windcutter_target\new_windcutter_target.str]],
                }
            },
        },
        [2002] = { -- SK_KN_SONICWAVE; skill IDs and effect IDs are separate.
            name = "Sonic Wave",
            motion_type = 2,
            -- The server sends skill damage, without a separate effect notification.
            source = {
                {
                    effect_id = 1678,
                    renderer = "sonic_wave",
                    texture = [[data\texture\effect\new_sonicwave\new_sonicwave\son_wave_00.tga]],
                    trail_texture = [[data\texture\effect\new_sonicwave\new_sonicwave\son_wave_01.tga]],
                    offset_y = -1,
                    radius = 18,
                    height = 21,
                    wave_count = 3,
                    wave_interval_ms = 336,
                    wave_duration_ms = 360,
                    wav = [[data\wav\effect\sonic_blade.wav]],
                },
                {
                    effect_id = 2024,
                    str = [[data\texture\effect\new_chainlightning\new_chainlightning_cast_bottom\new_chainlightning_cast_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [146] = {
            name = "Counterstrike", -- SK_SM_AUTOBERSERK in this server's skill database.
            server_notified = true,
            source = { { effect_id = 1032 }, {effect_id = 2016, str = [[data\texture\effect\attack_stance\attack_stance\attack_stance.str]]} }, -- The server sends EF_MAXPAIN once on the caster.
        },
        [249] = { -- SK_CR_AUTOGUARD
            name = "Auto Guard",
            skill_motion_type = 1,
            source = { -- Manual activation only; automatic blocks skip the caster presentation.
                {
                    effect_id = 2062,
                    str = [[data\texture\effect\guard_stance\guard_stance\guard_stance.str]],
                },
            },
            target = {
                { effect_id = 2042 }, -- Parry's white guard barrier, including automatic blocks.
            },
        },
        [356] = { -- SK_CM_PARRY
            name = "Parry",
            server_notified = false, -- Activation and blocks send skill-use packets, without effect packets.
            target = { -- Target attachment also plays when a block occurs during another cast.
                {
                    effect_id = 2042,
                    renderer = "guard_sphere",
                    texture = [[data\texture\effect\guardK2.tga]],
                    wav = [[data\wav\effect\kyrie_guard.wav]],
                    offset_y = -7,
                    radius = 7,
                    height = 16,
                    color = 0xFFFFFF,
                    alpha = 0.8,
                    duration_ms = 700,
                    fade_in_ms = 100,
                    fade_out_ms = 200,
                },
                {
                    effect_id = 2043,
                    str = [[data\texture\effect\guardianshield\guardianshield_sub\guardianshield_sub.str]],
                },
                {
                    effect_id = 2044,
                    str = [[data\texture\effect\competentia\competentia_loop_bottom\competentia_loop_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                }
            },
        },
        [359] = { -- SK_CM_FRENZY; 2015 belongs to Stone Skin.
            name = "Frenzy",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2025,
                    str = [[data\texture\effect\4ig_explosionblaster\explosionblaster\explosionblaster.str]],
                    wav = [[data\wav\effect\assasin_poisonreact.wav]],
                },
                {
                    effect_id = 2026,
                    str = [[data\texture\effect\4ig_explosionblaster\explosionblaster_bottom\explosionblaster_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [863] = {
            name = "Release the Beast",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2027,
                    str = [[data\texture\effect\4iq_massivefblaster\massivefblaster_hit\massivefblaster_hit.str]],
                },
                {
                    effect_id = 2028,
                    str = [[data\texture\effect\4iq_massivefblaster\massivefblaster_hit_bottom\massivefblaster_hit_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [953] = { -- SK_CM_REND; the server must preserve this ID in damage packets.
            name = "Rend",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2031,
                    str = [[data\texture\effect\4iq_massivefblaster\massivefblaster\massivefblaster.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
                {
                    effect_id = 2032,
                    str = [[data\texture\effect\new_axeboomerang\new_axeboomerang_cast\new_axeboomerang_cast.str]],
                },
            },
        },
        [2001] = {
            name = "Blood Sport",
            motion_type = 1,
            server_notified = true,
            target = {
                {
                    effect_id = 2033,
                    str = [[data\texture\effect\new_chargingpierce\new_chargingpierce_cast\new_chargingpierce_cast.str]],
                    wav = [[data\wav\effect\ag_storm_cannon.wav]],
                },
                {
                    effect_id = 2034,
                    str = [[data\texture\effect\new_axe_stomp\new_axe_stomp_bottom\new_axe_stomp_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [5004] = { -- SK_CM_DRAGONBREATH
            name = "Dragon Breath",
            motion_type = 1,
            server_notified = true, -- The server sends effect 1310 once on the caster.
            source = {
                {
                    effect_id = 1310,
                    renderer = "directional_str",
                    offset_y = -10,
                    -- Endpoints are authored screen pixels relative to the STR's (320, 320) anchor.
                    directions = {
                        {
                            str = [[data\texture\effect\new_dragonbreath_water\new_dragonbreath_water_01_clock\new_dragonbreath_water_01_clock.str]],
                            origin_x = 102, origin_y = -88,
                            end_x = 225, end_y = -195,
                        },
                        {
                            str = [[data\texture\effect\new_dragonbreath_water\new_dragonbreath_water_05_clock\new_dragonbreath_water_05_clock.str]],
                            origin_x = 98, origin_y = 14,
                            end_x = 230, end_y = 170,
                        },
                        {
                            str = [[data\texture\effect\new_dragonbreath_water\new_dragonbreath_water_07_clock\new_dragonbreath_water_07_clock.str]],
                            origin_x = -98, origin_y = 14,
                            end_x = -230, end_y = 170,
                        },
                        {
                            str = [[data\texture\effect\new_dragonbreath_water\new_dragonbreath_water_11_clock\new_dragonbreath_water_11_clock.str]],
                            origin_x = -102, origin_y = -88,
                            end_x = -225, end_y = -195,
                        },
                    },
                    wav = [[data\wav\dragon_breath.wav]],
                },
            },
        },
        [2312] = {
            name = "A Hundred Spears",
            motion_type = 2,
            server_notified = true,
            target = {
                -- {
                --     effect_id = 2035,
                --     str = [[data\texture\effect\servant_phantom\data\texture\effect\servant_phantom\servant_phantom_shot\servant_phantom_shot.str]],
                --     once_per_hit = true,
                --     delay_ms = 100,
                -- },
                {
                    effect_id = 2036,
                    str = [[data\texture\effect\servant_demolition\servant_demolition_leftdown\servant_demolition_leftdown.str]],
                    once_per_hit = true,
                    delay_ms = 100,
                },
                {
                    effect_id = 2037,
                    str = [[data\texture\effect\servant_demolition\servant_demolition_hit\servant_demolition_hit.str]],
                    once_per_hit = true,
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    delay_ms = 100,
                },
                {
                    effect_id = 2038,
                    str = [[data\texture\effect\acidified_zone_water\acidified_zone_water_cast_bottom\acidified_zone_water_cast_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
            },
        },
        [943] = {
            name = "Ignition Break",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2039,
                    str = [[data\texture\effect\research_report\researchreport_cast\researchreport_cast.str]],
                },
                {
                    effect_id = 2040,
                    str = [[data\texture\effect\research_report\researchreport_cast_bottom\researchreport_cast_bottom.str]],
                    
                    bottom_effect = true,
                    bottom_offset_px = 25,
                },
                {
                    effect_id = 2041,
                    str = [[data\texture\effect\petitio\petitio_bottom\petitio_bottom.str]],
                    bottom_effect = true,
                    bottom_offset_px = 25,
                    hue_shift_degrees = 180, -- Yellow/gold to blue; 0 restores the original.
                },
            },
        },
    },
    -- Effect IDs also configure recovery boosts: 331 = HP, 332 = SP.
    -- These are triggered by recovery packets, independently of skills[4]/skills[9].
    effects = {
        [50088] = { -- Water Tetra Vortex: the server's water-form completion selects this.
            str = [[data\texture\effect\tetravortex\tetra_04_water_explosion\tetra_04_water_expn.str]],
        },
        [50089] = {
            str = [[data\texture\effect\diluvio\diluvio_cold_force\diluvio_cold_force.str]],
        },
        [50090] = {
            str = [[data\texture\effect\diluvio\diluvio_ice_bottom\diluvio_ice_bottom.str]],
            bottom_effect = true,
        },
        [50091] = { -- Wind Tetra Vortex: selected by the server's wind-form completion.
            str = [[data\texture\effect\tetravortex\tetra_02_wind_explosion\tetra_02_wind_expn.str]],
        },
        [50092] = {
            str = [[data\texture\effect\destructive_hurricane\destructive_hurricane\destructive_hurricane.str]],
        },
        [50093] = {
            str = [[data\texture\effect\destructive_hurricane\destructive_hurricane_bottom\destructive_hurricane_bottom.str]],
            bottom_effect = true,
        },
        [50094] = { -- Fire Tetra Vortex: selected by the server's fire-form completion.
            str = [[data\texture\effect\tetravortex\tetra_01_fire_explosion\tetra_01_fire_expn.str]],
        },
        [50095] = {
            str = [[data\texture\effect\ardor\ardor\ardor.str]],
        },
        [50096] = {
            str = [[data\texture\effect\ardor\ardor_flametechnic_bottom\ardor_flametechnic_bottom.str]],
            bottom_effect = true,
        },
        [50097] = { -- Earth Tetra Vortex: selected by the server's earth-form completion.
            str = [[data\texture\effect\tetravortex\tetra_03_earth_explosion\tetra_03_earth_expn.str]],
        },
        [50098] = {
            str = [[data\texture\effect\elemental_buster\elemental_buster\elemental_buster_land\elemental_buster_land.str]],
        },
        [50099] = {
            str = [[data\texture\effect\effligo\effligo_bottom\effligo_bottom.str]],
            bottom_effect = true,
        },
        [50100] = { -- Mixed Tetra Vortex: selected by the server's neutral-form completion.
            str = [[data\texture\effect\tetravortex\tetra_01_fire_explosion\tetra_01_fire_expn.str]],
            hue_shift_degrees = 280, -- Red/orange to purple.
        },
        [50101] = {
            str = [[data\texture\effect\chainlight.str]],
        },
        [50102] = {
            str = [[data\texture\effect\conflagration\conflagration\conflagration.str]],
            hue_shift_degrees = 210, -- Orange/red to blue.
            bottom_effect = true,
        },
        [50103] = {
            str = [[data\texture\effect\new_psychicwave\new_psychicwave\new_psychicwave.str]],
            bottom_effect = true,
        },
        [95] = { -- SK_MG_QUAGMIRE: only the center ground unit controls this loop.
            str = [[data\texture\effect\terremotus\terremotus_earth_care_bottom\terremotus_earth_care_bottom.str]],
            additional_strs = {
                [[data\texture\effect\violentquake\violentquake_bottom\violentquake_bottom.str]],
            },
            loop_wav = [[data\wav\effect\wizard_quagmire.wav]],
            bottom_effect = true,
        },
        [312] = { -- EF_HEAL: shared by every skill that selects this Heal effect.
            str = [[data\texture\effect\argutustelum\argutustelum_cast\argutustelum_cast.str]],
        },
        [313] = { -- EF_HEAL2: shared by every skill that selects this Heal effect.
            str = [[data\texture\effect\argutustelum\argutustelum_cast\argutustelum_cast.str]],
        },
        [1260] = {
            str = [[data\texture\effect\tetravortex\tetra_cast\tetra_cast.str]],
        },
        [1277] = {
            str = [[data\texture\effect\4abc_abyss_slayer\abyss_slayer_cast\abyss_slayer_cast.str]],
        },
        [2045] = { -- Swords to Plowshares: referenced by its casting array.
            str = [[data\texture\effect\increasing_activity\increasing_activity_cast\increasing_activity_cast.str]],
            -- SetCastDurationMs keeps this alive until completion or cancellation.
        },
        [2048] = { 
            str = [[data\texture\effect\competentia\competentia_loop\competentia_loop.str]],
        },
        [2070] = { 
            str = [[data\texture\effect\mil_shield.str]],
            bottom_effect = true,
            bottom_offset_px = 65,
            draw_over_actors = true,
        },
        [2072] = { 
            str = [[data\texture\effect\provoke.str]], 
        },
        [2051] = { 
            str = [[data\texture\effect\competentia\competentia_loop\competentia_loop.str]],
        },
        [2055] = { 
            str = [[data\texture\effect\crossrain\cross_rain_cast\cross_rain_cast.str]],
        },
        [2057] = { 
            str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis_cast\new_rayofgenesis_cast.str]],
        },
        [50016] = { -- STATUS_MILLENIUMSHIELDS: the actor's status controls this loop.
            str = [[data\texture\effect\holy_shield\holy_shield\holy_shield.str]],
            wav = [[data\wav\effect\ab_offertorium.wav]],
            loop = true,
        },
        [50071] = {
            str = [[data\texture\effect\frostbite\frostbite\frostbite.str]],
        },
        [50076] = { -- Earth sphere artwork, selected by cast level or status element.
            str = [[data\texture\effect\tetravortex\tetra_ball_03_earth\tetra_03_earth.str]],
            loop = true,
        },
        [50077] = { -- Wind sphere artwork.
            str = [[data\texture\effect\tetravortex\tetra_ball_02_wind\tetra_02_wind.str]],
            loop = true,
        },
        [50078] = { -- Fire sphere artwork.
            str = [[data\texture\effect\tetravortex\tetra_ball_01_fire\tetra_01_fire.str]],
            loop = true,
        },
        [50079] = { -- Water sphere artwork.
            str = [[data\texture\effect\tetravortex\tetra_ball_04_water\tetra_04_water.str]],
            loop = true,
        },
        [50063] = { -- White variant shared by Extreme Vacuum, Phantom Spear, and Comet while casting.
            str = [[data\texture\effect\tetravortex\tetra_04_water_ground\tetra_04_water_grd.str]],
            bottom_effect = true,
            saturation_percent = 0,
        },
        [50080] = {
            str = [[data\texture\effect\tetravortex\tetra_04_water_ground\tetra_04_water_grd.str]],
            wav = [[data\wav\effect\ef_beginspell.wav]],
            bottom_effect = true,
            saturation_percent = 0,
        },
        [50081] = {
            str = [[data\texture\effect\tetravortex\tetra_04_water_ground\tetra_04_water_grd.str]],
            wav = [[data\wav\effect\ef_beginspell.wav]],
            bottom_effect = true,
            hue_shift_degrees = 80,
        },
        -- Reuse these IDs in a skill's casting_ground array, or add another color here.
        [50048] = ground_casting_circle(),           -- Extreme Vacuum: original texture colors.
        [50050] = ground_casting_circle(0xB47844),   -- Earth Strain: brown.
        [50051] = ground_casting_circle(0x40E8FF),   -- Lord of Vermilion: cyan.
        [50052] = ground_casting_circle(0x3264FF),   -- Storm Gust: deep blue.
        [50053] = ground_casting_circle(0xFF4030),   -- Meteor Storm: red.
        [50054] = ground_casting_circle(0xB85CFF),   -- Reality Breaker: purple.
        [1032] = { -- EF_MAXPAIN: brief orange guard barrier; there is no authored STR.
            renderer = "guard_sphere",
            texture = [[data\texture\effect\guardK2.tga]],
            wav = [[data\wav\effect\kyrie_guard.wav]],
            offset_y = -7,
            radius = 7,
            height = 16,
            color = 0xFF9F24,
            alpha = 0.8,
            duration_ms = 700,
            fade_in_ms = 100,
            fade_out_ms = 200,
        },
        -- Uncomment either example and choose the STR you want for that boost.
        -- recovery.str is an installed example asset; it is the Priest Recovery visual.
        -- Entries here replace the built-in visual and audio; omit wav for silence.
        [332] = { -- EF_HPTIME / SK_SM_RECOVERY HP boost
            str = [[data\texture\effect\arbitrium\arbitrium_cast\arbitrium_cast.str]],
            wav = [[data\wav\_heal_effect.wav]],
        },
        [331] = { -- EF_SPTIME / SK_SM_SRECOVERY SP boost
            str = [[data\texture\effect\presensacies\presensacies_cast\presensacies_cast.str]],
            wav = [[data\wav\_heal_effect.wav]],
        },
        -- Shared Quicken effect for all weapon types.
        [130] = {
            str = [[data\texture\effect\twohand.str]],
            wav = [[data\wav\effect\knight_twohandquicken.wav]],
            offset_y = -10,
        },
    },
}
