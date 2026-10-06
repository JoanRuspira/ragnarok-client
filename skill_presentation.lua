-- Open Midgard's source of truth for the skills migrated here.
-- Paths are full GRF keys. Omit wav for silence; variants pair each STR with its WAV.
-- source/target/ground select the attachment. server_notified means the server
-- already sends each effect_id: skill-use packets must not launch them again.
-- Per-attachment *_server_notified overrides allow locally played caster effects.
return {
    version = 1,
    -- Skill IDs here add audio while preserving the skill's existing visuals.
    -- trigger selects "no_damage" (default), "damage", or "ground" notifications.
    sounds = {
        [6] = { -- SK_SM_PROVOKE
            wav = [[data\wav\effect\swordman_provoke.wav]],
            trigger = "no_damage",
        },
    },
    skills = {
        [3] = {
            name = "Two-Handed Sword Mastery", -- SK_SM_TWOHAND
            passive = true, -- A passive weapon bonus, with no cast visual or sound.
        },
        [398] = {
            name = "Traumatic Strike", 
            motion_type = 2,
            server_notified = true,
            target = {
                {
                    effect_id = 2003,
                    str = [[data\texture\effect\chain_reaction_shot\chain_reaction_shot_hit\chain_reaction_shot_hit.str]],
                    wav = [[data\wav\_hit_sword.wav]],
                },
                {
                    effect_id = 2004,
                    str = [[data\texture\effect\acidified_zone_fire\acidified_zone_fire_b_bottom\acidified_zone_fire_b_bottom.str]],
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
                    wav = [[data\wav\_hit_sword.wav]],
                },
                {
                    effect_id = 2001,
                    str = [[data\texture\effect\acidified_zone_ground\acidified_zone_ground_b_bottom\acidified_zone_ground_b_bottom.str]],
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
                    wav = [[data\wav\_hit_sword.wav]] 
                },
                { 
                    effect_id = 2006, 
                    str = [[data\texture\effect\servantweapon\new_servantweapon\new_servantweapon_hit\new_servantweapon_hit.str]] 
                },
                { 
                    effect_id = 2007,
                    str = [[data\texture\effect\servantweapon\new_servantweapon\new_servantweapon_hit_bottom\new_servantweapon_hit_bottom.str]] 
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


        -- [146] = {
        --     name = "Counterstrike", -- SK_SM_AUTOBERSERK in this server's skill database.
        --     server_notified = true,
        --     source = { { effect_id = 1032 } }, -- The server sends EF_MAXPAIN once on the caster.
        -- },
    },
    -- Effect IDs also configure recovery boosts: 331 = HP, 332 = SP.
    -- These are triggered by recovery packets, independently of skills[4]/skills[9].
    effects = {
        -- [1032] = { -- EF_MAXPAIN: brief orange guard barrier; there is no authored STR.
        --     renderer = "guard_sphere",
        --     texture = [[data\texture\effect\guardK2.tga]],
        --     wav = [[data\wav\effect\kyrie_guard.wav]],
        --     offset_y = -7,
        --     radius = 7,
        --     height = 16,
        --     color = 0xFF9F24,
        --     alpha = 0.8,
        --     duration_ms = 700,
        --     fade_in_ms = 100,
        --     fade_out_ms = 200,
        -- },
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
