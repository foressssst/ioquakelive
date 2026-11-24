#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 1, binding = 0) uniform sampler2D bloom_texture;

layout(push_constant) uniform Bloom {
	uint enabled;
} bloom_pc;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float obScale = 2.0;
layout(constant_id = 2) const float greyscale = 0.0;
layout(constant_id = 4) const float intensity = 0.5;
layout(constant_id = 7) const int ditherMode = 0;
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
layout(constant_id = 11) const float contrast = 1.0;
layout(constant_id = 12) const float saturation = 0.8;
layout(constant_id = 15) const float roundBias = 0.5;

const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };
const vec3 GREYSCALE = vec3(0.3, 0.59, 0.11);

const int bayerSize = 8;
const float bayerMatrix[bayerSize * bayerSize] = {
	0,  32, 8,  40, 2,  34, 10, 42,
	48, 16, 56, 24, 50, 18, 58, 26,
	12, 44, 4,  36, 14, 46, 6,  38,
	60, 28, 52, 20, 62, 30, 54, 22,
	3,  35, 11, 43, 1,  33, 9,  41,
	51, 19, 59, 27, 49, 17, 57, 25,
	15, 47, 7,  39, 13, 45, 5,  37,
	63, 31, 55, 23, 61, 29, 53, 21
};

float threshold() {
	ivec2 coordDenormalized = ivec2(gl_FragCoord.xy);
	ivec2 bayerCoord = coordDenormalized % bayerSize;
	float bayerSample = bayerMatrix[bayerCoord.x + bayerCoord.y * bayerSize];
	float threshold = (bayerSample + 0.5) / float(bayerSize * bayerSize);
	return threshold;
}

vec3 dither(vec3 color) {
	ivec3 depth = ivec3(depth_r, depth_g, depth_b);
	vec3 cDenormalized = color * depth;
	vec3 cLow = floor(cDenormalized);
	vec3 cFractional = cDenormalized - cLow;
	vec3 cDithered = cLow + step(threshold(), cFractional);
	return cDithered / depth;
}

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;
	if ( bloom_pc.enabled != 0u )
	{
		vec4 bloom = textureLod(bloom_texture, frag_tex_coord, 0.0);
		bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, saturation);
		bloom *= intensity;
		vec3 src = floor(clamp(bloom.rgb, 0.0, 1.0) * 255.0 + roundBias) / 255.0;
		base = floor(clamp(src + base * (1.0 - src), 0.0, 1.0) * 255.0 + roundBias) / 255.0;
	}
	if ( greyscale == 1 )
	{
		base = vec3(dot(base, sRGB));
	}
	else if ( greyscale != 0 )
	{
		vec3 luma = vec3(dot(base, sRGB));
		base = mix(base, luma, greyscale);
	}
	if ( gamma != 1.0 )
	{
		out_color = vec4(pow(base, vec3(gamma)) * obScale, 1);
	}
	else
	{
		out_color = vec4(base * obScale, 1);
	}
	if ( contrast != 1.0 )
	{
		out_color.rgb = mix( vec3( 0.5 ), out_color.rgb, contrast );
	}
	if ( ditherMode == 1 ) {
		out_color.rgb = dither(out_color.rgb);
	}
}
