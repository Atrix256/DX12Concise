#pragma once

// TODO: includes to dx12 stuff here

#include <vector>

class GraphicsAPIDX12
{
public:
    bool Create(bool gpuDebug, bool useWarpDevice, unsigned int frameCount, unsigned int width, unsigned int height, HWND hWnd);

    void Destroy()
    {
        m_commandAllocator->Release();
        m_commandAllocator = nullptr;

        for (ID3D12Resource* r : m_renderTargetsColor)
            r->Release();
        m_renderTargetsColor.clear();

        m_depthStencil->Release();
        m_depthStencil = nullptr;

        m_rtvHeap->Release();
        m_rtvHeap = nullptr;

        m_dsvHeap->Release();
        m_dsvHeap = nullptr;

        m_samplerHeap->Release();
        m_samplerHeap = nullptr;

        m_swapChain->Release();
        m_swapChain = nullptr;

        m_commandQueue->Release();
        m_commandQueue = nullptr;

        m_device->Release();
        m_device = nullptr;
    }

    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12CommandAllocator* m_commandAllocator = nullptr;

    std::vector<ID3D12Resource*> m_renderTargetsColor;
    ID3D12Resource* m_depthStencil = nullptr;

    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_dsvHeap = nullptr;
    ID3D12DescriptorHeap* m_samplerHeap = nullptr;
    unsigned int m_rtvHeapDescriptorSize = 0;
    unsigned int m_dsvHeapDescriptorSize = 0;
    unsigned int m_samplerHeapDescriptorSize = 0;


    static const unsigned int c_maxRTVDescriptors = 50; // Render Target Views
    static const unsigned int c_maxDSVDescriptors = 50; // Depth Stencil Views
    static const unsigned int c_maxSamplerDescriptors = 10; // Texture Samplers
    static const unsigned int c_maxSRVDescriptors = 200; // Shader Resource Views
};