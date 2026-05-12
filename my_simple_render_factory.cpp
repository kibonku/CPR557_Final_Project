#include "my_simple_render_factory.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <stdexcept>
#include <vector>
#include <string>

// Push constant layout (matches simple_shader.vert / simple_shader.frag from HW5)
struct MySimplePushConstantData
{
    glm::mat4 modelMatrix { 1.0f };
    glm::mat4 normalMatrix{ 1.0f };
};

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------

MySimpleRenderFactory::MySimpleRenderFactory(
    MyDevice& device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout)
    : m_myDevice{ device }
{
    _createPipelineLayout(globalSetLayout);
    _createPipeline(renderPass);
}

MySimpleRenderFactory::~MySimpleRenderFactory()
{
    vkDestroyPipelineLayout(m_myDevice.device(), m_vkPipelineLayout, nullptr);
}

void MySimpleRenderFactory::_createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size   = sizeof(MySimplePushConstantData);

    std::vector<VkDescriptorSetLayout> layouts{ globalSetLayout };

    VkPipelineLayoutCreateInfo info{};
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount         = static_cast<uint32_t>(layouts.size());
    info.pSetLayouts            = layouts.data();
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &range;

    if (vkCreatePipelineLayout(m_myDevice.device(), &info, nullptr, &m_vkPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout!");
}

void MySimpleRenderFactory::_createPipeline(VkRenderPass renderPass)
{
    assert(m_vkPipelineLayout != nullptr);
    PipelineConfigInfo cfg{};
    MyPipeline::defaultPipelineConfigInfo(cfg);
    cfg.renderPass     = renderPass;
    cfg.pipelineLayout = m_vkPipelineLayout;
    m_pMyPipeline = std::make_unique<MyPipeline>(
        m_myDevice,
        "shaders/simple_shader.vert.spv",
        "shaders/simple_shader.frag.spv",
        cfg);
}

// -----------------------------------------------------------------------
// renderSceneGraph — public entry point (HW3 pattern)
// Binds the pipeline once, then recurses through the tree starting at root.
// -----------------------------------------------------------------------
void MySimpleRenderFactory::renderSceneGraph(
    MyFrameInfo& frameInfo,
    std::shared_ptr<MySceneGraphNode>& root)
{
    if (!root) return;

    m_pMyPipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_vkPipelineLayout,
        0, 1,
        &frameInfo.globalDescriptorSet,
        0, nullptr);

    // Root's own local transform is the starting parent transform
    glm::mat4 rootLocal = root->transform.mat4();
    bool rootSelected   = root->isCurrent();

    for (auto& child : root->getChildren())
        _renderNode(frameInfo, child, rootLocal, rootSelected);
}

// -----------------------------------------------------------------------
// _renderNode — recursive traversal (HW3 pattern + HW5 lighting)
//
// Interior/group nodes (no model): multiply transform, recurse into children.
// Leaf nodes (have model):         emit draw call with accumulated world matrix.
//
// Wind highlight: isCurrent() is true on the leaves subtree
// when the leaves group is selected, so the instructor can see
// parent-child propagation during the demo.
// -----------------------------------------------------------------------
void MySimpleRenderFactory::_renderNode(
    MyFrameInfo& frameInfo,
    const std::shared_ptr<MySceneGraphNode>& node,
    glm::mat4 parentTransform,
    bool parentSelected)
{
    if (!node) return;

    glm::mat4 world   = parentTransform * node->transform.mat4();
    bool thisSelected = parentSelected || node->isCurrent();

    if (node->model)
    {
        // Leaf: render with full HW5 lighting pipeline
        MySimplePushConstantData push{};
        push.modelMatrix  = world;
        // Normal matrix = upper-left 3×3 of transpose(inverse(world))
        push.normalMatrix = glm::mat4(
            glm::transpose(glm::inverse(glm::mat3(world))));
        // [3][3] is unused by the shader's mat3(normalMatrix) read.
        // We repurpose it as a deform-enable flag: 1.0 = leaf, 0.0 = everything else.
        bool isLeaf = node->getName().rfind("leaf", 0) == 0;
        bool isPot  = (node->getName() == "pot" || node->getName() == "built_pot");
        bool isStem = (node->getName() == "stem");
        push.normalMatrix[3][3] = isLeaf ? 1.0f : 0.0f;
        push.normalMatrix[3][2] = isPot  ? 1.0f
                                 : isLeaf ? 2.0f
                                 : isStem ? 3.0f
                                 : 0.0f;


        // Selection highlight: brighten the color slightly
        // The simple_shader.frag multiplies fragColor by diffuse + ambient.
        // We scale the vertex color by pushing a brighter normal matrix tint.
        // Simplest approach: just scale the model slightly — but that changes
        // shape. Instead we tint in the push normal matrix's w-column (unused
        // by the shader). The shader doesn't read it, so we leave normalMatrix
        // as-is and handle highlight by boosting ambient in the global UBO.
        // For demo purposes the selected node spins (see application update),
        // which is clearly visible without shader changes.
        (void)thisSelected; // used in demo via spin animation

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            m_vkPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(MySimplePushConstantData),
            &push);

        node->model->bind(frameInfo.commandBuffer);
        node->model->draw(frameInfo.commandBuffer);
    }
    else
    {
        // Interior/group node: no geometry, recurse into children
        for (auto& child : node->getChildren())
            _renderNode(frameInfo, child, world, thisSelected);
    }
}

// -----------------------------------------------------------------------
// renderGameObjects — flat list (HW5 edit-mode overlays: control points,
// bezier curve, center line, surface normals).  Unchanged from HW5.
// -----------------------------------------------------------------------
void MySimpleRenderFactory::renderGameObjects(
    MyFrameInfo& frameInfo,
    std::vector<MyGameObject>& gameObjects)
{
    m_pMyPipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_vkPipelineLayout,
        0, 1,
        &frameInfo.globalDescriptorSet,
        0, nullptr);

    for (auto& obj : gameObjects)
    {
        if (obj.name() == "bezier_surface" && obj.model != nullptr)
        {
            MySimplePushConstantData push{};
            push.modelMatrix  = obj.transform.mat4();
            push.normalMatrix = glm::mat4(obj.transform.normalMatrix());

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                m_vkPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(MySimplePushConstantData),
                &push);

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }
    }
}
