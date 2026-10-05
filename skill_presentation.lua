-- Open Midgard's source of truth for the skills migrated here.
-- Paths are full GRF keys. Omit wav for silence; variants pair each STR with its WAV.
-- source/target/ground select the attachment. server_notified means the server
-- already sends each effect_id: skill-use packets must not launch them again.
-- Per-attachment *_server_notified overrides allow locally played caster effects.
return {
    version = 1,
    skills = {
        [3] = {
            name = "Two-Handed Sword Mastery", -- SK_SM_TWOHAND
            passive = true, -- A passive weapon bonus, with no cast visual or sound.
        },
        [398] = {
            name = "Traumatic Strike", -- SK_SM_TRAUMATIC_STRIKE (Head Crush)
            motion_type = 2,
            server_notified = true, -- The server owns the impact effects on the target.
            source_server_notified = false, -- It does not send the caster presentation.
            target = {
                { effect_id = 677, str = [[data\texture\effect\cwound.str]] },
                -- EF_MADNESS_RED (626) is also server-sent; its renderer is not implemented yet.
            },
            -- {
            --     effect_id = 399, -- EF_BASH3D3, an effect ID independent of skill IDs.
            --     renderer = "body_burst", -- This effect is procedural; it has no STR.
            --     texture = [[data\texture\effect\alpha_center.tga]],
            --     -- Head Crush WAV: CP949 filename bytes, independent of Lua file encoding.
            --     wav = "data\\wav\\effect\\\199\236\181\229 \197\169\183\175\189\172.wav",
            --     flash_start_color = 0xFFFFCD,
            --     flash_peak_color = 0xFFFF05,
            --     flash_peak_ms = 200,
            --     flash_end_ms = 500,
            --     halo_alpha = 0.6,
            --     halo_radius = 2,
            --     burst_delay_ms = 500,
            --     burst_duration_ms = 175,
            --     burst_count = 6,
            --     burst_arc_degrees = 12,
            --     burst_height = 7.5, -- Above the actor; reference cell units converted to world units (x5).
            --     burst_alpha = 0.6,
            --     burst_radii = { 22.5, 36 },
            -- },
        },
        [399] = {
            name = "Arcane Strike", -- SK_SM_LIGHTNING_STRIKE
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 1537,
                    str = [[data\texture\effect\new_comet\new_comet_sub_explosion\new_comet_sub_explosion.str]],
                    wav = [[data\wav\_hit_sword.wav]],
                },
                {
                    effect_id = 1538,
                    str = [[data\texture\effect\new_comet\new_comet_sub_explosion_bottom\new_comet_sub_explosion_bottom.str]],
                },
                {
                    effect_id = 1539,
                    str = [[data\texture\effect\4ig_judge\judge_cast\judge_cast.str]],
                },
            },
        },
        [5] = {
            name = "Bash",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2000,
                    str = [[data\texture\effect\acidified_zone_ground\acidified_zone_ground_hit\acidified_zone_ground_hit.str]],
                    wav = [[data\wav\_hit_sword.wav]],
                },
                {
                    effect_id = 2001,
                    str = [[data\texture\effect\acidified_zone_ground\acidified_zone_ground_b_bottom\acidified_zone_ground_b_bottom.str]],
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
                    str = [[data\texture\effect\arbitrium\arbitrium_cast\arbitrium_cast.str]],
                },
                {
                    effect_id = 2003,
                    str = [[data\texture\effect\grand_judgement\grand_judgement_buff\grand_judgement_buff.str]],
                },

            },
        },
        [942] = {
            name = "Magnum Break",
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    wav = [[data\wav\magnumbr.wav]],
                }
            },
        },
    },
    -- Shared server effect: preserve the existing Quicken correction for all weapon types.
    -- This is separate from passive Two-Handed Sword Mastery above.
    effects = {
        [130] = {
            str = [[data\texture\effect\twohand.str]],
            wav = [[data\wav\effect\knight_twohandquicken.wav]],
            offset_y = -10,
        },
    },
}
