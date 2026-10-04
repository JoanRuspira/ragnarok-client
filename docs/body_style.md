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

The 2008 character, spawn, and movement packets omit body style. Movement and
spawn updates therefore preserve the actor's known style. The local session
keeps it across map changes and resets it when selecting a character; the
server's map-entry look refresh supplies the saved style after login.

## Server compatibility

The supplied rAthena source at `C:/ragnarok/ragnarok` defines
`PACKETVER 20080910` in `src/custom/defines_pre.hpp`. Its
`clif_changelook(..., LOOK_BODY2, ...)` returns before updating `view_data` or
sending a look packet when `PACKETVER < 20150513`. `pc_changelook` saves the
database-facing body value before reaching that guard, which can produce a
successful command and a saved value without a live sprite update.

[rathena_body_style_2008.patch](rathena_body_style_2008.patch) permits the existing
`0x01D7` packet for this custom client without changing the map packet profile.
It is provided for a server using this source configuration; the running
server binary may differ. The server workspace has not been edited.

## Validation

No build was run. GRF inspection verified 84 complete ACT/SPR pairs (168
decompressed files) for classic transcendent jobs, both sexes where applicable,
including mounted lord knights, and styles `1`, `2`, and `3`. The archives also
contain palettes under all three costume directories. The server patch passed
`git apply --check` against the supplied server source.

Runtime checks after the changed sources are available in a client executable:

1. Use `@bodystyle 2` on a job with an alternate sprite. Confirm a client log
   entry with `sprite change ... type=13 value=2` and the changed body.
2. Walk, sit, attack, and open equipment to check animation and preview paths.
3. Observe the character from another client and change maps or reconnect.
4. Use `@bodystyle 0` and confirm the original sprite returns.

If the live command produces no `type=13` look update, check whether the server
binary uses the version guard described above.
