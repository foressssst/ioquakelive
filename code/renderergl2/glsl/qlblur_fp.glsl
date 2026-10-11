uniform sampler2D u_TextureMap;

uniform vec2      u_InvTexRes;

varying vec2      var_TexCoords;

void main()
{
#if defined(BLUR_VERTICAL)
	vec2 texel = vec2(0.0, u_InvTexRes.y);
#else
	vec2 texel = vec2(u_InvTexRes.x, 0.0);
#endif
	vec4 result = 0.179335 * texture2D(u_TextureMap, var_TexCoords);

	result += 0.235377 * texture2D(u_TextureMap, var_TexCoords - (10.0 / 7.0) * texel);
	result += 0.235377 * texture2D(u_TextureMap, var_TexCoords + (10.0 / 7.0) * texel);

	result += 0.132400 * texture2D(u_TextureMap, var_TexCoords - (24.0 / 7.0) * texel);
	result += 0.132400 * texture2D(u_TextureMap, var_TexCoords + (24.0 / 7.0) * texel);

	result += 0.042557 * texture2D(u_TextureMap, var_TexCoords - 5.0 * texel);
	result += 0.042557 * texture2D(u_TextureMap, var_TexCoords + 5.0 * texel);

	result.a = 1.0;
	gl_FragColor = result;
}
