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
