# Sprite pose interpolation

Sprite interpolation is enabled by default. The shared actor compositor samples
ACT animations at a target of 60 poses per second while retaining the original
ACT delays, distance-based walking cycle and server combat motion times. The
renderer must still run at 60 FPS to display that many poses.

To compare with original playback, set this value in `open-midgard.ini` and
restart the client:

```ini
[Graphics]
SpriteInterpolation=0
```

Use `1` to restore interpolation. The setting is loaded and saved with the
other graphics settings; it does not have a separate options-window control.

## Sharp sprites

Sharp rendering is also enabled by default and works independently of the
animation clock:

```ini
[Graphics]
SpriteInterpolation=1
SharpSprites=1
SpriteSharpness=4
```

Set `SharpSprites=0` and restart to compare against the original, softer
interpolation while retaining 60 Hz pose sampling. Both settings are preserved
when saving the other graphics options.

`SpriteSharpness` controls how narrowly neighboring source pixels blend during
fractional movement. It is an integer from 1 to 16 and only applies with
`SharpSprites=1`. Restart after changing it in `open-midgard.ini`.

| Value | Appearance |
| --- | --- |
| 1 | Broad, bilinear pixel transitions. |
| 4 | Default; retains the existing sharp mode. |
| 8 | Tighter pixel edges; a useful first comparison for extra sharpness. |
| 16 | Narrowest supported transitions; moving edges may shimmer more. |

Higher values retain the same pose resolution, flow estimation, animation
sampling and per-pose memory size. They only narrow the color transition around
a pixel boundary, keeping it centered. The setting is included in world pose
cache keys and character preview revisions. It affects resampling within poses;
the final GPU texture filter and whole-frame anti-aliasing can still soften the
displayed image. Actual frame rate must be checked in-game.

`AntiAliasing=0` disables the existing whole-frame anti-aliasing pass. Compare it
if that pass is softening sprite outlines; this also changes the appearance of
the map geometry. The global `TextureUpscale` setting does not control these
actor pose surfaces, which already have their own raster resolution.

Sharp sampling keeps each source pixel's color across most of its footprint,
with a narrow transition at boundaries for fractional movement. When the two
warped frames disagree strongly on color or transparency, the compositor keeps
the stronger sample instead of averaging an outline into its background. This
reduces washed-out details and translucent duplicate outlines; matching colors
and ACT position, scale, rotation, attachments and tint still interpolate.

World actors are composed directly at twice the source resolution, including
the intermediate poses. At default or higher sharpness, a static pixel occupies
four identical texture pixels. The final texture filter can
smooth subpixel movement over this finer grid. Geometry, attachment anchors,
labels and hover bounds retain their original units. Texture coordinates also
account for padding on the legacy backend. Legacy and Qt character selection
accessories use the same sharp sampler at their UI surface resolution.

The sharp mode is part of the pose cache keys, so sharp and soft textures cannot
be confused. A 2x pose takes four times the pixel storage and raster work; the
existing cache byte budgets remain in effect and therefore retain fewer poses.
Composition surfaces larger than 1024 pixels in either dimension stay at their
original raster resolution to limit temporary allocations and texture sizes.
This change preserves authored details; it does not invent higher-resolution
artwork. Occluded or mismatched details can still switch between source frames.

## Sampling and composition

`ActMotionSample` retains the current motion, next motion and a quantized
fraction. A 100 ms ACT interval has six sample positions; a 300 ms interval has
18. Nearest sampling accommodates the millisecond clock without repeatedly
missing a sample just below a 60 Hz boundary. Samples at an original pose reuse
that pose's cache entry. Intervals shorter than one rendered frame need no
additional poses. Subdivisions are capped at 120 for unusually long intervals.

Only rendering uses these samples. State transitions, attack impacts, sounds
and action completion continue to use the discrete motion cursor. One-shot
actions hold their last frame and never interpolate back to their first frame.
Frozen or completed body actions retain their pose. Changing actions, facing
directions or player head poses does not blend unrelated actions.

Player body, hair, weapon and shield layers follow the body sample. Their IMF
offsets and attachment points interpolate with that same sample. Idle and
sitting headgear and costumes sample their own ACT delay within the selected
head-direction group. Each of the three equipment slots has an independent
sample; its attachments combine body movement with equipment movement. Walking
and combat equipment continue to follow the character's action clock.

Monsters and NPCs use the same interpolator. Non-player composition bounds
account for the interpolated transforms and image sizes. Hover entries refresh
each rendered frame even when the original motion number has not changed.
Legacy and Qt character selection previews share the equipment sampler, and
their image revisions include the fractional poses.

## Motion estimation

`SpriteInterpolation.cpp` draws into the compositor's premultiplied ARGB
surface. Matching layers interpolate position, scale, the shortest rotation
and color/opacity. Reusing an image only requires transformed sampling.

When the sprite image changes, the compositor estimates bidirectional optical
flow with coarse-to-fine block matching on a four-pixel grid. ACT translations
and attachments are handled separately from the estimated movement within the
artwork. Forward/backward consistency controls confidence, and the renderer
warps both source images before combining them. Transparent pixels and palette
overrides are retained; motion estimates use the original palette and are
shared between dye variants.

The flow cache holds at most 512 pairs or 16 MiB of vector data. Keys include a
sprite revision that changes on resource reload, so reused resource addresses
cannot reuse stale motion estimates. Interpolated player and non-player texture
poses use the existing separate 64 MiB shared billboard caches. Live actors
retain shared ownership of their displayed texture. Both fractional poses and
equipment clocks are included in the texture keys.

This implementation uses the existing CPU compositor and texture APIs on all
render backends. It needs no additional shaders, model downloads or runtime
libraries. Motion estimates and textures are generated on demand; encountering
new artwork can therefore add CPU work and texture uploads before caches warm.

## Limits and validation

Block matching is an approximation and can distort small outlines. Sharp mode
reduces the extra softness caused by spatial and temporal color averaging.
Uncertain pixel matches use the nearer original artwork. Changes in layer
count, image bank or mirroring use a nearer original pose. Images larger than
256 pixels in either dimension retain their original artwork while their ACT
transforms still interpolate. The local search is limited to 21 pixels of
movement within an image. Static poses remain static. This is not a guarantee
of distinct, artifact-free artwork for every frame of every sprite.

All changed C++ sources were parsed without syntax errors, and diff checks were
performed. Direct inspection of the local `data.grf` and `patchRunerok.grf`
covered male Novice bodies, Poring, Lunatic, Fabre, NPC Poring and both sexes of
Books of Magic. Their common transitions support matching image layers; the
monster assets also contain layer-count and mirror changes requiring fallback.
Books of Magic retains its three groups of eight frames at 100 ms per frame.

A separate numerical sampling experiment on 24 indexed images from local
Poring, Lunatic and Fabre SPR files confirmed exact static pixel replication at
2x, valid premultiplied alpha during fractional translations, and UV mappings
for odd and padded dimensions. This experiment does not execute the C++ client.

The tunable sharpness arithmetic was checked against the previous fixed kernel
over 131,073 fractional coordinates. The default gain of 4 matched exactly;
gains 8 and 16 narrowed the transitions while retaining their center, monotonic
coverage and static 2x pixels. Changed sources parsed without syntax errors,
sampler call argument counts were checked, and diff checks passed.

No build, compiled test or in-game validation was run. Source parsing does not
check C++ types, linking, rendering quality or runtime performance.

After a user-provided build, compare enabled and disabled playback:

1. Walk and attack with a player wearing a weapon and shield. Check hair,
   equipment alignment, attack impact sounds and ordinary/high ASPD.
2. Watch Poring, Lunatic and Fabre while idle, walking, attacking, taking damage
   and dying. Check the final death pose, direction changes and hovering.
3. Observe an animated NPC and a static NPC. Static artwork should stay still.
4. Equip animated items in all three visible slots, including Books of Magic.
   Check independent loops while idle/sitting and each head direction, then
   move and attack. Check translucent edges and both character sexes.
5. Leave character selection idle with animated equipment in legacy and Qt UI,
   including Qt CPU composition. Check that frames update without mouse input.
6. Compare a crowded map and an area with unfamiliar actors. Watch frame time,
   first-use hitches and memory use as the caches fill and evict old poses.
7. Compare `SharpSprites=1` and `0` with interpolation enabled. Check eyes,
   black outlines, palette dyes and translucent equipment at normal and closer
   camera zoom. Verify that actor size, attachments and hover bounds agree.
8. With sharp mode enabled, compare `SpriteSharpness=4`, `8` and `16` during
   walking and animated equipment loops. Check tighter edges, moving-edge
   shimmer and frame time. Compare whole-frame anti-aliasing separately.
