#!/usr/bin/env python3
"""Generate the fourteen deterministic forest tree GLB variants used by VoX3D.

The script derives six broadleaf trees and six conifers from the three Blender
base assets, plus two low-probability special trees. Materials stay embedded in
GLB files; all outputs use Y-up coordinates, are centered on X/Z, and touch
Y=0 at their lowest point.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Callable
import copy
import json
import struct

import numpy as np
import trimesh

ROOT = Path(__file__).resolve().parents[1]
TREE_DIR = ROOT / "assets" / "models" / "trees"

DECIDUOUS_BASE = TREE_DIR / "forest-tree-deciduous.glb"
CONIFER_SIMPLE_BASE = TREE_DIR / "forest-tree-conifer-simple.glb"
CONIFER_DETAILED_BASE = TREE_DIR / "forest-tree-conifer-detailed.glb"


@dataclass(frozen=True)
class Variant:
    filename: str
    source: Path
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)
    foliage_scale: tuple[float, float, float] = (1.0, 1.0, 1.0)
    shear_x_per_y: float = 0.0
    keep_foliage: Callable[[int], bool] | None = None
    single_crown: bool = False
    dead_colors: bool = False


def material_name(mesh: trimesh.Trimesh) -> str:
    material = getattr(mesh.visual, "material", None)
    return str(getattr(material, "name", ""))


def is_foliage(mesh: trimesh.Trimesh) -> bool:
    return "green" in material_name(mesh).lower()


def clone_material(mesh: trimesh.Trimesh, dead_colors: bool) -> None:
    if not dead_colors or not is_foliage(mesh):
        return
    mesh.visual = trimesh.visual.TextureVisuals(
        material=trimesh.visual.material.PBRMaterial(
            name="Dead needles",
            baseColorFactor=np.asarray((92, 75, 52, 255), dtype=np.uint8),
            metallicFactor=0.0,
            roughnessFactor=0.95,
        )
    )


def transformed_scene(variant: Variant) -> trimesh.Scene:
    source = trimesh.load(variant.source, force="scene")
    output = trimesh.Scene()
    foliage_index = 0

    all_bounds = source.bounds
    crown_center = np.array([
        (all_bounds[0][0] + all_bounds[1][0]) * 0.5,
        all_bounds[0][1] + (all_bounds[1][1] - all_bounds[0][1]) * 0.72,
        (all_bounds[0][2] + all_bounds[1][2]) * 0.5,
    ])

    for node_name in source.graph.nodes_geometry:
        transform, geometry_name = source.graph[node_name]
        mesh = source.geometry[geometry_name].copy()
        mesh.apply_transform(transform)
        foliage = is_foliage(mesh)

        if foliage:
            if variant.single_crown:
                continue
            if variant.keep_foliage is not None and not variant.keep_foliage(foliage_index):
                foliage_index += 1
                continue
            foliage_index += 1
            mesh.vertices = (
                (mesh.vertices - crown_center)
                * np.asarray(variant.foliage_scale)
                + crown_center
            )

        mesh.vertices *= np.asarray(variant.scale)
        if variant.shear_x_per_y != 0.0:
            mesh.vertices[:, 0] += mesh.vertices[:, 1] * variant.shear_x_per_y
        clone_material(mesh, variant.dead_colors)
        output.add_geometry(mesh, node_name=node_name, geom_name=node_name)

    if variant.single_crown:
        crown = trimesh.creation.icosphere(subdivisions=2, radius=1.0)
        crown.apply_scale((1.30, 0.82, 1.12))
        crown.apply_translation((0.0, 2.58, 0.0))
        crown.visual = trimesh.visual.TextureVisuals(
            material=trimesh.visual.material.PBRMaterial(
                name="Green single crown",
                baseColorFactor=np.asarray((43, 119, 62, 255), dtype=np.uint8),
                metallicFactor=0.0,
                roughnessFactor=0.95,
            )
        )
        output.add_geometry(crown, node_name="SingleCrown", geom_name="SingleCrown")

    normalize_scene(output)
    return merge_scene_by_material(output)



def merge_scene_by_material(scene: trimesh.Scene) -> trimesh.Scene:
    """Collapse all parts sharing one material into one mesh.

    Runtime trees are rendered with instancing, but raylib still submits one
    instanced draw per mesh. Keeping Blender parts separate therefore creates
    dozens of draw calls per tree type.
    """
    groups: dict[str, list[trimesh.Trimesh]] = {}
    materials: dict[str, object] = {}
    for node_name in scene.graph.nodes_geometry:
        transform, geometry_name = scene.graph[node_name]
        mesh = scene.geometry[geometry_name].copy()
        mesh.apply_transform(transform)
        material = getattr(mesh.visual, "material", None)
        name = str(getattr(material, "name", "default"))
        groups.setdefault(name, []).append(mesh)
        if name not in materials and material is not None:
            materials[name] = copy.deepcopy(material)

    merged = trimesh.Scene()
    for index, (name, meshes) in enumerate(sorted(groups.items())):
        combined = trimesh.util.concatenate(meshes)
        if name in materials:
            combined.visual = trimesh.visual.TextureVisuals(material=materials[name])
        combined.remove_unreferenced_vertices()
        node_name = f"Merged_{index:02d}_{name}"
        merged.add_geometry(combined, node_name=node_name, geom_name=node_name)
    return merged


def convert_glb_indices_to_u16(path: Path) -> None:
    data = path.read_bytes()
    magic, version, total = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        raise RuntimeError(f"not a GLB 2.0 file: {path}")
    offset = 12
    json_len, json_type = struct.unpack_from("<II", data, offset); offset += 8
    doc = json.loads(data[offset:offset + json_len].decode("utf-8").rstrip(" \0")); offset += json_len
    bin_len, bin_type = struct.unpack_from("<II", data, offset); offset += 8
    blob = bytearray(data[offset:offset + bin_len])

    index_accessors = {p["indices"] for m in doc.get("meshes", []) for p in m.get("primitives", []) if "indices" in p}
    for accessor_index in sorted(index_accessors):
        accessor = doc["accessors"][accessor_index]
        if accessor.get("componentType") != 5125:
            continue
        view = doc["bufferViews"][accessor["bufferView"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        count = accessor["count"]
        values = struct.unpack_from(f"<{count}I", blob, start)
        if max(values, default=0) >= 65536:
            continue
        while len(blob) % 4:
            blob.append(0)
        new_offset = len(blob)
        blob.extend(struct.pack(f"<{count}H", *values))
        doc["bufferViews"].append({"buffer": 0, "byteOffset": new_offset, "byteLength": count * 2, "target": 34963})
        accessor["bufferView"] = len(doc["bufferViews"]) - 1
        accessor["byteOffset"] = 0
        accessor["componentType"] = 5123
    while len(blob) % 4:
        blob.append(0)
    doc["buffers"][0]["byteLength"] = len(blob)
    json_bytes = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4:
        json_bytes += b" "
    out = bytearray(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(json_bytes) + 8 + len(blob)))
    out.extend(struct.pack("<II", len(json_bytes), 0x4E4F534A)); out.extend(json_bytes)
    out.extend(struct.pack("<II", len(blob), 0x004E4942)); out.extend(blob)
    path.write_bytes(out)


def normalize_scene(scene: trimesh.Scene) -> None:
    bounds = scene.bounds
    center_x = (bounds[0][0] + bounds[1][0]) * 0.5
    center_z = (bounds[0][2] + bounds[1][2]) * 0.5
    translation = np.array((-center_x, -bounds[0][1], -center_z))
    for node_name in list(scene.graph.nodes_geometry):
        transform, geometry_name = scene.graph[node_name]
        mesh = scene.geometry[geometry_name]
        mesh.apply_translation(translation)


def export_variant(variant: Variant) -> None:
    scene = transformed_scene(variant)
    output_path = TREE_DIR / variant.filename
    output_path.write_bytes(scene.export(file_type="glb"))
    convert_glb_indices_to_u16(output_path)
    size = scene.extents
    triangles = sum(len(mesh.faces) for mesh in scene.geometry.values())
    print(
        f"generated {output_path.relative_to(ROOT)} "
        f"triangles={triangles} size={size[0]:.2f}x{size[1]:.2f}x{size[2]:.2f}"
    )


def variants() -> list[Variant]:
    return [
        # Six broadleaf silhouettes.
        Variant("forest-deciduous-clustered.glb", DECIDUOUS_BASE),
        Variant("forest-deciduous-single-crown.glb", DECIDUOUS_BASE, single_crown=True),
        Variant(
            "forest-deciduous-tall.glb",
            DECIDUOUS_BASE,
            scale=(0.82, 1.18, 0.82),
            foliage_scale=(0.90, 1.05, 0.90),
        ),
        Variant(
            "forest-deciduous-wide.glb",
            DECIDUOUS_BASE,
            scale=(1.18, 0.92, 1.12),
            foliage_scale=(1.12, 0.90, 1.08),
        ),
        Variant(
            "forest-deciduous-young.glb",
            DECIDUOUS_BASE,
            scale=(0.68, 0.72, 0.68),
            foliage_scale=(0.88, 0.90, 0.88),
            keep_foliage=lambda index: index not in {1, 5},
        ),
        Variant(
            "forest-deciduous-crooked.glb",
            DECIDUOUS_BASE,
            scale=(0.95, 1.02, 0.95),
            foliage_scale=(1.02, 0.95, 1.02),
            shear_x_per_y=0.15,
        ),
        # Six conifer silhouettes.
        Variant("forest-conifer-simple.glb", CONIFER_SIMPLE_BASE),
        Variant("forest-conifer-detailed.glb", CONIFER_DETAILED_BASE),
        Variant(
            "forest-conifer-young.glb",
            CONIFER_SIMPLE_BASE,
            scale=(0.70, 0.70, 0.70),
        ),
        Variant(
            "forest-conifer-tall.glb",
            CONIFER_DETAILED_BASE,
            scale=(0.74, 1.18, 0.74),
        ),
        Variant(
            "forest-conifer-wide.glb",
            CONIFER_SIMPLE_BASE,
            scale=(1.28, 0.94, 1.28),
            foliage_scale=(1.08, 0.96, 1.08),
        ),
        Variant(
            "forest-conifer-sparse.glb",
            CONIFER_DETAILED_BASE,
            scale=(0.94, 1.04, 0.94),
            keep_foliage=lambda index: index % 2 == 0,
        ),
        # Rare special silhouettes.
        Variant(
            "forest-rare-ancient-deciduous.glb",
            DECIDUOUS_BASE,
            scale=(1.35, 1.18, 1.30),
            foliage_scale=(1.15, 0.95, 1.10),
            shear_x_per_y=-0.12,
        ),
        Variant(
            "forest-rare-dead-conifer.glb",
            CONIFER_DETAILED_BASE,
            scale=(0.88, 1.12, 0.88),
            keep_foliage=lambda index: index % 4 == 0,
            shear_x_per_y=0.05,
            dead_colors=True,
        ),
    ]


def main() -> None:
    TREE_DIR.mkdir(parents=True, exist_ok=True)
    for variant in variants():
        export_variant(variant)


if __name__ == "__main__":
    main()
