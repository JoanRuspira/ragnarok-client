# Client frame pacing and renderer changes

The installed client's `open-midgard.ini` selects backend 1 (Direct3D 11), and
`C:\ragnarok\client\debug_hp_7692.log` confirms it initialized that backend.
Historical view counters in that log report approximately 0.25 ms for ground,
0.65 ms for actors and 0.30 ms for background preparation. These CPU counters
do not measure GPU completion or individual displayed frame intervals, so they
cannot establish the full cause of visible stutter.

Source inspection found several contributors to uneven motion:

- The frame limiter scheduled every deadline from its actual wake-up time.
  Small scheduling delays therefore accumulated into a slower cadence.
- Rendering waited for the Win32 message queue to become empty. Continuous
  input could delay drawing, and input received during the frame wait was used
  only on a later iteration.
- Camera easing and background model animation advanced by a fixed amount per
  frame, changing their speed with frame rate. Background throttling also
  slowed animation time for objects that were evaluated less frequently.
- Direct3D 11 used one back buffer, the default frame queue limit and unsynchronized
  presentation. Sprite pose reuse was restricted to Vulkan; Direct3D 11 composed
  and uploaded the same poses again on subsequent animation cycles.
- Local player sprite diagnostics could compose two additional palette comparison
  images and write resource information whenever a pose changed.

The limiter now retains absolute deadlines through small scheduling delays and
starts a fresh interval after delays of at least one quarter of a frame. The
main loop pumps up to 256 window messages after waiting and then renders, even
when more messages remain. Redundant gameplay and login sleeps were removed.

Camera easing uses `1 - 0.9^(60 * elapsedSeconds)`, preserving the original
60 Hz response across different or uneven frame rates. Model animations use
fractional elapsed-time progress at their previous nominal 60 Hz speed, wrap
loops before converting to an integer frame, and clamp one-shots to the final
frame. Player and monster ACT timing remains as described in
[actor_animation.md](actor_animation.md).

Direct3D 11 now uses two back buffers and requests a maximum queued frame latency
of one. Resizing unbinds render targets before releasing them and resizing the
swap chain. Presentation retains `DXGI_SWAP_EFFECT_DISCARD` because native UI
fallbacks draw through GDI directly onto the window. Microsoft's
[swap effect documentation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect)
allows multiple buffers for this effect and describes the GDI interoperability
restriction on flip-model swap chains. See also
[frame latency](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice1-setmaximumframelatency)
and [resize requirements](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers).

All modern backends now reuse composed player and monster poses on demand.
Appearance keys include hair, palettes, headgear, weapons and shields. Each of
the two caches has a 64 MiB texture budget and evicts the least recently used
poses that no actor currently displays. Active poses and the newly composed
pose can temporarily exceed this budget. Shared ownership protects displayed
textures from eviction; caches are cleared after mode teardown, while the
original rendering backend is still available. Expensive per-pose diagnostics
are disabled by default; missing-resource and rendering-error reports remain.

## Settings

The following defaults apply when these keys are absent from the executable's
`open-midgard.ini`. Saving graphics options preserves both values.

```ini
[Graphics]
FrameRate=0
VerticalSync=1
```

`FrameRate=0` follows the window's monitor on Windows, rechecking once per second
so moving between monitors updates the cap. If detection fails, or on another
platform, the cap is 60 FPS. Explicit caps are clamped to 30–360 FPS.
`VerticalSync=1` requests synchronized presentation through the selected backend;
`0` disables that request. Actual frame rate depends on display refresh and the
work required by the scene. A cap below refresh can produce uneven display
cadence unless it divides that refresh rate evenly.

## Verification

No build, compilation, executable replacement or in-game test was performed.
Non-build checks cover source integration and numerical timing analysis:

- Camera settling at 24, 30, 60, 120, 144, 240 and 360 Hz matches the analytic
  one-second response, including irregular intervals and long pauses.
- Deadline simulations retain cadence with 0.25 ms wake-up delays at 60, 144
  and 360 Hz and leave a full interval after substantial stalls.
- Background animation phase stays consistent across 30–360 Hz and evaluations
  performed every one, two or eight frames.
- Source checks cover message-pump order, all presentation callers, Direct3D 11
  buffer/latency/resize settings, appearance keys and shared texture ownership.

These calculations and static checks do not validate driver behavior or prove
that visible stutter has been eliminated.

For an in-game check with an executable containing these changes:

1. Confirm `[Mode] Frame pacing target=...` matches the monitor. Compare idle,
   walking, combat, camera rotation and zoom after initial resource loading.
2. Inspect `[FramePacing]` reports every five seconds. They measure main-loop
   start intervals, including waits, update/render work and presentation blocks;
   they are not GPU display timestamps. At 60 Hz, steady intervals should be
   near 16.67 ms; at 144 Hz, near 6.94 ms. Compare p95, p99 and maximum intervals,
   not just average FPS. Map loading and window dragging can inflate a report.
3. Verify actor labels, equipment changes, native menus, loading screens and
   window resizing. Check that walking/combat tempo and background model speed
   remain consistent at 60 and the monitor's higher refresh rate.
4. Repeat a familiar route after its first pass to check pose reuse. Visit a
   crowded area and change equipment to exercise cache eviction and appearance
   changes. Check shutdown on the selected backend.
