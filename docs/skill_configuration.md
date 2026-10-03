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
