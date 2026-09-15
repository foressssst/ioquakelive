uniform sampler2D u_TextureMap;

// [QL] Quake Live's colour correction, which replaces the hardware gamma ramp: gamma, then the
// overbright factor, then the contrast around mid grey.
//   u_Color.x  1 / r_gamma
//   u_Color.y  the overbright factor, 2 per r_overBrightBits (1 without any)
//   u_Color.z  r_contrast: 0 is flat grey, 1 unchanged, more is stronger
// USE_GAMMA and USE_CONTRAST are left out while r_gamma and r_contrast are 1, where they change
// nothing.
uniform vec4      u_Color;

varying vec2      var_TexCoords;

void main()
{
	vec3 color = texture2D(u_TextureMap, var_TexCoords).rgb;

#if defined(USE_GAMMA)
	color = pow(color, vec3(u_Color.x));
#endif
	color *= u_Color.y;
#if defined(USE_CONTRAST)
	color = mix(vec3(0.5), color, u_Color.z);
#endif

	gl_FragColor = vec4(color, 1.0);
}
