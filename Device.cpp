#include "stdafx.h"

#include "Device.h"

void Device::Create(ID3D12Device* device, UINT numDescriptorsCBV_SRV_UAV)
{
    Device& mgr = Get(true);
    mgr.m_created = true;

    // CBV, SRV, UAV heap
    {
        // shader visible, CPU write only
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = numDescriptorsCBV_SRV_UAV;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mgr.m_heap_CBV_SRV_UAV)));

        mgr.m_descriptorSize_CBV_SRV_UAV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        mgr.m_numDescriptors_CBV_SRV_UAV = numDescriptorsCBV_SRV_UAV;

        // shader invisible, CPU read/write
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mgr.m_heap_CBV_SRV_UAV_ShaderInvisible)));
    }
}

void Device::Destroy()
{
    Device& mgr = Get();

    // CBV, SRV, UAV heap
    mgr.m_heap_CBV_SRV_UAV->Release();
    mgr.m_heap_CBV_SRV_UAV = nullptr;
    mgr.m_heap_CBV_SRV_UAV_ShaderInvisible->Release();
    mgr.m_heap_CBV_SRV_UAV_ShaderInvisible = nullptr;

    mgr.m_created = false;
}