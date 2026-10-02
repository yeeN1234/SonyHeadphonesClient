"""Convert a COLLADA (.dae) model into the client's compact mesh format (.mesh).

The client shows a model in 3D when client/Models/ holds a .mesh whose file name appears in the
product's name (Models/WH-1000XM5.mesh for "WH-1000XM5"); otherwise it draws a generated one.
Only geometry is kept: positions, normals and, per group of triangles, a role that tells the
client how to shade it in the product's colour. Textures are ignored.

    python tooling/ConvertModel.py model.dae client/Models/WH-1000XM5.mesh --yaw 90 \
        --roles trim,body,cushion,band,trim,trim,accent,trim

Run without --roles first: it lists the triangle groups (one per material slot in the file) with
their material, colour, size and extent, to decide the roles. The roles are
    body     the shell, in the product colour
    cushion  ear pads: soft, darker, matte
    band     headband padding: slightly matte
    trim     small parts (buttons, grilles): darker
    accent   keeps the colour it has in the file
--yaw turns the model about the vertical axis (degrees) so that it faces the viewer with the ear
cups left and right. Requires numpy.

File layout (little-endian):
    char[4]  "SHM1"
    uint32   vertex count, triangle count, group count, index size (2 or 4)
    float32  extent: positions are int16 / 32767 * extent, centred on the model
    groups   uint32 triangle count, uint8 role, uint8 r, g, b  (triangles are stored group by group)
    vertices int16 px, py, pz, nx, ny, nz  (normals are int16 / 32767)
    indices  three per triangle, counter-clockwise seen from the front
"""
import argparse
import math
import struct
import sys
import xml.etree.ElementTree as ET

import numpy as np

ROLES = {"body": 0, "cushion": 1, "band": 2, "trim": 3, "accent": 4}
NS = "{http://www.collada.org/2005/11/COLLADASchema}"


def floats(text):
    return np.array(text.split(), dtype=np.float64)


def load_collada(path):
    root = ET.parse(path).getroot()
    effect_colour = {}
    for effect in root.iter(NS + "effect"):
        colour = effect.find(".//" + NS + "diffuse/" + NS + "color")
        effect_colour[effect.get("id")] = floats(colour.text)[:3] if colour is not None else np.array([0.5, 0.5, 0.5])
    material = {}
    for m in root.iter(NS + "material"):
        effect = m.find(NS + "instance_effect").get("url")[1:]
        material[m.get("id")] = (m.get("name") or m.get("id"), effect_colour.get(effect, np.array([0.5, 0.5, 0.5])))
    geometries = {g.get("id"): g for g in root.iter(NS + "geometry")}

    up = root.find(NS + "asset/" + NS + "up_axis")
    up = up.text.strip() if up is not None else "Y_UP"

    groups = []  # (name, colour, positions (n,3,3), normals (n,3,3))

    def visit(node, parent):
        matrix = parent
        for child in node:
            if child.tag == NS + "matrix":
                matrix = matrix @ floats(child.text).reshape(4, 4)
            elif child.tag == NS + "translate":
                t = np.eye(4); t[:3, 3] = floats(child.text); matrix = matrix @ t
            elif child.tag == NS + "scale":
                s = np.eye(4); s[:3, :3] = np.diag(floats(child.text)); matrix = matrix @ s
            elif child.tag == NS + "rotate":
                x, y, z, angle = floats(child.text)
                axis = np.array([x, y, z]) / np.linalg.norm([x, y, z])
                c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
                k = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
                r = np.eye(4); r[:3, :3] = np.eye(3) * c + s * k + (1 - c) * np.outer(axis, axis); matrix = matrix @ r
        for child in node:
            if child.tag == NS + "instance_geometry":
                bindings = {b.get("symbol"): b.get("target")[1:] for b in child.iter(NS + "instance_material")}
                add_geometry(geometries[child.get("url")[1:]], matrix, bindings)
            elif child.tag == NS + "node":
                visit(child, matrix)

    def add_geometry(geometry, matrix, bindings):
        mesh = geometry.find(NS + "mesh")
        sources = {}
        for source in mesh.findall(NS + "source"):
            stride = int(source.find(".//" + NS + "accessor").get("stride", "1"))
            sources[source.get("id")] = floats(source.find(NS + "float_array").text).reshape(-1, stride)
        vertices = mesh.find(NS + "vertices")
        vertex_sources = {i.get("semantic"): i.get("source")[1:] for i in vertices.findall(NS + "input")}
        positions = sources[vertex_sources["POSITION"]][:, :3]
        positions = (np.c_[positions, np.ones(len(positions))] @ matrix.T)[:, :3]
        normal_matrix = np.linalg.inv(matrix[:3, :3]).T
        for element in list(mesh.findall(NS + "triangles")) + list(mesh.findall(NS + "polylist")):
            inputs = element.findall(NS + "input")
            stride = max(int(i.get("offset")) for i in inputs) + 1
            p = np.array(element.find(NS + "p").text.split(), dtype=np.int64).reshape(-1, stride)
            offsets = {i.get("semantic"): int(i.get("offset")) for i in inputs}
            normal_input = [i for i in inputs if i.get("semantic") == "NORMAL"]
            if element.tag == NS + "polylist":  # Fan-triangulate
                counts = np.array(element.find(NS + "vcount").text.split(), dtype=np.int64)
                corners, start = [], 0
                for n in counts:
                    for k in range(1, n - 1):
                        corners += [start, start + k, start + k + 1]
                    start += n
                p = p[np.array(corners)]
            tri_positions = positions[p[:, offsets["VERTEX"]]].reshape(-1, 3, 3)
            if normal_input:
                normals = sources[normal_input[0].get("source")[1:]][:, :3] @ normal_matrix.T
                tri_normals = normals[p[:, offsets["NORMAL"]]].reshape(-1, 3, 3)
            else:  # Flat normals
                face = np.cross(tri_positions[:, 1] - tri_positions[:, 0], tri_positions[:, 2] - tri_positions[:, 0])
                tri_normals = np.repeat(face[:, None, :], 3, axis=1)
            tri_normals /= np.linalg.norm(tri_normals, axis=2, keepdims=True) + 1e-12
            name, colour = material.get(bindings.get(element.get("material"), ""), (element.get("material") or "?", np.array([0.5, 0.5, 0.5])))
            groups.append((name, colour, tri_positions, tri_normals))

    for scene in root.iter(NS + "visual_scene"):
        for node in scene.findall(NS + "node"):
            visit(node, np.eye(4))
    return up, groups


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("source")
    parser.add_argument("output", nargs="?")
    parser.add_argument("--yaw", type=float, default=0.0, help="turn about the vertical axis, degrees")
    parser.add_argument("--roles", help="comma-separated role per triangle group, in file order")
    args = parser.parse_args()

    up, groups = load_collada(args.source)
    if not groups:
        sys.exit("No triangles found.")

    # Y up, then the requested turn; applied to positions and normals alike.
    def reorient(v):
        if up == "Z_UP":
            v = np.stack([v[..., 0], v[..., 2], -v[..., 1]], axis=-1)
        elif up == "X_UP":
            v = np.stack([-v[..., 1], v[..., 0], v[..., 2]], axis=-1)
        a = math.radians(args.yaw)
        c, s = math.cos(a), math.sin(a)
        return np.stack([v[..., 0] * c - v[..., 2] * s, v[..., 1], v[..., 0] * s + v[..., 2] * c], axis=-1)

    groups = [(n, c, reorient(p), reorient(nr)) for n, c, p, nr in groups]
    everything = np.concatenate([g[2].reshape(-1, 3) for g in groups])
    lo, hi = everything.min(0), everything.max(0)
    centre = (lo + hi) / 2
    print(f"{len(groups)} triangle groups, {sum(len(g[2]) for g in groups)} triangles, size {np.round(hi - lo, 4)}")
    for i, (name, colour, p, _) in enumerate(groups):
        glo, ghi = p.reshape(-1, 3).min(0) - centre, p.reshape(-1, 3).max(0) - centre
        print(f"  [{i}] {name:18s} {len(p):7d} triangles  colour {np.round(colour, 3)}  extent {np.round(glo, 3)} .. {np.round(ghi, 3)}")
    if not args.output:
        return
    if args.roles:
        roles = [ROLES[r.strip()] for r in args.roles.split(",")]
        if len(roles) != len(groups):
            sys.exit(f"--roles lists {len(roles)} roles for {len(groups)} groups.")
    else:  # Dark materials take the product colour, coloured ones keep theirs
        roles = [ROLES["body"] if c.max() < 0.25 else ROLES["accent"] for _, c, _, _ in groups]

    extent = float(np.abs(everything - centre).max())
    vertex_index, vertices, triangles, flipped = {}, [], [], 0
    group_records = []
    for g, (name, colour, positions, normals) in enumerate(groups):
        positions = positions - centre
        # Counter-clockwise from the side the normals face.
        face = np.cross(positions[:, 1] - positions[:, 0], positions[:, 2] - positions[:, 0])
        wrong = np.einsum("ij,ij->i", face, normals.sum(axis=1)) < 0
        flipped += int(wrong.sum())
        qp = np.round(positions / extent * 32767).astype(np.int64)
        qn = np.round(normals * 32767).astype(np.int64)
        count = 0
        for t in range(len(positions)):
            corners = [0, 2, 1] if wrong[t] else [0, 1, 2]
            ids = []
            for k in corners:
                key = (g, *qp[t, k], *qn[t, k])
                if key not in vertex_index:
                    vertex_index[key] = len(vertices)
                    vertices.append(key[1:])
                ids.append(vertex_index[key])
            if ids[0] != ids[1] and ids[1] != ids[2] and ids[0] != ids[2]:
                triangles.append(ids)
                count += 1
        rgb = [int(round(min(max(v, 0.0), 1.0) * 255)) for v in colour[:3]]
        group_records.append((count, roles[g], rgb))

    index_size = 2 if len(vertices) <= 65536 else 4
    with open(args.output, "wb") as out:
        out.write(b"SHM1")
        out.write(struct.pack("<4I", len(vertices), len(triangles), len(group_records), index_size))
        out.write(struct.pack("<f", extent))
        for count, role, rgb in group_records:
            out.write(struct.pack("<I4B", count, role, *rgb))
        out.write(np.array(vertices, dtype="<i2").tobytes())
        out.write(np.array(triangles, dtype="<u2" if index_size == 2 else "<u4").tobytes())
    print(f"Wrote {args.output}: {len(vertices)} vertices, {len(triangles)} triangles "
          f"({flipped} turned to face their normals), roles {[r for _, r, _ in group_records]}")


if __name__ == "__main__":
    main()
