#pragma once

// TODO: includes to dx12 stuff here

#include <vector>

struct cdRootSignatureParameter
{
    D3D12_DESCRIPTOR_RANGE_TYPE type;
    UINT                        count;
};

#define SAFE_RELEASE(x) {if (x) {x->Release(); x = nullptr;}}

class cdGraphicsAPIDX12
{
public:
    bool Create(bool gpuDebug, bool useWarpDevice, unsigned int frameCount, unsigned int width, unsigned int height, HWND hWnd);

    ID3D12RootSignature* CreateRootSignature(const std::vector<cdRootSignatureParameter>& rootSignatureParameters);

    bool CompileVSPS(const WCHAR* fileName, ID3DBlob*& vertexShader, ID3DBlob*& pixelShader, bool shaderDebug, const std::vector<D3D_SHADER_MACRO> &defines);

    bool CreateCommandList(ID3D12PipelineState* pso);

    unsigned int ReserveGeneralHeapID(unsigned int count=1)
    {
        unsigned int ret = m_generalHeapDescriptorNextID;
        m_generalHeapDescriptorNextID += count;
        return ret;
    }

    bool CloseAndExecuteCommandList();
    bool OpenCommandList(ID3D12RootSignature* rootSignature, ID3D12PipelineState* pso);

    void OnFrameComplete()
    {
        for (ID3D12Resource* r : m_textureUploadHeaps)
            SAFE_RELEASE(r);

        m_textureUploadHeaps.clear();
    }

    void Destroy()
    {
        for (ID3D12Resource* r : m_renderTargetsColor)
            SAFE_RELEASE(r);
        m_renderTargetsColor.clear();

        for (ID3D12Resource* r : m_textureUploadHeaps)
            SAFE_RELEASE(r);

        SAFE_RELEASE(m_depthStencil);
        SAFE_RELEASE(m_rtvHeap);
        SAFE_RELEASE(m_dsvHeap);
        SAFE_RELEASE(m_samplerHeap);
        SAFE_RELEASE(m_generalHeap);
        SAFE_RELEASE(m_generalHeapShaderInvisible);
        SAFE_RELEASE(m_swapChain);
        SAFE_RELEASE(m_commandList);
        SAFE_RELEASE(m_commandAllocator);
        SAFE_RELEASE(m_commandQueue);
        SAFE_RELEASE(m_device);
    }

    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12CommandAllocator* m_commandAllocator = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;

    std::vector<ID3D12Resource*> m_renderTargetsColor;
    ID3D12Resource* m_depthStencil = nullptr;

    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_dsvHeap = nullptr;
    ID3D12DescriptorHeap* m_samplerHeap = nullptr;
    ID3D12DescriptorHeap* m_generalHeap = nullptr;
    ID3D12DescriptorHeap* m_generalHeapShaderInvisible = nullptr;

    unsigned int m_rtvHeapDescriptorSize = 0;
    unsigned int m_dsvHeapDescriptorSize = 0;
    unsigned int m_samplerHeapDescriptorSize = 0;
    unsigned int m_generalHeapDescriptorSize = 0;

    unsigned int m_generalHeapDescriptorNextID = 0;

    // kept until the texture uploads are done
    std::vector<ID3D12Resource*>    m_textureUploadHeaps;
};

// Number of descriptors allowed of each type. Increase these counts if needed
static const unsigned int c_maxRTVDescriptors = 50; // Render Target Views
static const unsigned int c_maxDSVDescriptors = 50; // Depth Stencil Views
static const unsigned int c_maxSamplerDescriptors = 10; // Texture Samplers
static const unsigned int c_maxGeneralDescriptors = 200; // Shader Resource Views, unordered access views, constant buffer views