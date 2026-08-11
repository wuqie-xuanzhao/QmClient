#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D gTextureSampler;
layout (std140, set = 1, binding = 1) uniform SMsdfParams {
	vec4 gMsdfParams;
} gMsdf;

layout (location = 0) noperspective in vec2 TexCoord;
layout (location = 1) noperspective in vec4 Tint;
layout (location = 0) out vec4 FragClr;

float Median(vec3 Value)
{
	return max(min(Value.r, Value.g), min(max(Value.r, Value.g), Value.b));
}

void main()
{
	float SignedDistance = Median(texture(gTextureSampler, TexCoord).rgb) - 0.5;
	vec2 UnitRange = vec2(gMsdf.gMsdfParams.x) / gMsdf.gMsdfParams.yz;
	vec2 ScreenTexSize = vec2(1.0) / fwidth(TexCoord);
	float ScreenPxRange = max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);
	float Opacity = clamp(SignedDistance * ScreenPxRange + 0.5, 0.0, 1.0);
	FragClr = vec4(Tint.rgb, Tint.a * Opacity);
}
