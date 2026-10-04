# Body style support

OpenMidgard receives body style through `ZC_SPRITE_CHANGE2` (`0x01D7`), with
look type `LOOK_BODY2` (`13`) and a 16-bit style ID. The client now stores that
ID for each actor and for the local session, invalidates the actor's sprite
resources, and includes the style in both frame and animation-strip caches.

The resource paths match the GRFs in `C:/ragnarok/client`:

```text
data/sprite/<human>/<body>/<sex>/costume_<style>/<job>_<sex>_<style>.act
data/sprite/<human>/<body>/<sex>/costume_<style>/<job>_<sex>_<style>.spr
data/palette/<body>/costume_<style>/<job>_<sex>_<color>_<style>.pal
```

`<human>`, `<body>`, `<sex>`, and `<job>` use the existing Korean resource
tokens. Alternate bodies can use different job spellings from classic bodies,
so lookup also checks palette and Lua identity tokens and the mounted lord
knight alias. ACT and SPR must both exist; otherwise lookup falls back to the
original body. Style `0` selects the original body. Equipment previews and the
local map-entry presentation use the same style-aware lookup.

The 2008 character records, spawn packets, and movement packets omit body
style. Movement and spawn updates therefore preserve the actor's known style.
The local session keeps it across map changes and loads the selected
character's style from the character-list metadata described below. Live look
updates also update the local character's stored appearance.

## Server compatibility

The supplied rAthena source at `C:/ragnarok/ragnarok` defines
`PACKETVER 20080910` in `src/custom/defines_pre.hpp`. Its
original `clif_changelook(..., LOOK_BODY2, ...)` returned before updating
`view_data` or sending a look packet when `PACKETVER < 20150513`.
`pc_changelook` saved the database-facing body value before reaching that
guard, producing a successful command and a saved value without a live sprite
update. The patch enables the existing `0x01D7` look packet for this profile,
so body styles, including resetting to `0`, update immediately.

[rathena_body_style_2008.patch](rathena_body_style_2008.patch) has been applied to
the supplied server source. It also sends saved body styles in the reserved
20-byte header extension of `HC_ACCEPT_ENTER` (`0x006B`) for the classic Sakexe
profile with at most nine slots:

```text
offset in extension   size   meaning
0                     2      ASCII "BS"
2 + 2 * slot          2      little-endian body style for slots 0 through 8
```

The extension starts at packet offset `4` for the 24-byte header (or `7` for
the 27-byte header). Styles come from the same SQL rows as the character
records and are indexed by slot, so database row order and empty slots do not
affect preview selection. Character records remain 108 bytes, and packet
lengths are unchanged. Other profiles keep the original reserved header.
An unpatched server's header leaves previews at style `0`; the client checks
the marker and bounds before decoding the metadata.

OpenMidgard stores the styles by character ID separately from `CHARACTER_INFO`
and passes them to the character-selection sprite and palette lookups. Each
new character list replaces the previous metadata to prevent styles carrying
over between accounts.

## Validation

No build was run. GRF inspection verified 84 complete ACT/SPR pairs (168
decompressed files) for classic transcendent jobs, both sexes where applicable,
including mounted lord knights, and styles `1`, `2`, and `3`. The archives also
contain palettes under all three costume directories. The combined server
patch passed `git apply --check` before application. No binaries have been
rebuilt or servers restarted. Diff checks and packet-layout checks cover the
reserved-header bounds, nonsequential character slots, and unchanged character
record offsets and packet lengths.

Runtime checks after the changed client and server sources are available in
their executables:

1. Use `@bodystyle 2` on a job with an alternate sprite. Confirm a client log
   entry with `sprite change ... type=13 value=2` and the changed body.
2. Walk, sit, attack, and open equipment to check animation and preview paths.
3. Return to character selection and confirm the saved alternate body is
   visible there. Confirm `character body style slot=... gid=... style=...`
   appears in the client log for each listed character.
4. Observe the character from another client and change maps or reconnect.
5. Use `@bodystyle 0` and confirm the original sprite returns immediately and
   remains the original body on the character-selection screen.

If the live command produces no `type=13` look update, check whether the server
binary uses the version guard described above.
