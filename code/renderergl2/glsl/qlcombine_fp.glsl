// the scene
uniform sampler2D u_TextureMap;
// the blurred bright pass
uniform sampler2D u_LevelsMap;

// [QL] the bloom's last pass: the bloom and the scene, each with its saturation and intensity
//   u_Color.x  r_bloomSaturation
//   u_Color.y  r_bloomSceneSaturation
//   u_Color.z  r_bloomIntensity
//   u_Color.w  r_bloomSceneIntensity
// With BLOOM_ONLY the shader reads the bloom from u_TextureMap and gives only its part, for a blend
// of one and one minus the source colour over the scene: with the scene's saturation and intensity
// at 1 that is the same sum, without reading the scene or copying the result back.
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
