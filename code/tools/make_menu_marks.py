"""Arma los logotipos de la franja del pie del menu principal.

Las fuentes son los logos que dibujo el usuario: tinta blanca sobre fondo negro, en JPEG,
en ql\\assets\\logos_src. De ahi sale un PNG por logo, blanco con alfa, en
ql\\assets\\pak01\\ui\\ioql (que es donde vive lo que viaja en pak01 pero no entra a git).

Como quitar el fondo: en blanco sobre negro la luminancia YA ES el alfa. Un pixel del
fondo es negro, o sea alfa 0; uno de la tinta es blanco, alfa 255; y un borde suavizado
al 40% de gris sale al 40% de alfa, que es exactamente lo que hay que dibujar. Por eso no
se recorta por umbral -- eso dejaria los bordes dentados -- sino que se pasa la
luminancia a alfa y se pinta todo de blanco.

Lo unico que hay que corregir es el JPEG: el negro no queda en 0 sino en 3 o 6, con
anillos alrededor de la tinta, y el blanco no llega a 255. Se estira el rango con NEGRO y
BLANCO, asi el fondo cae a cero de verdad y la tinta llega a opaca.

Se dibujan tenidos (SM_PicColor), asi que el color del PNG no importa: importa el alfa.

La vista previa queda en run\\scratch\\marks\\row.png, con las medidas que usa el UI.
"""

import os

from PIL import Image

SRC = r"C:\Users\attaraxia\ql\assets\logos_src"
OUT = r"C:\Users\attaraxia\ql\assets\pak01\ui\ioql"
PREVIEW = r"C:\Users\attaraxia\ql\run\scratch\marks\row.png"

NEGRO = 16   # hasta aqui es fondo del JPEG, no tinta
BLANCO = 236  # de aqui para arriba es tinta opaca
ALTO = 192   # el alto al que se guarda cada PNG; el UI lo achica desde ahi

# El orden de la franja, y el alto al que el UI dibuja cada uno. No son proporcionales a
# la imagen: cada logo pesa distinto -- uno es un simbolo apilado sobre su nombre, otro es
# una palabra sola -- asi que el alto se elige para que los cuatro pesen parecido.
MARCAS = [
    ("QUAKE_3.jpeg", "mark_quake3", 80),
    ("IOQUAKE_3.jpeg", "mark_ioquake3", 40),
    ("QUAKE_LIVE.jpeg", "mark_quakelive", 38),
    ("QUAKE_CLUB.jpeg", "mark_quakeclub", 75),
]

ZOOM = 2  # la vista previa se dibuja al doble, que a tamaño real no se juzga nada


def key(path):
    """La luminancia del JPEG pasa a alfa, y lo que queda es tinta blanca recortada."""
    gris = Image.open(path).convert("L")

    # el rango util del JPEG estirado a 0..255: el fondo a cero, la tinta a opaca
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

    # la vista previa, con las medidas que dibuja el UI
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
