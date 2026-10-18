"""Builds bolt.gltf, the robot villager of the custom content example, from boxes on the villager rig.

    python3 make_bolt.py ../src/models/bolt.gltf

Run with Pillow installed. The rig, its joint names and units are described in tools/npc_model.py.
"""

import base64
import json
import math
import struct
import sys
from io import BytesIO

from PIL import Image, ImageDraw

JOINT_NAMES = [
    "root", "pelvis", "hip_l", "leg_l", "foot_l", "toe_l", "hip_r", "leg_r", "foot_r", "toe_r", "tail_base",
    "tail_1", "tail_2", "chest", "shoulder_l", "arm_l", "hand_l", "shoulder_r", "arm_r", "hand_r", "hand_r_end",
    "neck", "neck_2", "neck_end", "head", "head_end",
]
JOINT_PARENTS = [-1, 0, 1, 2, 3, 4, 1, 6, 7, 8, 1, 10, 11, 1, 13, 14, 15, 13, 17, 18, 19, 13, 21, 22, 21, 24]
TRANSLATIONS = [
    (0, 1.0, 0), (0, 0, 0), (0, -0.25, 0), (0, 0, 0), (0.5, 0, 0), (0.4, 0, 0), (0, 0.25, 0), (0, 0, 0),
    (0.5, 0, 0), (0.4, 0, 0), (0.075, 0, -0.475), (0, 0, 0), (0.65, 0, 0), (0.325, 0, 0), (0.425, -0.45, 0),
    (0, 0, 0), (0.35, 0, 0), (0.425, 0.45, 0), (0, 0, 0), (0.35, 0, 0), (0.35, 0, 0), (0.675, 0, 0),
    (0.5, 0, 0.6), (0, 0, 0), (0, 0, 0), (1.2, 0, 0),
]

# boxes: joint, from (along x), to, half size y, half size z, texture region (u0, v0, u1, v1), material
BOXES = [
    ("pelvis", -0.35, 0.35, 0.42, 0.38, (0, 0, 0.5, 0.5), "plates"),
    ("chest", 0.0, 0.7, 0.5, 0.42, (0.5, 0, 1, 0.5), "plates"),
    ("head", -0.1, 1.1, 0.55, 0.6, (0, 0.5, 0.5, 1), "plates"),
    ("leg_l", -0.1, 0.55, 0.17, 0.17, (0.5, 0.5, 1, 1), "plates"),
    ("foot_l", 0.0, 0.45, 0.2, 0.3, (0.5, 0.5, 1, 1), "plates"),
    ("leg_r", -0.1, 0.55, 0.17, 0.17, (0.5, 0.5, 1, 1), "plates"),
    ("foot_r", 0.0, 0.45, 0.2, 0.3, (0.5, 0.5, 1, 1), "plates"),
    ("arm_l", -0.1, 0.4, 0.14, 0.14, (0.5, 0.5, 1, 1), "plates"),
    ("hand_l", 0.0, 0.4, 0.17, 0.17, (0.5, 0.5, 1, 1), "plates"),
    ("arm_r", -0.1, 0.4, 0.14, 0.14, (0.5, 0.5, 1, 1), "plates"),
    ("hand_r", 0.0, 0.4, 0.17, 0.17, (0.5, 0.5, 1, 1), "plates"),
    ("tail_1", 0.0, 0.6, 0.08, 0.08, (0.5, 0.5, 1, 1), "plates"),
]


def plates_texture() -> bytes:
    img = Image.new("RGB", (32, 32), (120, 132, 150))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 15, 15], fill=(96, 112, 136))
    d.rectangle([16, 0, 31, 15], fill=(140, 152, 168))
    d.rectangle([0, 16, 15, 31], fill=(232, 200, 72))
    d.rectangle([16, 16, 31, 31], fill=(88, 96, 112))
    for x in range(2, 32, 6):
        for y in range(2, 32, 6):
            d.point((x, y), fill=(40, 44, 52))
    d.rectangle([3, 19, 12, 28], fill=(48, 56, 72))
    d.rectangle([5, 21, 10, 26], fill=(96, 224, 255))
    buf = BytesIO()
    img.quantize(colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(buf, format="PNG")
    return buf.getvalue()


def box(joint: int, x0, x1, hy, hz, region, positions, normals, uvs, joints, weights, indices, eye_faces=None):
    u0, v0, u1, v1 = region
    faces = [
        ((x1, -hy, -hz), (x1, hy, -hz), (x1, hy, hz), (x1, -hy, hz), (1, 0, 0)),
        ((x0, -hy, hz), (x0, hy, hz), (x0, hy, -hz), (x0, -hy, -hz), (-1, 0, 0)),
        ((x0, hy, -hz), (x1, hy, -hz), (x1, hy, hz), (x0, hy, hz), (0, 1, 0)),
        ((x0, -hy, hz), (x1, -hy, hz), (x1, -hy, -hz), (x0, -hy, -hz), (0, -1, 0)),
        ((x0, -hy, hz), (x0, hy, hz), (x1, hy, hz), (x1, -hy, hz), (0, 0, 1)),
        ((x1, -hy, -hz), (x1, hy, -hz), (x0, hy, -hz), (x0, -hy, -hz), (0, 0, -1)),
    ]
    for fi, (a, b, c, d, n) in enumerate(faces):
        target = eye_faces if eye_faces is not None and fi == 4 else indices
        corners = ((u0, v1), (u1, v1), (u1, v0), (u0, v0)) if target is indices else ((0, 1), (1, 1), (1, 0), (0, 0))
        base = len(positions)
        for p, uv in zip((a, b, c, d), corners):
            positions.append(list(p))
            normals.append(list(n))
            uvs.append([uv[0], uv[1]])
            joints.append([joint, 0, 0, 0])
            weights.append([1.0, 0.0, 0.0, 0.0])
        target.extend([base, base + 1, base + 2, base, base + 2, base + 3])


def main(out_path: str) -> None:
    globals_ = []
    for j in range(len(JOINT_NAMES)):
        t = TRANSLATIONS[j]
        p = JOINT_PARENTS[j]
        globals_.append(tuple(t[k] + (globals_[p][k] if p >= 0 else 0) for k in range(3)))

    positions, normals, uvs, joints, weights, indices, eye_indices = [], [], [], [], [], [], []
    for jname, x0, x1, hy, hz, region, _ in BOXES:
        j = JOINT_NAMES.index(jname)
        eyes = eye_indices if jname == "head" else None
        before = len(positions)
        box(j, x0, x1, hy, hz, region, positions, normals, uvs, joints, weights, indices, eyes)
        for k in range(before, len(positions)):
            positions[k] = [positions[k][i] + globals_[j][i] for i in range(3)]

    bin_data = bytearray()
    views = []
    accessors = []

    def add(values, ctype, atype, target=None):
        fmt = {5126: "f", 5123: "H", 5121: "B"}[ctype]
        comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[atype]
        while len(bin_data) % 4:
            bin_data.append(0)
        start = len(bin_data)
        for v in values:
            bin_data.extend(struct.pack("<" + fmt * comps, *(v if comps > 1 else [v])))
        view = {"buffer": 0, "byteOffset": start, "byteLength": len(bin_data) - start}
        if target:
            view["target"] = target
        views.append(view)
        acc = {"bufferView": len(views) - 1, "componentType": ctype, "count": len(values), "type": atype}
        if atype == "VEC3" and ctype == 5126:
            acc["min"] = [min(v[i] for v in values) for i in range(3)]
            acc["max"] = [max(v[i] for v in values) for i in range(3)]
        accessors.append(acc)
        return len(accessors) - 1

    nodes = []
    for j, jname in enumerate(JOINT_NAMES):
        node = {"name": jname, "translation": list(TRANSLATIONS[j])}
        children = [k for k in range(len(JOINT_NAMES)) if JOINT_PARENTS[k] == j]
        if children:
            node["children"] = children
        nodes.append(node)
    ibm = []
    for j in range(len(JOINT_NAMES)):
        g = globals_[j]
        ibm.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -g[0], -g[1], -g[2], 1])

    attributes = {"POSITION": add(positions, 5126, "VEC3", 34962), "NORMAL": add(normals, 5126, "VEC3", 34962),
                  "TEXCOORD_0": add(uvs, 5126, "VEC2", 34962), "JOINTS_0": add(joints, 5121, "VEC4", 34962),
                  "WEIGHTS_0": add(weights, 5126, "VEC4", 34962)}
    primitives = [{"attributes": attributes, "indices": add(indices, 5123, "SCALAR", 34963), "material": 0},
                  {"attributes": attributes, "indices": add(eye_indices, 5123, "SCALAR", 34963), "material": 1}]

    png = plates_texture()
    while len(bin_data) % 4:
        bin_data.append(0)
    start = len(bin_data)
    bin_data.extend(png)
    views.append({"buffer": 0, "byteOffset": start, "byteLength": len(png)})
    png_view = len(views) - 1

    # walk: legs and arms swing, 1 second loop
    frames = 30
    times = [f / 30.0 for f in range(frames + 1)]
    samplers = []
    channels = []

    def swing(jname, degrees, phase):
        q = []
        for f in range(frames + 1):
            a = math.radians(degrees * math.sin(2 * math.pi * (f / frames + phase)))
            q.append([0, 0, math.sin(a / 2), math.cos(a / 2)])
        samplers.append({"input": add(times, 5126, "SCALAR"), "output": add(q, 5126, "VEC4"),
                         "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1,
                         "target": {"node": JOINT_NAMES.index(jname), "path": "rotation"}})

    swing("leg_l", 35, 0.0)
    swing("leg_r", 35, 0.5)
    swing("arm_l", 40, 0.5)
    swing("arm_r", 40, 0.0)
    bob = [[0, 1.0 + 0.04 * abs(math.sin(2 * math.pi * f / frames)), 0] for f in range(frames + 1)]
    samplers.append({"input": add(times, 5126, "SCALAR"), "output": add(bob, 5126, "VEC3"),
                     "interpolation": "LINEAR"})
    channels.append({"sampler": len(samplers) - 1, "target": {"node": 0, "path": "translation"}})

    armature = {"name": "Armature", "rotation": [0, 0, math.sin(math.radians(45)), math.cos(math.radians(45))],
                "children": [0, len(nodes) + 1]}
    mesh_node = {"name": "bolt_mesh", "mesh": 0, "skin": 0}
    all_nodes = nodes + [armature, mesh_node]
    doc = {
        "asset": {"version": "2.0", "generator": "make_bolt"},
        "scene": 0,
        "scenes": [{"nodes": [len(nodes)]}],
        "nodes": all_nodes,
        "meshes": [{"name": "bolt", "primitives": primitives}],
        "skins": [{"name": "villager", "joints": list(range(len(JOINT_NAMES))), "skeleton": 0,
                   "inverseBindMatrices": add(ibm, 5126, "MAT4")}],
        "materials": [{"name": "plates", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                                   "metallicFactor": 0, "roughnessFactor": 1}},
                      {"name": "eyes", "pbrMetallicRoughness": {"baseColorFactor": [0.4, 0.9, 1, 1],
                                                                 "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0, "sampler": 0}],
        "images": [{"name": "plates", "mimeType": "image/png", "bufferView": png_view}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
        "animations": [{"name": "walk", "samplers": samplers, "channels": channels}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(bin_data),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(bin_data)).decode("ascii")}],
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(doc, f)
    print(f"wrote {out_path}: {len(positions)} vertices, {len(indices) // 3 + len(eye_indices) // 3} triangles")


if __name__ == "__main__":
    main(sys.argv[1])
