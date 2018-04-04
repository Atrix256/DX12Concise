#pragma once

#include "DXSample.h"
#include "dx12.h"

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

    static void Create (cdGraphicsAPIDX12& graphicsAPI);
    static void Destroy ();

    static TextureID LoadTexture (cdGraphicsAPIDX12& graphicsAPI, const char* fileName, bool isLinear, bool makeMips);

    static TextureID LoadCubeMap (cdGraphicsAPIDX12& graphicsAPI, const char* baseFileName, bool isLinear);

    static TextureID LoadCubeMapMips (cdGraphicsAPIDX12& graphicsAPI, const char* baseFileName, int numMips, bool isLinear);

    static TextureID CreateUAVTexture (cdGraphicsAPIDX12& graphicsAPI, UINT64 width, UINT height);

    static unsigned int CreateTextureDescriptorTable(cdGraphicsAPIDX12& graphicsAPI, size_t numTextures, TextureID* textures);

    static void OnFrameComplete ();

    inline static CD3DX12_GPU_DESCRIPTOR_HANDLE MakeGPUHandle (cdGraphicsAPIDX12& graphicsAPI, TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            graphicsAPI.m_generalHeap->GetGPUDescriptorHandleForHeapStart(),
            (UINT)texture.m_heapID,
            graphicsAPI.m_generalHeapDescriptorSize
        );
    }

    inline static CD3DX12_GPU_DESCRIPTOR_HANDLE MakeGPUHandleShaderInvisible (cdGraphicsAPIDX12& graphicsAPI, TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            graphicsAPI.m_generalHeapShaderInvisible->GetGPUDescriptorHandleForHeapStart(),
            (UINT)texture.m_heapID,
            graphicsAPI.m_generalHeapDescriptorSize
        );
    }

    inline static CD3DX12_CPU_DESCRIPTOR_HANDLE MakeCPUHandle (cdGraphicsAPIDX12& graphicsAPI, TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            graphicsAPI.m_generalHeap->GetCPUDescriptorHandleForHeapStart(),
            (UINT)texture.m_heapID,
            graphicsAPI.m_generalHeapDescriptorSize
        );
    }

    inline static CD3DX12_CPU_DESCRIPTOR_HANDLE MakeCPUHandleShaderInvisible (cdGraphicsAPIDX12& graphicsAPI, TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            graphicsAPI.m_generalHeapShaderInvisible->GetCPUDescriptorHandleForHeapStart(),
            (UINT)texture.m_heapID,
            graphicsAPI.m_generalHeapDescriptorSize
        );
    }

    inline static ID3D12Resource* GetResource (cdGraphicsAPIDX12& graphicsAPI, TextureID index)
    {
        TextureMgr& mgr = Get();

        if (mgr.m_textures.find(index) == mgr.m_textures.end())
            throw std::exception();

        STexture& texture = mgr.m_textures[index];

        return texture.m_resource;
    }

    inline static ID3D12Resource* GetResourceShaderInvisible (cdGraphicsAPIDX12& graphicsAPI, TextureID index)
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
        unsigned int                    m_heapID = (unsigned int)-1;
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
