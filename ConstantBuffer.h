#pragma once

#include "DXSample.h"
#include "dx12.h"

using namespace DirectX;
using namespace Microsoft::WRL;

template <typename T>
class ConstantBuffer
{
public:
    ConstantBuffer ()
    {

    }

    ~ConstantBuffer ()
    {
    }

    void Init (cdGraphicsAPIDX12& graphicsAPI)
    {
        // Create the constant buffer.
        ThrowIfFailed(graphicsAPI.m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        // reserve a heap id for this constant buffer
        m_index = graphicsAPI.ReserveGeneralHeapID();

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = (sizeof(T) + 255) & ~255;	// CB size is required to be 256-byte aligned.
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
            graphicsAPI.m_generalHeap->GetCPUDescriptorHandleForHeapStart(),
            m_index,
            graphicsAPI.m_generalHeapDescriptorSize
        );
        graphicsAPI.m_device->CreateConstantBufferView(&cbvDesc, handle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferBegin)));

        // write the default constructed data
        Write([] (T&) {});
    }

    template <typename LAMBDA>
    void Write (LAMBDA& lambda)
    {
        lambda(m_constantBufferData);
        memcpy(m_constantBufferBegin, &m_constantBufferData, sizeof(T));
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(cdGraphicsAPIDX12& graphicsAPI)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
            graphicsAPI.m_generalHeap->GetGPUDescriptorHandleForHeapStart(),
            m_index,
            graphicsAPI.m_generalHeapDescriptorSize
        );
        return handle;
    }

private:
    ComPtr<ID3D12Resource>          m_constantBuffer;      // the actual d3d resource
    T                               m_constantBufferData;  // the CPU copy that can be read/write.
    T*                              m_constantBufferBegin; // write to here to make it go to the video card. Write only.
    unsigned int                    m_index;
};