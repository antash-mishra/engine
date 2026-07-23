"""CPU-only glTF structure, payload, fixture, and closure validation."""

from __future__ import annotations

import base64
import hashlib
import math
import pathlib
import struct
from typing import Any, Iterable

from tools.assets.asset_common import (
    REPOSITORY_ROOT,
    VerificationError,
    compare_fields,
    read_json,
    require_array,
    require_index,
    require_nonnegative_integer,
    require_object,
    resolve_under,
    safe_relative_uri,
    sha256_file,
)


COMPONENT_BYTES = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
COMPONENT_FORMATS = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
TYPE_COMPONENTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


def decode_buffer_uri(uri: str, gltf_directory: pathlib.Path) -> bytes:
    if uri.startswith("data:"):
        header, separator, payload = uri.partition(",")
        if not separator or ";base64" not in header:
            raise VerificationError("only base64 data buffer URIs are supported")
        try:
            return base64.b64decode(payload, validate=True)
        except ValueError as error:
            raise VerificationError(f"invalid base64 buffer URI: {error}") from error
    relative = safe_relative_uri(uri)
    path = resolve_under(gltf_directory, relative)
    try:
        return path.read_bytes()
    except OSError as error:
        raise VerificationError(f"cannot read buffer {path}: {error}") from error


def object_array(gltf: dict[str, Any], field: str) -> list[dict[str, Any]]:
    values = require_array(gltf.get(field, []), field)
    return [
        require_object(value, f"{field}[{index}]") for index, value in enumerate(values)
    ]


def range_end(offset: int, count: int, element_bytes: int, stride: int) -> int:
    return offset if count == 0 else offset + stride * (count - 1) + element_bytes


def validate_sparse_accessor(
    accessor: dict[str, Any],
    accessor_index: int,
    buffer_views: list[dict[str, Any]],
) -> None:
    sparse = require_object(accessor.get("sparse"), f"accessors[{accessor_index}].sparse")
    accessor_count = require_nonnegative_integer(
        accessor.get("count"), f"accessors[{accessor_index}].count"
    )
    sparse_count = require_nonnegative_integer(
        sparse.get("count"), f"accessors[{accessor_index}].sparse.count"
    )
    if sparse_count == 0 or sparse_count > accessor_count:
        raise VerificationError(
            f"accessor {accessor_index} sparse count must be within [1, accessor count]"
        )

    indices = require_object(
        sparse.get("indices"), f"accessors[{accessor_index}].sparse.indices"
    )
    indices_view_index = require_index(
        indices.get("bufferView"),
        len(buffer_views),
        f"accessors[{accessor_index}].sparse.indices.bufferView",
    )
    indices_component_type = indices.get("componentType")
    if indices_component_type not in (5121, 5123, 5125):
        raise VerificationError(
            f"accessor {accessor_index} sparse indices require unsigned integer components"
        )
    indices_offset = require_nonnegative_integer(
        indices.get("byteOffset", 0),
        f"accessors[{accessor_index}].sparse.indices.byteOffset",
    )
    indices_view = buffer_views[indices_view_index]
    indices_bytes = COMPONENT_BYTES[indices_component_type]
    if range_end(indices_offset, sparse_count, indices_bytes, indices_bytes) > indices_view[
        "byteLength"
    ]:
        raise VerificationError(f"accessor {accessor_index} sparse indices exceed bufferView")

    values = require_object(
        sparse.get("values"), f"accessors[{accessor_index}].sparse.values"
    )
    values_view_index = require_index(
        values.get("bufferView"),
        len(buffer_views),
        f"accessors[{accessor_index}].sparse.values.bufferView",
    )
    values_offset = require_nonnegative_integer(
        values.get("byteOffset", 0),
        f"accessors[{accessor_index}].sparse.values.byteOffset",
    )
    component_type = accessor.get("componentType")
    value_type = accessor.get("type")
    element_bytes = COMPONENT_BYTES[component_type] * TYPE_COMPONENTS[value_type]
    if range_end(values_offset, sparse_count, element_bytes, element_bytes) > buffer_views[
        values_view_index
    ]["byteLength"]:
        raise VerificationError(f"accessor {accessor_index} sparse values exceed bufferView")


def validate_references(gltf: dict[str, Any], gltf_path: pathlib.Path) -> dict[str, Any]:
    """Validate glTF containers, indices, and byte ranges independently."""
    asset_metadata = require_object(gltf.get("asset"), "asset")
    buffers = object_array(gltf, "buffers")
    buffer_views = object_array(gltf, "bufferViews")
    accessors = object_array(gltf, "accessors")
    meshes = object_array(gltf, "meshes")
    materials = object_array(gltf, "materials")
    nodes = object_array(gltf, "nodes")
    scenes = object_array(gltf, "scenes")
    images = object_array(gltf, "images")
    textures = object_array(gltf, "textures")
    samplers = object_array(gltf, "samplers")
    cameras = object_array(gltf, "cameras")
    decoded_buffers: list[bytes] = []
    declared_buffer_lengths: list[int] = []

    extensions = require_object(gltf.get("extensions", {}), "extensions")
    punctual_extension = require_object(
        extensions.get("KHR_lights_punctual", {}), "extensions.KHR_lights_punctual"
    )
    lights = require_array(
        punctual_extension.get("lights", []), "extensions.KHR_lights_punctual.lights"
    )
    for index, light in enumerate(lights):
        require_object(light, f"extensions.KHR_lights_punctual.lights[{index}]")

    if asset_metadata.get("version") != "2.0":
        raise VerificationError("asset.version must be exactly '2.0'")
    if not scenes:
        raise VerificationError("scenes must contain at least one scene")
    default_scene = gltf.get("scene", 0)
    if not isinstance(default_scene, int) or not 0 <= default_scene < len(scenes):
        raise VerificationError(f"default scene index is invalid: {default_scene!r}")

    for index, buffer in enumerate(buffers):
        uri = buffer.get("uri")
        if not isinstance(uri, str):
            raise VerificationError(f"buffer {index} has no URI")
        payload = decode_buffer_uri(uri, gltf_path.parent)
        declared = require_nonnegative_integer(
            buffer.get("byteLength"), f"buffers[{index}].byteLength"
        )
        if len(payload) < declared:
            raise VerificationError(
                f"buffer {index} has {len(payload)} bytes, declared byteLength is {declared!r}"
            )
        decoded_buffers.append(payload)
        declared_buffer_lengths.append(declared)

    for index, view in enumerate(buffer_views):
        buffer_index = require_index(
            view.get("buffer"), len(buffers), f"bufferViews[{index}].buffer"
        )
        offset = require_nonnegative_integer(
            view.get("byteOffset", 0), f"bufferViews[{index}].byteOffset"
        )
        length = require_nonnegative_integer(
            view.get("byteLength"), f"bufferViews[{index}].byteLength"
        )
        stride = view.get("byteStride")
        if stride is not None and (not isinstance(stride, int) or not 4 <= stride <= 252):
            raise VerificationError(f"bufferView {index} has invalid byteStride {stride!r}")
        if offset + length > declared_buffer_lengths[buffer_index]:
            raise VerificationError(
                f"bufferView {index} exceeds declared byteLength of buffer {buffer_index}"
            )

    for index, accessor in enumerate(accessors):
        component_type = accessor.get("componentType")
        value_type = accessor.get("type")
        count = require_nonnegative_integer(accessor.get("count"), f"accessors[{index}].count")
        if component_type not in COMPONENT_BYTES or value_type not in TYPE_COMPONENTS:
            raise VerificationError(f"accessor {index} has unsupported component/type contract")
        view_index = accessor.get("bufferView")
        if view_index is None:
            if "sparse" not in accessor:
                raise VerificationError(f"accessor {index} has neither bufferView nor sparse data")
        else:
            view_index = require_index(
                view_index, len(buffer_views), f"accessors[{index}].bufferView"
            )
            element_bytes = COMPONENT_BYTES[component_type] * TYPE_COMPONENTS[value_type]
            view = buffer_views[view_index]
            stride = view.get("byteStride", element_bytes)
            if stride < element_bytes:
                raise VerificationError(f"accessor {index} element exceeds its byteStride")
            accessor_offset = require_nonnegative_integer(
                accessor.get("byteOffset", 0), f"accessors[{index}].byteOffset"
            )
            if range_end(accessor_offset, count, element_bytes, stride) > view["byteLength"]:
                raise VerificationError(f"accessor {index} exceeds bufferView {view_index}")
        if "sparse" in accessor:
            validate_sparse_accessor(accessor, index, buffer_views)

    for mesh_index, mesh in enumerate(meshes):
        primitives = mesh.get("primitives", [])
        if not isinstance(primitives, list) or not primitives:
            raise VerificationError(f"mesh {mesh_index} has no primitives")
        for primitive_index, primitive_value in enumerate(primitives):
            primitive = require_object(
                primitive_value, f"meshes[{mesh_index}].primitives[{primitive_index}]"
            )
            attributes = require_object(
                primitive.get("attributes"),
                f"meshes[{mesh_index}].primitives[{primitive_index}].attributes",
            )
            references: list[Any] = list(attributes.values())
            if "indices" in primitive:
                references.append(primitive["indices"])
            for reference in references:
                require_index(
                    reference,
                    len(accessors),
                    f"meshes[{mesh_index}].primitives[{primitive_index}].accessor",
                )
            material = primitive.get("material")
            if material is not None:
                require_index(
                    material,
                    len(materials),
                    f"meshes[{mesh_index}].primitives[{primitive_index}].material",
                )

    for node_index, node in enumerate(nodes):
        mesh = node.get("mesh")
        if mesh is not None:
            require_index(mesh, len(meshes), f"nodes[{node_index}].mesh")
        children = require_array(node.get("children", []), f"nodes[{node_index}].children")
        for child in children:
            require_index(child, len(nodes), f"nodes[{node_index}].children")
        node_extensions = require_object(
            node.get("extensions", {}), f"nodes[{node_index}].extensions"
        )
        if "KHR_lights_punctual" in node_extensions:
            light_reference = require_object(
                node_extensions["KHR_lights_punctual"],
                f"nodes[{node_index}].extensions.KHR_lights_punctual",
            )
            require_index(
                light_reference.get("light"),
                len(lights),
                f"nodes[{node_index}].extensions.KHR_lights_punctual.light",
            )

    for scene_index, scene in enumerate(scenes):
        scene_nodes = require_array(scene.get("nodes", []), f"scenes[{scene_index}].nodes")
        for node in scene_nodes:
            require_index(node, len(nodes), f"scenes[{scene_index}].nodes")

    for texture_index, texture in enumerate(textures):
        source = texture.get("source")
        sampler = texture.get("sampler")
        if source is not None:
            require_index(source, len(images), f"textures[{texture_index}].source")
        if sampler is not None:
            require_index(sampler, len(samplers), f"textures[{texture_index}].sampler")

    for image_index, image in enumerate(images):
        uri = image.get("uri")
        view_index = image.get("bufferView")
        if uri is not None and not isinstance(uri, str):
            raise VerificationError(f"images[{image_index}].uri must be a string")
        if view_index is not None:
            require_index(view_index, len(buffer_views), f"images[{image_index}].bufferView")
        if uri is None and view_index is None:
            raise VerificationError(f"image {image_index} has neither uri nor bufferView")

    validate_node_cycles(nodes)
    return {
        "scene_count": len(scenes),
        "node_count": len(nodes),
        "mesh_count": len(meshes),
        "primitive_count": sum(len(mesh.get("primitives", [])) for mesh in meshes),
        "material_count": len(materials),
        "light_count": len(lights),
        "camera_count": len(cameras),
        "default_scene": default_scene,
    }


def validate_node_cycles(nodes: list[dict[str, Any]]) -> None:
    visited: set[int] = set()
    active: set[int] = set()

    def visit(node_index: int) -> None:
        if node_index in active:
            raise VerificationError(f"node hierarchy contains a cycle at node {node_index}")
        if node_index in visited:
            return
        active.add(node_index)
        for child in nodes[node_index].get("children", []):
            visit(child)
        active.remove(node_index)
        visited.add(node_index)

    for index in range(len(nodes)):
        visit(index)


def local_uri_closure(
    gltf: dict[str, Any], entrypoint: pathlib.PurePosixPath
) -> list[pathlib.PurePosixPath]:
    closure = {entrypoint}
    for section in ("buffers", "images"):
        for item in gltf.get(section, []):
            uri = item.get("uri")
            if not isinstance(uri, str) or uri.startswith("data:"):
                continue
            closure.add(entrypoint.parent / safe_relative_uri(uri))
    return sorted(closure, key=str)


def closure_fingerprint(
    root: pathlib.Path, paths: Iterable[pathlib.PurePosixPath]
) -> dict[str, Any]:
    """Hash sorted path, byte count, and content hash tuples."""
    tree_digest = hashlib.sha256()
    total_bytes = 0
    file_count = 0
    for relative in paths:
        path = resolve_under(root, relative)
        if not path.is_file():
            raise VerificationError(f"required closure file is missing: {relative}")
        size = path.stat().st_size
        content_digest = sha256_file(path)
        tree_digest.update(str(relative).encode("utf-8"))
        tree_digest.update(b"\0")
        tree_digest.update(str(size).encode("ascii"))
        tree_digest.update(b"\0")
        tree_digest.update(content_digest.encode("ascii"))
        tree_digest.update(b"\n")
        total_bytes += size
        file_count += 1
    return {
        "algorithm": "sha256-v1",
        "file_count": file_count,
        "total_bytes": total_bytes,
        "tree_sha256": tree_digest.hexdigest(),
    }


def normalized_component(value: int | float, component_type: int) -> float:
    if component_type == 5120:
        return max(float(value) / 127.0, -1.0)
    if component_type == 5121:
        return float(value) / 255.0
    if component_type == 5122:
        return max(float(value) / 32767.0, -1.0)
    if component_type == 5123:
        return float(value) / 65535.0
    if component_type == 5125:
        return float(value) / 4294967295.0
    return float(value)


def decode_accessor(
    gltf: dict[str, Any], gltf_path: pathlib.Path, accessor_index: int
) -> list[int | float | list[int | float]]:
    """Decode an accessor payload, applying strides, normalization, and sparse overrides."""
    accessors = object_array(gltf, "accessors")
    buffer_views = object_array(gltf, "bufferViews")
    buffers = object_array(gltf, "buffers")
    require_index(accessor_index, len(accessors), "accessor")
    accessor = accessors[accessor_index]
    component_type = accessor["componentType"]
    component_count = TYPE_COMPONENTS[accessor["type"]]
    component_bytes = COMPONENT_BYTES[component_type]
    element_bytes = component_bytes * component_count
    count = accessor["count"]
    normalized = accessor.get("normalized", False)
    if not isinstance(normalized, bool):
        raise VerificationError(f"accessor {accessor_index} normalized must be boolean")
    payloads = [decode_buffer_uri(buffer["uri"], gltf_path.parent) for buffer in buffers]

    def read_elements(
        view_index: int,
        byte_offset: int,
        element_count: int,
        value_component_type: int,
        value_component_count: int,
        stride: int | None = None,
        apply_normalization: bool = False,
    ) -> list[int | float | list[int | float]]:
        view = buffer_views[view_index]
        payload = payloads[view["buffer"]]
        component_format = COMPONENT_FORMATS[value_component_type]
        value_component_bytes = COMPONENT_BYTES[value_component_type]
        value_element_bytes = value_component_bytes * value_component_count
        value_stride = stride if stride is not None else value_element_bytes
        absolute_start = view.get("byteOffset", 0) + byte_offset
        decoded: list[int | float | list[int | float]] = []
        for element_index in range(element_count):
            element_start = absolute_start + element_index * value_stride
            values = list(
                struct.unpack_from(
                    "<" + component_format * value_component_count, payload, element_start
                )
            )
            if apply_normalization:
                values = [
                    normalized_component(value, value_component_type) for value in values
                ]
            decoded.append(values[0] if value_component_count == 1 else values)
        return decoded

    view_index = accessor.get("bufferView")
    if view_index is None:
        zero: int | list[int] = 0 if component_count == 1 else [0] * component_count
        result = [zero if component_count == 1 else list(zero) for _ in range(count)]
    else:
        view = buffer_views[view_index]
        result = read_elements(
            view_index,
            accessor.get("byteOffset", 0),
            count,
            component_type,
            component_count,
            view.get("byteStride", element_bytes),
            normalized,
        )

    if "sparse" in accessor:
        sparse = accessor["sparse"]
        indices_contract = sparse["indices"]
        sparse_indices = read_elements(
            indices_contract["bufferView"],
            indices_contract.get("byteOffset", 0),
            sparse["count"],
            indices_contract["componentType"],
            1,
        )
        indices = [int(value) for value in sparse_indices]
        if indices != sorted(set(indices)):
            raise VerificationError(
                f"accessor {accessor_index} sparse indices must be strictly increasing"
            )
        if indices and indices[-1] >= count:
            raise VerificationError(
                f"accessor {accessor_index} sparse index exceeds accessor count"
            )
        values_contract = sparse["values"]
        sparse_values = read_elements(
            values_contract["bufferView"],
            values_contract.get("byteOffset", 0),
            sparse["count"],
            component_type,
            component_count,
            None,
            normalized,
        )
        for target_index, value in zip(indices, sparse_values):
            result[target_index] = value
    return result


def translation_vector(node: dict[str, Any]) -> tuple[float, float, float]:
    translation = node.get("translation", [0.0, 0.0, 0.0])
    if not isinstance(translation, list) or len(translation) != 3:
        raise VerificationError("fixture node translation must be a three-component array")
    try:
        result = (float(translation[0]), float(translation[1]), float(translation[2]))
    except (TypeError, ValueError) as error:
        raise VerificationError("fixture node translation must contain numbers") from error
    if not all(math.isfinite(value) for value in result):
        raise VerificationError("fixture node translation must be finite")
    return result


def fixture_world_bounds(gltf: dict[str, Any], gltf_path: pathlib.Path) -> dict[str, list[float]]:
    nodes = gltf["nodes"]
    meshes = gltf["meshes"]
    bounds_min = [math.inf, math.inf, math.inf]
    bounds_max = [-math.inf, -math.inf, -math.inf]
    active_nodes: set[int] = set()

    def visit(node_index: int, parent_translation: tuple[float, float, float]) -> None:
        if node_index in active_nodes:
            raise VerificationError(f"node hierarchy contains a cycle at node {node_index}")
        active_nodes.add(node_index)
        node = nodes[node_index]
        local = translation_vector(node)
        world = tuple(parent_translation[i] + local[i] for i in range(3))
        mesh_index = node.get("mesh")
        if mesh_index is not None:
            for primitive in meshes[mesh_index]["primitives"]:
                positions = decode_accessor(
                    gltf, gltf_path, primitive["attributes"]["POSITION"]
                )
                for position in positions:
                    if not isinstance(position, list) or len(position) != 3:
                        raise VerificationError("fixture POSITION accessor must decode to VEC3")
                    for axis in range(3):
                        transformed = world[axis] + float(position[axis])
                        bounds_min[axis] = min(bounds_min[axis], transformed)
                        bounds_max[axis] = max(bounds_max[axis], transformed)
        for child in node.get("children", []):
            visit(child, world)
        active_nodes.remove(node_index)

    for root in gltf["scenes"][gltf.get("scene", 0)].get("nodes", []):
        visit(root, (0.0, 0.0, 0.0))
    if not all(math.isfinite(value) for value in bounds_min + bounds_max):
        raise VerificationError(f"fixture has no finite POSITION bounds: {gltf_path}")
    return {"min": bounds_min, "max": bounds_max}


def fixture_semantics(gltf: dict[str, Any], gltf_path: pathlib.Path) -> dict[str, Any]:
    """Extract asserted material, light, decoded geometry, and hierarchy facts."""
    materials = gltf["materials"]
    lights = gltf["extensions"]["KHR_lights_punctual"]["lights"]
    if not materials or not lights:
        raise VerificationError("fixture must contain at least one PBR material and punctual light")
    pbr = materials[0].get("pbrMetallicRoughness", {})
    material = {
        "base_color_factor": pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0]),
        "metallic_factor": pbr.get("metallicFactor", 1.0),
        "roughness_factor": pbr.get("roughnessFactor", 1.0),
    }
    light = {
        "type": lights[0].get("type"),
        "color": lights[0].get("color", [1.0, 1.0, 1.0]),
        "intensity": lights[0].get("intensity", 1.0),
    }
    primitive = gltf["meshes"][0]["primitives"][0]
    positions = decode_accessor(gltf, gltf_path, primitive["attributes"]["POSITION"])
    normals = decode_accessor(gltf, gltf_path, primitive["attributes"]["NORMAL"])
    texcoords = decode_accessor(gltf, gltf_path, primitive["attributes"]["TEXCOORD_0"])
    indices = [int(value) for value in decode_accessor(gltf, gltf_path, primitive["indices"])]
    if any(index >= len(positions) for index in indices):
        raise VerificationError("fixture index references a missing position")
    geometry = {
        "positions": positions,
        "normals": normals,
        "texcoords_0": texcoords,
        "indices": indices,
        "position_bounds": {
            "min": [min(float(position[axis]) for position in positions) for axis in range(3)],
            "max": [max(float(position[axis]) for position in positions) for axis in range(3)],
        },
    }

    mesh_translations: list[list[float]] = []
    light_translations: list[list[float]] = []
    active_nodes: set[int] = set()

    def visit(node_index: int, parent_translation: tuple[float, float, float]) -> None:
        if node_index in active_nodes:
            raise VerificationError(f"node hierarchy contains a cycle at node {node_index}")
        active_nodes.add(node_index)
        node = gltf["nodes"][node_index]
        local = translation_vector(node)
        world = tuple(parent_translation[axis] + local[axis] for axis in range(3))
        if "mesh" in node:
            mesh_translations.append(list(world))
        if "KHR_lights_punctual" in node.get("extensions", {}):
            light_translations.append(list(world))
        for child in node.get("children", []):
            visit(child, world)
        active_nodes.remove(node_index)

    for root in gltf["scenes"][gltf.get("scene", 0)].get("nodes", []):
        visit(root, (0.0, 0.0, 0.0))
    return {
        "material_0": material,
        "light_0": light,
        "geometry_0": geometry,
        "mesh_node_world_translations": mesh_translations,
        "light_node_world_translations": light_translations,
    }


def verify_gltf(path: pathlib.Path, expected_path: pathlib.Path | None = None) -> dict[str, Any]:
    gltf = read_json(path)
    summary = validate_references(gltf, path)
    summary.update(fixture_semantics(gltf, path))
    summary["world_position_bounds"] = fixture_world_bounds(gltf, path)
    if expected_path is not None:
        compare_fields(summary, read_json(expected_path))
    return {"status": "pass", "entrypoint": str(path), "summary": summary}


def verify_asset(asset: dict[str, Any], root_override: pathlib.Path | None) -> dict[str, Any]:
    """Verify one extracted asset root without network access or mutation."""
    local = asset["local"]
    root = root_override or (REPOSITORY_ROOT / local["root"])
    entrypoint = pathlib.PurePosixPath(local["entrypoint"])
    entrypoint_path = resolve_under(root, entrypoint)
    if not entrypoint_path.is_file():
        raise VerificationError(f"asset entrypoint is missing: {entrypoint_path}")
    if sha256_file(entrypoint_path) != local["entrypoint_sha256"]:
        raise VerificationError("glTF entrypoint SHA-256 mismatch")
    primary_buffer = resolve_under(root, pathlib.PurePosixPath(local["primary_buffer"]))
    if not primary_buffer.is_file():
        raise VerificationError(f"primary glTF buffer is missing: {primary_buffer}")
    if sha256_file(primary_buffer) != local["primary_buffer_sha256"]:
        raise VerificationError("primary glTF buffer SHA-256 mismatch")
    for required in local.get("required_files", []):
        path = resolve_under(root, pathlib.PurePosixPath(required["path"]))
        if not path.is_file() or path.stat().st_size != required["size"]:
            raise VerificationError(
                f"required asset file missing or wrong size: {required['path']}"
            )
        if sha256_file(path) != required["sha256"]:
            raise VerificationError(f"required asset file SHA-256 mismatch: {required['path']}")

    gltf = read_json(entrypoint_path)
    structure = validate_references(gltf, entrypoint_path)
    closure = closure_fingerprint(root, local_uri_closure(gltf, entrypoint))
    expected_closure = {
        field: asset["gltf_closure"][field]
        for field in ("algorithm", "file_count", "total_bytes", "tree_sha256")
    }
    compare_fields(closure, expected_closure)
    return {
        "status": "pass",
        "asset_id": asset["id"],
        "root": str(root.resolve()),
        "entrypoint_sha256": local["entrypoint_sha256"],
        "closure": closure,
        "structure": structure,
    }
