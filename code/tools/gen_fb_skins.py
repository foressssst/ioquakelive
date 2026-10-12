import os
import struct
import sys
import zipfile

DEFAULT_PAK = r"C:\Users\attaraxia\ql\ioquakelive\build\Release\x86\baseq3\pak00.pk3"
DEFAULT_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "content", "pak01")

PARTS = ("head", "upper", "lower")
SHADER = "fullBright"

def md3_names(data):
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
        off += struct.unpack_from("<i", data, off + 0x68)[0]

    return surfaces, tags

def main():
    pak = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PAK
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT

    z = zipfile.ZipFile(pak)
    models = sorted(set(n.split("/")[2] for n in z.namelist()
                        if n.startswith("models/players/") and n.endswith(".skin") and n.count("/") >= 3
                        and n.split("/")[2] == os.path.basename(n.split("/")[2])
                        and n.split("/")[2] not in ("", ".", "..")
                        and "\\" not in n.split("/")[2] and ":" not in n.split("/")[2]))

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
