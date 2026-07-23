# Phase 0 glTF Fixture

`phase0_scene.gltf` is a locally authored, deterministic glTF 2.0 fixture. It is intentionally
small enough for CPU-only CI while covering the importer contracts that the refactor must preserve:

- one indexed triangle with positions, normals, and texture coordinates;
- one metallic-roughness PBR material with non-default factors;
- a two-level transform hierarchy whose mesh has world translation `(10, 2, 0)`; and
- one `KHR_lights_punctual` directional light on a separate child node.

The binary payload is embedded as a data URI, so the fixture has no external or network dependency.
The expected counts, decoded positions/normals/UVs/indices, material/light values, hierarchy
translations, and transformed position bounds live in `phase0_scene.expected.json`. Geometry and
bounds are decoded from the embedded binary payload; accessor `min`/`max` metadata is not used as
the oracle.

## License

The locally authored files in this directory, including `phase0_scene.gltf` and
`phase0_scene.expected.json`, are released under
[CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/). This fixture-specific declaration
does not set or change the license of any other repository content.

Validate it with:

```bash
python3 tools/assets/manage_assets.py verify-gltf \
  tests/fixtures/gltf/phase0_scene.gltf \
  --expect tests/fixtures/gltf/phase0_scene.expected.json
```

This fixture proves deterministic input structure and reference integrity. It does not prove that
the current legacy loader implements every contract; Phase 4 importer tests must consume the same
fixture through `GltfImporter` and compare decoded CPU scene data.
