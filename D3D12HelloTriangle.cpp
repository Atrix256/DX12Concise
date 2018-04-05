//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloTriangle.h"

#include "Model.h"
#include "Math.h"
#include <vector>
#include <chrono>
#include "pix3.h"

// Root table parameter enum
enum RootTableParameter
{
    SceneConstantBuffer = 0,
    ModelConstantBuffer,
    TextureSampler,
    UAV,
    SplitsumTexture,
    DiffuseTexture,
    SkyboxTextureSet,
    MaterialTextureSet,

    Count
};


struct SModelLoadParams
{
    const char* fileName;
    const char* baseDir;
    bool flipV;
    float scale;
    XMFLOAT3 offset;
    EMaterial modelMaterial;
};

static const SModelLoadParams s_modelsToLoad[(size_t)EModel::Count] =
{
    {nullptr, nullptr, false, 1.0f, {3.0, 0.5f, 0.0f}, EMaterial::Count},
    { "assets/Models/cube/cube.obj", "assets/Models/cube/", false, 1.0f, {-3.0f, 0.5f, 0.0f}, EMaterial::Count },
};

static const char* s_skyboxBaseFileName[(size_t)ESkyBox::Count] = 
{
    "assets/Skyboxes/ashcanyon/ashcanyon%s.png",
    "assets/Skyboxes/mnight/mnight%s.png",
    "assets/Skyboxes/Vasa/Vasa%s.png",
};

static const char* s_skyboxBaseFileNameDiffuse [(size_t)ESkyBox::Count] = 
{
    "assets/Skyboxes/ashcanyon/ashcanyonDiffuse%s.png",
    "assets/Skyboxes/mnight/mnightDiffuse%s.png",
    "assets/Skyboxes/Vasa/VasaDiffuse%s.png",
};

static const char* s_skyboxBaseFileNameSpecular [(size_t)ESkyBox::Count] = 
{
    "assets/Skyboxes/ashcanyon/ashcanyon%iSpecular%s.png",
    "assets/Skyboxes/mnight/mnight%iSpecular%s.png",
    "assets/Skyboxes/Vasa/Vasa%iSpecular%s.png",
};

// in "assets/PBRMaterialTextures/"
static const char* s_materialFileNames[] =
{
    // albedo, metalness, normal, roughness, ao
    "DriedMud/Albedo.jpg", "../black.png", "DriedMud/Normal.jpg", "DriedMud/Roughness.jpg", "../white.png",
    "greasy-pan-2-albedo.png", "greasy-pan-2-metal.png", "greasy-pan-2-normal.png", "greasy-pan-2-roughness.png", nullptr,
    "Iron-Scuffed_basecolor.png", "Iron-Scuffed_metallic.png", "Iron-Scuffed_normal.png", "Iron-Scuffed_roughness.png", nullptr,
    "roughrockface2_Base_Color.png", "roughrockface2_Metallic.png", "roughrockface2_Normal.png", "roughrockface2_Roughness.png", "roughrockface2_Ambient_Occlusion.png",
    "rustediron2_basecolor.png", "rustediron2_metallic.png", "rustediron2_normal.png", "rustediron2_roughness.png", nullptr,
    "sculptedfloorboards2b_basecolor.png", "sculptedfloorboards2b_metalness.png", "sculptedfloorboards2b_normal.png", "sculptedfloorboards2b_roughness.png", "sculptedfloorboards2b_AO.png",
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "../white.png", "../white.png", "../flatnormal.png","../black.png","../white.png",
    "../black.png", "../black.png", "../flatnormal.png","../black.png","../white.png",
    "../white.png", "../black.png", "../flatnormal.png","../black.png","../white.png",
    "../black.png", "../white.png", "../flatnormal.png","../black.png","../white.png",
};
static_assert(sizeof(s_materialFileNames)/sizeof(s_materialFileNames[0]) == (size_t)EMaterial::Count*(size_t)EMaterialTexture::Count, "s_materialFileNames has the wrong number of entries");

static const bool s_materialTextureLinear[] = 
{
    // albedo, metalness, normal, roughness, ao
       false,  true,      true,   true,      true
};
static_assert(sizeof(s_materialTextureLinear)/sizeof(s_materialTextureLinear[0]) == (size_t)EMaterialTexture::Count, "s_materialTextureLinear has the wrong number of entries");

void D3D12HelloTriangle::MakePSOs()
{
    // create the model shaders
    for (size_t i = 0; i < SShaderPermutations::Count; ++i)
    {
        SShaderPermutations::EMaterialMode materialMode;
        SShaderPermutations::EStereoMode stereoMode;
        SShaderPermutations::GetSettings(i, materialMode, stereoMode);

        char materialModeString[2] = { 0, 0 };
        materialModeString[0] = '0' + (char)materialMode;

        char stereoModeString[2] = { 0, 0 };
        stereoModeString[0] = '0' + (char)stereoMode;

        ID3DBlob* vertexShader;
        ID3DBlob* pixelShader;
        m_graphicsAPI.CompileVSPS(L"./assets/Shaders/shaders.hlsl", vertexShader, pixelShader, m_shaderDebug,
            {
                { "MATERIAL_MODE", materialModeString },
                { "STEREO_MODE", stereoModeString },
                { nullptr, nullptr }
            }
        );

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature;
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 2;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        switch (stereoMode)
        {
            case SShaderPermutations::EStereoMode::none:
            {
                psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                break;
            }
            case SShaderPermutations::EStereoMode::red:
            {
                psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
                break;
            }
            case SShaderPermutations::EStereoMode::blue:
            {
                psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
                break;
            }
        }

        ThrowIfFailed(m_graphicsAPI.m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateModels[i])));

        vertexShader->Release();
        pixelShader->Release();
    }

	// create the skybox shader
    for (size_t i = 0; i < SShaderPermutations::Count; ++i)
    {
        SShaderPermutations::EMaterialMode materialMode;
        SShaderPermutations::EStereoMode stereoMode;
        SShaderPermutations::GetSettings(i, materialMode, stereoMode);

        char materialModeString[2] = { 0, 0 };
        materialModeString[0] = '0' + (char)materialMode;

        char stereoModeString[2] = { 0, 0 };
        stereoModeString[0] = '0' + (char)stereoMode;

        ID3DBlob* vertexShader;
        ID3DBlob* pixelShader;
        m_graphicsAPI.CompileVSPS(L"./assets/Shaders/skybox.hlsl", vertexShader, pixelShader, m_shaderDebug,
            {
                { "MATERIAL_MODE", materialModeString },
                { "STEREO_MODE", stereoModeString },
                { nullptr, nullptr }
            }
        );

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature;
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        // the skybox doesn't render any differently for the red and blue eye so we handle it all in the red pass and don't draw a blue pass.
        // Because of that, we don't limit the color channels written in any shaders for the skybox
        //psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		ThrowIfFailed(m_graphicsAPI.m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateSkybox[i])));

        vertexShader->Release();
        pixelShader->Release();
	}
}

D3D12HelloTriangle::D3D12HelloTriangle(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
    std::fill(m_keyState.begin(), m_keyState.end(), false);
}

void D3D12HelloTriangle::OnInit()
{
    m_graphicsAPI.Create(m_GPUDebug, false, FrameCount, m_width, m_height, Win32Application::GetHwnd());
    
    m_rootSignature = m_graphicsAPI.CreateRootSignature(
        {
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER , 1},
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5 }
        }
    );    

    MakePSOs();

    m_graphicsAPI.CreateCommandList(m_pipelineStateModels[0].Get());

    LoadAssets();

    m_frameIndex = m_graphicsAPI.m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12HelloTriangle::LoadTextures()
{
    m_splitSum = TextureMgr::LoadTexture(m_graphicsAPI, "assets/splitsum.png", true, false);

    // load the material textures
    size_t fileNameIndex = 0;
    for (size_t materialIndex = 0; materialIndex < (size_t)EMaterial::Count; ++materialIndex)
    {
        for (size_t textureIndex = 0; textureIndex < (size_t)EMaterialTexture::Count; ++textureIndex)
        {
            TextureID texture;

            if (s_materialFileNames[fileNameIndex])
            {
                char fileName[1024];
                sprintf_s(fileName, "assets/PBRMaterialTextures/%s", s_materialFileNames[fileNameIndex]);
                texture = TextureMgr::LoadTexture(m_graphicsAPI, fileName, s_materialTextureLinear[textureIndex], true);
            }
            else
            {
                texture = TextureMgr::LoadTexture(m_graphicsAPI, "Assets/white.png", true, false);
            }

            m_materials[materialIndex][textureIndex] = texture;

            fileNameIndex++;
        }

        static_assert((size_t)EMaterialTexture::Count == 5, "Please update this code");
        TextureID textures[] =
        {
            m_materials[materialIndex][0],
            m_materials[materialIndex][1],
            m_materials[materialIndex][2],
            m_materials[materialIndex][3],
            m_materials[materialIndex][4],
        };

        m_materialDescriptorTableHeapID[materialIndex] = TextureMgr::CreateTextureDescriptorTable(m_graphicsAPI, _countof(textures), textures);
    }
}

void D3D12HelloTriangle::MakeProceduralMeshes()
{
    // make the sphere mesh
    {
        // make the body of the sphere
        size_t slicesX = 20;
        size_t slicesY = 20;
        std::vector<Vertex> sphereVertices;
        for (size_t indexY = 1; indexY < slicesY-1; ++indexY)
        {
            float percentY1 = float(indexY) / float(slicesY);
            float percentY2 = float(indexY + 1) / float(slicesY);

            for (size_t indexX = 0; indexX < slicesX; ++indexX)
            {
                float percentX1 = float(indexX) / float(slicesX);
                float percentX2 = float(indexX + 1) / float(slicesX);

                float sinX1 = std::sinf(percentX1 * 2 * c_pi);
                float sinX2 = std::sinf(percentX2 * 2 * c_pi);
                float cosX1 = std::cosf(percentX1 * 2 * c_pi);
                float cosX2 = std::cosf(percentX2 * 2 * c_pi);

                float sinY1 = std::sinf(percentY1 * c_pi);
                float sinY2 = std::sinf(percentY2 * c_pi);
                float cosY1 = std::cosf(percentY1 * c_pi);
                float cosY2 = std::cosf(percentY2 * c_pi);

                // Position, normal, tangent, uv
                Vertex point00 = { { cosX1 * sinY1, cosY1, sinX1 * sinY1 },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX1, percentY1 } };
                Vertex point10 = { { cosX2 * sinY1, cosY1, sinX2 * sinY1 },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX2, percentY1 } };
                Vertex point01 = { { cosX1 * sinY2, cosY2, sinX1 * sinY2 },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX1, percentY2 } };
                Vertex point11 = { { cosX2 * sinY2, cosY2, sinX2 * sinY2 },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX2, percentY2 } };

                // triangle 1 = 00, 01, 10
                sphereVertices.push_back(point00);
                sphereVertices.push_back(point01);
                sphereVertices.push_back(point10);

                // triangle 2 = 01, 11, 10
                sphereVertices.push_back(point01);
                sphereVertices.push_back(point11);
                sphereVertices.push_back(point10);
            }
        }

        // make the caps of the sphere
        for (size_t indexX = 0; indexX < slicesX; ++indexX)
        {
            float percentX1 = float(indexX) / float(slicesX);
            float percentX2 = float(indexX + 1) / float(slicesX);

            float sinX1 = std::sinf(percentX1 * 2 * c_pi);
            float sinX2 = std::sinf(percentX2 * 2 * c_pi);
            float cosX1 = std::cosf(percentX1 * 2 * c_pi);
            float cosX2 = std::cosf(percentX2 * 2 * c_pi);

            // top cap
            {
                float percentY = 1.0f / float(slicesY);

                float cosY = std::cosf(percentY * c_pi);
                float sinY = std::sinf(percentY * c_pi);

                Vertex point0 = { { 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } };
                Vertex point1 = { { cosX1 * sinY, cosY, sinX1 * sinY },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX1, percentY } };
                Vertex point2 = { { cosX2 * sinY, cosY, sinX2 * sinY },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX2, percentY } };

                sphereVertices.push_back(point0);
                sphereVertices.push_back(point1);
                sphereVertices.push_back(point2);
            }


            // bottom cap
            {
                float percentY = 1.0f - (1.0f / float(slicesY));

                float cosY = std::cosf(percentY * c_pi);
                float sinY = std::sinf(percentY * c_pi);

                Vertex point0 = { { 0.0f, -1.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 1.0f } };
                Vertex point1 = { { cosX1 * sinY, cosY, sinX1 * sinY },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX1, percentY } };
                Vertex point2 = { { cosX2 * sinY, cosY, sinX2 * sinY },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ percentX2, percentY } };

                sphereVertices.push_back(point2);
                sphereVertices.push_back(point1);
                sphereVertices.push_back(point0);
            }
        }

        // calculate normals that are appropriate for a sphere
        for (Vertex& v : sphereVertices)
        {
            XMFLOAT3 normal = v.position;
            Normalize(normal);
            v.normal = normal;

            // TEMP: move the sphere over
            v.position.x += 3.0f;
            v.position.y += 0.5f;
        }

        ModelCreate(m_graphicsAPI, m_models[(size_t)EModel::Sphere], false, sphereVertices, "Sphere");
    }

    for (size_t i = 0; i < (size_t)EModel::Count; ++i)
    {
        if (i == (size_t)EModel::Sphere)
        {
            continue;
        }

        if (!ModelLoad(m_graphicsAPI, m_models[i], s_modelsToLoad[i].fileName, s_modelsToLoad[i].baseDir, s_modelsToLoad[i].scale, s_modelsToLoad[i].offset, s_modelsToLoad[i].flipV))
        {
            throw std::exception();
        }
    }
}

void D3D12HelloTriangle::LoadSkyboxes()
{
    // load the skyboxes
    for (size_t i = 0; i < (size_t)ESkyBox::Count; ++i)
    {
        m_skyboxes[i].m_tex = TextureMgr::LoadCubeMap(m_graphicsAPI, s_skyboxBaseFileName[i], false);
        m_skyboxes[i].m_texDiffuse = TextureMgr::LoadCubeMap(m_graphicsAPI, s_skyboxBaseFileNameDiffuse[i], false);
        m_skyboxes[i].m_texSpecular = TextureMgr::LoadCubeMapMips(m_graphicsAPI, s_skyboxBaseFileNameSpecular[i], 5, false);

        TextureID textures[] =
        {
            m_skyboxes[i].m_tex,
            m_skyboxes[i].m_texDiffuse,
            m_skyboxes[i].m_texSpecular
        };

        m_skyboxes[i].m_descriptorTableHeapID = TextureMgr::CreateTextureDescriptorTable(m_graphicsAPI, sizeof(textures) / sizeof(textures[0]), textures);
    }

    // Position, normal, tangent, uv
    Vertex v000{ { -1.0f, -1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } };
    Vertex v100{ {  1.0f, -1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } };
    Vertex v010{ { -1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 1.0f } };
    Vertex v110{ {  1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 1.0f, 1.0f } };

    Vertex v001{ { -1.0f, -1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } };
    Vertex v101{ {  1.0f, -1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } };
    Vertex v011{ { -1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 1.0f } };
    Vertex v111{ {  1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f },{ 1.0f, 1.0f } };

    std::vector<Vertex> skyboxVertices
    {
        // front
        v110,
        v100,
        v000,

        v110,
        v000,
        v010,

        // behind
        v111,
        v101,
        v001,

        v111,
        v001,
        v011,

        // top
        v111,
        v110,
        v010,

        v111,
        v010,
        v011,

        // bottom
        v101,
        v100,
        v000,

        v101,
        v000,
        v001,

        // left
        v011,
        v010,
        v000,

        v011,
        v000,
        v001,

        // right
        v111,
        v110,
        v100,

        v111,
        v100,
        v101,
    };

    // make the skybox model
    ModelCreate(m_graphicsAPI, m_skyboxModel, true, skyboxVertices, "Skybox");
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets()
{
    TextureMgr::Create(m_graphicsAPI);

    m_uav = TextureMgr::CreateUAVTexture(m_graphicsAPI, m_width, m_height);

	// Create a texture samplers
	{
        D3D12_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        m_graphicsAPI.m_device->CreateSampler(&sampler, m_graphicsAPI.m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
	}

    // load the basic textures
    LoadTextures();

    // load the sky boxes
    LoadSkyboxes();

    // make the procedural meshes
    MakeProceduralMeshes();

    // Close the command list and execute it to begin the initial GPU setup.
    m_graphicsAPI.CloseAndExecuteCommandList();

    m_constantBuffer.Init(m_graphicsAPI);
    m_constantBuffer.Write(
        [this] (SConstantBuffer& constantBuffer)
        {
            XMMATRIX translation = XMMatrixTranspose(XMMatrixTranslation(-m_cameraPos.x, -m_cameraPos.y, -m_cameraPos.z));
            XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f);
            constantBuffer.viewMatrix = XMMatrixMultiply(rotation, translation);

            XMVECTOR det;
            constantBuffer.viewMatrixIT = XMMatrixTranspose(XMMatrixInverse(&det, constantBuffer.viewMatrix));
            constantBuffer.projectionMatrix = XMMatrixTranspose(XMMatrixPerspectiveFovRH(45.0f, m_aspectRatio, 0.01f, 100.0f));
            constantBuffer.viewProjectionMatrix = XMMatrixMultiply(constantBuffer.projectionMatrix, constantBuffer.viewMatrix);
            constantBuffer.viewDimensions[0] = float(m_width);
            constantBuffer.viewDimensions[1] = float(m_height);
            constantBuffer.viewDimensions[2] = 1.0f / float(m_width);
            constantBuffer.viewDimensions[3] = 1.0f / float(m_height);

            constantBuffer.cameraPosition[0] = m_cameraPos.x;
            constantBuffer.cameraPosition[1] = m_cameraPos.y;
            constantBuffer.cameraPosition[2] = m_cameraPos.z;
            constantBuffer.cameraPosition[3] = 0.0f;
        }
    );

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_graphicsAPI.m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();
	}

    TextureMgr::OnFrameComplete();
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate()
{
    const float c_speed = 0.1f;

    XMFLOAT3 translation(0.0f, 0.0f, 0.0f);
    float yawAdjust = 0.0f;
    float pitchAdjust = 0.0f;

    if (m_keyState['W'])
    {
        translation.z += c_speed;
    }
    if (m_keyState['A'])
    {
        translation.x += c_speed;
    }
    if (m_keyState['S'])
    {
        translation.z -= c_speed;
    }
    if (m_keyState['D'])
    {
        translation.x -= c_speed;
    }

    if (m_keyState['1'])
    {
        m_skyBox = ESkyBox::AshCanyon;
    }
    if (m_keyState['2'])
    {
        m_skyBox = ESkyBox::MNight;
    }
    if (m_keyState['3'])
    {
        m_skyBox = ESkyBox::Vasa;
    }

    if (translation.x != 0 || translation.y != 0 || translation.z != 0)
    {
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f);
        XMFLOAT4X4 view;
        XMStoreFloat4x4(&view, rotation);

        XMFLOAT3 xAxis = { view._11, view._12, view._13 };
        XMFLOAT3 yAxis = { view._21, view._22, view._23 };
        XMFLOAT3 zAxis = { view._31, view._32, view._33 };

        m_cameraPos = m_cameraPos - xAxis * translation.x;
        m_cameraPos = m_cameraPos - yAxis * translation.y;
        m_cameraPos = m_cameraPos - zAxis * translation.z;

        m_constantBuffer.Write(
            [&] (SConstantBuffer& constantBuffer)
            {
                XMMATRIX translation = XMMatrixTranspose(XMMatrixTranslation(-m_cameraPos.x, -m_cameraPos.y, -m_cameraPos.z));
                XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f);
                constantBuffer.viewMatrix = XMMatrixMultiply(rotation, translation);

                XMVECTOR det;
                constantBuffer.viewMatrixIT = XMMatrixTranspose(XMMatrixInverse(&det, constantBuffer.viewMatrix));
                constantBuffer.viewProjectionMatrix = XMMatrixMultiply(constantBuffer.projectionMatrix, constantBuffer.viewMatrix);

                constantBuffer.cameraPosition[0] = m_cameraPos.x;
                constantBuffer.cameraPosition[1] = m_cameraPos.y;
                constantBuffer.cameraPosition[2] = m_cameraPos.z;
                constantBuffer.cameraPosition[3] = 0.0f;
            }
        );
    }
}

// Render the scene.
void D3D12HelloTriangle::OnRender()
{
    // fps display handling
    {
        static size_t frameCount = -1;
        static std::chrono::high_resolution_clock::time_point start;

        if (frameCount == -1)
        {
            start = std::chrono::high_resolution_clock::now();
        }
        else
        {
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> seconds = now - start;
            if (seconds.count() > 1.0f)
            {
                float fps = float(frameCount) / float(seconds.count());
                WCHAR buffer[256];
                swprintf_s(buffer, L"fps = %0.2f (%0.2f ms)", fps, 1000.0f / fps);
                SetCustomWindowText(buffer);
                frameCount = 0;
                start = now;
            }
        }

        ++frameCount;
    }

	// make and execute command list
    m_graphicsAPI.OpenCommandList(m_rootSignature, m_pipelineStateSkybox[0].Get());
	PopulateCommandList();
    m_graphicsAPI.CloseAndExecuteCommandList();

	// Present the frame.
    if (m_vsync)
	    ThrowIfFailed(m_graphicsAPI.m_swapChain->Present(1, 0));
    else
        ThrowIfFailed(m_graphicsAPI.m_swapChain->Present(0, 0));

	WaitForPreviousFrame();
}

void D3D12HelloTriangle::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

    m_rootSignature->Release();
    m_rootSignature = nullptr;

    m_graphicsAPI.Destroy();

    TextureMgr::Destroy();

	CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::SetMaterialTexturesForObject(EMaterial material)
{
    if (material == EMaterial::Count)
        material = m_material;

    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
        m_graphicsAPI.m_generalHeap->GetGPUDescriptorHandleForHeapStart(),
        (UINT)m_materialDescriptorTableHeapID[(size_t)material],
        m_graphicsAPI.m_generalHeapDescriptorSize
    );

    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::MaterialTextureSet, handle);
}

void D3D12HelloTriangle::PopulateCommandList()
{
    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::SceneConstantBuffer, m_constantBuffer.GetGPUHandle(m_graphicsAPI));
    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::TextureSampler, m_graphicsAPI.m_samplerHeap->GetGPUDescriptorHandleForHeapStart());
    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::UAV, TextureMgr::MakeGPUHandle(m_graphicsAPI, m_uav));

    m_graphicsAPI.m_commandList->RSSetViewports(1, &m_viewport);
    m_graphicsAPI.m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
    m_graphicsAPI.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_graphicsAPI.m_renderTargetsColor[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_graphicsAPI.m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_graphicsAPI.m_rtvHeapDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_graphicsAPI.m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_graphicsAPI.m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
    // Don't need to clear color, because we draw a skybox that erases everything anyways
	//const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	//m_graphicsAPI.m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_graphicsAPI.m_commandList->ClearDepthStencilView(m_graphicsAPI.m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_graphicsAPI.m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // clear the uav
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureMgr::MakeGPUHandleShaderInvisible(m_graphicsAPI, m_uav);
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = TextureMgr::MakeCPUHandleShaderInvisible(m_graphicsAPI, m_uav);
        ID3D12Resource* resource = TextureMgr::GetResourceShaderInvisible(m_graphicsAPI, m_uav);

        const float uavClear[] = { 0.0f, 0.0f, 1.0f, 0.0f };
        m_graphicsAPI.m_commandList->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, resource, uavClear, 0, nullptr);
    }

    // set the split sum texture
    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::SplitsumTexture, TextureMgr::MakeGPUHandle(m_graphicsAPI, m_splitSum));

    // set the sky box textures
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
        m_graphicsAPI.m_generalHeap->GetGPUDescriptorHandleForHeapStart(),
        m_skyboxes[(size_t)m_skyBox].m_descriptorTableHeapID,
        m_graphicsAPI.m_generalHeapDescriptorSize
    );
    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::SkyboxTextureSet, handle);

    // draw regularly
    if (!m_redBlue3DMode)
    {
        PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Render Meshes");

        // figure out the shader permutation parameters
        SShaderPermutations::EMaterialMode materialMode = (m_material == EMaterial::DiffuseWhite) ? SShaderPermutations::EMaterialMode::untextured : SShaderPermutations::EMaterialMode::textured;
        SShaderPermutations::EStereoMode stereoMode = SShaderPermutations::EStereoMode::none;
        size_t psoIndex = SShaderPermutations::GetIndex(materialMode, stereoMode);

        // draw the skybox model first
        {
            PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Model: %s", m_skyboxModel.m_name.c_str());

            m_graphicsAPI.m_commandList->SetPipelineState(m_pipelineStateSkybox[psoIndex].Get());
            m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::ModelConstantBuffer, m_skyboxModel.m_constantBuffer.GetGPUHandle(m_graphicsAPI));
            for (const SSubObject& subObject : m_skyboxModel.m_subObjects)
            {
                m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::DiffuseTexture, TextureMgr::MakeGPUHandle(m_graphicsAPI, subObject.m_textureDiffuse));
                m_graphicsAPI.m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                m_graphicsAPI.m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
            }
        }

        // draw the models
        m_graphicsAPI.m_commandList->SetPipelineState(m_pipelineStateModels[psoIndex].Get());
        for (size_t i = 0; i < (size_t)EModel::Count; ++i)
        {
            if (m_models[i].m_subObjects.size() == 0)
                continue;

            PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Model: %s", m_models[i].m_name.c_str());

            SetMaterialTexturesForObject(s_modelsToLoad[i].modelMaterial);

            m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::ModelConstantBuffer, m_models[i].m_constantBuffer.GetGPUHandle(m_graphicsAPI));
            for (const SSubObject& subObject : m_models[i].m_subObjects)
            {
                m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::DiffuseTexture, TextureMgr::MakeGPUHandle(m_graphicsAPI, subObject.m_textureDiffuse));
                m_graphicsAPI.m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                m_graphicsAPI.m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
            }
        }
    }
    // draw red/blue 3d
    else
    {
        // red
        {
            PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Render Meshes (Red)");

            // figure out the shader permutation parameters
            SShaderPermutations::EMaterialMode materialMode = (m_material == EMaterial::DiffuseWhite) ? SShaderPermutations::EMaterialMode::untextured : SShaderPermutations::EMaterialMode::textured;
            SShaderPermutations::EStereoMode stereoMode = SShaderPermutations::EStereoMode::red;
            size_t psoIndex = SShaderPermutations::GetIndex(materialMode, stereoMode);

            // draw the skybox model first
            {
                PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Model: %s", m_skyboxModel.m_name.c_str());
                m_graphicsAPI.m_commandList->SetPipelineState(m_pipelineStateSkybox[psoIndex].Get());
                m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::ModelConstantBuffer, m_skyboxModel.m_constantBuffer.GetGPUHandle(m_graphicsAPI));
                for (const SSubObject& subObject : m_skyboxModel.m_subObjects)
                {
                    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::DiffuseTexture, TextureMgr::MakeGPUHandle(m_graphicsAPI, subObject.m_textureDiffuse));
                    m_graphicsAPI.m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                    m_graphicsAPI.m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
                }
            }

            // draw the models
            m_graphicsAPI.m_commandList->SetPipelineState(m_pipelineStateModels[psoIndex].Get());
            for (size_t i = 0; i < (size_t)EModel::Count; ++i)
            {
                if (m_models[i].m_subObjects.size() == 0)
                    continue;

                PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Model: %s", m_models[i].m_name.c_str());

                SetMaterialTexturesForObject(s_modelsToLoad[i].modelMaterial);

                m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::ModelConstantBuffer, m_models[i].m_constantBuffer.GetGPUHandle(m_graphicsAPI));
                for (const SSubObject& subObject : m_models[i].m_subObjects)
                {
                    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::DiffuseTexture, TextureMgr::MakeGPUHandle(m_graphicsAPI, subObject.m_textureDiffuse));
                    m_graphicsAPI.m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                    m_graphicsAPI.m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
                }
            }
        }

        // clear depth
        m_graphicsAPI.m_commandList->ClearDepthStencilView(m_graphicsAPI.m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // blue
        {
            PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Render Meshes (Blue)");

            // figure out the shader permutation parameters
            SShaderPermutations::EMaterialMode materialMode = (m_material == EMaterial::DiffuseWhite) ? SShaderPermutations::EMaterialMode::untextured : SShaderPermutations::EMaterialMode::textured;
            SShaderPermutations::EStereoMode stereoMode = SShaderPermutations::EStereoMode::blue;
            size_t psoIndex = SShaderPermutations::GetIndex(materialMode, stereoMode);

            // no need to render the skybox the second time. It would find the same things, so we handle it all in the red pass

            // draw the models
            m_graphicsAPI.m_commandList->SetPipelineState(m_pipelineStateModels[psoIndex].Get());
            for (size_t i = 0; i < (size_t)EModel::Count; ++i)
            {
                if (m_models[i].m_subObjects.size() == 0)
                    continue;

                PIXScopedEvent(m_graphicsAPI.m_commandList, PIX_COLOR_INDEX(0), "Model: %s", m_models[i].m_name.c_str());

                SetMaterialTexturesForObject(s_modelsToLoad[i].modelMaterial);

                m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::ModelConstantBuffer, m_models[i].m_constantBuffer.GetGPUHandle(m_graphicsAPI));
                for (const SSubObject& subObject : m_models[i].m_subObjects)
                {
                    m_graphicsAPI.m_commandList->SetGraphicsRootDescriptorTable(RootTableParameter::DiffuseTexture, TextureMgr::MakeGPUHandle(m_graphicsAPI, subObject.m_textureDiffuse));
                    m_graphicsAPI.m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                    m_graphicsAPI.m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
                }
            }
        }
    }

	// Indicate that the back buffer will now be used to present.
    m_graphicsAPI.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_graphicsAPI.m_renderTargetsColor[m_frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_graphicsAPI.m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_graphicsAPI.m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12HelloTriangle::OnKeyDown(UINT8 key)
{
    m_keyState[key] = true;
}

void D3D12HelloTriangle::OnKeyUp(UINT8 key)
{
    if (key == VK_ESCAPE)
    {
        if (m_mouseLookMode)
        {
            m_mouseLookMode = false;
            ShowCursor(true);
            SetCapture(nullptr);
        }
        else
        {
            PostQuitMessage(0);
        }
    }

    if (key == 'R')
    {
        m_redBlue3DMode = !m_redBlue3DMode;
    }

    if (key == 'V')
    {
        m_vsync = !m_vsync;
    }

    if (key == 189) // '-' next to numbers
    {
        int current = int(m_material);
        if (current > int(EMaterial::FirstCyclable))
            --current;
        m_material = (EMaterial)current;
    }

    if (key == 187) // '=' next to numbers
    {
        int current = int(m_material);
        if (current < int(EMaterial::Count) - 1)
            ++current;
        m_material = (EMaterial)current;
    }

    m_keyState[key] = false;
}

void D3D12HelloTriangle::OnLeftMouseClick()
{
    if (m_mouseLookMode)
    {
        m_mouseLookMode = false;
        ShowCursor(true);
        SetCapture(nullptr);
    }
    else
    {
        m_mouseLookMode = true;
        ShowCursor(false);
        SetCapture(Win32Application::GetHwnd());
    }
}

void D3D12HelloTriangle::OnMouseMove(int relX, int relY)
{
    if (!m_mouseLookMode)
        return;

    if (relX == 0 && relY == 0)
        return;

    const float c_yawSpeed = 0.01f;
    const float c_pitchSpeed = 0.01f;

    float yawAdjust = float(relX) * c_yawSpeed;
    float pitchAdjust = float(relY) * c_pitchSpeed;

    if (yawAdjust != 0.0f || pitchAdjust != 0.0f)
    {
        m_cameraPitch -= pitchAdjust;
        m_cameraYaw -= yawAdjust;

        if (m_cameraPitch < -c_pi/2.0f)
            m_cameraPitch = -c_pi/2.0f;
        else if (m_cameraPitch > c_pi/2.0f)
            m_cameraPitch = c_pi/2.0f;

        m_constantBuffer.Write(
            [&] (SConstantBuffer& constantBuffer)
            {
                XMMATRIX translation = XMMatrixTranspose(XMMatrixTranslation(-m_cameraPos.x, -m_cameraPos.y, -m_cameraPos.z));
                XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f);
                constantBuffer.viewMatrix = XMMatrixMultiply(rotation, translation);

                XMVECTOR det;
                constantBuffer.viewMatrixIT = XMMatrixTranspose(XMMatrixInverse(&det, constantBuffer.viewMatrix));
                constantBuffer.viewProjectionMatrix = XMMatrixMultiply(constantBuffer.projectionMatrix, constantBuffer.viewMatrix);

                constantBuffer.cameraPosition[0] = m_cameraPos.x;
                constantBuffer.cameraPosition[1] = m_cameraPos.y;
                constantBuffer.cameraPosition[2] = m_cameraPos.z;
                constantBuffer.cameraPosition[3] = 0.0f;
            }
        );
    }
}