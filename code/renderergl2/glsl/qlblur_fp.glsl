uniform sampler2D u_TextureMap;

// one texel of the source
uniform vec2      u_InvTexRes;

varying vec2      var_TexCoords;

// [QL] the bloom's blur: 11 texels in a row, horizontal or vertical (BLUR_VERTICAL), with Quake
// Live's fixed weights. Each weight is 0.75 of the one nearer the centre, so texels 1 and 2, and 3
// and 4, are read together with one linear sample 3/7 of the way to the outer one, which gives the
// same sum with 7 reads instead of 11.
void main()
{
#if defined(BLUR_VERTICAL)
	vec2 texel = vec2(0.0, u_InvTexRes.y);
#else
	vec2 texel = vec2(u_InvTexRes.x, 0.0);
#endif
	vec4 result = 0.179335 * texture2D(u_TextureMap, var_TexCoords);

	// 0.134501 + 0.100876
	result += 0.235377 * texture2D(u_TextureMap, var_TexCoords - (10.0 / 7.0) * texel);
	result += 0.235377 * texture2D(u_TextureMap, var_TexCoords + (10.0 / 7.0) * texel);

	// 0.075657 + 0.056743
	result += 0.132400 * texture2D(u_TextureMap, var_TexCoords - (24.0 / 7.0) * texel);
	result += 0.132400 * texture2D(u_TextureMap, var_TexCoords + (24.0 / 7.0) * texel);

	result += 0.042557 * texture2D(u_TextureMap, var_TexCoords - 5.0 * texel);
	result += 0.042557 * texture2D(u_TextureMap, var_TexCoords + 5.0 * texel);

	result.a = 1.0;
	gl_FragColor = result;
}
