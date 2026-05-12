#include "my_model.h"
#include "my_utils.h"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cassert>
#include <cstring>
#include <unordered_map>

namespace std {
	template <>
	struct hash<MyModel::Vertex> {
		size_t operator()(MyModel::Vertex const& vertex) const {
			size_t seed = 0;
			myHashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
			return seed;
		}
	};
}

MyModel::MyModel(MyDevice& device, const std::vector<Vertex>& vertices) :
	m_myDevice{ device },
	m_iVertexCount{ 0 }
{
	_createVertexBuffer(vertices, false);
}

MyModel::MyModel(MyDevice& device, const MyModel::Builder& builder) : 
	m_myDevice{ device },
	m_iVertexCount{ 0 }
{
	_createVertexBuffer(builder.vertices, true);
	_createIndexBuffers(builder.indices);
}

// Dynamic surface: HOST_VISIBLE vertex buffer (updatable) + static DEVICE_LOCAL index buffer
MyModel::MyModel(MyDevice& device, const std::vector<Vertex>& vertices,
                 const std::vector<uint32_t>& indices, bool dynamic)
    : m_myDevice{device}, m_iVertexCount{0}, m_bDynamicVertex{dynamic}
{
    if (dynamic)
    {
        m_iVertexCount    = static_cast<uint32_t>(vertices.size());
        m_iMaxVertexCount = m_iVertexCount;
        m_pMyVertexBuffer = std::make_unique<MyBuffer>(
            m_myDevice,
            sizeof(Vertex),
            m_iMaxVertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_pMyVertexBuffer->map();
        m_pMyVertexBuffer->writeToBuffer((void*)vertices.data());
    }
    else
    {
        _createVertexBuffer(vertices, true);
    }
    _createIndexBuffers(indices);
}

// For dynamic data update to pre-allocate a big memory in GPU
MyModel::MyModel(MyDevice& device, const int maxVertices) :
	m_myDevice{ device },
	m_iVertexCount{ 0 },
	m_iMaxVertexCount{ (uint32_t)maxVertices }
{
	// This for points and lines to allocate big memory in GPU side
	// and fill up the data later
	VkDeviceSize maxbufferSize = sizeof(PointCurve) * m_iMaxVertexCount;

	m_pMyVertexBuffer = std::make_unique<MyBuffer>(
		m_myDevice,
		sizeof(PointCurve),
		m_iMaxVertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	m_pMyVertexBuffer->map();
}

std::unique_ptr<MyModel> MyModel::createModelFromFile(
	MyDevice& device, const std::string& filepath) 
{
	Builder builder{};
	builder.loadModel(filepath);
	return std::make_unique<MyModel>(device, builder);
}

void MyModel::_createVertexBuffer(const std::vector<Vertex>& vertices, bool bUseIndexBuffer)
{
    m_iVertexCount = static_cast<uint32_t>(vertices.size());
    assert(m_iVertexCount >= 3 && "Vertex count must be at least 3");
    
    // number of bytes need to store the vertex buffer
    // Note: we assume Color and Position are interleaved here
    // inside the vertex buffer
    VkDeviceSize bufferSize = sizeof(vertices[0]) * m_iVertexCount;
	uint32_t vertexSize = sizeof(vertices[0]);
    
    // Create buffer handle and allocate buffer memory on GPU side
    // Note: Host - CPU
    //       Device - GPU
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT - make sure the GPU memory is visible to the host
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT - make sure the host and device memory is consistent

    // Note: Use stage buffer to copy memory buffer can be faster
	// because VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT can be slower
	MyBuffer stagingBuffer
	{
	  m_myDevice,
	  vertexSize,     // size of an instance
	  m_iVertexCount, // number of instances
	  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	};

	// Note: it will unmap in the destructor
	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)vertices.data());

	m_pMyVertexBuffer = std::make_unique<MyBuffer>(
		m_myDevice,
		vertexSize,
		m_iVertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Copy from stage buffer to device local buffer
	// Note: because device local buffer can perform faster, but cannot access by CPU
	// thus we need to copy to a stage buffer first then copy from the stage buffer
	// to the device local buffer
	m_myDevice.copyBuffer(stagingBuffer.buffer(), m_pMyVertexBuffer->buffer(), bufferSize);
}

void MyModel::_createIndexBuffers(const std::vector<uint32_t>& indices) 
{
	m_iIndexCount = static_cast<uint32_t>(indices.size());
	m_bHasIndexBuffer = m_iIndexCount > 0;

	if (!m_bHasIndexBuffer) 
	{
		return;
	}

	VkDeviceSize bufferSize = sizeof(indices[0]) * m_iIndexCount;
	uint32_t indexSize = sizeof(indices[0]);

	MyBuffer stagingBuffer{
	  m_myDevice,
	  indexSize,
	  m_iIndexCount,
	  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	};

	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)indices.data());

	m_pMyIndexBuffer = std::make_unique<MyBuffer>(
		m_myDevice,
		indexSize,
		m_iIndexCount,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_myDevice.copyBuffer(stagingBuffer.buffer(), m_pMyIndexBuffer->buffer(), bufferSize);
}

void MyModel::bind(VkCommandBuffer commandBuffer)
{
	// Bind vertex buffer and index buffer
	VkBuffer buffers[] = { m_pMyVertexBuffer->buffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

	if (m_bHasIndexBuffer) 
	{
		vkCmdBindIndexBuffer(commandBuffer, m_pMyIndexBuffer->buffer(), 0, VK_INDEX_TYPE_UINT32); // can store up to 2^32 -1 indices
	}
}

void MyModel::draw(VkCommandBuffer commandBuffer)
{
	if (m_bHasIndexBuffer)
	{
		vkCmdDrawIndexed(commandBuffer, m_iIndexCount, 1, 0, 0, 0);
	}
	else
	{
		vkCmdDraw(commandBuffer, m_iVertexCount, 1, 0, 0);
	}
}

std::vector<VkVertexInputBindingDescription> MyModel::Vertex::getBindingDescriptions()
{
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> MyModel::Vertex::getAttributeDescriptions()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions{}; // now we have more info passing to shaders

	attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
	attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) });
	attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
	attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });

	return attributeDescriptions;
}

std::vector<VkVertexInputBindingDescription> MyModel::PointCurve::getBindingDescriptions()
{
	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(PointCurve);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> MyModel::PointCurve::getAttributeDescriptions()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

	attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PointCurve, position) });
	attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PointCurve, color) });

	return attributeDescriptions;
}

void MyModel::Builder::loadModel(const std::string& filepath)
{
	// Note: attrib contains vertex, color, normal, texture coordinate...
	// shapes contain the index elements for the geometry
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
		throw std::runtime_error(warn + err);
	}

	// Clear the original data if any
	vertices.clear();
	indices.clear();

	// This will look for duplicate
	// using overrrite hash funciton myHashCombine in my_untils.h
	// and need to implement == operator in MyModel::Vertex
	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			if (index.vertex_index >= 0)
			{
				vertex.position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				};

				// Color may be appended to the postion field in the same line
				// please see colored_cube.obj file
				vertex.color = {
					attrib.colors[3 * index.vertex_index + 0],
					attrib.colors[3 * index.vertex_index + 1],
					attrib.colors[3 * index.vertex_index + 2],
				};
			}

			if (index.normal_index >= 0)
			{
				vertex.normal = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2],
				};
			}

			if (index.texcoord_index >= 0)
			{
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1],
				};
			}

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);

			// None optimized way. Not using index buffer
			// more vertex to load
			//vertices.push_back(vertex);
		}
	}
}

void MyModel::updateSurface(const std::vector<Vertex>& verts)
{
    uint32_t n = static_cast<uint32_t>(verts.size());
    if (n == 0 || n > m_iMaxVertexCount) return;
    m_iVertexCount = n;
    m_pMyVertexBuffer->writeToBuffer((void*)verts.data(), sizeof(Vertex) * n);
}

void MyModel::updatePointCurves(const std::vector<PointCurve>& vertices)
{
	m_iVertexCount = static_cast<uint32_t>(vertices.size());
	if (m_iVertexCount <= 0 || m_iVertexCount > m_iMaxVertexCount)
		return;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * m_iVertexCount;
	m_pMyVertexBuffer->writeToBuffer((void*)vertices.data(), bufferSize);
}

