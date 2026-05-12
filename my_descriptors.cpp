#include "my_descriptors.h"

// std
#include <cassert>
#include <stdexcept>


// *************** Descriptor Set Layout Builder *********************

MyDescriptorSetLayout::Builder& MyDescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    uint32_t count) 
{
    assert(m_mapBindings.count(binding) == 0 && "Binding already in use");
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    m_mapBindings[binding] = layoutBinding;
    return *this;
}

std::unique_ptr<MyDescriptorSetLayout> MyDescriptorSetLayout::Builder::build() const 
{
    return std::make_unique<MyDescriptorSetLayout>(m_myDevice, m_mapBindings);
}

// *************** Descriptor Set Layout *********************

MyDescriptorSetLayout::MyDescriptorSetLayout(
    MyDevice& myDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
    : m_myDevice{ myDevice }, m_mapBindings{ bindings }
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
    for (auto kv : bindings)
    {
        setLayoutBindings.push_back(kv.second);
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

    if (vkCreateDescriptorSetLayout(
        m_myDevice.device(),
        &descriptorSetLayoutInfo,
        nullptr,
        &m_vkDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

MyDescriptorSetLayout::~MyDescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout(m_myDevice.device(), m_vkDescriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

MyDescriptorPool::Builder& MyDescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) 
{
    m_vVkDescriptorPoolSizes.push_back({ descriptorType, count });
    return *this;
}

MyDescriptorPool::Builder& MyDescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) 
{
    m_vkPoolFlags = flags;
    return *this;
}

MyDescriptorPool::Builder& MyDescriptorPool::Builder::setMaxSets(uint32_t count) 
{
    m_iMaxSets = count;
    return *this;
}

std::unique_ptr<MyDescriptorPool> MyDescriptorPool::Builder::build() const 
{
    return std::make_unique<MyDescriptorPool>(m_myDevice, m_iMaxSets, m_vkPoolFlags, m_vVkDescriptorPoolSizes);
}

// *************** Descriptor Pool *********************

MyDescriptorPool::MyDescriptorPool(
    MyDevice& myDevice,
    uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize>& poolSizes)
    : m_myDevice{ myDevice } 
{
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = maxSets;
    descriptorPoolInfo.flags = poolFlags;

    if (vkCreateDescriptorPool(m_myDevice.device(), &descriptorPoolInfo, nullptr, &m_vkDescriptorPool) != VK_SUCCESS)
	{
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

MyDescriptorPool::~MyDescriptorPool()
{
    vkDestroyDescriptorPool(m_myDevice.device(), m_vkDescriptorPool, nullptr);
}

bool MyDescriptorPool::allocateDescriptorSet(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const 
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_vkDescriptorPool;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
    // a new pool whenever an old pool fills up. But this is beyond our current scope
    if (vkAllocateDescriptorSets(m_myDevice.device(), &allocInfo, &descriptor) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void MyDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const 
{
    vkFreeDescriptorSets(
        m_myDevice.device(),
        m_vkDescriptorPool,
        static_cast<uint32_t>(descriptors.size()),
        descriptors.data());
}

void MyDescriptorPool::resetPool() 
{
    vkResetDescriptorPool(m_myDevice.device(), m_vkDescriptorPool, 0);
}

// *************** Descriptor Writer *********************

MyDescriptorWriter::MyDescriptorWriter(MyDescriptorSetLayout& setLayout, MyDescriptorPool& pool)
    : m_myDescriptorSetLayout{ setLayout }, m_myDescriptorPool{ pool }
{
}

MyDescriptorWriter& MyDescriptorWriter::writeBuffer(
    uint32_t binding, VkDescriptorBufferInfo* bufferInfo) 
{
    assert(m_myDescriptorSetLayout.m_mapBindings.count(binding) == 1 && "Layout does not contain specified binding");

    auto& bindingDescription = m_myDescriptorSetLayout.m_mapBindings[binding];

    assert(
        bindingDescription.descriptorCount == 1 &&
        "Binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = bufferInfo;
    write.descriptorCount = 1;

    m_myDescriptorWrites.push_back(write);
    return *this;
}

MyDescriptorWriter& MyDescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo* imageInfo) 
{
    assert(m_myDescriptorSetLayout.m_mapBindings.count(binding) == 1 && "Layout does not contain specified binding");

    auto& bindingDescription = m_myDescriptorSetLayout.m_mapBindings[binding];

    assert(
        bindingDescription.descriptorCount == 1 &&
        "Binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = imageInfo;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;

    m_myDescriptorWrites.push_back(write);
    return *this;
}

bool MyDescriptorWriter::build(VkDescriptorSet& set) 
{
    bool success = m_myDescriptorPool.allocateDescriptorSet(m_myDescriptorSetLayout.descriptorSetLayout(), set);
    if (!success)
    {
        return false;
    }

    overwrite(set);
    return true;
}

void MyDescriptorWriter::overwrite(VkDescriptorSet& set) 
{
    for (auto& write : m_myDescriptorWrites)
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(m_myDescriptorPool.m_myDevice.device(), (uint32_t)m_myDescriptorWrites.size(), m_myDescriptorWrites.data(), 0, nullptr);
}

