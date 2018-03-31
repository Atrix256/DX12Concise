#include "stdafx.h"

#include "Model.h"
#include "Math.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

bool ModelLoad(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, SModel& model, const char* fileName, const char* baseFilePath, float scale, XMFLOAT3 offset, bool flipV)
{
    model.m_name = fileName;

    // try and load the model
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::string textureName;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fileName, baseFilePath, true))
    {
        OutputDebugStringA("TinyObj:\n");
        OutputDebugStringA(err.c_str());
        return false;
    }

    if (err.length() > 0)
    {
        OutputDebugStringA("TinyObj:\n");
        OutputDebugStringA(err.c_str());
    }

    // make a subobject for each shape in the model
    for (const tinyobj::shape_t& shape : shapes)
    {
        std::vector<Vertex> triangleVertices;

        SSubObject subObject;

        size_t index_offset = 0;
        bool calculateNormals = attrib.normals.size() == 0;

        // load textures
        subObject.m_textureDiffuse = TextureMgr::LoadTexture(device, commandList, "Assets/white.png", false, false);
        if (shape.mesh.material_ids.size() > 0)
        {
            int materialID = shape.mesh.material_ids[0];
            if (materialID >= 0 && materialID < materials.size())
            {
                const tinyobj::material_t& material = materials[materialID];

                if (material.diffuse_texname.size() > 0)
                {
                    textureName = baseFilePath == nullptr ? "" : baseFilePath;
                    textureName += material.diffuse_texname;
                    subObject.m_textureDiffuse = TextureMgr::LoadTexture(device, commandList, textureName.c_str(), false, true);
                }
            }
        }

        // for each face in the shape
        for (unsigned char numVertices : shape.mesh.num_face_vertices)
        {
            if (numVertices != 3)
            {
                throw std::exception();
            }

            size_t startIndex = triangleVertices.size();

            // for each vertex in the face, make an entry in the triangle vertices array
            for (unsigned char i = 0; i < numVertices; ++i)
            {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + i];

                Vertex newVertex;
                newVertex.position = *(XMFLOAT3*)&attrib.vertices[idx.vertex_index * 3];

                newVertex.normal = calculateNormals ? XMFLOAT3(0.0f, 0.0f, 0.0f) : *(XMFLOAT3*)&attrib.normals[idx.normal_index * 3];
                newVertex.uv = *(XMFLOAT2*)&attrib.texcoords[idx.texcoord_index * 2];

                // flip V axis if we should
                if (flipV)
                {
                    newVertex.uv.y = 1.0f - newVertex.uv.y;
                }

                triangleVertices.push_back(newVertex);
            }

            if (calculateNormals)
            {
                XMFLOAT3 ab = triangleVertices[startIndex + 1].position - triangleVertices[startIndex].position;
                XMFLOAT3 ac = triangleVertices[startIndex + 2].position - triangleVertices[startIndex].position;

                XMFLOAT3 norm = Cross(ac, ab);
                Normalize(norm);
                triangleVertices[startIndex + 0].normal = norm;
                triangleVertices[startIndex + 1].normal = norm;
                triangleVertices[startIndex + 2].normal = norm;
            }

            // calculate tangents
            {
                XMFLOAT3 abPos = triangleVertices[startIndex + 1].position - triangleVertices[startIndex].position;
                XMFLOAT3 acPos = triangleVertices[startIndex + 2].position - triangleVertices[startIndex].position;

                XMFLOAT2 abUV = triangleVertices[startIndex + 1].uv - triangleVertices[startIndex].uv;
                XMFLOAT2 acUV = triangleVertices[startIndex + 2].uv - triangleVertices[startIndex].uv;

                float f = 1.0f / (abUV.x * acUV.y - acUV.x * abUV.y);

                XMFLOAT3 tangent;
                tangent.x = f * (acUV.y * abPos.x - abUV.y * acPos.x);
                tangent.y = f * (acUV.y * abPos.y - abUV.y * acPos.y);
                tangent.z = f * (acUV.y * abPos.z - abUV.y * acPos.z);

                triangleVertices[startIndex + 0].tangent = tangent;
                triangleVertices[startIndex + 1].tangent = tangent;
                triangleVertices[startIndex + 2].tangent = tangent;
            }

            index_offset += numVertices;
        }

        subObject.m_numVertices = UINT(triangleVertices.size());
        UINT vertexBufferSize = UINT(triangleVertices.size() * sizeof(triangleVertices[0]));

        // the obj's have winding backwards compared to what i want. reverse it
        std::reverse(triangleVertices.begin(), triangleVertices.end());

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&subObject.m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        ThrowIfFailed(subObject.m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, &triangleVertices[0], vertexBufferSize);
        subObject.m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        subObject.m_vertexBufferView.BufferLocation = subObject.m_vertexBuffer->GetGPUVirtualAddress();
        subObject.m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        subObject.m_vertexBufferView.SizeInBytes = vertexBufferSize;

        // add the subobject to the list
        model.m_subObjects.push_back(subObject);
    }

    // init the per model constant buffer
    model.m_constantBuffer.Init(device);
    model.m_constantBuffer.Write(
        [&] (SModelConstantBuffer& buffer) {
            buffer.modelMatrix = XMMatrixTranspose(XMMatrixMultiply(XMMatrixScaling(scale, scale, scale), XMMatrixTranslation(offset.x, offset.y, offset.z)));
        }
    );

    return true;
}

void ModelCreate (ID3D12Device* device, ID3D12GraphicsCommandList* commandList, SModel& model, bool calculateNormals, std::vector<Vertex>& triangleVertices, const char* debugName)
{
    model.m_name = debugName;

    // make sure we have a multiple of 3 vertices
    if (triangleVertices.size() % 3 != 0)
    {
        throw std::exception();
    }

    // calculate normals if we should
    if (calculateNormals)
    {
        for (size_t triangleIndex = 0; triangleIndex < triangleVertices.size(); triangleIndex+=3)
        {
            XMFLOAT3 ab = triangleVertices[triangleIndex + 1].position - triangleVertices[triangleIndex].position;
            XMFLOAT3 ac = triangleVertices[triangleIndex + 2].position - triangleVertices[triangleIndex].position;

            XMFLOAT3 norm = Cross(ac, ab);
            Normalize(norm);
            triangleVertices[triangleIndex + 0].normal = norm;
            triangleVertices[triangleIndex + 1].normal = norm;
            triangleVertices[triangleIndex + 2].normal = norm;
        }
    }

    // calculate tangents
    for (size_t triangleIndex = 0; triangleIndex < triangleVertices.size(); triangleIndex += 3)
    {
        XMFLOAT3 abPos = triangleVertices[triangleIndex + 1].position - triangleVertices[triangleIndex].position;
        XMFLOAT3 acPos = triangleVertices[triangleIndex + 2].position - triangleVertices[triangleIndex].position;

        XMFLOAT2 abUV = triangleVertices[triangleIndex + 1].uv - triangleVertices[triangleIndex].uv;
        XMFLOAT2 acUV = triangleVertices[triangleIndex + 2].uv - triangleVertices[triangleIndex].uv;

        float f = 1.0f / (abUV.x * acUV.y - acUV.x * abUV.y);

        XMFLOAT3 tangent;
        tangent.x = f * (acUV.y * abPos.x - abUV.y * acPos.x);
        tangent.y = f * (acUV.y * abPos.y - abUV.y * acPos.y);
        tangent.z = f * (acUV.y * abPos.z - abUV.y * acPos.z);

        triangleVertices[triangleIndex + 0].tangent = tangent;
        triangleVertices[triangleIndex + 1].tangent = tangent;
        triangleVertices[triangleIndex + 2].tangent = tangent;
    }

    // make a subobject
    model.m_subObjects.resize(1);
    SSubObject &subObject = *model.m_subObjects.begin();
    subObject.m_numVertices = UINT(triangleVertices.size());
    UINT vertexBufferSize = UINT(triangleVertices.size() * sizeof(triangleVertices[0]));
    subObject.m_textureDiffuse = TextureMgr::LoadTexture(device, commandList, "Assets/white.png", false, false);

    // Note: using upload heaps to transfer static data like vert buffers is not 
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&subObject.m_vertexBuffer)));

    // Copy the triangle data to the vertex buffer.
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
    ThrowIfFailed(subObject.m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, &triangleVertices[0], vertexBufferSize);
    subObject.m_vertexBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    subObject.m_vertexBufferView.BufferLocation = subObject.m_vertexBuffer->GetGPUVirtualAddress();
    subObject.m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    subObject.m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // init the per model constant buffer
    model.m_constantBuffer.Init(device);
    model.m_constantBuffer.Write(
        [] (SModelConstantBuffer& buffer) {
            buffer.modelMatrix = XMMatrixIdentity();
        }
    );
}