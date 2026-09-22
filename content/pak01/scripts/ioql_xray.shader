// [ioquakelive] cg_xray: a player seen through walls, added on top in the colour cgame gives the
// entity. Only demos and spectators ever draw it.
ioqlXray
{
	nopicmip
	{
		map $whiteimage
		blendFunc GL_ONE GL_ONE
		rgbGen entity
	}
}
