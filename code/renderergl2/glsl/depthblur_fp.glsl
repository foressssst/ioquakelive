uniform sampler2D u_ScreenImageMap;
uniform sampler2D u_ScreenDepthMap;

uniform vec4   u_ViewInfo;
varying vec2   var_ScreenTex;

#define BLUR_SIZE 4

#if !defined(USE_DEPTH)
#endif

float getLinearDepth(sampler2D depthMap, const vec2 tex, const float zFarDivZNear)
{
	float sampleZDivW = texture2D(depthMap, tex).r;
	return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}

vec4 depthGaussian1D(sampler2D imageMap, sampler2D depthMap, vec2 tex, float zFarDivZNear, float zFar, vec2 scale)
{
	float gauss[4];

	gauss[0] = 0.40;
	gauss[1] = 0.24;
	gauss[2] = 0.054;
	gauss[3] = 0.0044;

#if defined(USE_DEPTH)
	float depthCenter = getLinearDepth(depthMap, tex, zFarDivZNear);
	vec2 slope = vec2(dFdx(depthCenter), dFdy(depthCenter)) / vec2(dFdx(tex.x), dFdy(tex.y));
	scale /= clamp(zFarDivZNear * depthCenter / 32.0, 1.0, 2.0);
#endif

#if defined(USE_HORIZONTAL_BLUR)
	vec2 direction = vec2(scale.x * 2.0, 0.0);
	vec2 nudge = vec2(0.0, scale.y * 0.5);
#else
	vec2 direction = vec2(0.0, scale.y * 2.0);
	vec2 nudge = vec2(-scale.x * 0.5, 0.0);
#endif

#if defined(USE_GAUSS)
	vec4 result = texture2D(imageMap, tex) * gauss[0];
	float total = gauss[0];
#else
	vec4 result = texture2D(imageMap, tex);
	float total = 1.0;
#endif

	float zLimit = 5.0 / zFar;
	for (int i = 0; i < 2; i++)
	{
		for (int j = 1; j < BLUR_SIZE; j++)
		{
			vec2 offset = direction * (float(j) - 0.25) + nudge;
#if defined(USE_DEPTH)
			float depthSample = getLinearDepth(depthMap, tex + offset, zFarDivZNear);
			float depthExpected = depthCenter + dot(slope, offset);
			float useSample = float(abs(depthSample - depthExpected) < zLimit);
#else
			float useSample = 1.0;
#endif
#if defined(USE_GAUSS)
			result += texture2D(imageMap, tex + offset) * (gauss[j] * useSample);
			total += gauss[j] * useSample;
#else
			result += texture2D(imageMap, tex + offset) * useSample;
			total += useSample;
#endif
			nudge = -nudge;
		}

		direction = -direction;
		nudge = -nudge;
	}

	return result / total;
}

void main()
{
	gl_FragColor = depthGaussian1D(u_ScreenImageMap, u_ScreenDepthMap, var_ScreenTex, u_ViewInfo.x, u_ViewInfo.y, u_ViewInfo.zw);
}
