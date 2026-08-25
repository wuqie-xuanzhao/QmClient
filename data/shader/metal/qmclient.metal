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

struct SMetalTileVertex
{
	float2 m_Position [[attribute(0)]];
	uchar4 m_TexCoord [[attribute(1)]];
};

struct SMetalTilePlainVertex
{
	float2 m_Position [[attribute(0)]];
};

struct SMetalQuadVertex
{
	float4 m_PositionCenter [[attribute(0)]];
	uchar4 m_Color [[attribute(1)]];
	float2 m_TexCoord [[attribute(2)]];
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

struct SMetalTileVertexOut
{
	float4 m_Position [[position]];
	float4 m_TexCoord;
};

vertex SMetalTileVertexOut qmclient_tile_vertex(SMetalTileVertex Vertex [[stage_in]], constant SMetalTileUniforms &Uniforms [[buffer(1)]])
{
	SMetalTileVertexOut Out;
	const float2 Position = Vertex.m_Position * Uniforms.m_Transform.zw + Uniforms.m_Transform.xy;
	Out.m_Position = Uniforms.m_MVP * float4(Position, 0.0, 1.0);
	float2 TexScale = Uniforms.m_Transform.zw;
	if(Vertex.m_TexCoord.w > 0)
		TexScale = TexScale.yx;
	Out.m_TexCoord = float4(float2(Vertex.m_TexCoord.xy) * TexScale, float(Vertex.m_TexCoord.z), float(Vertex.m_TexCoord.w));
	return Out;
}

vertex SMetalTileVertexOut qmclient_tile_plain_vertex(SMetalTilePlainVertex Vertex [[stage_in]], constant SMetalTileUniforms &Uniforms [[buffer(1)]])
{
	SMetalTileVertexOut Out;
	const float2 Position = Vertex.m_Position * Uniforms.m_Transform.zw + Uniforms.m_Transform.xy;
	Out.m_Position = Uniforms.m_MVP * float4(Position, 0.0, 1.0);
	Out.m_TexCoord = float4(0.0);
	return Out;
}

fragment float4 qmclient_tile_fragment(SMetalTileVertexOut Input [[stage_in]], constant SMetalTileUniforms &Uniforms [[buffer(1)]])
{
	return Uniforms.m_Color;
}

fragment float4 qmclient_tile_textured_fragment(SMetalTileVertexOut Input [[stage_in]], texture2d<float> Texture [[texture(0)]], sampler Sampler [[sampler(0)]], constant SMetalTileUniforms &Uniforms [[buffer(1)]])
{
	return Texture.sample(Sampler, Input.m_TexCoord.xy) * Uniforms.m_Color;
}

struct SMetalQuadVertexOut
{
	float4 m_Position [[position]];
	float4 m_Color;
	float2 m_TexCoord;
};

SMetalQuadVertexOut QuadVertexImpl(SMetalQuadVertex Vertex, constant SMetalQuadUniforms &Uniforms, uint VertexId, bool Grouped)
{
	SMetalQuadVertexOut Out;
	uint QuadIndex = Grouped ? 0 : (VertexId / 4) - uint(Uniforms.m_QuadOffset);
	float2 Position = Vertex.m_PositionCenter.xy;
	const float Rotation = Uniforms.m_aOffsetsRotations[QuadIndex].z;
	if(Rotation != 0.0)
	{
		const float2 Relative = Position - Vertex.m_PositionCenter.zw;
		const float SinRotation = sin(Rotation);
		const float CosRotation = cos(Rotation);
		Position = float2(Relative.x * CosRotation - Relative.y * SinRotation, Relative.x * SinRotation + Relative.y * CosRotation) + Vertex.m_PositionCenter.zw;
	}
	Position += Uniforms.m_aOffsetsRotations[QuadIndex].xy;
	Out.m_Position = Uniforms.m_MVP * float4(Position, 0.0, 1.0);
	Out.m_Color = float4(Vertex.m_Color) * Uniforms.m_aColors[QuadIndex];
	Out.m_TexCoord = Vertex.m_TexCoord;
	return Out;
}

vertex SMetalQuadVertexOut qmclient_quad_vertex_grouped(SMetalQuadVertex Vertex [[stage_in]], constant SMetalQuadUniforms &Uniforms [[buffer(1)]], uint VertexId [[vertex_id]])
{
	return QuadVertexImpl(Vertex, Uniforms, VertexId, true);
}

vertex SMetalQuadVertexOut qmclient_quad_vertex_ungrouped(SMetalQuadVertex Vertex [[stage_in]], constant SMetalQuadUniforms &Uniforms [[buffer(1)]], uint VertexId [[vertex_id]])
{
	return QuadVertexImpl(Vertex, Uniforms, VertexId, false);
}

fragment float4 qmclient_quad_fragment(SMetalQuadVertexOut Input [[stage_in]])
{
	return Input.m_Color;
}

fragment float4 qmclient_quad_textured_fragment(SMetalQuadVertexOut Input [[stage_in]], texture2d<float> Texture [[texture(0)]], sampler Sampler [[sampler(0)]])
{
	return Texture.sample(Sampler, Input.m_TexCoord) * Input.m_Color;
}
