#version 450

layout(set = 0, binding = 0) uniform sampler2D scene_texture;
layout(set = 1, binding = 0) uniform sampler2D bloom_texture;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 4) const float intensity = 0.5;
layout(constant_id = 12) const float saturation = 0.8;
layout(constant_id = 13) const float scene_saturation = 1.0;
layout(constant_id = 14) const float scene_intensity = 1.0;

const vec3 GREYSCALE = vec3(0.3, 0.59, 0.11);

void main() {
	vec4 scene = texture(scene_texture, frag_tex_coord);
	vec4 bloom = texture(bloom_texture, frag_tex_coord);

	bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, saturation);
	bloom *= intensity;
	scene *= 1.0 - clamp(bloom, 0.0, 1.0);
	scene = mix(vec4(dot(scene.rgb, GREYSCALE)), scene, scene_saturation);
	out_color = scene * scene_intensity + bloom;
}
