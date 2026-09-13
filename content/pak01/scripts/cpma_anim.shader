// [CPMA] cg_animateChatBalloon: glow over the dots of Quake Live's chat balloon.
// "sort additive" draws it after sprites/balloon3, which would cover it otherwise.
sprites/ioql_chatdot
{
	nopicmip
	sort additive
	{
		clampmap gfx/misc/flare.tga
		blendfunc add
		rgbGen vertex
	}
}
