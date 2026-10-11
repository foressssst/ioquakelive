attribute vec3 attr_Position;
attribute vec4 attr_TexCoord0;

uniform mat4   u_ModelViewProjectionMatrix;
uniform vec3   u_ToneMinAvgMaxLinear;

varying vec2   var_TexCoords;
varying float  var_InvWhite;

float FilmicTonemap(float x)
{
	const float SS  = 0.22;
	const float LS  = 0.30;
	const float LA  = 0.10;
	const float TS  = 0.20;
	const float TAN = 0.01;
	const float TAD = 0.30;

	return ((x*(SS*x+LA*LS)+TS*TAN)/(x*(SS*x+LS)+TS*TAD)) - TAN/TAD;
}

void main()
{
	gl_Position = u_ModelViewProjectionMatrix * vec4(attr_Position, 1.0);
	var_TexCoords = attr_TexCoord0.st;
	var_InvWhite = 1.0 / FilmicTonemap(u_ToneMinAvgMaxLinear.z - u_ToneMinAvgMaxLinear.x);
}
