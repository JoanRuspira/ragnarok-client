-- Open Midgard's source of truth for the skills migrated here.
-- Paths are full GRF keys; variants pair each STR with its WAV.
-- Damage skills use normal player attack hit audio,
-- or data\wav\effect\ef_hit2.wav for non-player casters, once per hit.
-- Hit audio follows WhiteDamageNumbers.DelayBetweenNumbersMs in open-midgard.ini.
-- Completion WAVs play alongside hit audio, once even for per-hit visuals.
-- In a skills entry, silence_hit_sound = false disables only its per-hit audio.
-- Omit silence_hit_sound or set it to true to keep hit audio enabled.
-- Casting WAVs belong to the casting stage; omitting an effect WAV silences that effect.
-- source/target/ground select the attachment. server_notified means the server
-- already sends each effect_id: skill-use packets must not launch them again.
-- Per-attachment *_server_notified overrides allow locally played caster effects.
-- casting effects attach to the caster for the cast's duration and stop together.
-- A nonempty casting array replaces the legacy begin_effect_id.
-- ground effects play at the notified ground cell after casting and stay there.
-- cast_motion_type and skill_motion_type independently select each stage's motion.
-- Omit either to preserve that stage's legacy motion_type behavior.
-- Motion 1 holds the job's skill invocation pose at either stage; 7 plays the full action.
-- Effect entries support bottom_effect = true to draw behind actors, with
-- bottom_offset_px = 4 (default) shifting the visual down in screen pixels.
-- once_per_hit = true repeats an effect at the damage packet's hit cadence.
-- For server-notified effects, the server supplies the first playback.
-- delay_ms = 0 (default) delays the visual and its first WAV; per-hit visual delays accumulate.
return {
    version = 1,
    -- Skill IDs here add audio while preserving the skill's existing visuals.
    -- trigger selects "no_damage" (default), "damage", or "ground" notifications.
    sounds = {
        [6] = { -- SK_SM_PROVOKE
            wav = [[data\wav\effect\swordman_provoke.wav]],
            trigger = "no_damage",
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
    },
    skills = {
        [3] = {
            name = "Two-Handed Sword Mastery", -- SK_SM_TWOHAND
            passive = true, -- A passive weapon bonus, with no cast visual or sound.
        },
        [369] = { -- SK_PA_GOSPEL
            name = "Gospel",
            cast_motion_type = 1,
            skill_motion_type = 1,
            server_notified = true, -- The server supplies effects and moving ground units.
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
                    wav = [[data\wav\effect\cd_presens_acies.wav]],
                },
            },
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
                },
            },
            -- Cast completion supplies the ground cell; damage supplies each victim.
            ground = {
                {
                    effect_id = 50018, -- Local ground presentation ID.
                    str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis\new_rayofgenesis.str]],
                },
            },
            target = {
                {
                    effect_id = 2059,
                    str = [[data\texture\effect\new_rayofgenesis\new_rayofgenesis_hit\new_rayofgenesis_hit.str]],
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
            source = {
                {
                    effect_id = 151,
                    str = [[data\texture\effect\spearboomerang.str]],
                    wav = [[data\wav\effect\knight_spear_boomerang.wav]],
                    offset_y = -10,
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
                },
            },
        },
    },
    -- Effect IDs also configure recovery boosts: 331 = HP, 332 = SP.
    -- These are triggered by recovery packets, independently of skills[4]/skills[9].
    effects = {
        [312] = { -- EF_HEAL: shared by every skill that selects this Heal effect.
            str = [[data\texture\effect\argutustelum\argutustelum_cast\argutustelum_cast.str]],
        },
        [313] = { -- EF_HEAL2: shared by every skill that selects this Heal effect.
            str = [[data\texture\effect\argutustelum\argutustelum_cast\argutustelum_cast.str]],
        },
        [2045] = { -- Swords to Plowshares: referenced by its casting array.
            str = [[data\texture\effect\increasing_activity\increasing_activity_cast\increasing_activity_cast.str]],
            -- SetCastDurationMs keeps this alive until completion or cancellation.
        },
        [2048] = { 
            str = [[data\texture\effect\competentia\competentia_loop\competentia_loop.str]],
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
