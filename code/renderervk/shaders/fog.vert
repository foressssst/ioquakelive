#version 450

layout(push_constant) uniform Transform {
	mat4 mvp;
};

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

layout(location = 0) in vec3 in_position;

layout(location = 4) out vec2 fog_tex_coord;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = mvp * vec4(in_position, 1.0);

	float s = dot(in_position, fogDistanceVector.xyz) + fogDistanceVector.w;
	float t = dot(in_position, fogDepthVector.xyz) + fogDepthVector.w;

	if ( fogEyeT.y == 1.0 ) {
		if ( t < 0.0 ) {
			t = 1.0 / 32.0;
		} else {
			t = 31.0 / 32.0;
		}
	} else {
		if ( t < 1.0 ) {
			t = 1.0 / 32.0;
		} else {
			t = 1.0 / 32.0 + (30.0 / 32.0 * t) / ( t - fogEyeT.x );
		}
	}

	fog_tex_coord = vec2(s, t);
}
