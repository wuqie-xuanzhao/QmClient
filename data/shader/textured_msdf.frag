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
	if(gMsdfParams.x < 0.0)
	{
		const float InnerRadius = -gMsdfParams.x;
		const float OuterRadius = gMsdfParams.y;
		float Sweep = max(gMsdfParams.w - gMsdfParams.z, 0.0);
		vec2 Point = TexCoord - vec2(0.5);
		float Radius = length(Point);
		float RadialDistance = max(InnerRadius - Radius, Radius - OuterRadius);
		float RadialFeather = max(fwidth(Radius), 0.0005);
		float RadialCoverage = 1.0 - smoothstep(-RadialFeather * 0.5, RadialFeather * 0.5, RadialDistance);
		float Angle = atan(Point.y, Point.x);
		float RelativeAngle = mod(Angle - gMsdfParams.z, 6.28318530718);
		if(RelativeAngle < 0.0)
			RelativeAngle += 6.28318530718;
		float AngularFeather = max(fwidth(Angle), 0.0015);
		float AngularCoverage = Sweep >= 6.2830 ? 1.0 : smoothstep(0.0, AngularFeather, RelativeAngle) * smoothstep(0.0, AngularFeather, Sweep - RelativeAngle);
		FragClr = vec4(Tint.rgb, Tint.a * RadialCoverage * AngularCoverage);
		return;
	}
	float SignedDistance = Median(texture(gTextureSampler, TexCoord).rgb) - 0.5;
	vec2 UnitRange = vec2(gMsdfParams.x) / gMsdfParams.yz;
	vec2 ScreenTexSize = vec2(1.0) / fwidth(TexCoord);
	float ScreenPxRange = max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);
	float Opacity = clamp(SignedDistance * ScreenPxRange + 0.5, 0.0, 1.0);
	FragClr = vec4(Tint.rgb, Tint.a * Opacity);
}
