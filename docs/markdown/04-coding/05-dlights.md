# A muzzle flash makes the ground darker

**Status: unsolved, parked, worked around.** `r_worldDlights 0` in
`misc/android/autoexec.cfg` turns the world's dynamic lighting off on the
device. Flashes no longer light the scenery, which is a smaller loss than the
ground going black every time a shot is fired.

This is the record of what was measured, so that picking it up again does not
start from nothing. Seven theories were tried and all seven were wrong. Every
one came from reading the renderer; every useful fact came from the device.

---

## 1. The symptom, measured

Firing in `m4l1` makes a large area of ground **darker**, not black, in lockstep
with the trigger. From a 30 fps capture, the fraction of the frame darker than
luminance 12:

```
206: 0.183  │ 207: 0.035  208: 0.035  209: 0.035
210: 0.181  │ 211: 0.175  212: 0.173
213: 0.036  │ 214: 0.035  215: 0.036
216: 0.179  │ 217: 0.172  218: 0.173
219: 0.037  │ 220: 0.037  221: 0.037
```

Three frames on, three off, at the firing cadence. Differencing frame 216
against 215:

- **336,359 pixels — 16.2% of the frame** go from mean luminance 25.1 to 6.2
- that is a **4.06× multiply**, and the pixels keep their texture (std 2–8,
  max 68), so it is a modulate and not a paint-over
- the mask is the **ground plane**: the lower half of the frame with hard
  straight polygon edges following the road and the hedgerow silhouette

"Renders black" in the original report is a surface being multiplied down, not
geometry being covered. Chasing a paint-over cost four rounds.

## 2. What is ruled out, and by what

Everything below was measured on the device, not reasoned about.

| Claim | How it died |
|---|---|
| `R_UploadDlights` early-returns on a cleared `tr.pc.c_dlightSurfaces`, so the atlas is never sent | The trace prints `surfaces 36–46` *at the point of upload*. The guard was passing. |
| `glTexSubImage2D` sends `GL_RGBA` into a `GL_RGB` texture — illegal on ES | `err 0x0`. gl4es accepts it. `internal 0x1907` confirms the format really is `GL_RGB`. |
| Terrain is missing the world-origin term in its dynamic texcoords | It is there, `tr_light.c:650-652`. And terrain is **never dlit here** — zero `dlight: terrain` lines in any run. |
| Surfaces shift their texcoords into the atlas frame without the atlas being bound | Zero batches reported it. Every batch that shifted, bound. |
| Faces address atlas rows past the uploaded height | All 24 sampled faces land exactly on their own block, inside the uploaded rows. Worked example below. |
| Quake 3's `ProjectDlightTexture` second pass is drawing black | It **never runs**. Zero lines. |
| The dynamic patch and the static lightmap use different brightness scales | `R_ColorShiftLightingBytes` (`tr_bsp.c:132`) is `<< overbrightShift` then normalise-by-max. `R_DlightSample`'s tail is the same two operations, on the same bytes — `lmData` points into `s_worldData.lighting`, the memcpy of the lump `R_LoadLightmaps` built the static lightmaps from. For an unlit texel the two are identical by construction. |

Worked example of the texcoord check, from `vr_traceDlight 1`:

```
face 1  rows 4.6..6.6  cols 109.7..111.7  offset 0.4062,-0.0234  lm 4x4  at 57,7
```

`offset = x/128 - lmX/128`, so `0.4062 = x/128 - 57/128` gives `x = 109`, and the
face covers cols 109.7–111.7. Same for t: `y = 4`, block 4 tall, rows 4.6–6.6.
Exact. Face 12 (`22x16 at 49,60`, offset `-0.3828,-0.3750`) gives `x=0, y=12`,
covering rows 13–27 of an atlas 28 rows tall. Also exact.

Also checked and not the cause:

- **SMP.** `smpFrame` is declared in both `trGlobals_t` and `backEndState_t` and
  **never assigned anywhere**. Front end and back end are always index 0.
- **`texels` exceeding the uploaded area.** `texels 9275` against `rows 28`
  (3584 texels) looks impossible but is not: `R_RealDlightFace` counts
  `c_dlightTexels` *before* it knows whether any light landed, then bails at
  `if (!added)` without committing to `dli.allocated`, so the block is handed
  straight back out to the next surface. Failed surfaces are counted and take no
  space. This is not a bug and cost a round.

## 3. The one thing that is known to matter

Turning the world's dynamic lighting off — `r_worldDlights 0`, which makes
`R_RealDlightFace`, `R_RealDlightPatch` and `R_RealDlightTerrain` return 0 so
no surface ever gets an atlas block — **makes the dimming stop**.

So the subsystem is involved, despite every component of it measuring correct.
That is the contradiction to resume on.

## 4. Where to pick it up

The next measurement was half-written when this was parked: print, for a few
dlit faces, the raw source texel, the texel actually written into the atlas, and
what `R_ColorShiftLightingBytes` would produce for that same raw texel. All
three should agree for any texel no light reached. If they do not, that is the
bug and the numbers say where. If they do agree, the dimming is downstream of
the patch contents entirely, and the next suspect is what changes in the *draw*
when `tess.dlightMap` becomes non-zero — a different texture object bound on
TMU1, with whatever env, filter and wrap state that carries with it under
gl4es.

One specific thing never checked: `tr.dlightImages[]` are created by
`R_CreateImageOld` with the name `*lightmapD%i`, and the `isLightmap` test is
`strncmp(name, "*lightmap", 9)` — which those names match, so the
`else if (!strncmp(name, "*lightmapD", 10))` branch that sets
`dynamicallyUpdated` is **dead code**. `dynamicallyUpdated` is passed to
`Upload32` and never read there, so it appears inert, but the classification
also decides `internalFormat` and `TMU`. Worth an hour before anything else.

## 5. The diagnostics, and how to use them

All in `vr_traceDlight`, off by default, registered in `tr_init.c`:

| Level | What it does |
|---|---|
| `1` | One line per atlas upload: page, uploaded rows, surface and texel counts, brightest texel of the patch out of 765, the texture's internal format, the GL error, and the overbright and identity scales. Plus per-face and per-terrain-patch reports of exactly which atlas rows and columns each surface's texcoords cover, against the block it was allocated. |
| `2` | Stops measuring, starts staining: the whole atlas is painted **white** before the front end writes to it, and every texel a light reaches is painted magenta on top. Read off the walls. White can only brighten, so anything reading unwritten atlas is unmissable. |
| `3` | Additionally skips `ProjectDlightTexture`. Measured: it never runs here, so this changes nothing. Kept because proving that took a run. |

Two lessons about the probes themselves, which cost more than the bug so far:

**A stain must not look like the level.** The first stain was dark green, in a
level made of hedgerows. Neither of us could tell it from foliage by eye, and
two rounds were spent on "is that green or is that a bush". White was
unambiguous immediately. The pixels also killed it independently: the dimmed
areas have R, G and B roughly equal, which a modulate against a green-only
stain cannot produce.

**Screen recording destroys the log.** A whole run was lost because the Quest's
recorder floods logcat and evicts everything but the shutdown lines.
`adb logcat -G 32M` helps but does not fix it. Record video **or** read the log,
never both in one run.

**Frame differencing beat six rounds of description.** Pulling the recording,
extracting every frame, and differencing a flash frame against its neighbour
produced the cadence, the 4× ratio and the affected geometry in one step. Do
that first next time, not seventh.
