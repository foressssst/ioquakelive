#version 450

// [QL] eleven texels with Quake Live's weights, read as seven linear samples

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float texoffset_x = 0.0;
layout(constant_id = 1) const float texoffset_y = 0.0;

void main() {
	vec2 texel = vec2(texoffset_x, texoffset_y);
	vec4 result = 0.179335 * texture(texture0, frag_tex_coord);

	result += 0.235377 * texture(texture0, frag_tex_coord - (10.0 / 7.0) * texel);
	result += 0.235377 * texture(texture0, frag_tex_coord + (10.0 / 7.0) * texel);
	result += 0.132400 * texture(texture0, frag_tex_coord - (24.0 / 7.0) * texel);
	result += 0.132400 * texture(texture0, frag_tex_coord + (24.0 / 7.0) * texel);
	result += 0.042557 * texture(texture0, frag_tex_coord - 5.0 * texel);
	result += 0.042557 * texture(texture0, frag_tex_coord + 5.0 * texel);
	result.a = 1.0;
	out_color = result;
}
