//
// Encapsulates a vulkan buffer
//
// Initially based off VulkanBuffer by Sascha Willems -
// https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
//
#include "my_buffer.h"

// std
#include <cassert>
#include <cstring>

//
// Returns the minimum instance size required to be compatible with devices minOffsetAlignment
//
// @param instanceSize The size of an instance
// @param minOffsetAlignment The minimum required alignment, in bytes, for the offset member (eg
// minUniformBufferOffsetAlignment)
//
// @return VkResult of the buffer mapping call
//
VkDeviceSize MyBuffer::alignmentSize(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
{
    if (minOffsetAlignment > 0)
    {
        return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
    }

    return instanceSize;
}

MyBuffer::MyBuffer(
    MyDevice& device,
    VkDeviceSize instanceSize,
    uint32_t instanceCount,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memoryPropertyFlags,
    VkDeviceSize minOffsetAlignment)
    : m_myDevice{ device },
    m_vkInstanceSize{ instanceSize },
    m_iInstanceCount{ instanceCount },
    m_vkUsageFlags{ usageFlags },
    m_vkMemoryPropertyFlags{ memoryPropertyFlags }
{
    // Get the smallest size required for alignment/padding
    m_vkAlignmentSize = alignmentSize(instanceSize, minOffsetAlignment);
    m_vkBufferSize = m_vkAlignmentSize * instanceCount;
    m_myDevice.createBuffer(m_vkBufferSize, usageFlags, memoryPropertyFlags, m_vkBuffer, m_vkMemory);
}

MyBuffer::~MyBuffer()
{
    unmap();
    vkDestroyBuffer(m_myDevice.device(), m_vkBuffer, nullptr);
    vkFreeMemory(m_myDevice.device(), m_vkMemory, nullptr);
}

//
// Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
//
// @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
// buffer range.
// @param offset (Optional) Byte offset from beginning
//
// @return VkResult of the buffer mapping call
//
VkResult MyBuffer::map(VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_vkBuffer && m_vkMemory && "Called map on buffer before create");
    return vkMapMemory(m_myDevice.device(), m_vkMemory, offset, size, 0, &m_pMappedMemeoy);
}

//
// Unmap a mapped memory range
//
// @note Does not return a result as vkUnmapMemory can't fail
//
void MyBuffer::unmap() 
{
    if (m_pMappedMemeoy)
    {
        vkUnmapMemory(m_myDevice.device(), m_vkMemory);
        m_pMappedMemeoy = nullptr;
    }
}

//
// Copies the specified data to the mapped buffer. Default value writes whole buffer range
//
// @param data Pointer to the data to copy
// @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
// range.
// @param offset (Optional) Byte offset from beginning of mapped region
//
//
void MyBuffer::writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_pMappedMemeoy && "Cannot copy to unmapped buffer");

    if (size == VK_WHOLE_SIZE)
    {
        memcpy(m_pMappedMemeoy, data, m_vkBufferSize);
    }
    else
    {
        char* memOffset = (char*)m_pMappedMemeoy;
        memOffset += offset;
        memcpy(memOffset, data, size);
    }
}

//
// Flush a memory range of the buffer to make it visible to the device
//
// @note Only required for non-coherent memory
//
// @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the
// complete buffer range.
// @param offset (Optional) Byte offset from beginning
//
// @return VkResult of the flush call
//
VkResult MyBuffer::flush(VkDeviceSize size, VkDeviceSize offset) 
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(m_myDevice.device(), 1, &mappedRange);
}

//
// Create a buffer info descriptor
//
// @param size (Optional) Size of the memory range of the descriptor
// @param offset (Optional) Byte offset from beginning
//
// @return VkDescriptorBufferInfo of specified offset and range
//
VkDescriptorBufferInfo MyBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset)
{
    return VkDescriptorBufferInfo {
        m_vkBuffer,
        offset,
        size,
    };
}

