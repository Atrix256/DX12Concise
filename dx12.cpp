#include "stdafx.h"

#include "dx12.h"

static void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
    IDXGIAdapter1* adapter;
    *ppAdapter = nullptr;

    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            // If you want a software adapter, pass in "/warp" on the command line.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    *ppAdapter = adapter;
}


namespace DX12
{
    ID3D12Device* CreateDevice(bool gpuDebug, bool useWarpDevice, IDXGIFactory4** outFactory)
    {
        ID3D12Device* device = nullptr;

        UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
        bool enableDebugLayer = true;
#else
        bool enableDebugLayer = gpuDebug;
#endif

        // Enable the debug layer (requires the Graphics Tools "optional feature").
        // NOTE: Enabling the debug layer after device creation will invalidate the active device.
        if (enableDebugLayer)
        {
            ID3D12Debug* debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();

                // Enable additional debug layers.
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
            debugController->Release();
        }

        IDXGIFactory4* factory;
        if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory))))
            return nullptr;

        if (gpuDebug)
        {
            ID3D12Debug* spDebugController0;
            ID3D12Debug1* spDebugController1;
            if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0))))
                return nullptr;
            if (FAILED(spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1))))
                return nullptr;
            spDebugController1->SetEnableGPUBasedValidation(true);
            spDebugController0->Release();
            spDebugController1->Release();
        }

        if (useWarpDevice)
        {
            IDXGIAdapter* warpAdapter;
            if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))))
                return nullptr;

            if (FAILED(D3D12CreateDevice(
                warpAdapter,
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)
            )))
                return nullptr;

            warpAdapter->Release();
        }
        else
        {
            IDXGIAdapter1* hardwareAdapter;
            GetHardwareAdapter(factory, &hardwareAdapter);

            if (FAILED(D3D12CreateDevice(
                hardwareAdapter,
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)
            )))
                return nullptr;

            hardwareAdapter->Release();
        }

        *outFactory = factory;

        return device;
    }
};