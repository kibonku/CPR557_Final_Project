#ifndef __MY_FRAMEINFO_H__
#define __MY_FRAMEINFO_H__

#include "my_camera.h"

// lib
#include <vulkan/vulkan.h>

struct MyFrameInfo
{
	int                frameIndex;
	float              frameTime;
	VkCommandBuffer    commandBuffer;
	MyCamera&          camera;
	VkDescriptorSet    globalDescriptorSet;
	glm::vec3          color;
};

#endif

