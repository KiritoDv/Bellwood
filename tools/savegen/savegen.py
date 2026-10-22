#!/usr/bin/env python3
"""Generate the field-wise JSON adapters and the GameCube readers for the save structures."""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

ROOTS = [
    ("Save_t", "SAVEJSON_SAVE"),
    ("mCD_keep_mail_c", "SAVEJSON_KEEP_MAIL"),
    ("mCD_keep_original_c", "SAVEJSON_KEEP_ORIGINAL"),
    ("mCD_keep_diary_c", "SAVEJSON_KEEP_DIARY"),
    ("mCD_foreigner_c", "SAVEJSON_FOREIGNER"),
]

HEADERS = ["m_common_data.h", "m_card.h"]

NAME_TYPE = "mActor_name_t"

NAME_RECORDS = ["struct animal_personal_id_s", "struct private_cloth_s", "struct home_wall_floor_s"]

GC_ROOTS = [
    "aAPC_event_save_data_c",
    "aEANG_event_data_c",
    "aEv_hatumode_save_c",
    "aEv_tokyoso_c",
    "aEv_tunahiki_c",
    "aEv_turkey_save_c",
    "aEv_yomise_save_c",
    "aNTT_event_save_c",
    "mEv_dozaemon_c",
    "mEv_gst_c",
    "mEv_place_data_c",
    "mEv_santa_event_c",
]

GC_HEADERS = [
    "ac_aprilfool_control_h.h",
    "ac_ev_angler.h",
    "ac_hatumode_control.h",
    "ac_tokyoso_control.h",
    "ac_tunahiki_control.h",
    "ac_ev_turkey.h",
    "ac_ev_yomise.h",
    "ac_npc_totakeke.h",
]

GC_TAGGED_UNIONS = {"mQst_errand_info_u", "memuni_u", "anmuni_u", "mEv_special_u", "mEv_weekly_u"}

GC_CUSTOM_READERS = {"mNW_original_tex_c"}

GC_TARGET = ["-target", "powerpc-unknown-eabi", "-ffreestanding"]

GC_STUB_HEADERS = {
    "assert.h": "#define static_assert _Static_assert\n",
    "ctype.h": "",
    "math.h": "",
    "memory.h": "",
    "stdio.h": "",
    "stdlib.h": "",
    "string.h": "",
    "wchar.h": "",
}

CFLAGS = [
    "-x", "c", "-std=gnu11", "-funsigned-char", "-fsyntax-only",
    "-DTARGET_PC", "-DVERSION=VER_GAFE01_00", "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DNDEBUG", "-DDEBUG=0",
    "-I" + os.path.join(REPO, "include"),
    "-I" + os.path.join(REPO, "src"),
    "-I" + os.path.join(REPO, "libultraship", "include"),
    "-I" + os.path.join(REPO, "libultraship", "include", "libultraship"),
    "-I" + REPO,
    "-Wno-everything",
]

BUILTINS = {
    "char": (1, False),
    "signed char": (1, True),
    "unsigned char": (1, False),
    "_Bool": (1, False),
    "short": (2, True),
    "unsigned short": (2, False),
    "int": (4, True),
    "unsigned int": (4, False),
    "long": (8, True),
    "unsigned long": (8, False),
    "long long": (8, True),
    "unsigned long long": (8, False),
    "float": (4, True),
    "double": (8, True),
}

LOC_RE = re.compile(r"\((?:unnamed|anonymous)(?: struct| union)? at ([^)]+)\)")
LOC_STRIP_RE = re.compile(r"\((unnamed|anonymous)((?: struct| union)?) at [^)]+\)")


def fail(msg):
    sys.stderr.write("savegen: " + msg + "\n")
    sys.exit(1)


def run_clang(clang, extra, probe_src):
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "savegen_probe.c")
        with open(path, "w") as f:
            f.write(probe_src)
        base = [clang] + CFLAGS + extra + [path]
        ast = subprocess.run(base + ["-Xclang", "-ast-dump=json"], capture_output=True, text=True)
        if ast.returncode != 0:
            fail("clang AST dump failed:\n" + ast.stderr)
        lay = subprocess.run(base + ["-Xclang", "-fdump-record-layouts"], capture_output=True, text=True)
        if lay.returncode != 0:
            fail("clang layout dump failed:\n" + lay.stderr)
        return json.loads(ast.stdout), lay.stdout


def run_gc_layout(clang, extra, probe_src):
    with tempfile.TemporaryDirectory() as tmp:
        stubs = os.path.join(tmp, "stubs")
        os.makedirs(stubs)
        for name, text in GC_STUB_HEADERS.items():
            with open(os.path.join(stubs, name), "w") as f:
                f.write(text)
        path = os.path.join(tmp, "savegen_probe.c")
        with open(path, "w") as f:
            f.write(probe_src)
        cmd = [clang] + CFLAGS + GC_TARGET + ["-isystem", stubs] + extra + [path, "-Xclang", "-fdump-record-layouts"]
        lay = subprocess.run(cmd, capture_output=True, text=True)
        if lay.returncode != 0:
            fail("clang GameCube layout dump failed:\n" + lay.stderr)
        return lay.stdout


def strip_locations(type_str):
    return LOC_STRIP_RE.sub(lambda m: "(%s%s)" % (m.group(1), m.group(2)), type_str)


def split_dims(type_str):
    dims = [int(d) for d in re.findall(r"\[(\d+)\]", type_str)]
    base = type_str.split("[", 1)[0] if "[" in type_str else type_str
    base = re.sub(r"\b(const|volatile)\b", "", base).strip()
    return base, dims


def loc_key(type_str):
    m = LOC_RE.search(type_str)
    return m.group(1) if m else None


class Record:
    def __init__(self, node):
        self.node = node
        self.id = node["id"]
        self.tag = node["tagUsed"]
        self.name = node.get("name", "")
        self.spell = None
        self.fields = None
        self.probe = None
        self.access = None


class Field:
    def __init__(self, name, type_str, bitwidth, record, qual=None):
        self.name = name
        self.type_str = type_str
        self.bitwidth = bitwidth
        self.record = record
        self.qual = qual


class Model:
    def __init__(self, ast):
        self.by_id = {}
        self.by_tag = {}
        self.typedefs = {}
        self.by_loc = {}
        self.loc_of = {}
        self._index(ast)
        self.records = {}

    def _index(self, node):
        kind = node.get("kind")
        if kind == "RecordDecl" and node.get("completeDefinition"):
            self.by_id[node["id"]] = node
            if node.get("name"):
                self.by_tag[node["tagUsed"] + " " + node["name"]] = node
        elif kind == "TypedefDecl":
            self.typedefs[node["name"]] = node
        for child in node.get("inner", []) or []:
            self._index(child)

    def record(self, node):
        rec = self.records.get(node["id"])
        if rec is None:
            rec = Record(node)
            self.records[node["id"]] = rec
        return rec

    def typedef_record(self, name):
        td = self.typedefs.get(name)
        if td is None:
            return None
        for t in td.get("inner", []) or []:
            owned = t.get("ownedTagDecl")
            if owned and owned["id"] in self.by_id:
                return self.by_id[owned["id"]]
            for c in t.get("inner", []) or []:
                decl = c.get("decl")
                if c.get("kind") == "RecordType" and decl:
                    if decl["id"] in self.by_id:
                        return self.by_id[decl["id"]]
                    if decl.get("name"):
                        full = c["type"]["qualType"]
                        if full in self.by_tag:
                            return self.by_tag[full]
        return None

    def resolve(self, type_str, depth=0):
        if depth > 32:
            fail("typedef loop at " + type_str)
        base, dims = split_dims(type_str)
        key = loc_key(base)
        if key is not None:
            node = self.by_loc.get(key)
            if node is None:
                fail("unnamed record not registered: " + type_str)
            return "record", self.record(node), dims, None
        m = re.match(r"^(struct|union) (\w+)$", base)
        if m:
            node = self.by_tag.get(base)
            if node is None:
                fail("incomplete record: " + base)
            rec = self.record(node)
            return "record", rec, dims, base
        if base.startswith("enum "):
            return "builtin", ("int", 4, True), dims, "int"
        if base in BUILTINS:
            size, signed = BUILTINS[base]
            return "builtin", (base, size, signed), dims, base
        if base in self.typedefs:
            node = self.typedef_record(base)
            if node is not None:
                rec = self.record(node)
                spell = (rec.tag + " " + rec.name) if rec.name else base
                return "record", rec, dims, spell
            td = self.typedefs[base]["type"]
            inner = td.get("desugaredQualType", td["qualType"])
            kind, payload, inner_dims, _ = self.resolve(inner, depth + 1)
            return kind, payload, dims + inner_dims, base
        fail("unknown type: " + type_str)

    def fields_of(self, rec):
        if rec.fields is not None:
            return rec.fields
        fields = []
        last_def = None
        for c in rec.node.get("inner", []) or []:
            if c["kind"] == "RecordDecl" and c.get("completeDefinition") and not c.get("name"):
                last_def = c
                continue
            if c["kind"] != "FieldDecl":
                continue
            t = c["type"]
            type_str = t.get("desugaredQualType", t["qualType"])
            for s in (t["qualType"], type_str):
                key = loc_key(s)
                if key is not None:
                    if last_def is None:
                        fail("unnamed field type without definition in " + rec.name)
                    self.by_loc[key] = last_def
                    self.loc_of[last_def["id"]] = key
            bitwidth = None
            if c.get("isBitfield"):
                bitwidth = int(c["inner"][0]["value"])
            name = c.get("name") if not c.get("isImplicit") else None
            if "*" in type_str or "(" in type_str.replace("(unnamed", "").replace("(anonymous", ""):
                fail("pointer or function field %s.%s: %s" % (rec.name, name, type_str))
            fields.append(Field(name, type_str, bitwidth, rec, t["qualType"]))
        rec.fields = fields
        return fields


class Layout:

    LINE_RE = re.compile(r"^\s*(\d+)(?::(\d+)-(\d+))?\s*\|( *)(.*)$")

    def __init__(self, text):
        self.blocks = {}
        self.by_loc = {}
        for raw in text.split("*** Dumping AST Record Layout")[1:]:
            lines = [l for l in raw.split("\n") if "|" in l]
            head = self.LINE_RE.match(lines[0])
            name = head.group(5).strip()
            entries = []
            size = None
            for line in lines[1:]:
                if "[sizeof=" in line:
                    size = int(re.search(r"sizeof=(\d+)", line).group(1))
                    break
                m = self.LINE_RE.match(line)
                if not m:
                    continue
                depth = len(m.group(4)) // 2
                text_ = m.group(5).rstrip()
                if " " in text_ and not text_.endswith(")"):
                    tstr, fname = text_.rsplit(" ", 1)
                else:
                    tstr, fname = text_, None
                bits = None
                if m.group(2) is not None:
                    bits = (int(m.group(2)), int(m.group(3)))
                entries.append({"off": int(m.group(1)), "bits": bits, "depth": depth, "type": tstr, "name": fname})
            self.blocks[name] = {"entries": entries, "size": size}
            key = loc_key(name)
            if key is not None:
                self.by_loc[key] = self.blocks[name]


class Generator:
    def __init__(self, model, layout):
        self.model = model
        self.layout = layout
        self.cover_cache = {}
        self.union_choice = {}
        self.out = []

    def rec_block(self, rec):
        if rec.spell is not None:
            blk = self.layout.blocks.get("struct __savegen_probe_%d" % rec.probe)
            if blk is None:
                fail("missing probe layout for %s" % rec.spell)
            entries = [dict(e, depth=e["depth"] - 1) for e in blk["entries"] if e["depth"] >= 2]
            return entries, self.sizes[rec.probe]
        key = self.model.loc_of.get(rec.id)
        blk = self.layout.by_loc.get(key)
        if blk is None:
            fail("missing layout for unnamed record at %s" % key)
        return blk["entries"], blk["size"]

    def leaf_coverage(self, type_str, base_off, mask):
        kind, payload, dims, _ = self.model.resolve(type_str)
        count = 1
        for d in dims:
            count *= d
        if kind == "builtin":
            size = payload[1]
            for b in range(base_off, base_off + size * count):
                mask[b] = 0xFF
            return
        sub_mask, sub_size = self.coverage(payload)
        for i in range(count):
            off = base_off + i * sub_size
            for b in range(sub_size):
                mask[off + b] |= sub_mask[b]

    def entries_coverage(self, entries, mask):
        for i, e in enumerate(entries):
            nxt = entries[i + 1]["depth"] if i + 1 < len(entries) else -1
            if nxt > e["depth"]:
                continue
            if e["bits"] is not None:
                lo, hi = e["bits"]
                for bit in range(lo, hi + 1):
                    absbit = e["off"] * 8 + bit
                    mask[absbit // 8] |= 1 << (absbit % 8)
                continue
            self.leaf_coverage(e["type"], e["off"], mask)

    def coverage(self, rec):
        if rec.id in self.cover_cache:
            return self.cover_cache[rec.id]
        entries, size = self.rec_block(rec)
        mask = bytearray(size)
        self.entries_coverage(entries, mask)
        self.cover_cache[rec.id] = (bytes(mask), size)
        return self.cover_cache[rec.id]

    def member_subtrees(self, rec):
        entries, size = self.rec_block(rec)
        groups = []
        for e in entries:
            if e["depth"] == 1:
                groups.append([e])
            elif e["depth"] > 1:
                groups[-1].append(e)
        return groups, size

    def choose_union_members(self, rec):
        if rec.id in self.union_choice:
            return self.union_choice[rec.id]
        groups, size = self.member_subtrees(rec)
        fields = self.model.fields_of(rec)
        if len(groups) != len(fields):
            fail("union %s: layout has %d members, AST has %d" % (rec.spell or rec.id, len(groups), len(fields)))
        masks = []
        for group in groups:
            mask = bytearray(size)
            self.entries_coverage(group, mask)
            masks.append(mask)
        total = bytearray(size)
        for mask in masks:
            for i in range(size):
                total[i] |= mask[i]
        chosen = []
        have = bytearray(size)
        while have != total:
            best, gain = None, 0
            for idx, mask in enumerate(masks):
                g = sum(bin(mask[i] & ~have[i] & 0xFF).count("1") for i in range(size))
                if g > gain:
                    best, gain = idx, g
            chosen.append(best)
            for i in range(size):
                have[i] |= masks[best][i]
        chosen.sort()
        keys = []
        for idx in chosen:
            f = fields[idx]
            if f.name is not None:
                keys.append(f.name)
            else:
                kind, payload, dims, spell = self.model.resolve(f.type_str)
                keys.extend(self.flat_keys(payload))
        result = chosen if len(keys) == len(set(keys)) else None
        self.union_choice[rec.id] = result
        return result

    def flat_keys(self, rec):
        keys = []
        for f in self.model.fields_of(rec):
            if f.name is not None:
                keys.append(f.name)
            else:
                kind, payload, dims, spell = self.model.resolve(f.type_str)
                keys.extend(self.flat_keys(payload))
        return keys

    def collect(self):
        order = []
        seen = set()

        def visit(rec):
            if rec.id in seen:
                return
            seen.add(rec.id)
            for f in self.model.fields_of(rec):
                kind, payload, dims, spell = self.model.resolve(f.type_str)
                if kind != "record":
                    continue
                if payload.spell is None and spell is not None:
                    payload.spell = spell
                visit(payload)
            order.append(rec)

        for root, _ in ROOTS:
            kind, rec, dims, spell = self.model.resolve(root)
            if kind != "record" or dims:
                fail("root %s is not a record" % root)
            rec.spell = root
            visit(rec)
        self.json_order = list(order)
        for root in GC_ROOTS:
            kind, rec, dims, spell = self.model.resolve(root)
            if kind != "record" or dims:
                fail("GameCube root %s is not a record" % root)
            if rec.spell is None:
                rec.spell = root
            visit(rec)
        probes = []
        for rec in order:
            if rec.spell is not None:
                rec.probe = len(probes)
                probes.append("struct __savegen_probe_%d { %s x; };" % (rec.probe, rec.spell))
                probes.append("typedef char __savegen_size_%d[sizeof(struct __savegen_probe_%d)];" % (rec.probe, rec.probe))
                probes.append("typedef char __savegen_sizeof_%d[sizeof(%s)];" % (rec.probe, rec.spell))
        self.order = order
        return probes

    def emit(self, line=""):
        self.out.append(line)

    @staticmethod
    def int_type(payload):
        name, size, signed = payload
        if signed:
            return {1: "int8_t", 2: "int16_t", 4: "int32_t", 8: "int64_t"}[size]
        return {1: "uint8_t", 2: "uint16_t", 4: "uint32_t", 8: "uint64_t"}[size]

    def union_bytes_ref(self, rec, expr, anonymous):
        if not anonymous:
            return "&%s" % expr, "sizeof(%s)" % expr
        first = next((f for f in self.model.fields_of(rec) if f.name is not None), None)
        if first is None:
            fail("anonymous union without a named member")
        _, size = self.rec_block(rec)
        return "&%s.%s" % (expr, first.name), str(size)

    def emit_put_fields(self, rec, jv, expr, ind, anonymous=False):
        if rec.tag == "union":
            self.emit_put_union(rec, jv, expr, ind, anonymous)
            return
        for f in self.model.fields_of(rec):
            kind, payload, dims, spell = self.model.resolve(f.type_str)
            if f.name is None:
                if kind != "record":
                    fail("anonymous non-record member in %s" % (rec.spell or rec.id))
                self.emit_put_fields(payload, jv, expr, ind, True)
                continue
            self.emit_put_member(f, kind, payload, dims, jv, expr, ind)

    def emit_put_member(self, f, kind, payload, dims, jv, expr, ind):
        ref = "%s.%s" % (expr, f.name)
        key = "%s[\"%s\"]" % (jv, f.name)
        if f.bitwidth is not None:
            self.emit("%s%s = static_cast<%s>(%s);" % (ind, key, self.int_type(payload), ref))
        elif kind == "builtin" or payload.spell is not None:
            self.emit("%sSaveJsonPut(%s, %s);" % (ind, key, ref))
        else:
            self.emit_put_inline(payload, dims, key, ref, ind, 0)

    def emit_put_inline(self, rec, dims, jexpr, ref, ind, level):
        if level < len(dims):
            var = "i%d" % level
            self.emit("%s{" % ind)
            self.emit("%s    SaveJson& a%d = %s;" % (ind, level, jexpr))
            self.emit("%s    a%d = SaveJson::array();" % (ind, level))
            self.emit("%s    for (size_t %s = 0; %s < %d; %s++) {" % (ind, var, var, dims[level], var))
            self.emit("%s        a%d.push_back(SaveJson());" % (ind, level))
            self.emit_put_inline(rec, dims, "a%d.back()" % level, "%s[%s]" % (ref, var), ind + "        ", level + 1)
            self.emit("%s    }" % ind)
            self.emit("%s}" % ind)
            return
        self.emit("%s{" % ind)
        self.emit("%s    SaveJson& o = %s;" % (ind, jexpr))
        self.emit("%s    o = SaveJson::object();" % ind)
        self.emit_put_fields(rec, "o", ref, ind + "    ")
        self.emit("%s}" % ind)

    def emit_put_union(self, rec, jv, expr, ind, anonymous):
        chosen = self.choose_union_members(rec)
        if chosen is None:
            ptr, size = self.union_bytes_ref(rec, expr, anonymous)
            self.emit("%sSaveJsonPutBytes(%s[\"bytes\"], %s, %s);" % (ind, jv, ptr, size))
            return
        for idx in chosen:
            f = self.model.fields_of(rec)[idx]
            kind, payload, dims, spell = self.model.resolve(f.type_str)
            if f.name is None:
                self.emit_put_fields(payload, jv, expr, ind, True)
            else:
                self.emit_put_member(f, kind, payload, dims, jv, expr, ind)

    def emit_get_fields(self, rec, jv, expr, ind, anonymous=False):
        if rec.tag == "union":
            self.emit_get_union(rec, jv, expr, ind, anonymous)
            return
        for f in self.model.fields_of(rec):
            kind, payload, dims, spell = self.model.resolve(f.type_str)
            if f.name is None:
                self.emit_get_fields(payload, jv, expr, ind, True)
                continue
            self.emit_get_member(f, kind, payload, dims, jv, expr, ind)

    def emit_get_member(self, f, kind, payload, dims, jv, expr, ind):
        ref = "%s.%s" % (expr, f.name)
        if f.bitwidth is not None:
            self.emit("%sif (auto it = %s.find(\"%s\"); it != %s.end()) {" % (ind, jv, f.name, jv))
            self.emit("%s    %s = it->get<%s>();" % (ind, ref, self.int_type(payload)))
            self.emit("%s}" % ind)
        elif kind == "builtin" or payload.spell is not None:
            self.emit("%sSaveJsonGet(%s, \"%s\", %s);" % (ind, jv, f.name, ref))
        else:
            self.emit("%sif (auto it = %s.find(\"%s\"); it != %s.end()) {" % (ind, jv, f.name, jv))
            self.emit_get_inline(payload, dims, "(*it)", ref, ind + "    ", 0)
            self.emit("%s}" % ind)

    def emit_get_inline(self, rec, dims, jexpr, ref, ind, level):
        if level < len(dims):
            var = "i%d" % level
            self.emit("%sfor (size_t %s = 0; %s < %d && %s < %s.size(); %s++) {" %
                      (ind, var, var, dims[level], var, jexpr, var))
            self.emit_get_inline(rec, dims, "%s[%s]" % (jexpr, var), "%s[%s]" % (ref, var), ind + "    ", level + 1)
            self.emit("%s}" % ind)
            return
        self.emit("%s{" % ind)
        self.emit("%s    const SaveJson& o = %s;" % (ind, jexpr))
        self.emit("%s    SaveJsonExpectObject(o);" % ind)
        self.emit_get_fields(rec, "o", ref, ind + "    ")
        self.emit("%s}" % ind)

    def emit_get_union(self, rec, jv, expr, ind, anonymous):
        chosen = self.choose_union_members(rec)
        if chosen is None:
            ptr, size = self.union_bytes_ref(rec, expr, anonymous)
            self.emit("%sSaveJsonGetBytes(%s, \"bytes\", %s, %s);" % (ind, jv, ptr, size))
            return
        for idx in chosen:
            f = self.model.fields_of(rec)[idx]
            kind, payload, dims, spell = self.model.resolve(f.type_str)
            if f.name is None:
                self.emit_get_fields(payload, jv, expr, ind, True)
            else:
                self.emit_get_member(f, kind, payload, dims, jv, expr, ind)

    def layout_asserts(self, rec):
        entries, size = self.rec_block(rec)
        names = {f.name for f in self.model.fields_of(rec) if f.name is not None and f.bitwidth is None}
        lines = ["static_assert(sizeof(%s) == %d, \"%s\");" % (rec.spell, size, rec.spell)]
        if rec.tag == "union":
            return lines
        for e in entries:
            if e["depth"] == 1 and e["bits"] is None and e["name"] in names:
                lines.append("static_assert(offsetof(%s, %s) == %d, \"%s.%s\");" %
                             (rec.spell, e["name"], e["off"], rec.spell, e["name"]))
        return lines

    def padding_table(self, rec):
        mask, size = self.coverage(rec)
        runs = []
        i = 0
        while i < size:
            pad = (~mask[i]) & 0xFF
            if pad == 0:
                i += 1
                continue
            j = i
            while j < size and ((~mask[j]) & 0xFF) == pad:
                j += 1
            runs.append((i, j - i, pad))
            i = j
        return runs

    def layout_hash(self):
        h = hashlib.sha1()
        for rec in self.json_order:
            h.update((rec.spell or "").encode())
            entries, size = self.rec_block(rec)
            h.update(str(size).encode())
            for e in entries:
                h.update(("%d|%s|%s|%s|%s;" % (e["off"], e["bits"], e["depth"], strip_locations(e["type"]),
                                               e["name"])).encode())
        return h.hexdigest()[:16]

    def verify_gc_layout(self, gc_layout):
        for rec in self.order:
            if rec.spell is not None:
                name = "struct __savegen_probe_%d" % rec.probe
                host = self.layout.blocks.get(name)
                gc = gc_layout.blocks.get(name)
            else:
                key = self.model.loc_of.get(rec.id)
                host = self.layout.by_loc.get(key)
                gc = gc_layout.by_loc.get(key)
            what = rec.spell or self.model.loc_of.get(rec.id)
            if host is None or gc is None:
                fail("no GameCube layout for %s" % what)
            if host["size"] != gc["size"]:
                fail("%s is %d bytes on the host and %d on the GameCube" % (what, host["size"], gc["size"]))
            if len(host["entries"]) != len(gc["entries"]):
                fail("%s has a different field list on the GameCube" % what)
            for a, b in zip(host["entries"], gc["entries"]):
                if (a["off"], a["bits"], a["depth"], a["name"]) != (b["off"], b["bits"], b["depth"], b["name"]):
                    fail("%s.%s: host %d %s, GameCube %d %s" % (what, a["name"], a["off"], a["bits"], b["off"], b["bits"]))

    def gc_swap_map(self, entries, size):
        out = [None] * size
        for i, e in enumerate(entries):
            nxt = entries[i + 1]["depth"] if i + 1 < len(entries) else -1
            if nxt > e["depth"]:
                continue
            if e["bits"] is not None:
                lo, hi = e["bits"]
                start = e["off"] * 8 + lo
                for bit in range(start, e["off"] * 8 + hi + 1):
                    cur = out[bit // 8]
                    fields = cur[1] if cur is not None and cur[0] == "bits" else frozenset()
                    out[bit // 8] = ("bits", fields | {(start, hi - lo + 1)})
                continue
            self.gc_leaf_map(e["type"], e["off"], out)
        return out

    def gc_leaf_map(self, type_str, off, out):
        kind, payload, dims, _ = self.model.resolve(type_str)
        count = 1
        for d in dims:
            count *= d
        if kind == "builtin":
            size = payload[1]
            for k in range(count):
                for b in range(size):
                    out[off + k * size + b] = ("scalar", size, b)
            return
        entries, sub_size = self.rec_block(payload)
        sub = self.gc_swap_map(entries, sub_size)
        for k in range(count):
            for b in range(sub_size):
                if sub[b] is not None:
                    out[off + k * sub_size + b] = sub[b]

    def gc_union_members(self, rec):
        groups, size = self.member_subtrees(rec)
        fields = self.model.fields_of(rec)
        maps = [self.gc_swap_map(g, size) for g in groups]

        def conflict(a, b):
            return any(maps[a][k] is not None and maps[b][k] is not None and maps[a][k] != maps[b][k]
                       for k in range(size))

        plain = [self.model.resolve(f.type_str)[0] == "builtin" for f in fields]
        structured = [i for i in range(len(fields)) if not plain[i]]
        for a in structured:
            for b in structured:
                if a < b and conflict(a, b):
                    return None
        chosen = []
        have = [False] * size
        for i in structured + [i for i in range(len(fields)) if plain[i]]:
            if any(conflict(i, c) for c in chosen):
                continue
            if any(maps[i][k] is not None and not have[k] for k in range(size)):
                chosen.append(i)
                for k in range(size):
                    if maps[i][k] is not None:
                        have[k] = True
        if any(any(m[k] is not None for m in maps) and not have[k] for k in range(size)):
            return None
        return sorted(chosen)

    def gc_emit_fields(self, rec, p, v, ind):
        if rec.tag == "union":
            self.gc_emit_union(rec, p, v, ind)
            return
        entries, size = self.rec_block(rec)
        direct = [e for e in entries if e["depth"] == 1]
        fields = self.model.fields_of(rec)
        if len(direct) != len(fields):
            fail("%s: layout has %d fields, AST has %d" % (rec.spell or rec.id, len(direct), len(fields)))
        for f, e in zip(fields, direct):
            self.gc_emit_member(f, e, p, v, ind)

    def gc_emit_member(self, f, e, p, v, ind):
        kind, payload, dims, spell = self.model.resolve(f.type_str)
        if f.bitwidth is not None:
            if f.name is not None:
                start = e["off"] * 8 + e["bits"][0]
                self.emit("%s%s.%s = SaveGcBits<%s>(%s, %d, %d);" %
                          (ind, v, f.name, self.int_type(payload), p, start, f.bitwidth))
            return
        q = p if e["off"] == 0 else "%s + 0x%X" % (p, e["off"])
        if f.name is None:
            if kind != "record":
                fail("anonymous non-record member in %s" % f.record.spell)
            self.gc_emit_fields(payload, q, v, ind)
            return
        ref = "%s.%s" % (v, f.name)
        if kind == "builtin" or payload.spell is not None:
            self.emit("%sSaveGcRead(%s, %s);" % (ind, q, ref))
        else:
            self.gc_emit_inline(payload, dims, q, ref, ind, 0)

    def gc_emit_inline(self, rec, dims, q, ref, ind, level):
        if level < len(dims):
            stride = self.rec_block(rec)[1]
            for d in dims[level + 1:]:
                stride *= d
            var = "i%d" % level
            self.emit("%sfor (size_t %s = 0; %s < %d; %s++) {" % (ind, var, var, dims[level], var))
            self.gc_emit_inline(rec, dims, "%s + %s * 0x%X" % (q, var, stride), "%s[%s]" % (ref, var),
                                ind + "    ", level + 1)
            self.emit("%s}" % ind)
            return
        self.gc_emit_fields(rec, q, ref, ind)

    def gc_ids(self, names):
        return {self.model.resolve(name)[1].id for name in names}

    def gc_emit_union(self, rec, p, v, ind):
        chosen = self.gc_union_members(rec)
        if chosen is None:
            fail("union %s needs a GameCube decode rule (GC_TAGGED_UNIONS)" % (rec.spell or self.model.loc_of.get(rec.id)))
        groups, size = self.member_subtrees(rec)
        fields = self.model.fields_of(rec)
        for idx in chosen:
            self.gc_emit_member(fields[idx], groups[idx][0], p, v, ind)

    def generate_gc(self):
        custom = self.gc_ids(GC_CUSTOM_READERS)
        tagged = self.gc_ids(GC_TAGGED_UNIONS)
        named = [r for r in self.order if r.spell is not None and r.id not in custom]
        header = ["// Generated by tools/savegen/savegen.py. Do not edit.", "#pragma once", "",
                  "#include <cstdint>", "", "#include \"port/save/SaveGame.h\"", "", "#define this this_",
                  "extern \"C\" {"]
        header += ["#include \"%s\"" % h for h in GC_HEADERS]
        header += ["}", "#undef this", ""]
        for rec in named:
            header.append("void SaveGcRead(const uint8_t* p, %s& v);" % rec.spell)
        self.out = []
        self.emit("// Generated by tools/savegen/savegen.py. Do not edit.")
        self.emit("#include \"port/save/SaveGc.h\"")
        self.emit("")
        self.emit("#include <cstddef>")
        self.emit("#include <cstdint>")
        for rec in named:
            self.emit("")
            self.emit("void SaveGcRead(const uint8_t* p, %s& v) {" % rec.spell)
            if rec.id in tagged:
                self.emit("    SaveGcCopy(p, &v, sizeof(v));")
            else:
                self.gc_emit_fields(rec, "p", "v", "    ")
            self.emit("}")
        return "\n".join(header) + "\n", "\n".join(self.out) + "\n"

    def is_name(self, f):
        return f.qual is not None and split_dims(f.qual)[0] == NAME_TYPE

    def has_names(self, rec):
        if rec.id in self.names_cache:
            return self.names_cache[rec.id]
        result = False
        if rec.tag != "union":
            for f in self.model.fields_of(rec):
                if f.bitwidth is not None:
                    continue
                kind, payload, dims, spell = self.model.resolve(f.type_str)
                if kind == "builtin":
                    result = result or self.is_name(f)
                else:
                    result = result or self.visits(payload)
        self.names_cache[rec.id] = result
        return result

    def visits(self, rec):
        return rec.tag != "union" and (rec.spell in NAME_RECORDS or self.has_names(rec))

    def emit_names_loop(self, dims, ref, ind, depth, body):
        if not dims:
            body(ref, ind, depth)
            return
        var = "i%d" % depth
        self.emit("%sfor (size_t %s = 0; %s < %d; %s++) {" % (ind, var, var, dims[0], var))
        self.emit_names_loop(dims[1:], "%s[%s]" % (ref, var), ind + "    ", depth + 1, body)
        self.emit("%s}" % ind)

    def emit_names_fields(self, rec, expr, ind, depth):
        for f in self.model.fields_of(rec):
            if f.bitwidth is not None:
                continue
            kind, payload, dims, spell = self.model.resolve(f.type_str)
            if kind == "builtin":
                if self.is_name(f):
                    self.emit_names_loop(dims, "%s.%s" % (expr, f.name), ind, depth,
                                         lambda ref, i, d: self.emit("%sv(%s);" % (i, ref)))
                continue
            if not self.visits(payload):
                continue
            if f.name is None:
                self.emit_names_fields(payload, expr, ind, depth)
            elif payload.spell is None:
                self.emit_names_loop(dims, "%s.%s" % (expr, f.name), ind, depth,
                                     lambda ref, i, d, payload=payload: self.emit_names_fields(payload, ref, i, d))
            else:
                def body(ref, i, d, payload=payload):
                    if self.has_names(payload):
                        self.emit("%sSaveVisitNames(v, %s);" % (i, ref))
                    if payload.spell in NAME_RECORDS:
                        self.emit("%sv(%s);" % (i, ref))
                self.emit_names_loop(dims, "%s.%s" % (expr, f.name), ind, depth, body)

    def names_reachable(self, rec, seen):
        if rec.id in seen or not self.has_names(rec):
            return
        seen.add(rec.id)
        for f in self.model.fields_of(rec):
            kind, payload, dims, spell = self.model.resolve(f.type_str)
            if kind == "record" and f.bitwidth is None and payload.tag != "union":
                self.names_reachable(payload, seen)

    def generate_names(self):
        self.names_cache = {}
        reachable = set()
        for root, _ in ROOTS:
            self.names_reachable(self.model.resolve(root)[1], reachable)
        named = [r for r in self.json_order if r.spell is not None and r.id in reachable]
        self.out = []
        self.emit("// Generated by tools/savegen/savegen.py. Do not edit.")
        self.emit("#pragma once")
        self.emit("")
        self.emit("#include <cstddef>")
        self.emit("")
        self.emit("#include \"port/save/SaveGame.h\"")
        self.emit("")
        self.emit("// Calls v(field) for each %s field outside of unions, and v(record) for each" % NAME_TYPE)
        self.emit("// %s." % ", ".join(r.split(" ", 1)[1] for r in NAME_RECORDS))
        for rec in named:
            self.emit("template <typename V> void SaveVisitNames(V& v, %s& x);" % rec.spell)
        for rec in named:
            self.emit("")
            self.emit("template <typename V> void SaveVisitNames(V& v, %s& x) {" % rec.spell)
            self.emit_names_fields(rec, "x", "    ", 0)
            self.emit("}")
        return "\n".join(self.out) + "\n"

    def generate(self):
        named = [r for r in self.json_order if r.spell is not None]
        self.emit("// Generated by tools/savegen/savegen.py. Do not edit.")
        self.emit("#include \"port/save/SaveJson.h\"")
        self.emit("")
        self.emit("#include <cstddef>")
        self.emit("#include <cstdint>")
        self.emit("")
        self.emit("const char* const kSaveJsonLayoutHash = \"%s\";" % self.layout_hash())
        self.emit("")
        for rec in named:
            for line in self.layout_asserts(rec):
                self.emit(line)
        self.emit("")
        for rec in named:
            self.emit("void to_json(SaveJson& j, const %s& v);" % rec.spell)
            self.emit("void from_json(const SaveJson& j, %s& v);" % rec.spell)
        self.emit("")
        self.emit("#include \"port/save/SaveJsonHelpers.h\"")
        for rec in named:
            self.emit("")
            self.emit("void to_json(SaveJson& j, const %s& v) {" % rec.spell)
            self.emit("    j = SaveJson::object();")
            self.emit_put_fields(rec, "j", "v", "    ")
            self.emit("}")
            self.emit("")
            self.emit("void from_json(const SaveJson& j, %s& v) {" % rec.spell)
            self.emit("    SaveJsonExpectObject(j);")
            self.emit_get_fields(rec, "j", "v", "    ")
            self.emit("}")
        self.emit("")
        for root, enum in ROOTS:
            kind, rec, dims, spell = self.model.resolve(root)
            runs = self.padding_table(rec)
            self.emit("static const SaveJsonPadding k%sPadding[] = {" % root)
            for off, length, pad in runs:
                self.emit("    { 0x%X, %d, 0x%02X }," % (off, length, pad))
            if not runs:
                self.emit("    { 0, 0, 0 },")
            self.emit("};")
        self.emit("")
        self.emit("const SaveJsonPadding* SaveJson_Padding(SaveJsonRoot root, size_t* count) {")
        self.emit("    switch (root) {")
        for root, enum in ROOTS:
            kind, rec, dims, spell = self.model.resolve(root)
            n = len(self.padding_table(rec))
            self.emit("        case %s:" % enum)
            self.emit("            *count = %d;" % n)
            self.emit("            return k%sPadding;" % root)
        self.emit("        default:")
        self.emit("            *count = 0;")
        self.emit("            return nullptr;")
        self.emit("    }")
        self.emit("}")
        return "\n".join(self.out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--clang", default=os.environ.get("CLANG", "clang"))
    ap.add_argument("--out", default=os.path.join(REPO, "src", "port", "save", "SaveJson.gen.cpp"))
    ap.add_argument("--gc-out", default=os.path.join(REPO, "src", "port", "save", "SaveGc.gen.cpp"))
    ap.add_argument("--names-out", default=os.path.join(REPO, "src", "port", "save", "SaveNames.gen.h"))
    ap.add_argument("--report", action="store_true", help="print the union choices and padding summary")
    ap.add_argument("extra", nargs="*", help="extra clang flags")
    args = ap.parse_args()

    src = "".join('#include "%s"\n' % h for h in HEADERS + GC_HEADERS)
    ast, _ = run_clang(args.clang, args.extra, src)
    model = Model(ast)
    gen = Generator(model, None)
    probes = gen.collect()
    src += "\n".join(p for p in probes) + "\n"
    ast, lay = run_clang(args.clang, args.extra, src)
    gen.layout = Layout(lay)
    gen.sizes = {}
    for name, td in Model(ast).typedefs.items():
        m = re.match(r"^__savegen_sizeof_(\d+)$", name)
        if m:
            gen.sizes[int(m.group(1))] = split_dims(td["type"]["qualType"])[1][0]

    gen.verify_gc_layout(Layout(run_gc_layout(args.clang, args.extra, src)))

    code = gen.generate()
    with open(args.out, "w") as f:
        f.write(code)

    gc_header_path = os.path.splitext(args.gc_out)[0] + ".h"
    gc_header, gc_code = gen.generate_gc()
    with open(gc_header_path, "w") as f:
        f.write(gc_header)
    with open(args.gc_out, "w") as f:
        f.write(gc_code)

    with open(args.names_out, "w") as f:
        f.write(gen.generate_names())

    if args.report:
        for rec in gen.order:
            if rec.tag == "union":
                chosen = gen.choose_union_members(rec)
                fields = model.fields_of(rec)
                what = "bytes" if chosen is None else ", ".join(fields[i].name or "<anonymous>" for i in chosen)
                print("%s -> %s" % (rec.spell or model.loc_of.get(rec.id), what))
        for root, _ in ROOTS:
            kind, rec, dims, spell = model.resolve(root)
            mask, size = gen.coverage(rec)
            pad_bits = sum(bin((~b) & 0xFF).count("1") for b in mask)
            print("%s: %d bytes, %d padding bits" % (root, size, pad_bits))
    print("savegen: wrote %s (%d records)" % (os.path.relpath(args.out, REPO), len([r for r in gen.json_order if r.spell])))
    print("savegen: wrote %s (%d records)" % (os.path.relpath(args.gc_out, REPO), len([r for r in gen.order if r.spell])))


if __name__ == "__main__":
    main()
