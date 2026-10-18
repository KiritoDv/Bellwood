"""Converts villager models and animations between glTF and the game's formats.

    npc_model.py rig <ac.o2r> <species> <out.gltf>     reference rig and meshes of a built-in species
    npc_model.py model <in.gltf> <out dir> <name>      skeleton, meshes and textures for a villager
    npc_model.py anim <in.gltf> <out file> [animation] [--once]

A villager model uses the villager rig: 26 joints named as in JOINT_NAMES, bones along +X, 100 units per metre.
"""

import base64
import json
import math
import os
import struct
import sys
import zipfile
from typing import Dict, List, Optional, Tuple

GAME_UNITS_PER_METRE = 1000.0
FRAMES_PER_SECOND = 30.0

JOINT_NAMES = [
    "root", "pelvis", "hip_l", "leg_l", "foot_l", "toe_l", "hip_r", "leg_r", "foot_r", "toe_r", "tail_base",
    "tail_1", "tail_2", "chest", "shoulder_l", "arm_l", "hand_l", "shoulder_r", "arm_r", "hand_r", "hand_r_end",
    "neck", "neck_2", "neck_end", "head", "head_end",
]
JOINT_PARENTS = [-1, 0, 1, 2, 3, 4, 1, 6, 7, 8, 1, 10, 11, 1, 13, 14, 15, 13, 17, 18, 19, 13, 21, 22, 21, 24]
JOINT_CHILDREN = [1, 4, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 3, 1, 1, 0, 1, 1, 1, 0, 2, 1, 0, 1, 0]
DEFAULT_TRANSLATIONS = [
    (0, 1000, 0), (0, 0, 0), (0, -250, 0), (0, 0, 0), (500, 0, 0), (400, 0, 0), (0, 250, 0), (0, 0, 0),
    (500, 0, 0), (400, 0, 0), (75, 0, -475), (0, 0, 0), (650, 0, 0), (325, 0, 0), (425, -450, 0), (0, 0, 0),
    (350, 0, 0), (425, 450, 0), (0, 0, 0), (350, 0, 0), (350, 0, 0), (675, 0, 0), (500, 0, 600), (0, 0, 0),
    (0, 0, 0), (1200, 0, 0),
]
ROOT_ROTATION_Z = 90.0

SEGMENT_EYE = 0x08
SEGMENT_MOUTH = 0x09
PALETTE_VILLAGER = 15
PALETTE_MODEL = 13

RESOURCE_SKELETON = 0x4F4B4653
RESOURCE_DISPLAY_LIST = 0x4F444C54
RESOURCE_VERTEX = 0x4F565458
RESOURCE_NPC_ANIMATION = 0x4F4E5041
UCODE_F3DEX2_AC = 4

G_VTX_OTR_HASH = 0x32
G_SETTIMG_OTR_HASH = 0x20
G_MARKER = 0x33
EXTENDED_OPCODES = {0x20, 0x24, 0x25, 0x27, 0x29, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42}

CRC64_POLY = 0x42F0E1EBA9EA3693
_crc_table: List[int] = []


def crc64(text: str) -> int:
    if not _crc_table:
        for i in range(256):
            c = i << 56
            for _ in range(8):
                c = ((c << 1) ^ CRC64_POLY) & 0xFFFFFFFFFFFFFFFF if c & (1 << 63) else (c << 1) & 0xFFFFFFFFFFFFFFFF
            _crc_table.append(c)
    crc = 0xFFFFFFFFFFFFFFFF
    for b in text.encode("utf-8"):
        crc = (_crc_table[(crc >> 56) ^ b] ^ (crc << 8)) & 0xFFFFFFFFFFFFFFFF
    return crc


def otr_header(resource_type: int) -> bytes:
    header = struct.pack("<IIIQIQI", 0, resource_type, 0, 0xDEADBEEFDEADBEEF, 0, 0, 0)
    return header + b"\0" * (0x40 - len(header))


def clamp(value: float, low: int, high: int) -> int:
    return int(max(low, min(high, round(value))))


# Vectors and matrices (4x4, row-major lists of 16)

def mat_identity() -> List[float]:
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


def mat_mul(a: List[float], b: List[float]) -> List[float]:
    out = [0.0] * 16
    for r in range(4):
        for c in range(4):
            out[r * 4 + c] = sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))
    return out


def mat_apply(m: List[float], v, w: float = 1.0) -> Tuple[float, float, float]:
    x = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3] * w
    y = m[4] * v[0] + m[5] * v[1] + m[6] * v[2] + m[7] * w
    z = m[8] * v[0] + m[9] * v[1] + m[10] * v[2] + m[11] * w
    return (x, y, z)


def mat_invert(m: List[float]) -> List[float]:
    a = [row[:] for row in (m[0:4], m[4:8], m[8:12], m[12:16])]
    inv = [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for col in range(4):
        pivot = max(range(col, 4), key=lambda r: abs(a[r][col]))
        if abs(a[pivot][col]) < 1e-12:
            raise ValueError("singular matrix")
        a[col], a[pivot] = a[pivot], a[col]
        inv[col], inv[pivot] = inv[pivot], inv[col]
        f = a[col][col]
        a[col] = [x / f for x in a[col]]
        inv[col] = [x / f for x in inv[col]]
        for r in range(4):
            if r != col and a[r][col] != 0.0:
                f = a[r][col]
                a[r] = [x - f * y for x, y in zip(a[r], a[col])]
                inv[r] = [x - f * y for x, y in zip(inv[r], inv[col])]
    return [x for row in inv for x in row]


def mat_from_trs(t, q, s) -> List[float]:
    x, y, z, w = q
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz, wx, wy, wz = x * y, x * z, y * z, w * x, w * y, w * z
    r = [1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy), 2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx),
         2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)]
    return [r[0] * s[0], r[1] * s[1], r[2] * s[2], t[0], r[3] * s[0], r[4] * s[1], r[5] * s[2], t[1],
            r[6] * s[0], r[7] * s[1], r[8] * s[2], t[2], 0, 0, 0, 1]


def mat_from_gltf(m: List[float]) -> List[float]:
    return [m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]]


def quat_to_matrix3(q) -> List[float]:
    m = mat_from_trs((0, 0, 0), q, (1, 1, 1))
    return [m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]]


def quat_normalize(q):
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return [c / n for c in q]


def quat_slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b = [-c for c in b]
        dot = -dot
    if dot > 0.9995:
        return quat_normalize([x + (y - x) * t for x, y in zip(a, b)])
    theta = math.acos(dot)
    sa = math.sin(theta)
    wa = math.sin((1 - t) * theta) / sa
    wb = math.sin(t * theta) / sa
    return [x * wa + y * wb for x, y in zip(a, b)]


def euler_zyx_degrees(r: List[float]) -> Tuple[float, float, float]:
    """Angles (x, y, z) such that the rotation is Rz * Ry * Rx, the order the game applies to a joint."""
    sy = -r[6]
    sy = max(-1.0, min(1.0, sy))
    y = math.asin(sy)
    if abs(sy) < 0.999999:
        x = math.atan2(r[7], r[8])
        z = math.atan2(r[3], r[0])
    else:
        x = math.atan2(-r[5], r[4])
        z = 0.0
    return (math.degrees(x), math.degrees(y), math.degrees(z))


def rotation_z(degrees: float) -> List[float]:
    c = math.cos(math.radians(degrees))
    s = math.sin(math.radians(degrees))
    return [c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


# glTF reading

class Gltf:
    def __init__(self, path: str):
        self.path = path
        self.buffers: List[bytes] = []
        with open(path, "rb") as f:
            data = f.read()
        if data[:4] == b"glTF":
            length = struct.unpack_from("<I", data, 8)[0]
            pos = 12
            self.json = None
            bin_chunk = b""
            while pos < length:
                chunk_len, chunk_type = struct.unpack_from("<II", data, pos)
                chunk = data[pos + 8:pos + 8 + chunk_len]
                if chunk_type == 0x4E4F534A:
                    self.json = json.loads(chunk.decode("utf-8"))
                elif chunk_type == 0x004E4942:
                    bin_chunk = chunk
                pos += 8 + chunk_len
            for buffer in self.json.get("buffers", []):
                self.buffers.append(self._load_buffer(buffer, bin_chunk))
        else:
            self.json = json.loads(data.decode("utf-8"))
            for buffer in self.json.get("buffers", []):
                self.buffers.append(self._load_buffer(buffer, b""))

    def _load_buffer(self, buffer: dict, bin_chunk: bytes) -> bytes:
        uri = buffer.get("uri")
        if uri is None:
            return bin_chunk
        if uri.startswith("data:"):
            return base64.b64decode(uri.split(",", 1)[1])
        with open(os.path.join(os.path.dirname(self.path), uri), "rb") as f:
            return f.read()

    def accessor(self, index: int) -> List:
        acc = self.json["accessors"][index]
        view = self.json["bufferViews"][acc["bufferView"]]
        data = self.buffers[view["buffer"]]
        offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
        count = acc["count"]
        components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[acc["type"]]
        fmt, size = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2), 5125: ("I", 4),
                     5126: ("f", 4)}[acc["componentType"]]
        stride = view.get("byteStride", components * size)
        normalized = acc.get("normalized", False)
        out = []
        for i in range(count):
            values = list(struct.unpack_from("<" + fmt * components, data, offset + i * stride))
            if normalized:
                scale = {"b": 127.0, "B": 255.0, "h": 32767.0, "H": 65535.0}[fmt]
                values = [v / scale for v in values]
            out.append(values[0] if components == 1 else values)
        return out

    def image_bytes(self, index: int) -> bytes:
        image = self.json["images"][index]
        if "bufferView" in image:
            view = self.json["bufferViews"][image["bufferView"]]
            data = self.buffers[view["buffer"]]
            offset = view.get("byteOffset", 0)
            return data[offset:offset + view["byteLength"]]
        uri = image["uri"]
        if uri.startswith("data:"):
            return base64.b64decode(uri.split(",", 1)[1])
        with open(os.path.join(os.path.dirname(self.path), uri), "rb") as f:
            return f.read()

    def node_local(self, node: dict) -> List[float]:
        if "matrix" in node:
            return mat_from_gltf(node["matrix"])
        return mat_from_trs(node.get("translation", [0, 0, 0]), node.get("rotation", [0, 0, 0, 1]),
                            node.get("scale", [1, 1, 1]))

    def node_globals(self) -> List[List[float]]:
        nodes = self.json.get("nodes", [])
        parents = [-1] * len(nodes)
        for i, node in enumerate(nodes):
            for child in node.get("children", []):
                parents[child] = i
        globals_: List[Optional[List[float]]] = [None] * len(nodes)

        def compute(i: int) -> List[float]:
            if globals_[i] is None:
                local = self.node_local(nodes[i])
                globals_[i] = local if parents[i] < 0 else mat_mul(compute(parents[i]), local)
            return globals_[i]

        return [compute(i) for i in range(len(nodes))], parents


# Writing the game's formats

def write_skeleton(path: str, dl_paths: List[str], translations: List[Tuple[int, int, int]]) -> None:
    out = bytearray(otr_header(RESOURCE_SKELETON))
    out += struct.pack("<BBBB", len(JOINT_NAMES), sum(1 for p in dl_paths if p), 0, len(JOINT_NAMES))
    for i in range(len(JOINT_NAMES)):
        dl = dl_paths[i].encode("utf-8")
        out += struct.pack("<I", len(dl)) + dl
        out += struct.pack("<BBhhh", JOINT_CHILDREN[i], 0, *translations[i])
    with open(path, "wb") as f:
        f.write(out)


def write_vertices(path: str, vertices: List[tuple]) -> None:
    out = bytearray(otr_header(RESOURCE_VERTEX))
    out += struct.pack("<I", len(vertices))
    for x, y, z, s, t, nx, ny, nz, a in vertices:
        out += struct.pack("<hhhHhhbbbB", x, y, z, 0, s, t, nx, ny, nz, a)
    with open(path, "wb") as f:
        f.write(out)


def gfx(w0: int, w1: int) -> bytes:
    return struct.pack("<II", w0 & 0xFFFFFFFF, w1 & 0xFFFFFFFF)


def gfx_hash(text: str) -> bytes:
    h = crc64(text)
    return struct.pack("<II", h >> 32, h & 0xFFFFFFFF)


def settile(fmt, siz, line, tmem, tile, palette, cmt, maskt, shiftt, cms, masks, shifts) -> bytes:
    w0 = (0xF5 << 24) | (fmt << 21) | (siz << 19) | (line << 9) | tmem
    w1 = (tile << 24) | (palette << 20) | (cmt << 18) | (maskt << 14) | (shiftt << 10) | (cms << 8) | (masks << 4) | shifts
    return gfx(w0, w1)


def settilesize(tile, w, h) -> bytes:
    return gfx(0xF2 << 24, (tile << 24) | (((w - 1) << 2) << 12) | ((h - 1) << 2))


def texture_block_ci4(w: int, h: int, palette: int, wrap_s: bool, wrap_t: bool, image_words: bytes) -> bytes:
    """Loads a CI4 texture of w x h texels, the way gDPLoadTextureBlock_4b does."""
    out = bytearray(image_words)
    out += settile(2, 2, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0)
    out += gfx(0xE6 << 24, 0)
    texels = ((w * h + 3) >> 2) - 1
    dxt = (1 << 11) // max(1, (w >> 1) >> 3) if (w >> 1) >= 8 else 0
    dxt = clamp(((1 << 11) + (w >> 1 >> 3) - 1) // max(1, (w >> 1) >> 3), 0, 0xFFF) if (w >> 1) >= 8 else 0
    out += gfx(0xF3 << 24, (7 << 24) | (texels << 12) | dxt)
    out += gfx(0xE7 << 24, 0)
    line = (((w >> 1) + 7) >> 3)
    mask_s = int(math.log2(w)) if wrap_s else 0
    mask_t = int(math.log2(h)) if wrap_t else 0
    out += settile(2, 0, line, 0, 0, palette, 0 if wrap_t else 2, mask_t, 0, 0 if wrap_s else 2, mask_s, 0)
    out += settilesize(0, w, h)
    return bytes(out)


def segment_image(segment: int, width: int) -> bytes:
    return gfx((0xFD << 24) | (2 << 21) | (2 << 19) | 0, (segment << 24) | 1)


def hashed_image(path: str, fmt: int, siz: int) -> bytes:
    return gfx((G_SETTIMG_OTR_HASH << 24) | (fmt << 21) | (siz << 19), 0) + gfx_hash(path)


def palette_block(path: str, slot: int) -> bytes:
    out = bytearray(hashed_image(path, 0, 2))
    out += gfx(0xE8 << 24, 0)
    out += settile(0, 0, 0, 256 + slot * 16, 7, 0, 0, 0, 0, 0, 0, 0)
    out += gfx(0xE6 << 24, 0)
    out += gfx(0xF0 << 24, (7 << 24) | (15 << 14))
    out += gfx(0xE7 << 24, 0)
    return bytes(out)


def display_list_header(path: str) -> bytes:
    out = bytearray(struct.pack("<B", UCODE_F3DEX2_AC) + b"\xff" * 7)
    out += gfx(0, 0)
    out += gfx(G_MARKER << 24, 0xBEEFBEEF) + gfx_hash(path)
    out += gfx(0xD7000002, 0)
    out += gfx(0xD9000000, 0x00230405)
    out += gfx(0xE200001C, 0xC8112078)
    out += gfx(0xFC127E60, 0xFFFFF3F8)
    out += gfx(0xFA000080, 0xFFFFFFFF)
    return bytes(out)


def vertex_load(vtx_path: str, start: int, count: int, first_vertex: int) -> bytes:
    return gfx((G_VTX_OTR_HASH << 24) | (count << 12) | ((start + count) << 1), first_vertex * 16) + gfx_hash(vtx_path)


def triangle_batches(triangles: List[Tuple[int, int, int]], cache: int = 32):
    """Groups triangles so each group's vertices fit the vertex cache. Yields (vertex list, local triangles)."""
    remaining = list(triangles)
    while remaining:
        used: Dict[int, int] = {}
        batch = []
        rest = []
        for tri in remaining:
            needed = [v for v in tri if v not in used]
            if len(used) + len(set(needed)) <= cache:
                for v in needed:
                    if v not in used:
                        used[v] = len(used)
                batch.append(tuple(used[v] for v in tri))
            else:
                rest.append(tri)
        yield list(used.keys()), batch
        remaining = rest


def triangle_commands(local_tris: List[Tuple[int, int, int]]) -> bytes:
    out = bytearray()
    i = 0
    while i < len(local_tris):
        a = local_tris[i]
        if i + 1 < len(local_tris):
            b = local_tris[i + 1]
            out += gfx((0x06 << 24) | (a[0] << 17) | (a[1] << 9) | (a[2] << 1), (b[0] << 17) | (b[1] << 9) | (b[2] << 1))
            i += 2
        else:
            out += gfx((0x05 << 24) | (a[0] << 17) | (a[1] << 9) | (a[2] << 1), 0)
            i += 1
    return bytes(out)


# Model conversion

class Material:
    def __init__(self, name: str, kind: str, width: int, height: int, texture_path: str = "", palette_path: str = ""):
        self.name = name
        self.kind = kind
        self.width = width
        self.height = height
        self.texture_path = texture_path
        self.palette_path = palette_path


def material_kind(name: str) -> str:
    lowered = name.lower()
    if lowered.startswith("eye"):
        return "eye"
    if lowered.startswith("mouth"):
        return "mouth"
    return "texture"


def convert_model(gltf_path: str, out_dir: str, name: str, resource_prefix: str, log=print) -> str:
    """Writes <name>.skeleton, <name>.vtx, <name>.<joint>.dl and textures into out_dir. Returns the skeleton path."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import merge  # noqa: E402

    g = Gltf(gltf_path)
    nodes = g.json.get("nodes", [])
    globals_, parents = g.node_globals()
    joint_nodes = {}
    for i, node in enumerate(nodes):
        if node.get("name") in JOINT_NAMES and node["name"] not in joint_nodes:
            joint_nodes[node["name"]] = i
    translations = []
    for j, jname in enumerate(JOINT_NAMES):
        if jname not in joint_nodes:
            log(f"npc_model: joint {jname} missing, using the default position")
            translations.append(DEFAULT_TRANSLATIONS[j])
            continue
        pos = mat_apply(globals_[joint_nodes[jname]], (0, 0, 0))
        parent = JOINT_PARENTS[j]
        if parent >= 0 and JOINT_NAMES[parent] in joint_nodes:
            local = mat_apply(mat_invert(globals_[joint_nodes[JOINT_NAMES[parent]]]), pos)
        elif parents[joint_nodes[jname]] >= 0:
            local = mat_apply(mat_invert(globals_[parents[joint_nodes[jname]]]), pos)
        else:
            local = pos
        translations.append(tuple(clamp(c * GAME_UNITS_PER_METRE, -32768, 32767) for c in local))

    joint_inverse = {}
    for jname, ni in joint_nodes.items():
        joint_inverse[JOINT_NAMES.index(jname)] = mat_invert(globals_[ni])

    materials: Dict[int, Material] = {}
    os.makedirs(out_dir, exist_ok=True)
    for mi, mat in enumerate(g.json.get("materials", [])):
        mname = mat.get("name", f"material{mi}")
        kind = material_kind(mname)
        if kind == "eye" or kind == "mouth":
            materials[mi] = Material(mname, kind, 32, 16)
            continue
        safe = "".join(c if c.isalnum() else "_" for c in mname)
        tex_file = f"{name}.{safe}.ci4"
        pal_file = f"{name}.{safe}.ci4_pal"
        pbr = mat.get("pbrMetallicRoughness", {})
        png = None
        if "baseColorTexture" in pbr:
            texture = g.json["textures"][pbr["baseColorTexture"]["index"]]
            png = g.image_bytes(texture["source"])
        from PIL import Image
        from io import BytesIO
        if png is not None:
            img = Image.open(BytesIO(png)).convert("RGBA")
        else:
            color = pbr.get("baseColorFactor", [1, 1, 1, 1])
            img = Image.new("RGBA", (8, 8), tuple(int(c * 255) for c in color))
        w, h = img.size
        w2 = 1 << max(3, min(6, int(round(math.log2(max(1, w))))))
        h2 = 1 << max(3, min(6, int(round(math.log2(max(1, h))))))
        while w2 * h2 > 64 * 32:
            if w2 >= h2:
                w2 >>= 1
            else:
                h2 >>= 1
        if (w2, h2) != (w, h):
            log(f"npc_model: resizing texture of {mname} from {w}x{h} to {w2}x{h2}")
            img = img.resize((w2, h2), Image.NEAREST)
        buf = BytesIO()
        img.save(buf, format="PNG")
        key = f"{gltf_path}:{mname}"
        merge._palette_cache.pop((key, 16), None)
        with open(os.path.join(out_dir, tex_file), "wb") as f:
            f.write(merge.convert_png_asset(buf.getvalue(), key, "CI4"))
        with open(os.path.join(out_dir, pal_file), "wb") as f:
            f.write(merge.convert_png_asset(buf.getvalue(), key, "CI4_PAL"))
        materials[mi] = Material(mname, kind, w2, h2, resource_prefix + tex_file, resource_prefix + pal_file)
    if not materials:
        materials[-1] = Material("default", "texture", 8, 8)

    # vertices per joint and material
    vertices: List[tuple] = []
    vertex_index: Dict[tuple, int] = {}
    per_joint: Dict[int, Dict[int, List[Tuple[int, int, int]]]] = {}
    skins = g.json.get("skins", [])
    for ni, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        mesh = g.json["meshes"][node["mesh"]]
        skin = skins[node["skin"]] if "skin" in node else None
        bind_inverse = {}
        if skin is not None and "inverseBindMatrices" in skin:
            for k, m in enumerate(g.accessor(skin["inverseBindMatrices"])):
                bind_inverse[k] = mat_from_gltf(m)
        node_joint = None
        p = ni
        while p >= 0 and node_joint is None:
            if nodes[p].get("name") in joint_nodes:
                node_joint = JOINT_NAMES.index(nodes[p]["name"])
            p = parents[p]
        for prim in mesh.get("primitives", []):
            attrs = prim["attributes"]
            positions = g.accessor(attrs["POSITION"])
            normals = g.accessor(attrs["NORMAL"]) if "NORMAL" in attrs else [[0, 1, 0]] * len(positions)
            uvs = g.accessor(attrs["TEXCOORD_0"]) if "TEXCOORD_0" in attrs else [[0, 0]] * len(positions)
            joints = g.accessor(attrs["JOINTS_0"]) if skin and "JOINTS_0" in attrs else None
            weights = g.accessor(attrs["WEIGHTS_0"]) if skin and "WEIGHTS_0" in attrs else None
            indices = g.accessor(prim["indices"]) if "indices" in prim else list(range(len(positions)))
            mi = prim.get("material", -1)
            material = materials.get(mi, materials[next(iter(materials))])
            mesh_matrix = globals_[ni]
            remap: Dict[int, int] = {}
            tri_joint: Dict[int, int] = {}
            for vi, pos in enumerate(positions):
                if joints is not None:
                    best = max(range(4), key=lambda k: weights[vi][k])
                    skin_index = int(joints[vi][best])
                    joint_node = skin["joints"][skin_index]
                    jname = nodes[joint_node].get("name")
                    j = JOINT_NAMES.index(jname) if jname in JOINT_NAMES else (node_joint if node_joint is not None else 1)
                    inv = bind_inverse.get(skin_index, joint_inverse.get(j, mat_identity()))
                    local = mat_apply(inv, pos)
                    nrm = mat_apply(inv, normals[vi], 0.0)
                else:
                    j = node_joint if node_joint is not None else 1
                    inv = joint_inverse.get(j, mat_identity())
                    local = mat_apply(inv, mat_apply(mesh_matrix, pos))
                    nrm = mat_apply(inv, mat_apply(mesh_matrix, normals[vi], 0.0), 0.0)
                nlen = math.sqrt(sum(c * c for c in nrm)) or 1.0
                nrm = [c / nlen for c in nrm]
                s = clamp(uvs[vi][0] * material.width * 32, -32768, 32767)
                t = clamp(uvs[vi][1] * material.height * 32, -32768, 32767)
                vertex = (clamp(local[0] * GAME_UNITS_PER_METRE, -32768, 32767),
                          clamp(local[1] * GAME_UNITS_PER_METRE, -32768, 32767),
                          clamp(local[2] * GAME_UNITS_PER_METRE, -32768, 32767), s, t,
                          clamp(nrm[0] * 127, -128, 127), clamp(nrm[1] * 127, -128, 127),
                          clamp(nrm[2] * 127, -128, 127), 255)
                key = (j, mi, vertex)
                if key not in vertex_index:
                    vertices.append(vertex)
                    vertex_index[key] = len(vertices) - 1
                remap[vi] = vertex_index[key]
                tri_joint[vi] = j
            for k in range(0, len(indices) - 2, 3):
                a, b, c = indices[k], indices[k + 1], indices[k + 2]
                j = tri_joint[a]
                per_joint.setdefault(j, {}).setdefault(mi, []).append((remap[a], remap[b], remap[c]))

    vtx_file = f"{name}.vtx"
    write_vertices(os.path.join(out_dir, vtx_file), vertices)
    vtx_path = resource_prefix + vtx_file

    dl_paths = [""] * len(JOINT_NAMES)
    for j, by_material in per_joint.items():
        dl_file = f"{name}.{JOINT_NAMES[j]}.dl"
        dl_path = resource_prefix + dl_file
        out = bytearray(display_list_header(dl_path))
        for mi, tris in by_material.items():
            material = materials.get(mi, materials[next(iter(materials))])
            if material.kind == "eye":
                out += texture_block_ci4(32, 16, PALETTE_VILLAGER, False, False, segment_image(SEGMENT_EYE, 1))
            elif material.kind == "mouth":
                out += texture_block_ci4(32, 16, PALETTE_VILLAGER, False, False, segment_image(SEGMENT_MOUTH, 1))
            else:
                out += palette_block(material.palette_path, PALETTE_MODEL)
                out += texture_block_ci4(material.width, material.height, PALETTE_MODEL, True, True,
                                         hashed_image(material.texture_path, 2, 2))
            for cache_vertices, local_tris in triangle_batches(tris):
                runs = []
                start = 0
                while start < len(cache_vertices):
                    end = start + 1
                    while end < len(cache_vertices) and cache_vertices[end] == cache_vertices[end - 1] + 1:
                        end += 1
                    runs.append((start, end))
                    start = end
                for s, e in runs:
                    out += vertex_load(vtx_path, s, e - s, cache_vertices[s])
                out += triangle_commands(local_tris)
        out += gfx(0xDF << 24, 0)
        with open(os.path.join(out_dir, dl_file), "wb") as f:
            f.write(otr_header(RESOURCE_DISPLAY_LIST) + bytes(out))
        dl_paths[j] = dl_path

    skeleton_file = f"{name}.skeleton"
    write_skeleton(os.path.join(out_dir, skeleton_file), dl_paths, translations)
    log(f"npc_model: {len(vertices)} vertices, {len(per_joint)} joint meshes, {len(materials)} materials")
    return resource_prefix + skeleton_file


# Animation conversion

def sample_channel(times: List[float], values: List, t: float, interpolation: str, is_quat: bool):
    if t <= times[0]:
        return values[0]
    if t >= times[-1]:
        return values[-1]
    hi = 1
    while times[hi] < t:
        hi += 1
    lo = hi - 1
    if interpolation == "STEP":
        return values[lo]
    f = (t - times[lo]) / (times[hi] - times[lo]) if times[hi] != times[lo] else 0.0
    if is_quat:
        return quat_slerp(values[lo], values[hi], f)
    return [a + (b - a) * f for a, b in zip(values[lo], values[hi])]


def unwrap(degrees: List[float]) -> List[float]:
    out = [degrees[0]]
    for d in degrees[1:]:
        prev = out[-1]
        while d - prev > 180:
            d -= 360
        while d - prev < -180:
            d += 360
        out.append(d)
    return out


def keyframes_for(values: List[int]) -> Optional[List[Tuple[int, int, int]]]:
    """Keys (frame, value, tangent per second) for one channel, or None when it never changes."""
    if max(values) == min(values):
        return None
    keys = []
    n = len(values)
    for i in range(n):
        prev = values[max(0, i - 1)]
        nxt = values[min(n - 1, i + 1)]
        span = (min(n - 1, i + 1) - max(0, i - 1)) or 1
        tangent = clamp((nxt - prev) / span * FRAMES_PER_SECOND, -32768, 32767)
        keys.append((i + 1, values[i], tangent))
    return keys


def convert_animation(gltf_path: str, out_path: str, animation_name: Optional[str] = None, loop: bool = True,
                      log=print) -> None:
    g = Gltf(gltf_path)
    nodes = g.json.get("nodes", [])
    animations = g.json.get("animations", [])
    if not animations:
        raise ValueError("the file has no animations")
    anim = animations[0]
    if animation_name is not None:
        anim = next(a for a in animations if a.get("name") == animation_name)
    channels: Dict[Tuple[int, str], tuple] = {}
    end_time = 0.0
    for ch in anim["channels"]:
        sampler = anim["samplers"][ch["sampler"]]
        times = g.accessor(sampler["input"])
        values = g.accessor(sampler["output"])
        target = ch["target"]
        if "node" not in target:
            continue
        channels[(target["node"], target["path"])] = (times, values, sampler.get("interpolation", "LINEAR"))
        end_time = max(end_time, times[-1])
    frames = max(1, int(round(end_time * FRAMES_PER_SECOND)))
    joint_nodes = {}
    for i, node in enumerate(nodes):
        if node.get("name") in JOINT_NAMES and node["name"] not in joint_nodes:
            joint_nodes[node["name"]] = i

    flags = []
    keyframes: List[Tuple[int, int, int]] = []
    key_counts: List[int] = []
    fixed: List[int] = []

    def add_channel(samples: List[int], bit: int, flag_index: int):
        keys = keyframes_for(samples)
        if keys is None:
            fixed.append(samples[0])
        else:
            flags[flag_index] |= bit
            keyframes.extend(keys)
            key_counts.append(len(keys))

    root = joint_nodes.get("root")
    root_rest = nodes[root].get("translation", [0, 0, 0]) if root is not None else [0, 10, 0]
    trans = []
    for frame in range(frames):
        t = frame / FRAMES_PER_SECOND
        if root is not None and (root, "translation") in channels:
            times, values, interp = channels[(root, "translation")]
            pos = sample_channel(times, values, t, interp, False)
        else:
            pos = root_rest
        trans.append([clamp(c * GAME_UNITS_PER_METRE, -32768, 32767) for c in pos])
    flags.append(0)
    for axis, bit in ((0, 1 << 5), (1, 1 << 4), (2, 1 << 3)):
        add_channel([p[axis] for p in trans], bit, 0)

    for j, jname in enumerate(JOINT_NAMES):
        if j > 0:
            flags.append(0)
        ni = joint_nodes.get(jname)
        angles = [[], [], []]
        for frame in range(frames):
            t = frame / FRAMES_PER_SECOND
            q = [0, 0, 0, 1]
            if ni is not None and (ni, "rotation") in channels:
                times, values, interp = channels[(ni, "rotation")]
                q = sample_channel(times, values, t, interp, True)
            elif ni is not None:
                q = nodes[ni].get("rotation", [0, 0, 0, 1])
            x, y, z = euler_zyx_degrees(quat_to_matrix3(quat_normalize(q)))
            if j == 0:
                z += ROOT_ROTATION_Z
            angles[0].append(x)
            angles[1].append(y)
            angles[2].append(z)
        for axis, bit in ((0, 1 << 2), (1, 1 << 1), (2, 1 << 0)):
            samples = [clamp(d * 10, -32768, 32767) for d in unwrap(angles[axis])]
            add_channel(samples, bit, j)

    out = bytearray(otr_header(RESOURCE_NPC_ANIMATION))
    out += struct.pack("<B", 0)
    out += struct.pack("<I", len(flags)) + bytes(flags)
    out += struct.pack("<I", len(keyframes))
    for frame, value, tangent in keyframes:
        out += struct.pack("<hhh", frame, value, tangent)
    out += struct.pack("<I", len(key_counts)) + struct.pack("<%dh" % len(key_counts), *key_counts)
    out += struct.pack("<I", len(fixed)) + struct.pack("<%dh" % len(fixed), *fixed)
    out += struct.pack("<BBh", 0, 0, frames)
    out += struct.pack("<ffif", 1.0, float(frames), 1 if loop else 0, -5.0)
    out += struct.pack("<I", 0) + struct.pack("<hh", 0, 0)
    out += struct.pack("<I", 0) + struct.pack("<hh", 0, 0)
    out += struct.pack("<hh", 0, -1)
    out += struct.pack("<BB", 0, 0)
    with open(out_path, "wb") as f:
        f.write(out)
    log(f"npc_model: {frames} frames, {len(keyframes)} keys, {len(fixed)} fixed values")


# Reference rig from the game's assets

def read_otr(archive: zipfile.ZipFile, path: str) -> bytes:
    return archive.read(path)[0x40:]


def parse_skeleton(data: bytes):
    p = 4
    limbs = []
    for _ in range(data[3]):
        ln = struct.unpack_from("<I", data, p)[0]
        p += 4
        dl = data[p:p + ln].decode("utf-8")
        p += ln
        child, flags = data[p], data[p + 1]
        tx, ty, tz = struct.unpack_from("<hhh", data, p + 2)
        p += 8
        limbs.append((dl, child, flags, (tx, ty, tz)))
    return limbs


def parse_vertices(data: bytes):
    count = struct.unpack_from("<I", data, 0)[0]
    return [struct.unpack_from("<hhhHhhbbbB", data, 4 + i * 16) for i in range(count)]


def parse_display_list(data: bytes):
    p = 8
    commands = []
    while p + 8 <= len(data):
        w0, w1 = struct.unpack_from("<II", data, p)
        p += 8
        op = w0 >> 24
        ext = None
        if op in EXTENDED_OPCODES and p + 8 <= len(data):
            e0, e1 = struct.unpack_from("<II", data, p)
            p += 8
            ext = (e0 << 32) | e1
        commands.append((op, w0, w1, ext))
        if op == 0xDF:
            break
    return commands


def export_rig(archive_path: str, species: str, out_path: str, log=print) -> None:
    from PIL import Image

    archive = zipfile.ZipFile(archive_path)
    base = f"data/npc/model/mdl/{species}/"
    limbs = parse_skeleton(read_otr(archive, base + f"cKF_bs_r_{species}"))
    if len(limbs) != len(JOINT_NAMES):
        raise ValueError(f"{species} has {len(limbs)} joints, not the villager rig")
    vtx_entries = [n for n in archive.namelist() if n.startswith(base) and n.endswith("_v")]
    vertex_data = {crc64(n): parse_vertices(read_otr(archive, n)) for n in vtx_entries}
    model_joints = [i for i, limb in enumerate(limbs) if limb[0]]

    tex_base = f"data/npc/model/tex/{species}/"
    pal = read_otr(archive, tex_base + f"{species}_pal")[16:48]
    colours = []
    for i in range(16):
        v = (pal[i * 2] << 8) | pal[i * 2 + 1]
        colours.append((((v >> 11) & 31) << 3, ((v >> 6) & 31) << 3, ((v >> 1) & 31) << 3, 255))
    strip = read_otr(archive, tex_base + f"{species}_tmem_txt")[16:]

    def ci4_image(data: bytes, w: int, h: int, palette) -> Image.Image:
        img = Image.new("RGBA", (w, h))
        px = []
        for b in data[:w * h // 2]:
            px += [palette[b >> 4], palette[b & 15]]
        img.putdata(px[:w * h])
        return img

    images = {}
    materials = {}

    def material_for(segment: int, offset: int, w: int, h: int, palette_slot: int) -> int:
        key = (segment, offset, w, h)
        if key in materials:
            return materials[key]
        if segment == SEGMENT_EYE:
            name, data = "eye", read_otr(archive, tex_base + f"{species}_eye1_TA_tex_txt")[16:]
        elif segment == SEGMENT_MOUTH:
            name, data = "mouth", read_otr(archive, tex_base + f"{species}_mouth1_TA_tex_txt")[16:]
        elif segment == 0x0B:
            name, data = f"body_{offset:x}", strip[offset:]
        else:
            name, data = f"shirt", bytes(w * h // 2)
        img = ci4_image(data, w, h, colours if segment != 0x0A else [(200, 200, 200, 255)] * 16)
        images[name] = img
        materials[key] = len(materials)
        return materials[key]

    # one mesh per joint, assembled from the display lists
    joint_tris: Dict[int, List[Tuple[int, int, int]]] = {}
    joint_verts: Dict[int, List[tuple]] = {}
    joint_tri_materials: Dict[int, List[int]] = {}
    joint_lookup: Dict[int, Dict[tuple, int]] = {}
    for ji in model_joints:
        commands = parse_display_list(read_otr(archive, limbs[ji][0]))
        cache = {}
        joint = ji
        segment, offset, tw, th, pslot = 0x0B, 0, 32, 32, 15
        tile_w = tile_h = 32
        size_known = False
        for op, w0, w1, ext in commands:
            if op == 0xDA:
                index = ((w1 & 0xFFFFFE) // 64)
                joint = model_joints[index] if index < len(model_joints) else ji
            elif op == 0xFD:
                segment = (w1 >> 24) & 0xFF
                offset = w1 & 0xFFFFFE
                size_known = False
            elif op == 0xF5 and ((w1 >> 24) & 7) == 0:
                pslot = (w1 >> 20) & 0xF
            elif op == 0xF2 and ((w1 >> 24) & 7) == 0 and not size_known:
                tile_w = ((w1 >> 12) & 0xFFF) // 4 + 1
                tile_h = (w1 & 0xFFF) // 4 + 1
                size_known = True
            elif op == G_VTX_OTR_HASH:
                count = (w0 >> 12) & 0xFF
                start = ((w0 & 0xFF) >> 1) - count
                verts = vertex_data.get(ext)
                if verts is None:
                    continue
                first = w1 // 16
                for k in range(count):
                    cache[start + k] = (joint, verts[first + k])
            elif op in (0x05, 0x06):
                tris = [((w0 >> 17) & 0x7F, (w0 >> 9) & 0x7F, (w0 >> 1) & 0x7F)]
                if op == 0x06:
                    tris.append(((w1 >> 17) & 0x7F, (w1 >> 9) & 0x7F, (w1 >> 1) & 0x7F))
                mat = material_for(segment, offset, tile_w, tile_h, pslot)
                for tri in tris:
                    entries = [cache.get(v) for v in tri]
                    if any(e is None for e in entries):
                        continue
                    j = entries[0][0]
                    verts = joint_verts.setdefault(j, [])
                    lookup = joint_lookup.setdefault(j, {})
                    idx = []
                    for _, v in entries:
                        key = (v, tile_w, tile_h, mat)
                        if key not in lookup:
                            verts.append((v, tile_w, tile_h))
                            lookup[key] = len(verts) - 1
                        idx.append(lookup[key])
                    joint_tris.setdefault(j, []).append(tuple(idx))
                    joint_tri_materials.setdefault(j, []).append(mat)

    # glTF assembly
    rest_global = []
    for j in range(len(JOINT_NAMES)):
        t = limbs[j][3]
        parent = JOINT_PARENTS[j]
        local = [1, 0, 0, t[0] / GAME_UNITS_PER_METRE, 0, 1, 0, t[1] / GAME_UNITS_PER_METRE, 0, 0, 1,
                 t[2] / GAME_UNITS_PER_METRE, 0, 0, 0, 1]
        rest_global.append(local if parent < 0 else mat_mul(rest_global[parent], local))

    bin_data = bytearray()
    buffer_views = []
    accessors = []

    def add_accessor(values, component_type, type_name, target=None, normalized=False):
        fmt = {5126: "f", 5123: "H", 5121: "B"}[component_type]
        comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[type_name]
        while len(bin_data) % 4:
            bin_data.append(0)
        start = len(bin_data)
        for v in values:
            bin_data.extend(struct.pack("<" + fmt * comps, *(v if comps > 1 else [v])))
        view = {"buffer": 0, "byteOffset": start, "byteLength": len(bin_data) - start}
        if target:
            view["target"] = target
        buffer_views.append(view)
        acc = {"bufferView": len(buffer_views) - 1, "componentType": component_type, "count": len(values),
               "type": type_name}
        if normalized:
            acc["normalized"] = True
        if type_name == "VEC3" and component_type == 5126:
            acc["min"] = [min(v[i] for v in values) for i in range(3)]
            acc["max"] = [max(v[i] for v in values) for i in range(3)]
        accessors.append(acc)
        return len(accessors) - 1

    gltf_nodes = []
    for j, jname in enumerate(JOINT_NAMES):
        t = limbs[j][3]
        node = {"name": jname, "translation": [c / GAME_UNITS_PER_METRE for c in t]}
        children = [k for k in range(len(JOINT_NAMES)) if JOINT_PARENTS[k] == j]
        if children:
            node["children"] = children
        gltf_nodes.append(node)
    ibm = []
    for j in range(len(JOINT_NAMES)):
        inv = mat_invert(rest_global[j])
        ibm.append([inv[0], inv[4], inv[8], inv[12], inv[1], inv[5], inv[9], inv[13], inv[2], inv[6], inv[10],
                    inv[14], inv[3], inv[7], inv[11], inv[15]])
    skin = {"name": "villager", "joints": list(range(len(JOINT_NAMES))), "skeleton": 0,
            "inverseBindMatrices": add_accessor(ibm, 5126, "MAT4")}

    gltf_images = []
    gltf_textures = []
    gltf_materials = []
    for key, index in sorted(materials.items(), key=lambda kv: kv[1]):
        seg = key[0]
        name = "eye" if seg == SEGMENT_EYE else "mouth" if seg == SEGMENT_MOUTH else f"body_{key[1]:x}" if seg == 0x0B else "shirt"
        img = images[name]
        from io import BytesIO
        buf = BytesIO()
        img.save(buf, format="PNG")
        while len(bin_data) % 4:
            bin_data.append(0)
        start = len(bin_data)
        bin_data.extend(buf.getvalue())
        buffer_views.append({"buffer": 0, "byteOffset": start, "byteLength": len(buf.getvalue())})
        gltf_images.append({"name": name, "mimeType": "image/png", "bufferView": len(buffer_views) - 1})
        gltf_textures.append({"source": len(gltf_images) - 1, "sampler": 0})
        gltf_materials.append({"name": name, "pbrMetallicRoughness": {
            "baseColorTexture": {"index": len(gltf_textures) - 1}, "metallicFactor": 0, "roughnessFactor": 1}})

    meshes = []
    mesh_nodes = []
    for j in sorted(joint_tris):
        verts = joint_verts[j]
        tris = joint_tris[j]
        tri_mats = joint_tri_materials[j]
        positions, normals, uvs, joints_attr, weights_attr = [], [], [], [], []
        for (v, tw, th) in verts:
            world = mat_apply(rest_global[j], (v[0] / GAME_UNITS_PER_METRE, v[1] / GAME_UNITS_PER_METRE,
                                               v[2] / GAME_UNITS_PER_METRE))
            positions.append(list(world))
            n = [v[6] / 127.0, v[7] / 127.0, v[8] / 127.0]
            ln = math.sqrt(sum(c * c for c in n)) or 1.0
            normals.append([c / ln for c in n])
            uvs.append([v[4] / 32.0 / tw, v[5] / 32.0 / th])
            joints_attr.append([j, 0, 0, 0])
            weights_attr.append([1.0, 0.0, 0.0, 0.0])
        primitives = []
        for mat_index in sorted(set(tri_mats)):
            idx = []
            for tri, m in zip(tris, tri_mats):
                if m == mat_index:
                    idx.extend(tri)
            primitives.append({"attributes": {
                "POSITION": add_accessor(positions, 5126, "VEC3", 34962),
                "NORMAL": add_accessor(normals, 5126, "VEC3", 34962),
                "TEXCOORD_0": add_accessor(uvs, 5126, "VEC2", 34962),
                "JOINTS_0": add_accessor(joints_attr, 5121, "VEC4", 34962),
                "WEIGHTS_0": add_accessor(weights_attr, 5126, "VEC4", 34962)},
                "indices": add_accessor(idx, 5123, "SCALAR", 34963), "material": mat_index})
        meshes.append({"name": JOINT_NAMES[j], "primitives": primitives})
        mesh_nodes.append({"name": JOINT_NAMES[j] + "_mesh", "mesh": len(meshes) - 1, "skin": 0})

    armature = {"name": "Armature", "rotation": [0, 0, math.sin(math.radians(ROOT_ROTATION_Z / 2)),
                                                 math.cos(math.radians(ROOT_ROTATION_Z / 2))],
                "children": [0] + [len(gltf_nodes) + k for k in range(len(mesh_nodes))]}
    all_nodes = gltf_nodes + mesh_nodes + [armature]
    scene = {"nodes": [len(all_nodes) - 1]}
    doc = {"asset": {"version": "2.0", "generator": "Bellwood npc_model"}, "scene": 0, "scenes": [scene],
           "nodes": all_nodes, "meshes": meshes, "skins": [skin], "materials": gltf_materials,
           "textures": gltf_textures, "images": gltf_images,
           "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
           "accessors": accessors, "bufferViews": buffer_views,
           "buffers": [{"byteLength": len(bin_data), "uri": "data:application/octet-stream;base64," +
                        base64.b64encode(bytes(bin_data)).decode("ascii")}]}
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(doc, f)
    log(f"npc_model: wrote {out_path} with {len(meshes)} meshes and {len(materials)} materials")


def main(argv: List[str]) -> int:
    if len(argv) >= 5 and argv[1] == "rig":
        export_rig(argv[2], argv[3], argv[4])
        return 0
    if len(argv) >= 5 and argv[1] == "model":
        print(convert_model(argv[2], argv[3], argv[4], ""))
        return 0
    if len(argv) >= 4 and argv[1] == "anim":
        name = argv[4] if len(argv) > 4 and not argv[4].startswith("--") else None
        convert_animation(argv[2], argv[3], name, "--once" not in argv)
        return 0
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
