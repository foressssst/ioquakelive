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

// [ioquakelive] cg_drawGun 3, "Transparent": el arma propia se dibuja translucida, para ver donde
// apunta sin tapar lo que hay detras. El alfa entra por alphaGen entity, que lo pone CG_AddPlayerWeapon,
// y la textura sigue siendo la del arma, asi que se reconoce cual es.
// [ioquakelive] cg_drawGun 3, "Transparent": el arma propia se dibuja translucida, para ver donde
// apunta sin tapar lo que hay detras.
//
// Es una silueta y no la textura del arma, por una razon concreta: los shaders de las armas de
// pak00 no llevan alphaGen entity, asi que el alfa de la entidad no los toca y poner customShader
// es la unica forma de que la transparencia llegue. El color viene por rgbGen entity, asi que
// CG_AddPlayerWeapon lo oscurece en vez de dejarlo blanco, y con eso se distingue la forma del arma
// contra el fondo.
ioqlTransparentWeapon
{
	nopicmip
	cull none
	{
		map $whiteimage
		blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen entity
		alphaGen entity
	}
}
