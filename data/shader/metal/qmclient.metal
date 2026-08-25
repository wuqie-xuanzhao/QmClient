#include <metal_stdlib>
#include "metal_types.h"

using namespace metal;

struct SMetalVertex
{
	float2 m_Position [[attribute(0)]];
	float2 m_TexCoord [[attribute(1)]];
	uchar4 m_Color [[attribute(2)]];
};

struct SMetalVertexOut
{
	float4 m_Position [[position]];
	float2 m_TexCoord;
	float4 m_Color;
};

vertex SMetalVertexOut qmclient_vertex(SMetalVertex Vertex [[stage_in]], constant SMetalUniforms &Uniforms [[buffer(1)]])
{
	SMetalVertexOut Out;
	Out.m_Position = Uniforms.m_MVP * float4(Vertex.m_Position, 0.0, 1.0);
	Out.m_TexCoord = Vertex.m_TexCoord;
	Out.m_Color = float4(Vertex.m_Color) * Uniforms.m_Color;
	return Out;
}

fragment float4 qmclient_fragment(SMetalVertexOut Input [[stage_in]])
{
	return Input.m_Color;
}

fragment float4 qmclient_textured_fragment(SMetalVertexOut Input [[stage_in]], texture2d<float> Texture [[texture(0)]], sampler Sampler [[sampler(0)]])
{
	return Texture.sample(Sampler, Input.m_TexCoord) * Input.m_Color;
}

fragment float4 qmclient_text_fragment(SMetalVertexOut Input [[stage_in]], texture2d<float> TextTexture [[texture(0)]], texture2d<float> OutlineTexture [[texture(1)]], sampler Sampler [[sampler(0)]], constant SMetalTextUniforms &Uniforms [[buffer(1)]])
{
	const float2 TexCoord = Input.m_TexCoord / Uniforms.m_Params.x;
	const float TextAlpha = TextTexture.sample(Sampler, TexCoord).r * Input.m_Color.a;
	const float OutlineAlpha = OutlineTexture.sample(Sampler, TexCoord).r * Uniforms.m_OutlineColor.a;
	const float OutlineBlend = 1.0 - TextAlpha;
	const float3 OutlinePremultiplied = Uniforms.m_OutlineColor.rgb * OutlineAlpha * OutlineBlend;
	const float3 TextPremultiplied = Input.m_Color.rgb * TextAlpha;
	const float Alpha = OutlineAlpha * OutlineBlend + TextAlpha;
	return Alpha > 0.0 ? float4((OutlinePremultiplied + TextPremultiplied) / Alpha, Alpha) : float4(0.0);
}
