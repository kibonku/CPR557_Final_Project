#ifndef __MY_BUFFER_H__
#define __MY_BUFFER_H__

#include "my_device.h"

//
// Wrap VkBuffer and VkBuffer Memory into a single class
//
class MyBuffer
{
public:
    MyBuffer(
        MyDevice& device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment = 1);
    ~MyBuffer();

    MyBuffer(const MyBuffer&) = delete;
    MyBuffer& operator=(const MyBuffer&) = delete;
    MyBuffer(MyBuffer&&) = delete;
    MyBuffer& operator=(const MyBuffer&&) = delete;

    VkResult               map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void                   unmap();

    // Mapping memoey and write to device memory
    void                   writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult               flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkBuffer               buffer() const { return m_vkBuffer; }

private:
    // Need to follow certain memory padding guideline 
    static VkDeviceSize    alignmentSize(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    MyDevice&              m_myDevice;
    void*                  m_pMappedMemeoy = nullptr;
    VkBuffer               m_vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory         m_vkMemory = VK_NULL_HANDLE;

    VkDeviceSize           m_vkBufferSize;
    uint32_t               m_iInstanceCount;
    VkDeviceSize           m_vkInstanceSize;
    VkDeviceSize           m_vkAlignmentSize;
    VkBufferUsageFlags     m_vkUsageFlags;
    VkMemoryPropertyFlags  m_vkMemoryPropertyFlags;
};

#endif

