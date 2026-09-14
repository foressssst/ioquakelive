# Optional gore pack

The Quake 3 style body-part gibs, blood sprays and flesh-impact sounds. **Not
shipped.** Nothing here goes into `pak01.pk3`, and a default build never loads
it.

## Why it is not in pak01

Quake Live's `pak00.pk3` contains none of these files. In retail they are Steam
Workshop content, and `cgamex86.dll` decides which gib set to draw purely by
whether they resolved:

```
0x10014c94  cmp dword ptr [0x10a5f430], 0     ; the dlc_gibs/skull.md3 handle
0x10014ca7  je  0x10014d99                    ; -> electrogibs
```

That one test is the whole enable/disable. There is no cvar behind it - `cg_blood`
exists in retail's cvar table but nothing in the module ever reads it back, and
`cg_gibs` does not appear in the binary at all.

Shipping the models inside `pak01.pk3` made that handle resolve on every install,
so the Quake 3 gore became the default and the electrogibs never ran - the
opposite of retail. They live here instead so that the default matches retail and
installing the pack is a real choice.

The juiced effect files - `juicer.md3`, `juicer_1.md3`, `juicer.png`,
`invul_juiced.ogg` - belong to the pack too, although they once stayed in pak01.
Retail treats the whole `dlc_gibs/` folder as one switch: `CG_RegisterSounds`
(0x10020e70) loads every `dlc_gibs` sound, `invul_juiced.ogg` included, only when
`trap_FS_GetFileList("dlc_gibs", "")` finds something, and `EV_JUICED`
(0x1001ab39) plays the juiced effect only when the skull handle resolved. With
the four files in pak01 that listing was never empty, so a default build went
looking for gore sounds that are not there.

## Installing it

Zip the contents of this directory into a pk3 and drop it in `baseq3/`:

```
cd content/optional/gore
zip -r ../../../pak-gore.pk3 dlc_gibs
```

The archive must keep the `dlc_gibs/` prefix, because that is the path the client
registers. Load order does not matter; any pk3 in `baseq3/` is searched.

Nothing else is needed. On the next map load `dlc_gibs/skull.md3` resolves,
`CG_GibPlayer` takes the body-part branch, and the flesh-impact sounds come with
it. Delete the pk3 and the electrogibs come back.
