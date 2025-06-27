# A Gentle Dive into Clustered Deferred Shading

> *"Too many lights"* was once a hard stop for real-time renderers.  This post shows how **clustered
> deferred shading** turns that limit on its head—handling ten-thousand dynamic point lights without
> a single lost frame.  We'll keep the code snippets minimal and focus on the theory that makes it
> tick.

---

## 1 The Problem with 'Classic' Deferred Shading

Deferred shading already solved one headache of forward rendering—**overdraw**—by drawing geometry
once, storing its attributes in a G-buffer, and applying lights later in a full-screen pass.  That
works nicely until the light count itself explodes.  A vanilla deferred fragment shader loops over
*every* light every pixel; performance tanks linearly.

```
// pseudo-frag-shader (classic)
for (int i = 0; i < numLights; ++i)
    accumulateLight(i);
```

*1 000 lights at 1920×1080 means 2 billion light evaluations!*  On our test GPU FPS halves from 60 →
30.  We need a smarter way.

---

## 2 Enter Clusters — Space Subdivision in **3-D**

Tiled forward+ shaded each screen-space **tile**; clustered shading pushes the idea into a third
dimension.  Picture the camera frustum diced into a 3-D grid: \(16\times9\times24 = 3 456\)
**clusters**.  A point light now touches only the clusters whose axis-aligned bounding boxes
intersect its sphere.

Pros:

* The fragment shader considers *at most* the lights tagged to its cluster (≤100 in this demo).
* Lights far in Z never hit near fragments.
* Memory stays linear in cluster count, not in light × screen size.

A good intro to the family of algorithms lives in
[LearnOpenGL's deferred article](https://learnopengl.com/Advanced-Lighting/Deferred-Shading) and
Angelo Ortiz' excellent
*Primer on Efficient Rendering Algorithms & Clustered Shading* [@aortiz](https://www.aortiz.me/2018/12/21/CG.html).

---

## 3 High-Level Pipeline

```text
Geometry Pass → 3-attachment G-buffer
      │
      ▼
Compute ① buildClusterGrid   (grid = 16×9×24)
        ② cullLightsPerCluster  (10 000 lights → lists of ≤100)
      │
      ▼
Lighting Pass → full-screen quad, loops over that ≤100 list
```

Only *two* compute jobs and *one* memory barrier sit between geometry and shading.  Everything else
is ordinary OpenGL drawing.

---

## 4 Where the Magic Happens – Compute Shaders  ⌨️

### 4.1 `clusterShader.glsl` — carving the frustum
*Input*: screen resolution, log-distributed Z-planes, inverse projection matrix.  
*Output*: `Cluster` records (view-space AABB + empty light list) in `clusterSSBO` (binding **1**).

The logarithmic slicing is key—more clusters near the camera where lighting detail matters, fewer
in the distance [[c0de517e](https://c0de517e.blogspot.com/2016/08/the-real-time-rendering-continuum.html)].

### 4.2 `lightCulling.glsl` — filling the lists
Each GPU thread grabs one cluster, iterates over the global `pointLight[]` array (`lightSSBO`,
binding **2**) and writes indices of intersecting lights until the **cap of 100** is reached.  A
single `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)` later, the fragment stage can read the fully
populated SSBO.

`numGroups = ceil(numClusters / LOCAL_SIZE)` keeps the launch size compact—on our grid that is
3 456 / 128 ≈ 27 work-groups.

---

## 5 Lighting Pass – Small Loops, Big Win

```glsl
vec3 pos   = texelFetch(gPosition, …);
uint z     = clamp( log(|pos.z|/near) * gridZ / log(far/near), 0, gridZ-1 );
uint index = tileX + tileY*gridX + z*gridX*gridY;

for (uint i = 0; i < clusters[index].count; ++i)
    shade( Lights[ clusters[index].indices[i] ] );
```

No matter how many lights the *world* owns (1 → 100 000) the per-pixel loop is bounded.  In this
demo the limit is 100.

---

## 6 Performance Snapshot

| Renderer                          | Lights | 1080p FPS |
|-----------------------------------|--------|-----------|
| *Clustered deferred*              | 10 000 | **60 → 60** |
| *Plain deferred* (no culling)     | 1 000  | 60 → **30** |

The compute stage pays ~5 ms to classify 10 000 lights; the fragment stage is untouched.  The naïve
shader, instead, blows its ALU budget on 1 000 iterations per pixel.

---

## 7 Knobs & Dials

| Tunable                 | Effect |
|-------------------------|--------|
| `gridSizeX/Y/Z`         | Finer grid = fewer lights per cluster, **but** more compute work & SSBO memory. |
| `MAX_LIGHTS_PER_CLUSTER`| Raise above 100 only if pixels actually need more overlap; costs SSBO bytes and fragment ALU. |
| `NR_LIGHTS`             | Linear cost in the light-culling compute shader; negligible elsewhere. |

Instanced proxies, CPU frustum-culling and timer queries are your next-level optimisation friends.

---

## 8 Take-Aways

* 3-D clustering removes *most* per-pixel waste
even with sky-high light counts.
* Compute shaders + SSBOs make the data dance efficient and explicit.
* Hard caps (like *100 lights per cluster*) are pragmatic; raise them only when an art request proves
  you need to.

For more dive into Emil Persson's "Practical Clustered Shading" or the Doom (2016) SIGGRAPH
talk—clustered rendering at AAA scale.

---

*Happy hacking & may your frames stay high!* 