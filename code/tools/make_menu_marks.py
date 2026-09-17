"""Arma los logotipos de la franja del pie del menu principal.

Salen a ql\\assets\\pak01\\ui\\ioql\\, que es donde vive lo que viaja en pak01 pero no
entra a git. Cada uno queda recortado a su tinta, con alfa, para dibujarlo tenido.

  mark_quakeclub  el logo del club, tal cual, recortado
  mark_quake3     "QUAKE III" sobre "ARENA", compuesto en Handel Gothic Ex
  mark_ioquake3   "ioquake3" compuesto en Barlow SemiBold

Quake Live no esta aca: su logo sale de pak00, del archivo del jugador.

Al final deja una vista previa de la franja en run\\scratch\\marks\\row.png con las
medidas que usa el UI, para poder mirarla sin entrar al juego.
"""

import io
import os
import zipfile

from PIL import Image, ImageDraw, ImageFont

OUT = r"C:\Users\attaraxia\ql\assets\pak01\ui\ioql"
PREVIEW = r"C:\Users\attaraxia\ql\run\scratch\marks\row.png"
QC = r"C:\Users\attaraxia\ql\run\scratch\marks\qc-logo.png"
PAK00 = r"C:\Users\attaraxia\ql\ioquakelive\build\Release\x86\baseq3\pak00.pk3"
HANDEL = r"C:\Users\attaraxia\ql\assets\pak01\fonts\handelgothicex.ttf"
BARLOW = r"C:\Users\attaraxia\ql\ioquakelive\content\pak01\fonts\Barlow-SemiBold.ttf"

# lo que el UI dibuja: alto de cada marca en pixeles de pagina, elegido a ojo para que
# las cuatro pesen lo mismo aunque una sea apilada y otra una palabra sola
SIZES = {"mark_quakeclub": 44, "mark_quake3": 34, "mark_ioquake3": 20}
QL_HEIGHT = 40  # el logo de pak00 se dibuja entero, con el aire que trae


def render(lines, font_path, px, spacing, gap):
    """Compone las lineas en blanco sobre transparente, centradas, y recorta a la tinta."""
    font = ImageFont.truetype(font_path, px)
    pad = px
    canvas = Image.new("RGBA", (px * max(len(s) for s in lines) * 2 + pad * 2,
                                (px * 2 + gap) * len(lines) + pad * 2), (255, 255, 255, 0))
    draw = ImageDraw.Draw(canvas)

    widths = []
    for text in lines:
        widths.append(sum(draw.textlength(c, font=font) + spacing for c in text) - spacing)

    y = float(pad)
    for text, width in zip(lines, widths):
        x = (canvas.width - width) * 0.5
        for ch in text:
            draw.text((x, y), ch, font=font, fill=(255, 255, 255, 255))
            x += draw.textlength(ch, font=font) + spacing
        y += px * 1.15 + gap

    return canvas.crop(canvas.split()[3].getbbox())


def main():
    if not os.path.isdir(OUT):
        os.makedirs(OUT)

    qc = Image.open(QC).convert("RGBA")
    qc = qc.crop(qc.split()[3].getbbox())
    h = 256  # el filtro bueno al achicar, para que no se ensucie el orbe
    qc = qc.resize((int(round(qc.width * h / qc.height)), h), Image.LANCZOS)
    qc.save(os.path.join(OUT, "mark_quakeclub.png"))

    q3 = render(["QUAKE III", "ARENA"], HANDEL, 96, 6, 14)
    q3.save(os.path.join(OUT, "mark_quake3.png"))

    io3 = render(["ioquake3"], BARLOW, 128, 0, 0)
    io3.save(os.path.join(OUT, "mark_ioquake3.png"))

    # la vista previa, con el logo de Quake Live que sale de pak00
    z = zipfile.ZipFile(PAK00)
    ql = Image.open(io.BytesIO(z.read("ui/assets/main_menu/ql_logo.png"))).convert("RGBA")

    marks = [("mark_quakeclub", qc), ("mark_quake3", q3), ("ql_logo", ql), ("mark_ioquake3", io3)]
    gap = 46
    scaled = []
    for name, im in marks:
        height = QL_HEIGHT if name == "ql_logo" else SIZES[name]
        width = int(round(im.width * height / im.height))
        scaled.append((name, im.resize((width, height), Image.LANCZOS), width, height))
        print("%-16s origen %-11s dibujado %dx%d" % (name, "%dx%d" % im.size, width, height))

    total = sum(s[2] for s in scaled) + gap * (len(scaled) - 1)
    row = Image.new("RGBA", (total + 80, 120), (10, 10, 10, 255))
    x = 40
    for _, im, width, height in scaled:
        row.alpha_composite(im, (x, (120 - height) // 2))
        x += width + gap
    row.convert("RGB").save(PREVIEW)
    print("franja completa: %d de ancho" % total)


if __name__ == "__main__":
    main()
