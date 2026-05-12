#ifndef __MY_SIMPLE_RENDER_FACTORY_H__
#define __MY_SIMPLE_RENDER_FACTORY_H__

#include "my_device.h"
#include "my_game_object.h"
#include "my_pipeline.h"
#include "my_camera.h"
#include "my_frame_info.h"

#include <memory>
#include <vector>

class MySimpleRenderFactory
{
public:
    MySimpleRenderFactory(MyDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~MySimpleRenderFactory();

    MySimpleRenderFactory(const MySimpleRenderFactory&) = delete;
    MySimpleRenderFactory& operator=(const MySimpleRenderFactory&) = delete;

    // Recursive scene graph renderer — used for the plant scene (HW3 pattern)
    void renderSceneGraph(MyFrameInfo& frameInfo,
                          std::shared_ptr<MySceneGraphNode>& root);

    // Flat list renderer — used for HW5 edit-mode overlays
    void renderGameObjects(MyFrameInfo& frameInfo,
                           std::vector<MyGameObject>& gameObjects);

private:
    void _createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void _createPipeline(VkRenderPass renderPass);

    // Recursive helper: accumulates parentTransform down the tree
    void _renderNode(MyFrameInfo& frameInfo,
                     const std::shared_ptr<MySceneGraphNode>& node,
                     glm::mat4 parentTransform,
                     bool parentSelected);

    MyDevice&                   m_myDevice;
    std::unique_ptr<MyPipeline> m_pMyPipeline;
    VkPipelineLayout            m_vkPipelineLayout;
};

#endif
