#!/usr/bin/env python3
"""Generate deterministic low-poly tree GLB assets for VoX3D."""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
import trimesh

ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "assets" / "models" / "trees"


def material(name: str, rgba: tuple[int, int, int, int]) -> trimesh.visual.material.PBRMaterial:
    """Create a flat-colored PBR material."""
    return trimesh.visual.material.PBRMaterial(
        name=name,
        baseColorFactor=np.asarray(rgba, dtype=np.uint8),
        metallicFactor=0.0,
        roughnessFactor=0.95,
    )


def add_mesh(
    scene: trimesh.Scene,
    mesh: trimesh.Trimesh,
    name: str,
    rgba: tuple[int, int, int, int],
    transform: np.ndarray | None = None,
) -> None:
    """Add one colored mesh to a scene."""
    mesh.visual = trimesh.visual.TextureVisuals(material=material(name, rgba))
    scene.add_geometry(mesh, node_name=name, geom_name=name, transform=transform)


def cylinder_between(
    start: np.ndarray,
    end: np.ndarray,
    radius: float,
    sections: int = 7,
) -> tuple[trimesh.Trimesh, np.ndarray]:
    """Create a low-poly cylinder aligned between two points."""
    vector = end - start
    length = float(np.linalg.norm(vector))
    mesh = trimesh.creation.cylinder(radius=radius, height=length, sections=sections)
    transform = trimesh.geometry.align_vectors([0.0, 0.0, 1.0], vector)
    transform[:3, 3] = (start + end) * 0.5
    return mesh, transform


def make_conifer() -> trimesh.Scene:
    """Create a layered conifer with a narrow pointed silhouette."""
    scene = trimesh.Scene()
    trunk = trimesh.creation.cylinder(radius=0.13, height=2.9, sections=7)
    trunk.apply_translation([0.0, 1.45, 0.0])
    add_mesh(scene, trunk, "trunk", (92, 59, 35, 255))

    layers = [
        (0.78, 0.58, 0.72, (21, 72, 51, 255)),
        (1.15, 0.72, 0.82, (24, 91, 60, 255)),
        (1.55, 0.86, 0.88, (27, 109, 68, 255)),
        (1.98, 0.78, 0.82, (30, 126, 75, 255)),
        (2.38, 0.62, 0.72, (38, 143, 81, 255)),
        (2.72, 0.43, 0.58, (49, 158, 88, 255)),
        (3.00, 0.25, 0.44, (64, 173, 96, 255)),
    ]
    for index, (center_y, radius, height, color) in enumerate(layers):
        cone = trimesh.creation.cone(radius=radius, height=height, sections=9)
        cone.apply_translation([0.0, center_y - height * 0.38, 0.0])
        add_mesh(scene, cone, f"foliage_{index}", color)

    return scene


def make_spreading() -> trimesh.Scene:
    """Create a broad asymmetric tree with visible branches and clustered crown."""
    scene = trimesh.Scene()
    trunk_color = (104, 61, 31, 255)
    # Keep branch segments short, tapered, and rising into the crown. Long
    # horizontal cylinders read as detached logs at gameplay camera distances.
    branch_specs = [
        (np.array([0.0, 0.0, 0.0]), np.array([0.03, 1.42, 0.0]), 0.17),
        (np.array([0.02, 1.08, 0.0]), np.array([-0.28, 1.48, 0.06]), 0.095),
        (np.array([-0.28, 1.48, 0.06]), np.array([-0.58, 1.78, 0.12]), 0.060),
        (np.array([0.02, 1.12, 0.0]), np.array([0.31, 1.48, -0.05]), 0.095),
        (np.array([0.31, 1.48, -0.05]), np.array([0.64, 1.75, -0.12]), 0.060),
        (np.array([0.08, 1.30, 0.0]), np.array([0.15, 1.88, 0.12]), 0.080),
        (np.array([-0.18, 1.48, 0.05]), np.array([-0.46, 1.83, 0.13]), 0.055),
        (np.array([0.22, 1.48, -0.04]), np.array([0.50, 1.80, -0.13]), 0.055),
    ]
    for index, (start, end, radius) in enumerate(branch_specs):
        mesh, transform = cylinder_between(start, end, radius)
        add_mesh(scene, mesh, f"branch_{index}", trunk_color, transform)

    crown_specs = [
        ((-0.88, 2.05, 0.12), (0.88, 0.46, 0.72), (49, 128, 70, 255)),
        ((-0.23, 2.30, -0.02), (0.92, 0.58, 0.78), (60, 151, 76, 255)),
        ((0.52, 2.20, -0.10), (1.00, 0.52, 0.76), (42, 116, 69, 255)),
        ((1.05, 1.98, -0.18), (0.72, 0.42, 0.58), (34, 96, 65, 255)),
        ((0.18, 1.92, 0.36), (0.72, 0.42, 0.66), (73, 166, 84, 255)),
    ]
    for index, (position, scale, color) in enumerate(crown_specs):
        crown = trimesh.creation.icosphere(subdivisions=1, radius=1.0)
        crown.apply_scale(scale)
        crown.apply_translation(position)
        add_mesh(scene, crown, f"crown_{index}", color)

    return scene


def export(scene: trimesh.Scene, filename: str) -> None:
    """Export one generated scene as binary glTF."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUTPUT_DIR / filename
    path.write_bytes(scene.export(file_type="glb"))
    print(f"generated {path.relative_to(ROOT)}")


def main() -> None:
    """Generate all custom VoX3D tree assets."""
    export(make_conifer(), "vox-tree-conifer.glb")
    export(make_spreading(), "vox-tree-spreading.glb")


if __name__ == "__main__":
    main()
