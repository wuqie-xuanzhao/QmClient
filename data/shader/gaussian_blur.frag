uniform sampler2D gTextureSampler;
uniform vec2 gTexelOffset;
uniform int gRadius;
uniform float gWeights[11];

noperspective in vec2 texCoord;
out vec4 FragClr;

const int GAUSSIAN_BLUR_MAX_RADIUS = 10;

void main()
{
	vec4 Result = texture(gTextureSampler, texCoord) * gWeights[0];
	for(int Offset = 1; Offset <= GAUSSIAN_BLUR_MAX_RADIUS; ++Offset)
	{
		if(Offset > gRadius)
			break;
		vec2 SampleOffset = gTexelOffset * float(Offset);
		Result += texture(gTextureSampler, texCoord + SampleOffset) * gWeights[Offset];
		Result += texture(gTextureSampler, texCoord - SampleOffset) * gWeights[Offset];
	}
	FragClr = Result;
}
