#include "stdafx.h"

#include "TextureMgr.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

static float sRGBU8_To_LinearF32(stbi_uc value)
{
    return std::powf(float(value) / 255.0f, 2.2f);
}

static stbi_uc LinearF32_To_sRGBU8(float value)
{
    return stbi_uc(std::powf(value, 1.0f / 2.2f)*255.0f);
}

static void MakeNextMip(std::vector<stbi_uc>& srcU8, std::vector<float>& srcF32, int &textureWidth, int &textureHeight)
{
    // box blur for now. can replace with something else if needed.

    // allocate memory for the mip
    std::vector<float> dest;
    int destWidth = textureWidth / 2;
    int destHeight = textureWidth / 2;
    dest.resize(destWidth * destHeight * 4); 

    // make the mip via box filter
    size_t destPixel = 0;
    for (size_t y = 0; y < destHeight; ++y)
    {
        size_t srcPixel00 = (y * 2)*textureWidth * 4;
        size_t srcPixel10 = (y * 2)*textureWidth * 4 + 4;
        size_t srcPixel01 = (y * 2 + 1)*textureWidth * 4;
        size_t srcPixel11 = (y * 2 + 1)*textureWidth * 4 + 4;
        for (size_t x = 0; x < destWidth; ++x)
        {
            dest[destPixel + 0] = (srcF32[srcPixel00 + 0] + srcF32[srcPixel10 + 0] + srcF32[srcPixel01 + 0] + srcF32[srcPixel11 + 0]) / 4.0f;
            dest[destPixel + 1] = (srcF32[srcPixel00 + 1] + srcF32[srcPixel10 + 1] + srcF32[srcPixel01 + 1] + srcF32[srcPixel11 + 1]) / 4.0f;
            dest[destPixel + 2] = (srcF32[srcPixel00 + 2] + srcF32[srcPixel10 + 2] + srcF32[srcPixel01 + 2] + srcF32[srcPixel11 + 2]) / 4.0f;
            dest[destPixel + 3] = (srcF32[srcPixel00 + 3] + srcF32[srcPixel10 + 3] + srcF32[srcPixel01 + 3] + srcF32[srcPixel11 + 3]) / 4.0f;

            destPixel += 4;
            srcPixel00 += 8;
            srcPixel10 += 8;
            srcPixel01 += 8;
            srcPixel11 += 8;
        }
    }

    // dest is now src
    srcF32.resize(dest.size());
    srcU8.resize(dest.size());
    memcpy(&srcF32[0], &dest[0], dest.size()*sizeof(float));
    textureWidth = destWidth;
    textureHeight = destHeight;

    // convert the floating point mip to an sRGB u8 mip
    for (size_t i = 0; i < dest.size(); ++i)
        srcU8[i] = LinearF32_To_sRGBU8(srcF32[i]);
}

std::vector<UINT8> GenerateErrorTextureData (UINT TextureWidth, UINT TextureHeight, UINT TexturePixelSize)
{
    const UINT rowPitch = TextureWidth * TexturePixelSize;
    const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
    const UINT cellHeight = TextureWidth >> 3;	// The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * TextureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += TexturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 255;		// R
            pData[n + 1] = 0;	// G
            pData[n + 2] = 255;	// B
            pData[n + 3] = 0xff;	// A
        }
        else
        {
            pData[n] = 0;		// R
            pData[n + 1] = 255;	// G
            pData[n + 2] = 0;	// B
            pData[n + 3] = 0xff;	// A
        }
    }

    return data;
}

void TextureMgr::Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    TextureMgr& mgr = Get(true);
    mgr.m_created = true;

    // create an obvious error texture for invalid id

    TextureID newTextureID = mgr.ReserveTextureID();
    mgr.m_textures.insert({ newTextureID, {} });
    STexture& newTexture = mgr.m_textures.find(newTextureID)->second;

    UINT TextureWidth = 256;
    UINT TextureHeight = 256;
    UINT TexturePixelSize = 4;

    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = TextureWidth;
    textureDesc.Height = TextureHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&newTexture.m_resource)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(newTexture.m_resource, 0, 1);

    // Copy data to the intermediate upload heap and then schedule a copy 
    // from the upload heap to the Texture2D.
    std::vector<UINT8> pixels = GenerateErrorTextureData(TextureWidth, TextureHeight, TexturePixelSize);

    // Create the GPU upload buffer.
    ID3D12Resource* textureUploadHeap;
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));


    // add the texture upload to the command list
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &pixels[0];
    textureData.RowPitch = TextureWidth * TexturePixelSize;
    textureData.SlicePitch = textureData.RowPitch * TextureHeight;

    UpdateSubresources(commandList, newTexture.m_resource, textureUploadHeap, 0, 0, 1, &textureData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(newTexture.m_resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


    // add the texture upload heap to the list of heaps to clear when the frame completes
    mgr.m_textureUploadHeaps.insert(textureUploadHeap);

    // add the texture to the texture list
    newTexture.m_heapID = Device::ReserveHeapID_CBV_SRV_UAV();
    newTexture.m_resource = newTexture.m_resource;

    // Describe and create a SRV for the texture.
    newTexture.m_srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    newTexture.m_srvDesc.Format = textureDesc.Format;
    newTexture.m_srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    newTexture.m_srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(newTexture.m_resource, &newTexture.m_srvDesc, MakeCPUHandle(newTextureID));

    // for debugging
    SetNameIndexed(newTexture.m_resource, L"Texture", (UINT)newTextureID);
}

void TextureMgr::Destroy()
{
    TextureMgr& mgr = Get();

    // make sure texture upload heaps are clear
    OnFrameComplete();

    // free texture resources
    for (auto it : mgr.m_textures)
        it.second.m_resource->Release();

    mgr.m_texturesLoaded.clear();
    mgr.m_texturesLoadedCubeMaps.clear();

    mgr.m_nextTextureID = TextureID::invalid;

    mgr.m_created = false;
}

TextureID TextureMgr::LoadTexture (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const char* fileName, bool isLinear, bool makeMips)
{
    // TODO: temp?
    makeMips = false;

    // if we already have this file loaded, re-use it
    TextureMgr& mgr = Get();
    auto it = mgr.m_texturesLoaded.find(fileName);
    if (it != mgr.m_texturesLoaded.end())
        return it->second;

    int textureWidth, textureHeight, channelsInFile;
    stbi_uc* pixels = stbi_load(fileName, &textureWidth, &textureHeight, &channelsInFile, 4);
    if (!pixels)
        return TextureID::invalid;

    UINT16 numMips = 1;
    if (makeMips)
    {
        int size = min(textureWidth, textureHeight) / 2;
        while (size > 0)
        {
            ++numMips;
            size = size / 2;
        }
    }

    TextureID newTextureID = mgr.ReserveTextureID();
    mgr.m_textures.insert({ newTextureID,{} });
    STexture& newTexture = mgr.m_textures.find(newTextureID)->second;

    newTexture.m_heapID = Device::ReserveHeapID_CBV_SRV_UAV();

    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = numMips;
    textureDesc.Format = isLinear ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&newTexture.m_resource)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(newTexture.m_resource, 0, 1);

    // Create the GPU upload buffer.
    ID3D12Resource* textureUploadHeap;
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));

    // update first mip
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = pixels;
    textureData.RowPitch = textureWidth * 4;
    textureData.SlicePitch = textureData.RowPitch * textureHeight;
    UpdateSubresources(commandList, newTexture.m_resource, textureUploadHeap, 0, D3D12CalcSubresource(0, 0, 0, numMips, 1), 1, &textureData);

    // add the texture upload heap to the list of heaps to clear when the frame completes
    mgr.m_textureUploadHeaps.insert(textureUploadHeap);
    
    // update subsequent mips - box blur for now. should do something better later if this is noticeably bad looking.
    if (numMips > 1)
    {
        // data setup
        std::vector<stbi_uc> mipDataU8;
        std::vector<float> mipDataF32;
        mipDataU8.resize(textureWidth*textureHeight * 4);
        memcpy(&mipDataU8[0], pixels, mipDataU8.size());
        mipDataF32.resize(mipDataU8.size());
        for (size_t i = 0; i < mipDataU8.size(); ++i)
            mipDataF32[i] = sRGBU8_To_LinearF32(mipDataU8[i]);

        int width = textureWidth;
        int height = textureHeight;

        // for each mip
        for (UINT i = 1; i < numMips; ++i)
        {
            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(newTexture.m_resource, i, 1);

            // Create the GPU upload buffer.
            ID3D12Resource* textureUploadHeap;
            ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&textureUploadHeap)));

            // make the mip
            MakeNextMip(mipDataU8, mipDataF32, width, height);

            // upload the mip
            textureData.pData = &mipDataU8[0];
            textureData.RowPitch = width * 4;
            textureData.SlicePitch = textureData.RowPitch * height;
            UpdateSubresources(commandList, newTexture.m_resource, textureUploadHeap, 0, D3D12CalcSubresource(i, 0, 0, numMips, 1), 1, &textureData);

            // add the texture upload heap to the list of heaps to clear when the frame completes
            mgr.m_textureUploadHeaps.insert(textureUploadHeap);
        }
    }

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(newTexture.m_resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    // Describe and create a SRV for the texture.
    newTexture.m_srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    newTexture.m_srvDesc.Format = textureDesc.Format;
    newTexture.m_srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    newTexture.m_srvDesc.Texture2D.MipLevels = numMips;
    device->CreateShaderResourceView(newTexture.m_resource, &newTexture.m_srvDesc, MakeCPUHandle(newTextureID));

    // add this texture id by it's filename 
    mgr.m_texturesLoaded.insert({fileName, newTextureID});

    stbi_image_free(pixels);

    // for debugging
    SetNameIndexed(newTexture.m_resource, L"Texture", (UINT)newTextureID);

    return newTextureID;
}

TextureID TextureMgr::LoadCubeMap (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const char* baseFileName, bool isLinear)
{
    static const size_t c_numFaces = 6;

    // if we already have this file loaded, re-use it
    TextureMgr& mgr = Get();
    auto it = mgr.m_texturesLoadedCubeMaps.find(baseFileName);
    if (it != mgr.m_texturesLoadedCubeMaps.end())
        return it->second;

    // try and load the faces
    bool error = false;
    char fileName[256];
    stbi_uc *imagePixels[c_numFaces];
    int textureWidth[c_numFaces];
    int textureHeight[c_numFaces];
    for (size_t faceIndex = 0; faceIndex < c_numFaces; ++faceIndex)
    {
        sprintf_s(fileName, baseFileName, c_skyBoxSuffices[faceIndex]);

        int channelsInFile = 0;
        imagePixels[faceIndex] = stbi_load(fileName, &textureWidth[faceIndex], &textureHeight[faceIndex], &channelsInFile, 4);
        error |= imagePixels[faceIndex] == nullptr;
    }

    // make sure all images have the same dimensions
    if (!error)
    {
        for (size_t faceIndex = 1; faceIndex < c_numFaces; ++faceIndex)
        {
            error |= (textureWidth[faceIndex] != textureWidth[0]);
            error |= (textureHeight[faceIndex] != textureHeight[0]);
        }
    }

    // if there was an error during loading, free the memory and return an invalid handle
    if (error)
    {
        for (stbi_uc* pixels : imagePixels)
            stbi_image_free(pixels);
        return TextureID::invalid;
    }

    TextureID newTextureID = mgr.ReserveTextureID();
    mgr.m_textures.insert({ newTextureID,{} });
    STexture& newTexture = mgr.m_textures.find(newTextureID)->second;

    newTexture.m_heapID = Device::ReserveHeapID_CBV_SRV_UAV();

    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = isLinear ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.Width = textureWidth[0];
    textureDesc.Height = textureHeight[0];
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 6;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&newTexture.m_resource)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(newTexture.m_resource, 0, 1);

    for (size_t faceIndex = 0; faceIndex < c_numFaces; ++faceIndex)
    {
        // Create the GPU upload buffer.
        ID3D12Resource* textureUploadHeap;
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&textureUploadHeap)));

        // add the texture upload to the command list
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = imagePixels[faceIndex];
        textureData.RowPitch = textureWidth[0] * 4;
        textureData.SlicePitch = textureData.RowPitch * textureHeight[0];
        UpdateSubresources(commandList, newTexture.m_resource, textureUploadHeap, 0, (UINT)faceIndex, 1, &textureData);

        // add the texture upload heap to the list of heaps to clear when the frame completes
        mgr.m_textureUploadHeaps.insert(textureUploadHeap);
    }

    // resource barier for all these copies
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(newTexture.m_resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    // Describe and create a SRV for the texture.
    newTexture.m_srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    newTexture.m_srvDesc.Format = textureDesc.Format;
    newTexture.m_srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    newTexture.m_srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(newTexture.m_resource, &newTexture.m_srvDesc, MakeCPUHandle(newTextureID));

    // add this texture id by it's filename
    mgr.m_texturesLoadedCubeMaps.insert({fileName, newTextureID});

    // for debugging
    SetNameIndexed(newTexture.m_resource, L"CubeMap", (UINT)newTextureID);

    // free the memory
    for (stbi_uc* pixels : imagePixels)
        stbi_image_free(pixels);

    return newTextureID;
}

TextureID TextureMgr::LoadCubeMapMips(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const char* baseFileName, int numMips, bool isLinear)
{
    static const size_t c_numFaces = 6;
    size_t numImages = c_numFaces * numMips;

    // if we already have this file loaded, re-use it
    TextureMgr& mgr = Get();
    auto it = mgr.m_texturesLoadedCubeMaps.find(baseFileName);
    if (it != mgr.m_texturesLoadedCubeMaps.end())
        return it->second;

    // try and load the faces
    bool error = false;
    char fileName[256];
    std::vector<stbi_uc*> imagePixels;
    std::vector<int> textureWidth;
    std::vector<int> textureHeight;

    for (int mipIndex = 0; mipIndex < numMips; ++mipIndex)
    {
        for (size_t faceIndex = 0; faceIndex < c_numFaces; ++faceIndex)
        {
            sprintf_s(fileName, baseFileName, mipIndex, c_skyBoxSuffices[faceIndex]);

            int channelsInFile = 0;
            int width, height;

            stbi_uc *pixels = stbi_load(fileName, &width, &height, &channelsInFile, 4);
            imagePixels.push_back(pixels);
            error |= pixels == nullptr;

            // make sure all images for the same mip have the same dimensions
            if (faceIndex == 0)
            {
                textureWidth.push_back(width);
                textureHeight.push_back(height);
            }
            else
            {
                error |= (*textureWidth.rbegin()) != width;
                error |= (*textureHeight.rbegin()) != height;
            }
        }
    }

    // if there was an error during loading, free the memory and return an invalid handle
    if (error)
    {
        for (stbi_uc* pixels : imagePixels)
            stbi_image_free(pixels);
        return TextureID::invalid;
    }

    TextureID newTextureID = mgr.ReserveTextureID();
    mgr.m_textures.insert({ newTextureID,{} });
    STexture& newTexture = mgr.m_textures.find(newTextureID)->second;

    newTexture.m_heapID = Device::ReserveHeapID_CBV_SRV_UAV();

    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = numMips;
    textureDesc.Format = isLinear ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;;
    textureDesc.Width = textureWidth[0];
    textureDesc.Height = textureHeight[0];
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 6;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&newTexture.m_resource)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(newTexture.m_resource, 0, 1);

    int imageIndex = 0;
    for (int mipIndex = 0; mipIndex < numMips; ++mipIndex)
    {
        for (size_t faceIndex = 0; faceIndex < c_numFaces; ++faceIndex)
        {
            // Create the GPU upload buffer.
            ID3D12Resource* textureUploadHeap;
            ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&textureUploadHeap)));

            // add the texture upload to the command list
            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData = imagePixels[imageIndex];
            textureData.RowPitch = textureWidth[mipIndex] * 4;
            textureData.SlicePitch = textureData.RowPitch * textureHeight[mipIndex];
            UpdateSubresources(commandList, newTexture.m_resource, textureUploadHeap, 0, D3D12CalcSubresource(mipIndex, (UINT)faceIndex, 0, numMips, (UINT)c_numFaces), 1, &textureData);

            // add the texture upload heap to the list of heaps to clear when the frame completes
            mgr.m_textureUploadHeaps.insert(textureUploadHeap);

            ++imageIndex;
        }
    }

    // resource barier for all these copies
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(newTexture.m_resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    // Describe and create a SRV for the texture.
    newTexture.m_srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    newTexture.m_srvDesc.Format = textureDesc.Format;
    newTexture.m_srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    newTexture.m_srvDesc.Texture2D.MipLevels = numMips;
    device->CreateShaderResourceView(newTexture.m_resource, &newTexture.m_srvDesc, MakeCPUHandle(newTextureID));

    // add this texture id by it's filename
    mgr.m_texturesLoadedCubeMaps.insert({fileName, newTextureID});

    // for debugging
    SetNameIndexed(newTexture.m_resource, L"CubeMap", (UINT)newTextureID);

    // free the memory
    for (stbi_uc* pixels : imagePixels)
        stbi_image_free(pixels);

    return newTextureID;
}

TextureID TextureMgr::CreateUAVTexture(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 width, UINT height)
{
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // take a texture ID
    TextureMgr& mgr = Get();
    TextureID newTextureID = mgr.ReserveTextureID();

    // create a new STexture
    STexture newTexture;
    newTexture.m_heapID = Device::ReserveHeapID_CBV_SRV_UAV();
    
    // create the resource for the uav
    D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&newTexture.m_resource)));

    // create the resource for the uav that is invisible to shaders
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&newTexture.m_resourceShaderInvisible)));

    //commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(newTexture.m_resource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    
    // add the texture to the texture list
    mgr.m_textures.insert({ newTextureID, newTexture });

    // create the uav
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(newTexture.m_resource, nullptr, &uavDesc, MakeCPUHandle(newTextureID));

    // create the uav that is invisible to shaders
    device->CreateUnorderedAccessView(newTexture.m_resourceShaderInvisible, nullptr, &uavDesc, MakeCPUHandleShaderInvisible(newTextureID));

    // for debugging
    SetNameIndexed(newTexture.m_resource, L"UAV", (UINT)newTextureID);
    SetNameIndexed(newTexture.m_resourceShaderInvisible, L"UAVShaderInvisible", (UINT)newTextureID);

    return newTextureID;
}

HeapID_CBV_SRV_UAV TextureMgr::CreateTextureDescriptorTable(ID3D12Device* device, size_t numTextures, TextureID* textures)
{
    HeapID_CBV_SRV_UAV ret = Device::ReserveHeapID_CBV_SRV_UAV(numTextures);
    for (size_t i = 0; i < numTextures; ++i)
    {
        STexture& texture = GetTexture(textures[i]);
        device->CreateShaderResourceView(texture.m_resource, &texture.m_srvDesc, Device::MakeCPUHandleCBV_SRV_UAV(HeapID_CBV_SRV_UAV((size_t)ret + i)));
    }
    return ret;
}

void TextureMgr::OnFrameComplete ()
{
    // release all texture upload heaps now that we are done with them
    TextureMgr& mgr = Get();

    for (ID3D12Resource* res : mgr.m_textureUploadHeaps)
        res->Release();

    mgr.m_textureUploadHeaps.clear();
}