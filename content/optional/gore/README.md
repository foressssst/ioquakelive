# Paquete opcional de gore

Los gibs de cuerpo, la sangre y los sonidos de impacto al estilo de Quake 3. No viene incluido. Nada de esta carpeta entra en `pak01.pk3` y el juego no lo carga por defecto.

## Por qué no está en pak01

En Quake Live estos archivos no vienen en `pak00.pk3`, son contenido del Workshop de Steam. El juego usa los gibs de Quake 3 solo si encuentra `dlc_gibs/skull.md3`, y si no lo encuentra usa los electrogibs. Si el paquete fuera dentro de `pak01.pk3`, el gore de Quake 3 quedaría como predeterminado, al revés que en el juego original.

Los archivos del efecto juiced (`juicer.md3`, `juicer_1.md3`, `juicer.png` e `invul_juiced.ogg`) van en esta misma carpeta, porque el juego los carga con la misma condición.

## Cómo instalarlo

Comprime el contenido de esta carpeta en un pk3 y cópialo a `baseq3/`:

```
cd content/optional/gore
zip -r ../../../pak-gore.pk3 dlc_gibs
```

El archivo tiene que conservar la carpeta `dlc_gibs/`. Al cargar el siguiente mapa se usan los gibs de Quake 3. Para volver a los electrogibs, borra el pk3.
