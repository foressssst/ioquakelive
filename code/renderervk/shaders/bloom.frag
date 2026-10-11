#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 0.6;
layout(constant_id = 5) const int extract_mode = 0;
layout(constant_id = 6) const int base_modulate = 0;

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;

	if ( extract_mode == 1 )
	{
		if ( (base.r + base.g + base.b) * 0.33333333 >= threshold)
		{
			if ( base_modulate != 0 )
			{
				if ( base_modulate == 1 )
				{
					base *= base;
				}
				else
				{
					const vec3 luma = { 0.2126, 0.7152, 0.0722 };
					base *= dot( luma, base.rgb );
				}
			}
			out_color = vec4( base, 1.0 );
		}
		else
		{
			out_color = vec4( 0.0 );
		}
	}
	else if ( extract_mode == 2 )
	{
		const vec3 luma = { 0.2126, 0.7152, 0.0722 };
		const float v = dot( luma, base.rgb );
		if ( v >= threshold )
		{
			if ( base_modulate != 0 )
			{
				if ( base_modulate == 1 )
				{
					base *= base;
				}
				else
				{
					base *= v;
				}
			}
			out_color = vec4( base, 1.0 );
		}
		else
		{
			out_color = vec4( 0.0 );
		}
	}
	else
	{
		if ( base.r >= threshold || base.g >= threshold || base.b >= threshold )
		{
			if ( base_modulate != 0 )
			{
				if ( base_modulate == 1 )
				{
					base *= base;
				}
				else
				{
					const vec3 luma = { 0.2126, 0.7152, 0.0722 };
					base *= dot( luma, base.rgb );
				}
			}
			out_color = vec4( base, 1.0 );
		}
		else
		{
			out_color = vec4( 0.0 );
		}
	}
}
