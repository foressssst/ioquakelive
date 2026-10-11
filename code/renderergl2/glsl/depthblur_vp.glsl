attribute vec4 attr_Position;
attribute vec4 attr_TexCoord0;

uniform vec4   u_ViewInfo;

varying vec2   var_ScreenTex;

void main()
{
	gl_Position = attr_Position;
	vec2 wh = vec2(1.0) / u_ViewInfo.zw - vec2(1.0);
	var_ScreenTex = (floor(attr_TexCoord0.xy * wh) + vec2(0.5)) * u_ViewInfo.zw;

}
