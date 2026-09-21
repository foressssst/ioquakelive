#version 450

// [QL] the finished frame back over the scene, which in retail is a frame buffer copy

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
	out_color = texture(texture0, frag_tex_coord);
}
