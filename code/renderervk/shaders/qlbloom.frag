#version 450

// [QL] the light over the scene: with the scene at its own saturation and intensity the blend
// gives scene * (1 - bloom) + bloom, so the scene itself is never read

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 4) const float intensity = 0.5;
layout(constant_id = 12) const float saturation = 0.8;

const vec3 GREYSCALE = vec3(0.3, 0.59, 0.11);

void main() {
	vec4 bloom = texture(texture0, frag_tex_coord);

	bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, saturation);
	bloom *= intensity;
	out_color = vec4(bloom.rgb, 0.0);
}
