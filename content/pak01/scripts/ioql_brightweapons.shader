// [CPMA] cg_drawBrightWeapons: a weapon drawn as a flat silhouette of one colour per weapon, with no
// texture and no light from the map, as CPMA 1.53 does with its fullBright shader. The colour comes in
// through rgbGen entity, set by CG_BrightWeapon.
ioqlBrightWeapon
{
	nopicmip
	{
		map $whiteimage
		rgbGen entity
	}
}

// El arma transparente de cg_drawGun 3 no esta aca: es ghostWeaponShader, que pak00 ya trae en
// scripts/gfx.shader, y es el que usa retail (cgamex86.dll 0x100523a8).
