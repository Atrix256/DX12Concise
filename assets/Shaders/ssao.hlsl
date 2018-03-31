#include "Shaders.h"

struct PSInput
{
	float4 position : SV_POSITION;
    float3 uvw : TEXCOORD0;
};

PSInput VSMain(in VSInput input)
{
	PSInput result;
    
    float4x4 viewMatrixNoTranslation = float4x4(
        float4(viewMatrix[0].xyz, 0.0f),
        float4(viewMatrix[1].xyz, 0.0f),
        float4(viewMatrix[2].xyz, 0.0f),
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    );

    float4x4 vp = mul(viewMatrixNoTranslation, projectionMatrix);

    result.position = mul(input.position, vp);
    result.uvw = input.position.xyz;
    result.uvw.z *= -1.0f;

	return result;
}

float4 PSMain(in PSInput input) : SV_TARGET
{
    // uvw visualization
    //float3 ret = input.uvw;

    // sample texture
    float3 ret = g_textureSky.Sample(sampleWrap, input.uvw).rgb;

    // sRGB correct
    ret = pow(abs(ret), 1.0f / 2.2f);

    // two different anaglyph styles
    // red / blue 3d handling
    /*
#if STEREO_MODE == STEREO_MODE_RED || STEREO_MODE == STEREO_MODE_BLUE
    float luma = dot(ret, float3(0.3f, 0.59f, 0.11f));
    ret = float3(luma, luma, luma);
#endif
    */

    return float4(ret, 1.0f);
}
