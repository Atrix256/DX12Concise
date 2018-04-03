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

#pragma once

// uncomment this for pix data (and more renderdoc data) in release
//#define USE_PIX

#include "DXSample.h"
#include "Model.h"
#include "ConstantBuffer.h"
#include <array>
#include "TextureMgr.h"
#include "Math.h"
#include "dx12.h"

using namespace DirectX;

struct SShaderPermutations
{
    enum class EMaterialMode
    {
        textured,
        untextured,

        Count
    };

    enum class EStereoMode
    {
        none,
        red,
        blue,

        Count
    };

    static size_t GetIndex (EMaterialMode materialMode, EStereoMode stereoMode)
    {
        size_t ret = (size_t)stereoMode;
        ret *= (size_t)EMaterialMode::Count;
        ret += (size_t)materialMode;
        return ret;
    }

    static void GetSettings (size_t index, EMaterialMode &materialMode, EStereoMode &stereoMode)
    {
        materialMode = (EMaterialMode)(index % (size_t)EMaterialMode::Count);
        index /= (size_t)EMaterialMode::Count;
        stereoMode = (EStereoMode)index;
    }

    static const size_t Count = (size_t)EMaterialMode::Count * (size_t)EStereoMode::Count;
};

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

struct SConstantBuffer
{
    XMMATRIX viewMatrix;
    XMMATRIX viewMatrixIT;
    XMMATRIX projectionMatrix;
    XMMATRIX viewProjectionMatrix;
    float    viewDimensions[4];
    float    cameraPosition[4]; // w is unused
};

enum class ESkyBox
{
   AshCanyon,
   MNight,
   Vasa,

   Count
};

enum class EMaterial
{
    DriedMud,
    GreasyPan,
    ScuffedIron,
    RoughRock,
    RustedIron,
    FloorBoards,
    DiffuseWhite,
    ShinyMetal,
    Obsidian,
    a,
    b,

    Count,
    FirstCyclable = DriedMud,
};

enum class EMaterialTexture
{
    Albedo,
    Metalness,
    Normal,
    Roughness,
    AO,

    Count
};

enum class EModel
{
    Sphere,
    Cube,

    Count
};

struct SSkyBoxTextures
{
    HeapID_CBV_SRV_UAV m_descriptorTableHeapID;
    TextureID m_tex;
    TextureID m_texDiffuse;
    TextureID m_texSpecular;
};

class D3D12HelloTriangle : public DXSample
{
public:
	D3D12HelloTriangle(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);
    virtual void OnLeftMouseClick();
    virtual void OnMouseMove(int relX, int relY);

private:
	static const UINT FrameCount = 2;

	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<ID3D12PipelineState> m_pipelineStateModels[SShaderPermutations::Count];
    ComPtr<ID3D12PipelineState> m_pipelineStateSkybox[SShaderPermutations::Count];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

    GraphicsAPIDX12 m_graphicsAPI;

	// App resources.
    SModel m_skyboxModel;
    SModel m_models[(size_t)EModel::Count];
    ConstantBuffer<SConstantBuffer> m_constantBuffer;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

    void CompileVSPS(const WCHAR* fileName, ComPtr<ID3DBlob>& vertexShader, ComPtr<ID3DBlob>& pixelShader, SShaderPermutations::EMaterialMode materialMode, SShaderPermutations::EStereoMode stereoMode);

    void MakePSOs();

	void LoadPipeline();

    void LoadTextures();
    void LoadSkyboxes();

    void MakeProceduralMeshes();

	void LoadAssets();
    
    void SetMaterialTexturesForObject(EMaterial material);

	void PopulateCommandList();
	void WaitForPreviousFrame();

    std::array<bool, 256> m_keyState;

    TextureID m_uav = TextureID::invalid;

    std::array<SSkyBoxTextures, (size_t)ESkyBox::Count> m_skyboxes;

    ESkyBox m_skyBox = ESkyBox::Vasa;
    EMaterial m_material = EMaterial::ScuffedIron;

    TextureID m_splitSum = TextureID::invalid;

    bool m_mouseLookMode = false;

    TextureID m_materials[(size_t)EMaterial::Count][(size_t)EMaterialTexture::Count];
    HeapID_CBV_SRV_UAV m_materialDescriptorTableHeapID[(size_t)EMaterial::Count];   

    XMFLOAT3 m_cameraPos = { 0.0f, 1.0f, -5.0f };
    float m_cameraPitch = 0.0f;
    float m_cameraYaw = c_pi;

    bool m_redBlue3DMode = false;

    bool m_vsync = true;
};
