uniform sampler2D u_TextureMap;

// [QL] the bloom's bright pass: what is over r_bloomBrightThreshold (u_Color.x), stretched back
// to the full range
uniform vec4      u_Color;

varying vec2      var_TexCoords;

void main()
{
	vec3 color = texture2D(u_TextureMap, var_TexCoords).rgb;

	gl_FragColor = vec4(clamp((color - u_Color.x) / (1.0 - u_Color.x), 0.0, 1.0), 1.0);
}
