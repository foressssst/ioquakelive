#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 1, binding = 0) uniform sampler2D texture1;
layout(set = 2, binding = 0) uniform sampler2D texture2;
layout(set = 3, binding = 0) uniform sampler2D texture3;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 4) const float factor = 0.5;

void main()
{
	vec3 base = texture(texture0, tex_coord).rgb + texture(texture1, tex_coord).rgb + texture(texture2, tex_coord).rgb + texture(texture3, tex_coord).rgb;

	if ( dot(base,base) == 0.0 )
	{
		discard;
	}

	out_color = vec4( base * factor, 0.0 );
}
