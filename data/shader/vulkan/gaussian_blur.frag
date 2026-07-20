#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D gTextureSampler;
layout(push_constant) uniform SGaussianBlurPushConstants
{
	vec2 gTexelOffset;
	int gRadius;
	int gPadding;
	float gWeights[11];
} gBlur;

layout(location = 0) noperspective in vec2 texCoord;
layout(location = 0) out vec4 FragClr;

const int GAUSSIAN_BLUR_MAX_RADIUS = 10;

void main()
{
	vec4 Result = texture(gTextureSampler, texCoord) * gBlur.gWeights[0];
	for(int Offset = 1; Offset <= GAUSSIAN_BLUR_MAX_RADIUS; ++Offset)
	{
		if(Offset > gBlur.gRadius)
			break;
		vec2 SampleOffset = gBlur.gTexelOffset * float(Offset);
		Result += texture(gTextureSampler, texCoord + SampleOffset) * gBlur.gWeights[Offset];
		Result += texture(gTextureSampler, texCoord - SampleOffset) * gBlur.gWeights[Offset];
	}
	FragClr = Result;
}
