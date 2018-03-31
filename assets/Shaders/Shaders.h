// Shader permutation defines.
// Ideally these would be code gen'd but not worth the effort in this "demo" code base
#define MATERIAL_MODE_TEXTURED   0
#define MATERIAL_MODE_UNTEXTURED 1

#define STEREO_MODE_NONE 0
#define STEREO_MODE_RED  1
#define STEREO_MODE_BLUE 2

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 viewMatrix;
    float4x4 viewMatrixIT;
    float4x4 projectionMatrix;
    float4x4 viewProjectionMatrix;
    float4   viewDimensions;
    float4   cameraPosition;  // w is unused
};

cbuffer ModelConstantBuffer : register(b1)
{
    float4x4 modelMatrix;
};

SamplerState sampleWrap : register(s0);

RWTexture2D<float4> g_uav : register(u1);

Texture2D<float4> g_textureSplitSum : register(t0);

Texture2D<float4> g_textureDiffuse : register(t1);

TextureCube<float4> g_textureSky : register(t2);
TextureCube<float4> g_textureSkyDiffuse : register(t3);
TextureCube<float4> g_textureSkySpecular : register(t4);

Texture2D<float4> g_texturePBR_Albedo : register(t5);
Texture2D<float4> g_texturePBR_Metalness : register(t6);
Texture2D<float4> g_texturePBR_Normal : register(t7);
Texture2D<float4> g_texturePBR_Roughness : register(t8);
Texture2D<float4> g_texturePBR_AO : register(t9);
