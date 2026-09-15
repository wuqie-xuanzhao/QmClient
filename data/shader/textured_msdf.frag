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
	const vec4 Sample = texture(gTextureSampler, TexCoord);
	float TrueSignedDistance = Sample.a - 0.5;
	const bool UseTrueSdf = gMsdfParams.w < -0.0005;
	float SignedDistance = UseTrueSdf ? TrueSignedDistance : Median(Sample.rgb) - 0.5;
	vec2 UnitRange = vec2(gMsdfParams.x) / gMsdfParams.yz;
	vec2 ScreenTexSize = vec2(1.0) / fwidth(TexCoord);
	float ScreenPxRange = max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);
	float RequestedOutline = UseTrueSdf ? max(-gMsdfParams.w - 0.001, 0.0) : gMsdfParams.w;
	if(RequestedOutline > 0.0)
	{
		// 距离场在当前 quad 上最多只能表示约 0.5 * ScreenPxRange 的外扩。
		// 超出这个范围会把 atlas 背景也推成不透明矩形；限制到留出一个抗锯齿像素的可表示范围。
		const float MaxRepresentableOutline = min(max(0.0, 0.5 * ScreenPxRange - 0.5), 0.5);
		const float OutlineWidth = min(RequestedOutline, MaxRepresentableOutline);
		const float FillCoverage = clamp(SignedDistance * ScreenPxRange + 0.5, 0.0, 1.0);
		// 描边直接沿同一 signed distance 外扩。不要对 UV 做邻域采样：glyph tile
		// 之间虽有 padding，但外扩采样仍可能串到相邻字形，形成孤立白点或波纹。
		// MTSDF alpha is a true single-channel distance, so the outer edge does
		// not inherit MSDF corner-channel interpolation artifacts.
		if(UseTrueSdf)
		{
			const float OuterCoverage = clamp(TrueSignedDistance * ScreenPxRange + OutlineWidth + 0.5, 0.0, 1.0);
			const float OutlineCoverage = max(OuterCoverage - FillCoverage, 0.0);
			FragClr = vec4(Tint.rgb, Tint.a * OutlineCoverage);
			return;
		}
		const float OuterCoverage = clamp(SignedDistance * ScreenPxRange + OutlineWidth + 0.5, 0.0, 1.0);
		const float OutlineCoverage = max(OuterCoverage - FillCoverage, 0.0);
		FragClr = vec4(Tint.rgb, Tint.a * OutlineCoverage);
		return;
	}
	float Opacity = clamp(SignedDistance * ScreenPxRange + 0.5, 0.0, 1.0);
	FragClr = vec4(Tint.rgb, Tint.a * Opacity);
}
