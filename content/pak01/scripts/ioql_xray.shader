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
