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
    return output


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
