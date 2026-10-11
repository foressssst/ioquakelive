uniform sampler2D u_TextureMap;

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
