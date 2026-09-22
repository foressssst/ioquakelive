// [ioquakelive] cg_xray: a player's outline seen through walls, as Valorant draws it. The mask
// writes the model's depth and no colour, so the inside stays clear; the rim is the model grown a
// little and seen from behind, in the colour cgame gives it. Both go after everything else
ioqlXrayMask
{
	nopicmip
	sort 15
	{
		map $whiteimage
		blendFunc GL_ZERO GL_ONE
		depthWrite
	}
}

ioqlXray
{
	nopicmip
	sort 16
	cull back
	deformVertexes wave 100 sin 1.5 0 0 0
	{
		map $whiteimage
		rgbGen entity
	}
}
