#version 450

layout(set = 0, binding = 0) uniform UBO {
	vec4 eyePos;
	vec4 lightPos;
	vec4 lightColor;
	vec4 lightVector;
	vec4 fogDistanceVector;
	vec4 fogDepthVector;
	vec4 fogEyeT;
	vec4 fogColor;
};

layout(set = 2, binding = 0) uniform sampler2D fog_texture;

layout(location = 4) in vec2 fog_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {

	vec4 fog = texture(fog_texture, fog_tex_coord);
	out_color = fog * fogColor;
}
