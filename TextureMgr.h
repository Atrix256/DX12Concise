#pragma once

#include "DXSample.h"
#include "Device.h"

#include <unordered_set>
#include <unordered_map>

static const char *c_skyBoxSuffices[6] =
{
    "Right",
    "Left",
    "Up",
    "Down",
    "Front",
    "Back"
};

enum class TextureID : UINT32
{
    invalid = 0
};

// A static class, which internally uses a singleton
class TextureMgr
{
public:

    static void Create (ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    static void Destroy ();

    static TextureID LoadTexture (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const char* fileName, bool isLinear, bool makeMips);

    static TextureID LoadCubeMap (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const char* baseFileName, bool isLinear);

    static TextureID LoadCubeMapMips (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const char* baseFileName, int numMips, bool isLinear);

    static TextureID CreateUAVTexture (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 width, UINT height);

    static HeapID_CBV_SRV_UAV CreateTextureDescriptorTable(ID3D12Device* device, size_t numTextures, TextureID* textures);

    static void OnFrameComplete ();

    inline static CD3DX12_GPU_DESCRIPTOR_HANDLE MakeGPUHandle (TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return Device::MakeGPUHandleCBV_SRV_UAV(texture.m_heapID);
    }

    inline static CD3DX12_GPU_DESCRIPTOR_HANDLE MakeGPUHandleShaderInvisible (TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return Device::MakeGPUHandle_ShaderInvisible_CBV_SRV_UAV(texture.m_heapID);
    }

    inline static CD3DX12_CPU_DESCRIPTOR_HANDLE MakeCPUHandle (TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return Device::MakeCPUHandleCBV_SRV_UAV(texture.m_heapID);
    }

    inline static CD3DX12_CPU_DESCRIPTOR_HANDLE MakeCPUHandleShaderInvisible (TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return Device::MakeCPUHandle_ShaderInvisible_CBV_SRV_UAV(texture.m_heapID);
    }

    inline static ID3D12Resource* GetResource (TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return texture.m_resource;
    }

    inline static ID3D12Resource* GetResourceShaderInvisible (TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return texture.m_resource;
    }

private:
    struct STexture
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC m_srvDesc = {};
        ID3D12Resource*                 m_resource  = nullptr;
        ID3D12Resource*                 m_resourceShaderInvisible = nullptr;
        HeapID_CBV_SRV_UAV              m_heapID    = HeapID_CBV_SRV_UAV::invalid;
    };

private:
    TextureMgr() {}
    ~TextureMgr() {}

    inline static TextureMgr& Get(bool skipCreatedTest = false)
    {
        static TextureMgr mgr;

        if (!skipCreatedTest && !mgr.m_created)
            throw std::exception();

        return mgr;
    }
    
    inline TextureID ReserveTextureID ()
    {
        TextureID ret = m_nextTextureID;
        m_nextTextureID = (TextureID)(static_cast<std::underlying_type<TextureID>::type>(m_nextTextureID) + 1);
        return ret;
    }

    inline static TextureMgr::STexture& GetTexture(TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        return mgr.m_textures[index];
    }

    bool m_created = false;

    // kept until the texture uploads are done
    std::unordered_set<ID3D12Resource*>             m_textureUploadHeaps;

    // texture ID to texture resource map
    std::unordered_map<TextureID, STexture>         m_textures;

    // a map of file names to texture ID's, to find a texture by filename
    std::unordered_map<std::string, TextureID>      m_texturesLoaded;
    std::unordered_map<std::string, TextureID>      m_texturesLoadedCubeMaps;
    
    // next texture id
    TextureID                                       m_nextTextureID = TextureID::invalid;
};
