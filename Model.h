#pragma once

#include "DXSample.h"
#include <list>
#include "TextureMgr.h"
#include "ConstantBuffer.h"

using namespace DirectX;
using namespace Microsoft::WRL;

struct SModelConstantBuffer
{
    XMMATRIX modelMatrix;
};

// Define the vertex input layout.
const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA , 0},
    { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA , 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA , 0 }
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT3 tangent;
    XMFLOAT2 uv;
};

struct SSubObject
{
    ComPtr<ID3D12Resource>      m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW    m_vertexBufferView;
    UINT                        m_numVertices;
    TextureID                   m_textureDiffuse = TextureID::invalid;
};

struct SModel
{
    std::string                             m_name;
    std::list<SSubObject>                   m_subObjects;
    ConstantBuffer<SModelConstantBuffer>    m_constantBuffer;
};

bool ModelLoad (cdGraphicsAPIDX12& graphicsAPI, SModel& model, const char* fileName, const char* baseFilePath, float scale, XMFLOAT3 offset, bool flipV);

void ModelCreate (cdGraphicsAPIDX12& graphicsAPI, SModel& model, bool calculateNormals, std::vector<Vertex>& triangleVertices, const char* debugName);