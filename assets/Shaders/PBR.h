static const float c_roughnessEpsilon = 0.01f;
static const float c_F0 = 0.04f;

static const float c_pi = 3.14159265359f;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = c_pi * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 fresnelSchlickRoughness (float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 fresnelSchlick (float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float2 SampleSplitSum(float NdotV, float roughness, in Texture2D<float4> texIBLSplitSum)
{
    float2 uv = float2(NdotV, roughness);

    uint w, h;
    texIBLSplitSum.GetDimensions(w, h);
    float halfPixelW = 0.5f / w;
    float halfPixelH = 0.5f / h;

    uv.x = clamp(uv.x, halfPixelW, 1.0f - halfPixelW);
    uv.y = clamp(uv.y, halfPixelH, 1.0f - halfPixelH);

    return texIBLSplitSum.Sample(sampleWrap, uv).rg;
}

float3 ImageBasedLighting (in float3 N, in float3 V, in float3 R, in float3 albedo, in float metallic, in float roughness, in float scalarF0, in TextureCube<float4> texIBLDiffuse, in TextureCube<float4> texIBLSpecular, in Texture2D<float4> texIBLSplitSum)
{
    // avoid roughness asymptotes. Totally a legit, industry standard thing to do!
    if (roughness < c_roughnessEpsilon)
        roughness = c_roughnessEpsilon;
    else if (roughness > 1.0f - c_roughnessEpsilon)
        roughness = 1.0f - c_roughnessEpsilon;

    float3 F0 = float3(scalarF0, scalarF0, scalarF0);
    F0 = lerp(F0, albedo, metallic);
    float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    // diffuse IBL
    float3 irradiance = texIBLDiffuse.Sample(sampleWrap, N * float3(1.0f, 1.0f, -1.0f)).rgb;
    float3 diffuse = irradiance * albedo;

    // specular IBL
    const float MAX_REFLECTION_LOD = 4.0;

    float3 prefilteredColor = texIBLSpecular.SampleLevel(sampleWrap, R * float3(1.0f, 1.0f, -1.0f), roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = SampleSplitSum(max(dot(N, V), 0.0), roughness, texIBLSplitSum);
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    float3 ambient = (kD * diffuse + specular);
    
    return ambient;
}

float3 PositionalLight(in float3 worldPos, in float3 N, in float3 V, in float3 lightPos, in float3 lightColor, in float3 albedo, in float metallic, in float roughness, in float scalarF0)
{
    // avoid roughness asymptotes. Totally a legit, industry standard thing to do!
    if (roughness < c_roughnessEpsilon)
        roughness = c_roughnessEpsilon;
    else if (roughness > 1.0f - c_roughnessEpsilon)
        roughness = 1.0f - c_roughnessEpsilon;

    float distance = length(lightPos - worldPos);
    float attenuation = 1.0 / (distance * distance);
    float3 L = normalize(lightPos - worldPos);

    float3 H = normalize(V + L);

    float3 radiance = lightColor * attenuation;

    float3 F0 = float3(scalarF0, scalarF0, scalarF0);
    F0 = lerp(F0, albedo, metallic);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    float3 nominator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3 specular = nominator / max(denominator, 0.001);

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / c_pi + specular) * radiance * NdotL;
}

float3 DirectionalLight(in float3 N, in float3 V, in float3 lightDir, in float3 lightColor, in float3 albedo, in float metallic, in float roughness, in float scalarF0)
{
    // avoid roughness asymptotes. Totally a legit, industry standard thing to do!
    if (roughness < c_roughnessEpsilon)
        roughness = c_roughnessEpsilon;
    else if (roughness > 1.0f - c_roughnessEpsilon)
        roughness = 1.0f - c_roughnessEpsilon;

    float3 L = lightDir;
    float3 H = normalize(V + L);

    float3 radiance = lightColor;

    float3 F0 = float3(scalarF0, scalarF0, scalarF0);
    F0 = lerp(F0, albedo, metallic);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    float3 nominator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3 specular = nominator / max(denominator, 0.001);

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / c_pi + specular) * radiance * NdotL;
}