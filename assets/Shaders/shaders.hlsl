#include "Shaders.h"
#include "PBR.h"

struct PSInput
{
	float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
};

PSInput VSMain(in VSInput input)
{
    // make model, view, projection matrix
    float4x4 mvp = mul(modelMatrix, viewProjectionMatrix);
    
    // if we are doing right eye, move the object a bit
    #if STEREO_MODE == STEREO_MODE_BLUE
        mvp[3].x -= 0.125f;
    #elif STEREO_MODE == STEREO_MODE_RED
        mvp[3].x += 0.125f;
    #endif

    PSInput result;
    result.position = mul(input.position, mvp);
    result.normal = input.normal;
    result.tangent = input.tangent;
    result.uv = input.uv;
    result.worldPosition = mul(input.position, modelMatrix).xyz;
	return result;
}

float4 PSMain(in PSInput input) : SV_TARGET
{
    float3 ret = float3(0.0f, 0.0f, 0.0f);

    // calculate normal, tangent, bitangent
    float3 normal = normalize(input.normal);
    float3 tangent = normalize(input.tangent);
    float3 bitangent = normalize(cross(normal, tangent));

    // get PBR lighting parameters
    float3 albedo = g_textureDiffuse.Sample(sampleWrap, input.uv).rgb * g_texturePBR_Albedo.Sample(sampleWrap, input.uv).rgb;
    float metalness = g_texturePBR_Metalness.Sample(sampleWrap, input.uv).r;
    float roughness = g_texturePBR_Roughness.Sample(sampleWrap, input.uv).r;
    float AO = g_texturePBR_AO.Sample(sampleWrap, input.uv).r;
    float3 textureNormal = g_texturePBR_Normal.Sample(sampleWrap, input.uv).rgb  * 2.0 - 1.0;
    float3x3 tbn = float3x3(
        tangent,
        bitangent,
        normal
    );
    normal = normalize(mul(textureNormal, tbn));

#if MATERIAL_MODE == MATERIAL_MODE_UNTEXTURED
    normal = normalize(input.normal);
    albedo = g_textureDiffuse.Sample(sampleWrap, input.uv).rgb;
    metalness = 0.0f;
    roughness = 1.0f;
    AO = 1.0f;
#endif

    // calculate reflection vector
    float3 viewDirection = normalize(cameraPosition.xyz - input.worldPosition);
    float3 reflectDirection = normalize(reflect(-viewDirection, normal));

    // Image based lighting (IBL) from cube map
    ret += ImageBasedLighting(normal, viewDirection, reflectDirection, albedo, metalness, roughness, c_F0, g_textureSkyDiffuse, g_textureSkySpecular, g_textureSplitSum) * AO;

    // add some positional lights
    ret += PositionalLight(input.worldPosition, normal, viewDirection, float3(3.0f,  5.0f, -4.0f), float3(50.0f, 1.0f, 1.0f), albedo, metalness, roughness, c_F0);
    ret += PositionalLight(input.worldPosition, normal, viewDirection, float3(3.0f, -5.0f,  4.0f), float3(1.0f, 50.0f, 1.0f), albedo, metalness, roughness, c_F0);

    // add a directional light too
    ret += DirectionalLight(normal, viewDirection, normalize(float3(1.0f, 3.0f, 2.0f)), float3(5.0f, 1.0f, 10.0f), albedo, metalness, roughness, c_F0);

    // uv visualization
    //ret = float3(input.uv, 1.0f);

    // world space normal visualization
    //ret = normal.rgb * 0.5 + 0.5;

    // world space view direction visualization
    //ret = viewDirection.rgb * 0.5 + 0.5;

    // world space reflect direction visualization
    //ret = reflectDirection.rgb * 0.5 + 0.5;
    //ret.b = 0.0f;

    //float v = max(dot(normal, viewDirection), 0.0);
    //ret = float3(v, v, v);

    // write something to the uav
    float4 old = g_uav[uint2(input.position.xy)];
    g_uav[uint2(input.position.xy)] = float4(input.position.x * viewDimensions.z, input.position.y * viewDimensions.w, old.z, 1.0f);

    // two different anaglyph styles

    /*
    // red / blue 3d handling
#if STEREO_MODE == STEREO_MODE_RED
    float luma = dot(ret, float3(0.3f, 0.59f, 0.11f));
    ret = float3(luma, 0.0f, 0.0f);
#elif STEREO_MODE == STEREO_MODE_BLUE
    float luma = dot(ret, float3(0.3f, 0.59f, 0.11f));
    ret = float3(0.0f, luma, luma);
#endif
    */

#if STEREO_MODE == STEREO_MODE_RED
    ret = float3(ret.r, 0.0f, 0.0f);
#elif STEREO_MODE == STEREO_MODE_BLUE
    ret = float3(0.0f, ret.g, ret.b);
#endif


    return float4(ret, 1.0f);
}
