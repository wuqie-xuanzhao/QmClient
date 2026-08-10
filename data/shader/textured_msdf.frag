uniform sampler2D gTextureSampler;
uniform vec4 gMsdfParams;

noperspective in vec2 TexCoord;
noperspective in vec4 Tint;

out vec4 FragClr;

float Median(vec3 Value)
{
	return max(min(Value.r, Value.g), min(max(Value.r, Value.g), Value.b));
}

void main()
{
	float SignedDistance = Median(texture(gTextureSampler, TexCoord).rgb) - 0.5;
	vec2 UnitRange = vec2(gMsdfParams.x) / gMsdfParams.yz;
	vec2 ScreenTexSize = vec2(1.0) / fwidth(TexCoord);
	float ScreenPxRange = max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);
	float Opacity = clamp(SignedDistance * ScreenPxRange + 0.5, 0.0, 1.0);
	FragClr = vec4(Tint.rgb, Tint.a * Opacity);
}
