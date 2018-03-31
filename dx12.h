#pragma once

class GraphicsAPIDX12
{
public:
    bool Create(bool gpuDebug, bool useWarpDevice, unsigned int frameCount, unsigned int width, unsigned int height, HWND hWnd);

    void Destroy()
    {
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
};