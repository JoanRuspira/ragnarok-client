-- Buff/debuff icons shown on the right side of the screen.
-- Keys are server status IDs, not skill IDs. Add more entries to icons below.
-- icon is the full path inside your GRF; [[...]] keeps backslashes literal.
-- These paths override automatic icon lookup. Restart the client after editing.
return {
    version = 1,
    icons = {
        [1353] = { -- EFST_SPIRITGROWTH; skill 819 (SK_SM_SPIRITGROWTH)
            name = "Spirit Growth",
            -- Placeholder: replace with the path to your own icon in the GRF.
            icon = [[data\texture\effect\abc_abyss_dagger.tga]],
        },
    },
}
