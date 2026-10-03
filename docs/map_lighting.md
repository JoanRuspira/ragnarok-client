# Map lighting

The lighting data is loaded through the normal resource lookup, including the
GRFs. No server change or replacement assets are needed for the lighting fix.

GND lightmaps contain two independent parts: alpha is the grayscale shadow
intensity, and RGB is additive coloured illumination. Terrain rendering applies
them after the texture, shared vertex colour, and RSW environment colour:

```text
terrainRGB = clamp(textureRGB * vertexRGB * environmentRGB * lightmapAlpha
                   + lightmapRGB, 0, 1)
environmentRGB = 1 - (1 - ambientRGB) * (1 - diffuseRGB)
```

The old pass sampled only alpha, so it discarded coloured illumination even
though the GND loader and atlas already contained it. Direct3D 7 now uses
`D3DTOP_MODULATEALPHA_ADDCOLOR`; the Direct3D 11/12 and Vulkan shaders use the same
operation. Vertex alpha is preserved, and fog is applied after the lighting.
The embedded Vulkan fragment shader in `VulkanShaders.generated.h` must stay in
sync with `shaders/vulkan_world.hlsl`.

Model lighting also uses the face or smooth vertex normal and the RSW sun
direction. Previously it ignored both and used a constant ambient-plus-diffuse
colour for every face. Unlit models still follow their existing shading mode.
Lit models use the full RSW ambient colour plus directional diffuse light:

```text
sunDirection = -rswLightDirection
modelRGB = clamp(ambientRGB + diffuseRGB * max(dot(normal, sunDirection), 0), 0, 1)
```

Ambient and diffuse values are not reduced or capped by the terrain environment
colour; that extra adjustment made structures too dark.

RSW's light vector points along the rays, with positive Y pointing downward in
world space. Negate it for the normal/light dot product so roofs and the tops of
trees receive sunlight. Using the ray direction directly lit the undersides
instead. This correction is local to model lighting; terrain lighting is unchanged.

The packed RSM texture-vertex colour is not a boolean dimming flag. Previously,
any nonzero value halved lit RGB, including the white default used for RSM 1.1.
Model lighting now always packs RGB on the full 0–255 range. Model opacity and
the unlit shading mode keep their existing behaviour.

Both flat and smooth model normals are transformed with the inverse transpose
of the node's world matrix. Recomputing flat normals from transformed winding
reversed their lighting direction on mirrored instances; transforming smooth
normals with the position matrix also distorted them under nonuniform scale.
The normal matrix is computed once per node. Singular transforms use the local
normal as a finite fallback.

For a visual check, compare `job_knight` (also used by the `pvp_n_3-5` arena alias)
at the central emblem and lamps, then check an outdoor map with shaded buildings.
Use the same assets and camera position on both clients. Verify that coloured
ground illumination appears, model faces respond to the sun direction, and
shadows and fog remain visible.

The fix was checked without a build: the embedded Vulkan lighting instructions
passed six edge cases, 1,000 random samples, and all 66,688 lightmap pixels from
the local `job_knight.gnd`. Its vertex shaders and existing fog/alpha instructions
were unchanged. Direct3D and Vulkan shader source use the same arithmetic. An
in-game comparison is still required to confirm the final appearance.

The model direction correction was also checked without a build against the
first nodes of 35 local Izlude and nearby-object RSMs (4,832 faces). For the 781
upward-facing face normals, the mean red lighting multiplier changed from 0.300
to 0.997 using Izlude's RSW light values. This checks the light direction on asset
geometry, not the final rendering with node transforms and smooth normals.

The normal transform was checked against six Izlude instance scales and 1,000
random invertible transforms, including mirrors, rotations and nonuniform scale.
Transformed normals matched the outward geometric normals and stayed
perpendicular to the surface edges. Singular transforms also retained finite
fallback normals. These checks did not build or run the client.

References:

- [roBrowser terrain shader](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/Renderer/Map/Ground.fs)
- [BrowEdit terrain shader](https://github.com/Borf/BrowEdit3/blob/master/data/shaders/gnd.fs)
- [roBrowser sun direction](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/Renderer/MapRenderer.js)
- [roBrowser RSM mesh generation](https://github.com/MrAntares/roBrowserLegacy/blob/master/src/Loaders/Model.js)
- [Direct3D texture operations](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop)
