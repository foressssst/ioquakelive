uniform sampler2D u_TextureMap;
uniform sampler2D u_LevelsMap;

uniform vec4      u_Color;

varying vec2      var_TexCoords;

const vec3 GREYSCALE = vec3(0.3, 0.59, 0.11);

void main()
{
#if defined(BLOOM_ONLY)
	vec4 bloom = texture2D(u_TextureMap, var_TexCoords);

	bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, u_Color.x);
	bloom *= u_Color.z;

	gl_FragColor = vec4(bloom.rgb, 0.0);
#else
	vec4 scene = texture2D(u_TextureMap, var_TexCoords);
	vec4 bloom = texture2D(u_LevelsMap, var_TexCoords);

	bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, u_Color.x);
	bloom *= u_Color.z;
	scene *= 1.0 - clamp(bloom, 0.0, 1.0);
	scene = mix(vec4(dot(scene.rgb, GREYSCALE)), scene, u_Color.y);

	gl_FragColor = scene * u_Color.w + bloom;
#endif
}
