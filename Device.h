#pragma once

#include "DXSample.h"

enum class HeapID_CBV_SRV_UAV : UINT
{
    invalid = (UINT)-1
};

// A static class, which internally uses a singleton
class Device
{
public:
    static void Create (ID3D12Device* device, UINT numDescriptors_CBV_SRV_UAV);
    static void Destroy ();

    inline static ID3D12DescriptorHeap* GetHeap_CBV_SRV_UAV ()
    {
        return Get().m_heap_CBV_SRV_UAV;
    }

    inline static CD3DX12_GPU_DESCRIPTOR_HANDLE MakeGPUHandleCBV_SRV_UAV (HeapID_CBV_SRV_UAV index)
    {
        Device& mgr = Get();

        return CD3DX12_GPU_DESCRIPTOR_HANDLE (
            mgr.m_heap_CBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
            (UINT)index,
            mgr.m_descriptorSize_CBV_SRV_UAV
        );
    }

    inline static CD3DX12_GPU_DESCRIPTOR_HANDLE MakeGPUHandle_ShaderInvisible_CBV_SRV_UAV (HeapID_CBV_SRV_UAV index)
    {
        Device& mgr = Get();

        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            mgr.m_heap_CBV_SRV_UAV_ShaderInvisible->GetGPUDescriptorHandleForHeapStart(),
            (UINT)index,
            mgr.m_descriptorSize_CBV_SRV_UAV
        );
    }

    inline static CD3DX12_CPU_DESCRIPTOR_HANDLE MakeCPUHandleCBV_SRV_UAV (HeapID_CBV_SRV_UAV index)
    {
        Device& mgr = Get();

        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mgr.m_heap_CBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
            (UINT)index,
            mgr.m_descriptorSize_CBV_SRV_UAV
        );
    }

    inline static CD3DX12_CPU_DESCRIPTOR_HANDLE MakeCPUHandle_ShaderInvisible_CBV_SRV_UAV (HeapID_CBV_SRV_UAV index)
    {
        Device& mgr = Get();

        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mgr.m_heap_CBV_SRV_UAV_ShaderInvisible->GetCPUDescriptorHandleForHeapStart(),
            (UINT)index,
            mgr.m_descriptorSize_CBV_SRV_UAV
        );
    }

    inline static HeapID_CBV_SRV_UAV ReserveHeapID_CBV_SRV_UAV ()
    {
        // reserve an id by incrementing the next ID counter and returning the old value
        Device& mgr = Get();
        HeapID_CBV_SRV_UAV ret = mgr.m_nextID_CBV_SRV_UAV;
        mgr.m_nextID_CBV_SRV_UAV = (HeapID_CBV_SRV_UAV)(static_cast<std::underlying_type<HeapID_CBV_SRV_UAV>::type>(mgr.m_nextID_CBV_SRV_UAV) + 1);

        // if we use more descriptors than we were told we would, throw an error
        if (static_cast<std::underlying_type<HeapID_CBV_SRV_UAV>::type>(mgr.m_nextID_CBV_SRV_UAV) > static_cast<std::underlying_type<HeapID_CBV_SRV_UAV>::type>(mgr.m_numDescriptors_CBV_SRV_UAV))
        {
            throw std::exception();
        }

        return ret;
    }

private:
    Device() {}
    ~Device() {}

    inline static Device& Get(bool skipCreatedTest = false)
    {
        static Device mgr;

        if (!skipCreatedTest && !mgr.m_created)
            throw std::exception();

        return mgr;
    }
    
    bool m_created                              = false;

    // shader visible, CPU write only
    ID3D12DescriptorHeap* m_heap_CBV_SRV_UAV    = nullptr;
    UINT m_descriptorSize_CBV_SRV_UAV           = 0;
    UINT m_numDescriptors_CBV_SRV_UAV           = 0;
    HeapID_CBV_SRV_UAV m_nextID_CBV_SRV_UAV     = (HeapID_CBV_SRV_UAV)0;

    // shader invisible, CPU read/write
    ID3D12DescriptorHeap* m_heap_CBV_SRV_UAV_ShaderInvisible = nullptr;
};