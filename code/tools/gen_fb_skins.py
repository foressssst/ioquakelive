"""Write the fb skins of pak01: every surface of every player model drawn with the flat fullBright
shader, the way CPMA's and the Workshop's fullbright do it.

A .skin file is a list of "surface,shader" lines plus the model's tags, so these are generated from
the surface and tag names in pak00's .md3 files. Nothing of id's travels here: the names are what
the game already has on disk, and the shader and its texture are ours.

usage: gen_fb_skins.py [<pak00.pk3>] [<content/pak01 directory>]
"""
import os
import struct
import sys
import zipfile

DEFAULT_PAK = r"C:\Users\attaraxia\ql\ioquakelive\build\Release\x86\baseq3\pak00.pk3"
DEFAULT_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "content", "pak01")

PARTS = ("head", "upper", "lower")
SHADER = "fullBright"


def md3_names(data):
    """The surface names and the tag names of an .md3, in file order."""
    ident, version = struct.unpack_from("<4si", data, 0)
    if ident != b"IDP3":
        raise ValueError("not an md3")
    numTags, numSurfaces = struct.unpack_from("<ii", data, 0x50)
    ofsTags, ofsSurfaces = struct.unpack_from("<ii", data, 0x60)

    tags = []
    for i in range(numTags):
        name = struct.unpack_from("<64s", data, ofsTags + i * 112)[0]
        tags.append(name.split(b"\0")[0].decode("latin-1"))

    surfaces = []
    off = ofsSurfaces
    for i in range(numSurfaces):
        name = struct.unpack_from("<64s", data, off + 4)[0]
        surfaces.append(name.split(b"\0")[0].decode("latin-1"))
        off += struct.unpack_from("<i", data, off + 0x68)[0]  # ofsEnd

    return surfaces, tags


def main():
    pak = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PAK
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT

    z = zipfile.ZipFile(pak)
    models = sorted(set(n.split("/")[2] for n in z.namelist()
                        if n.startswith("models/players/") and n.endswith(".skin") and n.count("/") >= 3))

    written = 0
    for model in models:
        for part in PARTS:
            path = "models/players/%s/%s.md3" % (model, part)
            try:
                data = z.read(path)
            except KeyError:
                continue

            surfaces, tags = md3_names(data)
            lines = ["%s,%s" % (s, SHADER) for s in surfaces]
            lines += ["%s," % t for t in tags]

            dest = os.path.join(out, "models", "players", model, "%s_fb.skin" % part)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "w", newline="\n") as f:
                f.write("\n".join(lines) + "\n")
            written += 1

    print("%d skins written for %d models in %s" % (written, len(models), out))


if __name__ == "__main__":
    main()
