#include "stdafx.h"

#include "dx12.h"

#include <array>
#include <fstream>

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
    for (i = 0; i < bufferSize; i++)
    {
        fout << compileErrors[i];
    }

    // Close the file.
    fout.close();

    return;
}

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

bool cdGraphicsAPIDX12::Create(bool gpuDebug, bool useWarpDevice, unsigned int frameCount, unsigned int width, unsigned int height, HWND hWnd)
{

    // ==================== Create factory ====================
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
        return false;

    // ==================== Create Device ====================

    if (gpuDebug)
    {
        ID3D12Debug* spDebugController0;
        ID3D12Debug1* spDebugController1;
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0))))
            return false;
        if (FAILED(spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1))))
            return false;
        spDebugController1->SetEnableGPUBasedValidation(true);
        spDebugController0->Release();
        spDebugController1->Release();
    }

    if (useWarpDevice)
    {
        IDXGIAdapter* warpAdapter;
        if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))))
            return false;

        if (FAILED(D3D12CreateDevice(
            warpAdapter,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        )))
            return false;

        warpAdapter->Release();
    }
    else
    {
        IDXGIAdapter1* hardwareAdapter;
        GetHardwareAdapter(factory, &hardwareAdapter);

        if (FAILED(D3D12CreateDevice(
            hardwareAdapter,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        )))
            return false;

        hardwareAdapter->Release();
    }

    // ==================== Create Command Queue ====================

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue))))
        return false;

    // ==================== Create Swap Chain ====================

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = frameCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    IDXGISwapChain1* swapChain;
    if (FAILED(factory->CreateSwapChainForHwnd(
        m_commandQueue,		// Swap chain needs the queue so that it can force a flush on it.
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    )))
        return false;
    
    if (FAILED(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER)))
        return false;

    if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain))))
        return false;

    swapChain->Release();
    factory->Release();

    // ==================== Create Heaps ====================

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = c_maxRTVDescriptors;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    // Describe and create a depth stencil view (DSV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = c_maxDSVDescriptors;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    // Describe and create a sampler descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
    samplerHeapDesc.NumDescriptors = c_maxSamplerDescriptors;
    samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap))))
        return false;

    m_rtvHeapDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvHeapDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_samplerHeapDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    // ==================== Create RTV Descriptors ====================

	// Create frame resources.
	{
        m_renderTargetsColor.resize(frameCount);

        // Setup RTV descriptor to specify sRGB format.
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < frameCount; n++)
		{
            if (FAILED(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargetsColor[n]))))
                return false;
            m_device->CreateRenderTargetView(m_renderTargetsColor[n], &rtvDesc, rtvHandle);
			rtvHandle.Offset(1, m_rtvHeapDescriptorSize);
		}
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

        if (FAILED(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        )))
            return false;

        m_device->CreateDepthStencilView(m_depthStencil, &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // ==================== Create Command Allocator ====================

    if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator))))
        return false;

    return true;
}

ID3D12RootSignature* cdGraphicsAPIDX12::MakeRootSignature(const std::vector<cdRootSignatureParameter>& rootSignatureParameters)
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

    // create root parameters
    std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;
    std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
    ranges.resize(rootSignatureParameters.size());
    rootParameters.resize(rootSignatureParameters.size());

    std::array<UINT, 4> startingRegisters{};
    startingRegisters[D3D12_DESCRIPTOR_RANGE_TYPE_UAV] = 1; // the output is uav 0

    for (size_t i = 0; i < rootSignatureParameters.size(); ++i)
    {
        ranges[i].Init(rootSignatureParameters[i].type, (UINT)rootSignatureParameters[i].count, startingRegisters[rootSignatureParameters[i].type]);
        startingRegisters[rootSignatureParameters[i].type] += rootSignatureParameters[i].count;
    }

    for (size_t i = 0; i < rootSignatureParameters.size(); ++i)
    {
        rootParameters[i].InitAsDescriptorTable(1, &ranges[i]);
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1((UINT)rootParameters.size(), &rootParameters[0], 0, nullptr, rootSignatureFlags);

    ID3DBlob* signature;
    ID3DBlob* error;
    ID3D12RootSignature* rootSignature;
    if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
        return nullptr;
    if (FAILED(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
        return nullptr;

    signature->Release();
    if(error)
        error->Release();

    return rootSignature;
}

bool cdGraphicsAPIDX12::CompileVSPS(const WCHAR* fileName, ID3DBlob*& vertexShader, ID3DBlob*& pixelShader, bool shaderDebug, const std::vector<D3D_SHADER_MACRO> &defines)
{
    #if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #else
        UINT compileFlags = shaderDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;
    #endif

    ID3DBlob* error;
    HRESULT hr = D3DCompileFromFile(fileName, &defines[0], D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error);
    OutputShaderErrorMessage(error, fileName);
    if (FAILED(hr))
        return false;

    hr = D3DCompileFromFile(fileName, &defines[0], D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error);
    OutputShaderErrorMessage(error, fileName);
    if (FAILED(hr))
        return false;

    if(error)
        error->Release();

    return true;
}