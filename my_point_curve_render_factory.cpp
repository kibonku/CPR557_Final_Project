#include "my_point_curve_render_factory.h"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>

struct MyPointCurvePushConstantData
{
    glm::mat4 transform{ 1.0f };
    glm::vec3 push_color{ 1.0f, 0.0f, 0.0f };
};

MyPointCurveRenderFactory::MyPointCurveRenderFactory(MyDevice& device, VkRenderPass renderPass, VkPrimitiveTopology topology)
    : m_myDevice{ device } 
{
    _createPipelineLayout();
    _createPipeline(renderPass, topology);
}

MyPointCurveRenderFactory::~MyPointCurveRenderFactory()
{
    vkDestroyPipelineLayout(m_myDevice.device(), m_vkPipelineLayout, nullptr);
}

void MyPointCurveRenderFactory::_createPipelineLayout() 
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(MyPointCurvePushConstantData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(m_myDevice.device(), &pipelineLayoutInfo, nullptr, &m_vkPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void MyPointCurveRenderFactory::_createPipeline(VkRenderPass renderPass, VkPrimitiveTopology topology)
{
    assert(m_vkPipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    MyPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = m_vkPipelineLayout;
    pipelineConfig.inputAssemblyInfo.topology = topology; // override to draw in different topology
    pipelineConfig.pointLineRendering = true;             // render points or lines

    m_pMyPipeline = std::make_unique<MyPipeline>(
        m_myDevice,
        "shaders/point_curve_shader.vert.spv",
        "shaders/point_curve_shader.frag.spv",
        pipelineConfig);
}

void MyPointCurveRenderFactory::renderControlPoints(
    MyFrameInfo& frameInfo, 
    std::vector<MyGameObject>& gameObjects) 
{
    // TODO: make sure to match the name of your game object
    _renderPointsLines("control_points", frameInfo, gameObjects);
}

void MyPointCurveRenderFactory::renderCenterLine(
    MyFrameInfo& frameInfo,
    std::vector<MyGameObject>& gameObjects)
{
    // TODO: make sure to match the name of your game object
    _renderPointsLines("center_line", frameInfo, gameObjects);
}

void MyPointCurveRenderFactory::renderBezierCurve(
    MyFrameInfo& frameInfo,
    std::vector<MyGameObject>& gameObjects)
{
    // TODO: make sure to match the name of your game object
    _renderPointsLines("bezier_curve", frameInfo, gameObjects);
}

void MyPointCurveRenderFactory::renderNormals(
    MyFrameInfo& frameInfo,
    std::vector<MyGameObject>& gameObjects)
{
    // TODO: make sure to match the name of your game object
    _renderPointsLines("surface_normals", frameInfo, gameObjects);
}

void MyPointCurveRenderFactory::_renderPointsLines(std::string name, MyFrameInfo& frameInfo, std::vector<MyGameObject>& gameObjects)
{
    m_pMyPipeline->bind(frameInfo.commandBuffer);
    auto projectionView = frameInfo.camera.projectionMatrix() * frameInfo.camera.viewMatrix();

    for (auto& obj : gameObjects)
    {
        if (obj.name() == name && obj.model != nullptr)
        {
            MyPointCurvePushConstantData push{};

            auto modelMatrix = obj.transform.mat4();

            // Note: do this for now to perform on CPU
            // We will do it later to perform it on GPU
            push.transform = projectionView * modelMatrix;
            push.push_color = frameInfo.color;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                m_vkPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(MyPointCurvePushConstantData),
                &push);

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }
    }
}

