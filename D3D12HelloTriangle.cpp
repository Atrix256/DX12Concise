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

#include "dx12.h"

#include "Model.h"
#include "Math.h"
#include <fstream>
#include <vector>
#include <chrono>
#include "pix3.h"

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

static void OutputShaderErrorMessage(ID3D10Blob* errorMessage, const WCHAR* shaderFilename)
{
    if (!errorMessage)
        return;

    char* compileErrors;
    unsigned long long bufferSize, i;
    std::wofstream fout;

    // Get a pointer to the error message text buffer.
    compileErrors = (char*)(errorMessage->GetBufferPointer());

    OutputDebugStringA("\n\n===== Shader Errors =====\n");
    OutputDebugStringA(compileErrors);
    OutputDebugStringA("\n\n");

    // Get the length of the message.
    bufferSize = errorMessage->GetBufferSize();

    // Open a file to write the error message to.
    fout.open("shader-error.txt");

    fout << "Compiling " << shaderFilename << ":\n\n";

    // Write out the error message.
    for (i = 0; i<bufferSize; i++)
    {
        fout << compileErrors[i];
    }

    // Close the file.
    fout.close();

    return;
}

void D3D12HelloTriangle::CompileVSPS (const WCHAR* fileName, ComPtr<ID3DBlob>& vertexShader, ComPtr<ID3DBlob>& pixelShader, SShaderPermutations::EMaterialMode materialMode, SShaderPermutations::EStereoMode stereoMode)
{
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = m_shaderDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;
#endif

    char materialModeString[2] = { 0, 0 };
    materialModeString[0] = '0' + (char)materialMode;

    char stereoModeString[2] = { 0, 0 };
    stereoModeString[0] = '0' + (char)stereoMode;
    const D3D_SHADER_MACRO defines[] =
    {
        "MATERIAL_MODE", materialModeString,
        "STEREO_MODE", stereoModeString,
        NULL, NULL
    };

    ComPtr<ID3DBlob> error;
    HRESULT hr = D3DCompileFromFile(fileName, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error);
    OutputShaderErrorMessage(error.Get(), fileName);
    ThrowIfFailed(hr);
    hr = D3DCompileFromFile(fileName, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error);
    OutputShaderErrorMessage(error.Get(), fileName);
    ThrowIfFailed(hr);
}

void D3D12HelloTriangle::MakePSOs()
{
    // create the model shaders
    for (size_t i = 0; i < SShaderPermutations::Count; ++i)
    {
        SShaderPermutations::EMaterialMode materialMode;
        SShaderPermutations::EStereoMode stereoMode;
        SShaderPermutations::GetSettings(i, materialMode, stereoMode);

        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        CompileVSPS(L"./assets/Shaders/shaders.hlsl", vertexShader, pixelShader, materialMode, stereoMode);

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
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

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateModels[i])));        
    }

	// create the skybox shader
    for (size_t i = 0; i < SShaderPermutations::Count; ++i)
    {
        SShaderPermutations::EMaterialMode materialMode;
        SShaderPermutations::EStereoMode stereoMode;
        SShaderPermutations::GetSettings(i, materialMode, stereoMode);

        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        CompileVSPS(L"Assets/Shaders/skybox.hlsl", vertexShader, pixelShader, materialMode, stereoMode);

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
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

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateSkybox[i])));
	}
}

D3D12HelloTriangle::D3D12HelloTriangle(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0)
{
    std::fill(m_keyState.begin(), m_keyState.end(), false);
}

void D3D12HelloTriangle::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3D12HelloTriangle::LoadPipeline()
{
    // make the dx12 device
    IDXGIFactory4* factory;
    m_device = DX12::CreateDevice(m_GPUDebug, false, &factory);

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
		));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    factory->Release();

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount*2;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Describe and create a depth stencil view (DSV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        // Describe and create a sampler descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = 1;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));
	}

	// Create frame resources.
	{
        // Setup RTV descriptor to specify sRGB format.
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargetsColor[n])));
			m_device->CreateRenderTargetView(m_renderTargetsColor[n].Get(), &rtvDesc, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}

        // create the normal / color RTV
	}

    // Create the depth stencil view.
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        NAME_D3D12_OBJECT(m_depthStencil);

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void D3D12HelloTriangle::LoadTextures()
{
    m_splitSum = TextureMgr::LoadTexture(m_device, m_commandList.Get(), "assets/splitsum.png", true, false);

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
                texture = TextureMgr::LoadTexture(m_device, m_commandList.Get(), fileName, s_materialTextureLinear[textureIndex], true);
            }
            else
            {
                texture = TextureMgr::LoadTexture(m_device, m_commandList.Get(), "Assets/white.png", true, false);
            }

            m_materials[materialIndex][textureIndex] = texture;

            fileNameIndex++;
        }
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

        ModelCreate(m_device, m_commandList.Get(), m_models[(size_t)EModel::Sphere], false, sphereVertices, "Sphere");
    }

    for (size_t i = 0; i < (size_t)EModel::Count; ++i)
    {
        if (i == (size_t)EModel::Sphere)
        {
            continue;
        }

        if (!ModelLoad(m_device, m_commandList.Get(), m_models[i], s_modelsToLoad[i].fileName, s_modelsToLoad[i].baseDir, s_modelsToLoad[i].scale, s_modelsToLoad[i].offset, s_modelsToLoad[i].flipV))
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
        m_skyboxes[i].m_tex = TextureMgr::LoadCubeMap(m_device, m_commandList.Get(), s_skyboxBaseFileName[i], false);
        m_skyboxes[i].m_texDiffuse = TextureMgr::LoadCubeMap(m_device, m_commandList.Get(), s_skyboxBaseFileNameDiffuse[i], false);
        m_skyboxes[i].m_texSpecular = TextureMgr::LoadCubeMapMips(m_device, m_commandList.Get(), s_skyboxBaseFileNameSpecular[i], 5, false);
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
    ModelCreate(m_device, m_commandList.Get(), m_skyboxModel, true, skyboxVertices, "Skybox");
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets()
{
	// Create a root signature consisting of a descriptor table with a single CBV.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

        // NOTE: these root parameters must be the same order / type / etc as SetGraphicsRootDescriptorTable, which actually fills in the data
		CD3DX12_DESCRIPTOR_RANGE1 ranges[14];
		CD3DX12_ROOT_PARAMETER1 rootParameters[14];

		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
        ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
        ranges[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
        ranges[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0);
        ranges[10].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0);
        ranges[11].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8, 0);
        ranges[12].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9, 0);
        ranges[13].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2]);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[3]);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[4]);
        rootParameters[5].InitAsDescriptorTable(1, &ranges[5]);
        rootParameters[6].InitAsDescriptorTable(1, &ranges[6]);
        rootParameters[7].InitAsDescriptorTable(1, &ranges[7]);
        rootParameters[8].InitAsDescriptorTable(1, &ranges[8]);
        rootParameters[9].InitAsDescriptorTable(1, &ranges[9]);
        rootParameters[10].InitAsDescriptorTable(1, &ranges[10]);
        rootParameters[11].InitAsDescriptorTable(1, &ranges[11]);
        rootParameters[12].InitAsDescriptorTable(1, &ranges[12]);
        rootParameters[13].InitAsDescriptorTable(1, &ranges[13]);

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

    MakePSOs();

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineStateModels[0].Get(), IID_PPV_ARGS(&m_commandList)));

    Device::Create(m_device, 100);
    TextureMgr::Create(m_device, m_commandList.Get());

    m_uav = TextureMgr::CreateUAVTexture(m_device, m_commandList.Get(), m_width, m_height);

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
        m_device->CreateSampler(&sampler, m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
	}

    // load the basic textures
    LoadTextures();

    // load the sky boxes
    LoadSkyboxes();

    // make the procedural meshes
    MakeProceduralMeshes();

    // Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_constantBuffer.Init(m_device);
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
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
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

	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
    if (m_vsync)
	    ThrowIfFailed(m_swapChain->Present(1, 0));
    else
        ThrowIfFailed(m_swapChain->Present(0, 0));

	WaitForPreviousFrame();
}

void D3D12HelloTriangle::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

    m_device->Release();
    m_device = nullptr;

    TextureMgr::Destroy();
    Device::Destroy();

	CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::SetMaterialTexturesForObject(EMaterial material)
{
    if (material == EMaterial::Count)
        material = m_material;

    m_commandList->SetGraphicsRootDescriptorTable(8, TextureMgr::MakeGPUHandle(m_materials[(size_t)material][0]));
    m_commandList->SetGraphicsRootDescriptorTable(9, TextureMgr::MakeGPUHandle(m_materials[(size_t)material][1]));
    m_commandList->SetGraphicsRootDescriptorTable(10, TextureMgr::MakeGPUHandle(m_materials[(size_t)material][2]));
    m_commandList->SetGraphicsRootDescriptorTable(11, TextureMgr::MakeGPUHandle(m_materials[(size_t)material][3]));
    m_commandList->SetGraphicsRootDescriptorTable(12, TextureMgr::MakeGPUHandle(m_materials[(size_t)material][4]));
}

void D3D12HelloTriangle::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineStateModels[0].Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { Device::GetHeap_CBV_SRV_UAV(), m_samplerHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_commandList->SetGraphicsRootDescriptorTable(0, m_constantBuffer.GetGPUHandle());
    m_commandList->SetGraphicsRootDescriptorTable(1, m_samplerHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->SetGraphicsRootDescriptorTable(2, TextureMgr::MakeGPUHandle(m_uav));

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargetsColor[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
    // Don't need to clear color, because we draw a skybox that erases everything anyways
	//const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	//m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // clear the uav
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureMgr::MakeGPUHandleShaderInvisible(m_uav);
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = TextureMgr::MakeCPUHandleShaderInvisible(m_uav);
        ID3D12Resource* resource = TextureMgr::GetResourceShaderInvisible(m_uav);

        const float uavClear[] = { 0.0f, 0.0f, 1.0f, 0.0f };
        m_commandList->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, resource, uavClear, 0, nullptr);
    }

    // set the split sum texture
    m_commandList->SetGraphicsRootDescriptorTable(3, TextureMgr::MakeGPUHandle(m_splitSum));

    // set the sky box textures
    m_commandList->SetGraphicsRootDescriptorTable(5, TextureMgr::MakeGPUHandle(m_skyboxes[(size_t)m_skyBox].m_tex));
    m_commandList->SetGraphicsRootDescriptorTable(6, TextureMgr::MakeGPUHandle(m_skyboxes[(size_t)m_skyBox].m_texDiffuse));
    m_commandList->SetGraphicsRootDescriptorTable(7, TextureMgr::MakeGPUHandle(m_skyboxes[(size_t)m_skyBox].m_texSpecular));

    // draw regularly
    if (!m_redBlue3DMode)
    {
        PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Render Meshes");

        // figure out the shader permutation parameters
        SShaderPermutations::EMaterialMode materialMode = (m_material == EMaterial::DiffuseWhite) ? SShaderPermutations::EMaterialMode::untextured : SShaderPermutations::EMaterialMode::textured;
        SShaderPermutations::EStereoMode stereoMode = SShaderPermutations::EStereoMode::none;
        size_t psoIndex = SShaderPermutations::GetIndex(materialMode, stereoMode);

        // draw the skybox model first
        {
            PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Model: %s", m_skyboxModel.m_name.c_str());

            m_commandList->SetPipelineState(m_pipelineStateSkybox[psoIndex].Get());
            m_commandList->SetGraphicsRootDescriptorTable(13, m_skyboxModel.m_constantBuffer.GetGPUHandle());
            for (const SSubObject& subObject : m_skyboxModel.m_subObjects)
            {
                m_commandList->SetGraphicsRootDescriptorTable(4, TextureMgr::MakeGPUHandle(subObject.m_textureDiffuse));
                m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
            }
        }

        // draw the models
        m_commandList->SetPipelineState(m_pipelineStateModels[psoIndex].Get());
        for (size_t i = 0; i < (size_t)EModel::Count; ++i)
        {
            if (m_models[i].m_subObjects.size() == 0)
                continue;

            PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Model: %s", m_models[i].m_name.c_str());

            SetMaterialTexturesForObject(s_modelsToLoad[i].modelMaterial);

            m_commandList->SetGraphicsRootDescriptorTable(13, m_models[i].m_constantBuffer.GetGPUHandle());
            for (const SSubObject& subObject : m_models[i].m_subObjects)
            {
                m_commandList->SetGraphicsRootDescriptorTable(4, TextureMgr::MakeGPUHandle(subObject.m_textureDiffuse));
                m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
            }
        }
    }
    // draw red/blue 3d
    else
    {
        // red
        {
            PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Render Meshes (Red)");

            // figure out the shader permutation parameters
            SShaderPermutations::EMaterialMode materialMode = (m_material == EMaterial::DiffuseWhite) ? SShaderPermutations::EMaterialMode::untextured : SShaderPermutations::EMaterialMode::textured;
            SShaderPermutations::EStereoMode stereoMode = SShaderPermutations::EStereoMode::red;
            size_t psoIndex = SShaderPermutations::GetIndex(materialMode, stereoMode);

            // draw the skybox model first
            {
                PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Model: %s", m_skyboxModel.m_name.c_str());
                m_commandList->SetPipelineState(m_pipelineStateSkybox[psoIndex].Get());
                m_commandList->SetGraphicsRootDescriptorTable(13, m_skyboxModel.m_constantBuffer.GetGPUHandle());
                for (const SSubObject& subObject : m_skyboxModel.m_subObjects)
                {
                    m_commandList->SetGraphicsRootDescriptorTable(4, TextureMgr::MakeGPUHandle(subObject.m_textureDiffuse));
                    m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                    m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
                }
            }

            // draw the models
            m_commandList->SetPipelineState(m_pipelineStateModels[psoIndex].Get());
            for (size_t i = 0; i < (size_t)EModel::Count; ++i)
            {
                if (m_models[i].m_subObjects.size() == 0)
                    continue;

                PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Model: %s", m_models[i].m_name.c_str());

                SetMaterialTexturesForObject(s_modelsToLoad[i].modelMaterial);

                m_commandList->SetGraphicsRootDescriptorTable(13, m_models[i].m_constantBuffer.GetGPUHandle());
                for (const SSubObject& subObject : m_models[i].m_subObjects)
                {
                    m_commandList->SetGraphicsRootDescriptorTable(4, TextureMgr::MakeGPUHandle(subObject.m_textureDiffuse));
                    m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                    m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
                }
            }
        }

        // clear depth
        m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // blue
        {
            PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Render Meshes (Blue)");

            // figure out the shader permutation parameters
            SShaderPermutations::EMaterialMode materialMode = (m_material == EMaterial::DiffuseWhite) ? SShaderPermutations::EMaterialMode::untextured : SShaderPermutations::EMaterialMode::textured;
            SShaderPermutations::EStereoMode stereoMode = SShaderPermutations::EStereoMode::blue;
            size_t psoIndex = SShaderPermutations::GetIndex(materialMode, stereoMode);

            // no need to render the skybox the second time. It would find the same things, so we handle it all in the red pass

            // draw the models
            m_commandList->SetPipelineState(m_pipelineStateModels[psoIndex].Get());
            for (size_t i = 0; i < (size_t)EModel::Count; ++i)
            {
                if (m_models[i].m_subObjects.size() == 0)
                    continue;

                PIXScopedEvent(m_commandList.Get(), PIX_COLOR_INDEX(0), "Model: %s", m_models[i].m_name.c_str());

                SetMaterialTexturesForObject(s_modelsToLoad[i].modelMaterial);

                m_commandList->SetGraphicsRootDescriptorTable(13, m_models[i].m_constantBuffer.GetGPUHandle());
                for (const SSubObject& subObject : m_models[i].m_subObjects)
                {
                    m_commandList->SetGraphicsRootDescriptorTable(4, TextureMgr::MakeGPUHandle(subObject.m_textureDiffuse));
                    m_commandList->IASetVertexBuffers(0, 1, &subObject.m_vertexBufferView);
                    m_commandList->DrawInstanced(subObject.m_numVertices, 1, 0, 0);
                }
            }
        }
    }

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargetsColor[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
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