# Actor contact shadows

The original shadow path required `data/sprite/shadow.act` and
`data/sprite/shadow.spr`; a failed load silently disabled shadows for the rest of
the process. Contact shadows now use a generated mask instead.

Players, monsters and NPCs use a shared soft black contact shadow beneath their
sprite. A 64 × 64 alpha mask is generated once per world, so no extra GRF assets
are required. Maximum opacity is 88/255 (about 35%), fading smoothly to a
transparent edge. Perspective turns the ground footprint into an oval.

Player footprints stay fixed during walking, combat and equipment changes.
Monster/NPC footprints are measured from their idle ACT poses and cached by
display job, with a limited maximum size for large sprites. Shadows follow the
same map scale as billboards and honor `m_shadowZoom` and `m_shadowOn`.

The center and corners sample the rendered GND terrain, with the walk grid as a
fallback when GND is unavailable. Outer corners are clipped at map boundaries,
with matching texture coordinates, so actors at the edge retain their shadow.
A shadow uses the actor's forward depth placement and the same 0.0005 depth
offset as its billboard, rather than relying only on the unadjusted ground
depth. Shadows are drawn after opaque terrain/props and before sprite/effect
alpha passes, with depth testing enabled and depth writes disabled. This keeps them
under sprites while walls and props occlude them. Hidden actors and portal
effects do not get a contact shadow; despawning actors fade their shadow too.

Each actor submits one pooled face with four vertices in a non-indexed triangle
strip, using the same primitive path as character billboards.
The texture is cached only after a successful upload, allowing a failed upload
to be retried. The texture and size cache are released when the world ground is
cleared.

The first four player shadow attempts log `[ActorShadow] queued` or
`[ActorShadow] skipped`, including the reason for a skipped shadow. The first four
nonempty shadow passes also log their draw count. This distinguishes skipped
submission from a submitted shadow that is hidden during rendering.

The previous implementation remained invisible in a rebuilt client reported by
the user. Static and numeric checks alone did not establish visible rendering.
The revised path needs another in-game check; no build was run for this change.
Check flat ground,
ramps, camera rotation/zoom, crowded areas, hiding, despawn fades and map changes
with an executable containing the changes. The four-corner footprint approximates
terrain across sharp steps; it is not a full projected terrain decal.
