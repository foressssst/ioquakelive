#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 0.25;

void main() {
	vec3 color = texture(texture0, frag_tex_coord).rgb;

	out_color = vec4(clamp((color - threshold) / (1.0 - threshold), 0.0, 1.0), 1.0);
}
