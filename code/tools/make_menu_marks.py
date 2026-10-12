import os

from PIL import Image

SRC = r"C:\Users\attaraxia\ql\assets\logos_src"
OUT = r"C:\Users\attaraxia\ql\assets\pak01\ui\ioql"
PREVIEW = r"C:\Users\attaraxia\ql\run\scratch\marks\row.png"

NEGRO = 16
BLANCO = 236
ALTO = 192

MARCAS = [
    ("QUAKE_3.jpeg", "mark_quake3", 80),
    ("IOQUAKE_3.jpeg", "mark_ioquake3", 40),
    ("QUAKE_LIVE.jpeg", "mark_quakelive", 38),
    ("QUAKE_CLUB.jpeg", "mark_quakeclub", 75),
]

ZOOM = 2

def key(path):
    gris = Image.open(path).convert("L")

    escala = 255.0 / (BLANCO - NEGRO)
    alfa = gris.point(lambda v: 0 if v <= NEGRO else (255 if v >= BLANCO else
                                                     int((v - NEGRO) * escala)))

    blanco = Image.new("RGBA", gris.size, (255, 255, 255, 0))
    blanco.putalpha(alfa)
    return blanco.crop(alfa.getbbox())

def main():
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    if not os.path.isdir(os.path.dirname(PREVIEW)):
        os.makedirs(os.path.dirname(PREVIEW))

    hechas = []
    for archivo, nombre, alto_ui in MARCAS:
        im = key(os.path.join(SRC, archivo))
        guardado = im.resize((max(1, int(round(im.width * ALTO / im.height))), ALTO), Image.LANCZOS)
        guardado.save(os.path.join(OUT, nombre + ".png"))

        ancho_ui = int(round(guardado.width * alto_ui / guardado.height))
        hechas.append((nombre, guardado, ancho_ui, alto_ui))
        print("%-16s recorte %-11s png %-9s UI %dx%d" %
              (nombre, "%dx%d" % im.size, "%dx%d" % guardado.size, ancho_ui, alto_ui))

    hueco = 46
    total = sum(m[2] for m in hechas) + hueco * (len(hechas) - 1)
    banda = 120
    fila = Image.new("RGBA", ((total + 80) * ZOOM, banda * ZOOM), (10, 10, 10, 255))
    x = 40 * ZOOM
    for _, im, ancho, alto in hechas:
        puesto = im.resize((ancho * ZOOM, alto * ZOOM), Image.LANCZOS)
        fila.alpha_composite(puesto, (x, (banda * ZOOM - alto * ZOOM) // 2))
        x += (ancho + hueco) * ZOOM
    fila.convert("RGB").save(PREVIEW)
    print("franja completa: %d de ancho" % total)

if __name__ == "__main__":
    main()
