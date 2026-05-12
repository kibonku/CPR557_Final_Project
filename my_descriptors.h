#ifndef __MY_DESCRIPTORS_H__
#define __MY_DESCRIPTORS_H__

#include "my_device.h"

// std
#include <memory>
#include <unordered_map>
#include <vector>

class MyDescriptorSetLayout
{
public:
    class Builder
    {
      public:
        Builder(MyDevice& myDevice) : m_myDevice{ myDevice } {}

        Builder& addBinding(
            uint32_t binding,
            VkDescriptorType descriptorType,
            VkShaderStageFlags stageFlags,
            uint32_t count = 1);
        std::unique_ptr<MyDescriptorSetLayout> build() const;

      private:
        MyDevice&                                                  m_myDevice;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_mapBindings{};
    };

    MyDescriptorSetLayout(
        MyDevice& mDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
    ~MyDescriptorSetLayout();
    MyDescriptorSetLayout(const MyDescriptorSetLayout&) = delete;
    MyDescriptorSetLayout& operator=(const MyDescriptorSetLayout&) = delete;
    MyDescriptorSetLayout(MyDescriptorSetLayout&&) = delete;
    MyDescriptorSetLayout& operator=(const MyDescriptorSetLayout&&) = delete;

    VkDescriptorSetLayout descriptorSetLayout() const { return m_vkDescriptorSetLayout; }

private:
    MyDevice&                                                  m_myDevice;
    VkDescriptorSetLayout                                      m_vkDescriptorSetLayout;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_mapBindings;

    friend class MyDescriptorWriter;
};

class MyDescriptorPool
{
public:
    class Builder
    {
      public:
        Builder(MyDevice& myDevice) : m_myDevice{ myDevice } {}

        Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);
        Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder& setMaxSets(uint32_t count);
        std::unique_ptr<MyDescriptorPool> build() const;

      private:
        MyDevice&                         m_myDevice;
        std::vector<VkDescriptorPoolSize> m_vVkDescriptorPoolSizes{};
        uint32_t                          m_iMaxSets = 1000;
        VkDescriptorPoolCreateFlags       m_vkPoolFlags = 0;
    };

    MyDescriptorPool(
        MyDevice& myDevice,
        uint32_t maxSets,
        VkDescriptorPoolCreateFlags poolFlags,
        const std::vector<VkDescriptorPoolSize>& poolSizes);
    ~MyDescriptorPool();
    MyDescriptorPool(const MyDescriptorPool&) = delete;
    MyDescriptorPool& operator=(const MyDescriptorPool&) = delete;
	MyDescriptorPool(MyDescriptorPool&&) = delete;
    MyDescriptorPool& operator=(const MyDescriptorPool&&) = delete;

    bool allocateDescriptorSet(
        const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

    void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;

    void resetPool();

private:
    MyDevice&        m_myDevice;
    VkDescriptorPool m_vkDescriptorPool;

    friend class     MyDescriptorWriter;
};

class MyDescriptorWriter
{
public:
    MyDescriptorWriter(MyDescriptorSetLayout& setLayout, MyDescriptorPool& pool);

    MyDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
    MyDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

    bool build(VkDescriptorSet& set);
    void overwrite(VkDescriptorSet& set);

private:
    MyDescriptorSetLayout&            m_myDescriptorSetLayout;
    MyDescriptorPool&                 m_myDescriptorPool;
    std::vector<VkWriteDescriptorSet> m_myDescriptorWrites;
};

#endif

