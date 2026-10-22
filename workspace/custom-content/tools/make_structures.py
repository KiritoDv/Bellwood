"""Builds the structure models of the custom content example: a kiosk (and its winter version), a workshop and the
workshop's room.

    python3 make_structures.py ../src/models

Run with Pillow installed. Structures and rooms are authored in world units (a tile is 40 units). A structure's origin
is the centre of its anchor tile on the ground, x east, y up, z south (towards the camera); a room's origin is its
north-west corner at floor height. Materials with alphaMode BLEND (the glass) go into the translucent display list
that MODEL_XLU and ROOM_MODEL_XLU make, the others into MODEL and ROOM_MODEL.
"""

import base64
import json
import os
import struct
import sys
from io import BytesIO

from PIL import Image, ImageDraw


def png(img: Image.Image) -> bytes:
    buf = BytesIO()
    img.quantize(colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(buf, format="PNG")
    return buf.getvalue()


def planks() -> bytes:
    img = Image.new("RGB", (32, 32), (214, 178, 124))
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 8):
        d.line([(0, y), (31, y)], fill=(150, 108, 66))
        d.line([((y * 3) % 32, y), ((y * 3) % 32, y + 7)], fill=(170, 128, 84))
    return png(img)


def roof_tiles() -> bytes:
    img = Image.new("RGB", (32, 32), (186, 60, 48))
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 8):
        d.line([(0, y + 7), (31, y + 7)], fill=(120, 34, 28))
        for x in range((y // 8 % 2) * 4, 32, 8):
            d.line([(x, y), (x, y + 7)], fill=(150, 46, 38))
    return png(img)


def door() -> bytes:
    img = Image.new("RGB", (16, 32), (118, 76, 44))
    d = ImageDraw.Draw(img)
    d.rectangle([2, 2, 13, 29], outline=(80, 50, 28))
    d.rectangle([10, 15, 11, 17], fill=(236, 200, 80))
    return png(img)


def window() -> bytes:
    img = Image.new("RGB", (16, 16), (150, 206, 236))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 15, 15], outline=(250, 250, 240))
    d.line([(8, 0), (8, 15)], fill=(250, 250, 240))
    d.line([(0, 8), (15, 8)], fill=(250, 250, 240))
    d.line([(3, 3), (6, 3)], fill=(230, 246, 255))
    return png(img)


def snowy_roof() -> bytes:
    img = Image.new("RGB", (32, 32), (238, 242, 250))
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 8):
        d.line([(0, y + 7), (31, y + 7)], fill=(196, 208, 228))
        for x in range((y // 8 % 2) * 4, 32, 16):
            d.point((x, y + 3), fill=(214, 224, 240))
    return png(img)


def glass() -> bytes:
    img = Image.new("RGB", (16, 16), (170, 220, 250))
    d = ImageDraw.Draw(img)
    d.line([(2, 12), (12, 2)], fill=(236, 248, 255))
    d.line([(5, 13), (13, 5)], fill=(214, 240, 255))
    return png(img)


def brick() -> bytes:
    img = Image.new("RGB", (32, 32), (172, 92, 70))
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 8):
        d.line([(0, y), (31, y)], fill=(222, 210, 190))
        for x in range((y // 8 % 2) * 8, 32, 16):
            d.line([(x, y), (x, y + 7)], fill=(222, 210, 190))
    return png(img)


def slate() -> bytes:
    img = Image.new("RGB", (32, 32), (84, 96, 112))
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 4):
        d.line([(0, y), (31, y)], fill=(64, 74, 88))
    return png(img)


def sign() -> bytes:
    img = Image.new("RGB", (32, 16), (60, 62, 72))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 31, 15], outline=(236, 200, 80))
    d.ellipse([11, 3, 20, 12], outline=(236, 200, 80))
    d.ellipse([14, 6, 17, 9], fill=(236, 200, 80))
    for x, y in ((15, 1), (15, 13), (9, 7), (21, 7)):
        d.rectangle([x, y, x + 1, y + 1], fill=(236, 200, 80))
    return png(img)


def floorboards() -> bytes:
    img = Image.new("RGB", (32, 32), (196, 150, 98))
    d = ImageDraw.Draw(img)
    for x in range(0, 32, 8):
        d.line([(x, 0), (x, 31)], fill=(140, 100, 60))
        d.line([(x, (x * 5) % 32), (x + 7, (x * 5) % 32)], fill=(160, 116, 72))
    return png(img)


def wall_paper() -> bytes:
    img = Image.new("RGB", (32, 32), (230, 222, 196))
    d = ImageDraw.Draw(img)
    for x in range(0, 32, 8):
        d.rectangle([x, 0, x + 3, 31], fill=(206, 196, 166))
    d.rectangle([0, 26, 31, 31], fill=(120, 84, 56))
    return png(img)


class Model:
    def __init__(self):
        self.materials = []
        self.prims = {}

    def material(self, name: str, image: bytes, alpha: float = 1.0) -> int:
        self.materials.append((name, image, alpha))
        self.prims[len(self.materials) - 1] = ([], [], [], [])
        return len(self.materials) - 1

    def quad(self, mat: int, corners, uv=((0, 1), (1, 1), (1, 0), (0, 0))):
        positions, normals, uvs, indices = self.prims[mat]
        a, b, c = corners[0], corners[1], corners[2]
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = sum(x * x for x in n) ** 0.5 or 1.0
        n = [x / length for x in n]
        base = len(positions)
        for p, t in zip(corners, uv):
            positions.append(p)
            normals.append(n)
            uvs.append(t)
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    def box(self, mat: int, x0, x1, y0, y1, z0, z1, scale=40.0, top=True, bottom=False):
        def tiled(w, h):
            return ((0, h / scale), (w / scale, h / scale), (w / scale, 0), (0, 0))

        self.quad(mat, [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], tiled(x1 - x0, y1 - y0))
        self.quad(mat, [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], tiled(x1 - x0, y1 - y0))
        self.quad(mat, [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], tiled(z1 - z0, y1 - y0))
        self.quad(mat, [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], tiled(z1 - z0, y1 - y0))
        if top:
            self.quad(mat, [(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], tiled(x1 - x0, z1 - z0))
        if bottom:
            self.quad(mat, [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], tiled(x1 - x0, z1 - z0))

    @staticmethod
    def material_json(index: int, name: str, alpha: float) -> dict:
        pbr = {"baseColorTexture": {"index": index}}
        if alpha >= 1.0:
            return {"name": name, "pbrMetallicRoughness": pbr}
        pbr["baseColorFactor"] = [1.0, 1.0, 1.0, alpha]
        return {"name": name, "alphaMode": "BLEND", "pbrMetallicRoughness": pbr}

    def write(self, path: str, name: str) -> None:
        blob = bytearray()
        views = []
        accessors = []

        def add_view(data: bytes) -> int:
            while len(blob) % 4:
                blob.append(0)
            views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)})
            blob.extend(data)
            return len(views) - 1

        def add_accessor(fmt: str, items, gltf_type: str, component: int, minmax=False) -> int:
            data = b"".join(struct.pack("<" + fmt, *(i if isinstance(i, (list, tuple)) else (i,))) for i in items)
            acc = {"bufferView": add_view(data), "componentType": component, "count": len(items), "type": gltf_type}
            if minmax:
                acc["min"] = [min(p[k] for p in items) for k in range(3)]
                acc["max"] = [max(p[k] for p in items) for k in range(3)]
            accessors.append(acc)
            return len(accessors) - 1

        primitives = []
        images = []
        for mat, (positions, normals, uvs, indices) in self.prims.items():
            if not indices:
                continue
            primitives.append({
                "attributes": {
                    "POSITION": add_accessor("fff", positions, "VEC3", 5126, True),
                    "NORMAL": add_accessor("fff", normals, "VEC3", 5126),
                    "TEXCOORD_0": add_accessor("ff", uvs, "VEC2", 5126),
                },
                "indices": add_accessor("H", indices, "SCALAR", 5123),
                "material": mat,
            })
        for mat_name, image, alpha in self.materials:
            images.append({"bufferView": add_view(image), "mimeType": "image/png"})

        doc = {
            "asset": {"version": "2.0", "generator": "make_structures.py"},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"name": name, "mesh": 0}],
            "meshes": [{"name": name, "primitives": primitives}],
            "materials": [self.material_json(i, n, a) for i, (n, _, a) in enumerate(self.materials)],
            "textures": [{"source": i} for i in range(len(self.materials))],
            "images": images,
            "accessors": accessors,
            "bufferViews": views,
            "buffers": [{"byteLength": len(blob),
                         "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
        }
        with open(path, "w") as f:
            json.dump(doc, f)
        print("wrote", path)


def kiosk(path: str, winter: bool = False) -> None:
    """3 x 2 tiles: x -60..60, z -60..20 around the anchor tile. The windows are glass."""
    m = Model()
    walls = m.material("planks", planks())
    roof = m.material("snow" if winter else "roof", snowy_roof() if winter else roof_tiles())
    door_mat = m.material("door", door())
    glass_mat = m.material("glass", glass(), 0.55)
    m.box(walls, -56, 56, 0, 58, -56, 16, top=False)
    m.quad(roof, [(-66, 58, 26), (66, 58, 26), (66, 92, -20), (-66, 92, -20)], ((0, 1.5), (3, 1.5), (3, 0), (0, 0)))
    m.quad(roof, [(66, 58, -66), (-66, 58, -66), (-66, 92, -20), (66, 92, -20)], ((0, 1.5), (3, 1.5), (3, 0), (0, 0)))
    m.quad(walls, [(-56, 58, -56), (-56, 58, 16), (-56, 92, -20), (-56, 92, -20)], ((0, 1), (1.8, 1), (0.9, 0), (0.9, 0)))
    m.quad(walls, [(56, 58, 16), (56, 58, -56), (56, 92, -20), (56, 92, -20)], ((0, 1), (1.8, 1), (0.9, 0), (0.9, 0)))
    m.quad(door_mat, [(-13, 0, 16.5), (13, 0, 16.5), (13, 42, 16.5), (-13, 42, 16.5)])
    for x in (-44, 28):
        m.quad(glass_mat, [(x, 22, 16.5), (x + 16, 22, 16.5), (x + 16, 40, 16.5), (x, 40, 16.5)])
    m.write(path, "kiosk_winter" if winter else "kiosk")


def workshop(path: str) -> None:
    """3 x 3 tiles: x -60..60, z -100..20 around the anchor tile, with the door in the east column (door_x 1)."""
    m = Model()
    walls = m.material("brick", brick())
    roof = m.material("slate", slate())
    door_mat = m.material("door", door())
    glass = m.material("window", window())
    board = m.material("sign", sign())
    m.box(walls, -56, 56, 0, 70, -96, 16, top=False)
    m.box(roof, -62, 62, 70, 78, -102, 22, bottom=True)
    m.box(walls, 24, 40, 78, 112, -80, -64)
    m.quad(door_mat, [(26, 0, 16.5), (54, 0, 16.5), (54, 44, 16.5), (26, 44, 16.5)])
    m.quad(board, [(-40, 48, 16.5), (16, 48, 16.5), (16, 64, 16.5), (-40, 64, 16.5)])
    for x in (-48, -14):
        m.quad(glass, [(x, 20, 16.5), (x + 18, 20, 16.5), (x + 18, 40, 16.5), (x, 40, 16.5)])
    for z in (-80, -40):
        m.quad(glass, [(56.5, 24, z + 18), (56.5, 24, z), (56.5, 44, z), (56.5, 44, z + 18)])
        m.quad(glass, [(-56.5, 24, z), (-56.5, 24, z + 18), (-56.5, 44, z + 18), (-56.5, 44, z)])
    m.write(path, "workshop")


def workshop_room(path: str) -> None:
    """10 x 9 tiles with walls on the outer tiles; the floor runs from x 40 to 360 and z 40 to 360. A glass window sits
    in the east wall."""
    m = Model()
    floor = m.material("floorboards", floorboards())
    walls = m.material("wallpaper", wall_paper())
    bench = m.material("bench", planks())
    glass_mat = m.material("glass", glass(), 0.5)
    m.quad(floor, [(40, 0, 360), (360, 0, 360), (360, 0, 40), (40, 0, 40)], ((0, 4), (4, 4), (4, 0), (0, 0)))
    m.quad(walls, [(40, 0, 40), (360, 0, 40), (360, 120, 40), (40, 120, 40)], ((0, 1), (4, 1), (4, 0), (0, 0)))
    m.quad(walls, [(40, 0, 360), (40, 0, 40), (40, 120, 40), (40, 120, 360)], ((0, 1), (4, 1), (4, 0), (0, 0)))
    m.quad(walls, [(360, 0, 40), (360, 0, 360), (360, 120, 360), (360, 120, 40)], ((0, 1), (4, 1), (4, 0), (0, 0)))
    m.box(bench, 46, 154, 0, 30, 44, 76)
    m.box(bench, 50, 58, 30, 70, 46, 54)
    m.box(bench, 142, 150, 30, 70, 46, 54)
    m.box(bench, 46, 154, 62, 68, 44, 58)
    m.box(bench, 354, 360, 26, 30, 196, 304)
    m.box(bench, 354, 360, 90, 94, 196, 304)
    m.quad(glass_mat, [(357, 30, 200), (357, 30, 300), (357, 90, 300), (357, 90, 200)], ((0, 1), (2, 1), (2, 0), (0, 0)))
    m.write(path, "workshop_room")


def main(out_dir: str) -> None:
    os.makedirs(out_dir, exist_ok=True)
    kiosk(os.path.join(out_dir, "kiosk.gltf"))
    kiosk(os.path.join(out_dir, "kiosk_winter.gltf"), winter=True)
    workshop(os.path.join(out_dir, "workshop.gltf"))
    workshop_room(os.path.join(out_dir, "workshop_room.gltf"))


if __name__ == "__main__":
    main(sys.argv[1])
