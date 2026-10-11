#version 450

layout(push_constant) uniform Transform {
	mat4 mvp;
};

layout(location = 0) in vec3 in_position;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = mvp * vec4(in_position, 1.0);

}
