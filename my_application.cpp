#include "my_application.h"
#include <GLFW/glfw3.h>

#include "my_simple_render_factory.h"
#include "my_point_curve_render_factory.h"
#include "my_keyboard_controller.h"

#define STB_IMAGE_IMPLEMENTATION   
#include "stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <stdexcept>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <cmath>

// ============================================================
// buildFlatLeafMesh — single-layer parametric flat leaf
//   position.x ∈ [0,1]  = axial (root→tip)
//   position.z = v * halfWidth(u)  (lateral, -halfW to +halfW)
//   position.y = camber*(1-v²)     (arch: high at midrib, zero at edges)
// VK_CULL_MODE_NONE renders both sides; normals point generally +Y.
// Reversed winding so the computed face normal points UP.
// ============================================================
static void buildFlatLeafMesh(
    const std::vector<glm::vec2>& widthCP,
    float camber, float bendAmount,
    glm::vec3 color,
    int uRes, int vRes,
    std::vector<MyModel::Vertex>& verts,
    std::vector<uint32_t>& idx)
{
    verts.clear();
    idx.clear();

    // De Casteljau: evaluate the width profile Bezier at t
    auto evalHalfWidth = [&](float t) -> float {
        std::vector<float> p;
        for (auto& cp : widthCP) p.push_back(cp.y);
        int n = (int)p.size();
        for (int k = 1; k < n; k++)
            for (int i = 0; i < n - k; i++)
                p[i] = (1.0f - t) * p[i] + t * p[i + 1];
        return std::max(0.0f, p[0]);
    };

    for (int ui = 0; ui <= uRes; ui++)
    {
        float u     = (float)ui / uRes;
        float halfW = evalHalfWidth(u);

        for (int vi = 0; vi <= vRes; vi++)
        {
            float v    = -1.0f + 2.0f * vi / vRes;
            float posX = u;
            float posZ = v * halfW;
            // camber: midrib arch (high at center, zero at edges)
            // bendAmount*u²: cantilever droop (0 at root, full at tip)
            float posY = camber * (1.0f - v * v) + bendAmount * u * u;

            // Normal: negate of cross(tangentU, tangentV)
            // tangentU ≈ (1, 2*bendAmount*u, 0)
            // tangentV ≈ (0, -2*camber*v, halfW)
            // cross → (-2*bendAmount*u*halfW, halfW, 2*camber*v) after negation
            glm::vec3 n = (halfW > 0.001f)
                ? glm::normalize(glm::vec3(-2.0f * bendAmount * u * halfW,
                                           halfW,
                                           2.0f * camber * v))
                : glm::normalize(glm::vec3(-2.0f * bendAmount * u, 1.0f, 0.0f));

            MyModel::Vertex vert{};
            vert.position = { posX, posY, posZ };
            vert.color    = color;
            vert.normal   = n;
            vert.uv       = { u, (v + 1.0f) * 0.5f };
            verts.push_back(vert);
        }
    }

    // Reversed winding so that face normals point +Y (upward)
    for (int ui = 0; ui < uRes; ui++)
    {
        for (int vi = 0; vi < vRes; vi++)
        {
            uint32_t i00 = ui       * (vRes + 1) + vi;
            uint32_t i10 = (ui + 1) * (vRes + 1) + vi;
            uint32_t i01 = ui       * (vRes + 1) + vi + 1;
            uint32_t i11 = (ui + 1) * (vRes + 1) + vi + 1;

            idx.push_back(i00); idx.push_back(i11); idx.push_back(i10);
            idx.push_back(i00); idx.push_back(i01); idx.push_back(i11);
        }
    }
}

// ============================================================
// Global UBO  (HW5 — lighting data sent to shaders each frame)
// ============================================================
struct MyGlobalUBO
{
    alignas(16) glm::mat4 projectionView{ 1.0f };
    alignas(16) glm::vec4 ambientLightColor{ 1.0f, 1.0f, 1.0f, 0.06f };
    alignas(16) glm::vec3 lightPosition{ 2.0f, 3.0f, 2.0f };
    alignas(16) glm::vec4 lightColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    alignas( 8) glm::vec2 windPos{ 0.0f, 0.0f };  // Phase 5: GPU vertex deformation
};

// ============================================================
// Phase 7: Texture helpers
// ============================================================

void MyApplication::_transitionImageLayout(VkImage image,
                                            VkImageLayout oldLayout,
                                            VkImageLayout newLayout)
{
    VkCommandBuffer cmd = m_myDevice.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
    m_myDevice.endSingleTimeCommands(cmd);
}

void MyApplication::_copyBufferToImage(VkBuffer buffer, VkImage image,
                                        uint32_t w, uint32_t h)
{
    VkCommandBuffer cmd = m_myDevice.beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent      = { w, h, 1 };

    vkCmdCopyBufferToImage(cmd, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    m_myDevice.endSingleTimeCommands(cmd);
}

void MyApplication::_createTexture(const std::string& path,
                                    VkImage& image, VkDeviceMemory& memory,
                                    VkImageView& view, VkSampler& sampler)
{
    int w, h, ch;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("Failed to load texture: " + path);
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w * h * 4);

    MyBuffer staging{ m_myDevice, imageSize, 1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
    staging.map();
    staging.writeToBuffer(pixels);
    stbi_image_free(pixels);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = { (uint32_t)w, (uint32_t)h, 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    m_myDevice.createImageWithInfo(imageInfo,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   image, memory);

    _transitionImageLayout(image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    _copyBufferToImage(staging.buffer(), image, (uint32_t)w, (uint32_t)h);
    _transitionImageLayout(image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image            = image;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(m_myDevice.device(), &viewInfo, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;
    samplerInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable    = VK_FALSE;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (vkCreateSampler(m_myDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler");

    std::cout << "Texture loaded: " << path << " (" << w << "x" << h << ")\n";
}

// ============================================================
// Destructor — release Phase 7 texture resources
// ============================================================
MyApplication::~MyApplication()
{
    VkDevice dev = m_myDevice.device();
    if (m_potTexSampler)  vkDestroySampler(dev, m_potTexSampler, nullptr);
    if (m_potTexView)     vkDestroyImageView(dev, m_potTexView, nullptr);
    if (m_potTexImage)    vkDestroyImage(dev, m_potTexImage, nullptr);
    if (m_potTexMemory)   vkFreeMemory(dev, m_potTexMemory, nullptr);
}

// ============================================================
// Constructor
// ============================================================
MyApplication::MyApplication() : m_bPerspectiveProjection(true)
{
    m_pMyGlobalPool =
        MyDescriptorPool::Builder(m_myDevice)
        .setMaxSets(MySwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MySwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MySwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();

    _createTexture("textures/pot_texture.png",
                   m_potTexImage, m_potTexMemory, m_potTexView, m_potTexSampler);

    _loadGameObjects();

    // Default view: navigation mode, plant centered with comfortable framing.
    // Plant spans ~-0.85 to +0.9 in world Y, ~±1.0 in world X — pull camera back to z=-5.
    m_myCamera.setViewTarget(glm::vec3(0.f, 0.0f, -5.0f), glm::vec3(0.f, 0.0f, 0.0f));
}

// ============================================================
// Main render loop
// ============================================================
void MyApplication::run()
{
    // --- UBO buffers (one per frame-in-flight) ---
    std::vector<std::unique_ptr<MyBuffer>> uboBuffers(MySwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < (int)uboBuffers.size(); i++)
    {
        uboBuffers[i] = std::make_unique<MyBuffer>(
            m_myDevice, sizeof(MyGlobalUBO), 1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[i]->map();
    }

    auto globalSetLayout =
        MyDescriptorSetLayout::Builder(m_myDevice)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS)
        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build();

    VkDescriptorImageInfo potImgInfo{};
    potImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    potImgInfo.imageView   = m_potTexView;
    potImgInfo.sampler     = m_potTexSampler;

    std::vector<VkDescriptorSet> globalDescriptorSets(MySwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < (int)globalDescriptorSets.size(); i++)
    {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        MyDescriptorWriter(*globalSetLayout, *m_pMyGlobalPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &potImgInfo)
            .build(globalDescriptorSets[i]);
    }

    // --- Render factories ---
    MySimpleRenderFactory     simpleRF { m_myDevice, m_myRenderer.swapChainRenderPass(), globalSetLayout->descriptorSetLayout() };
    MyPointCurveRenderFactory pointRF  { m_myDevice, m_myRenderer.swapChainRenderPass(), VK_PRIMITIVE_TOPOLOGY_POINT_LIST  };
    MyPointCurveRenderFactory lineRF   { m_myDevice, m_myRenderer.swapChainRenderPass(), VK_PRIMITIVE_TOPOLOGY_LINE_STRIP  };
    MyPointCurveRenderFactory normalRF { m_myDevice, m_myRenderer.swapChainRenderPass(), VK_PRIMITIVE_TOPOLOGY_LINE_LIST   };

    m_myWindow.bindMyApplication(this);

    auto currentTime = std::chrono::high_resolution_clock::now();

    std::cout << "\n=== Final Project – Plant Modeler ===\n";
    std::cout << "SPACE       Toggle Edit / Navigation mode\n";
    std::cout << "N           Select next scene graph node\n";
    std::cout << "P           Print scene graph\n";
    std::cout << "W + mouse   Set wind direction (leaves bend)\n";
    std::cout << "R/P/Z/F     Camera rotate / pan / zoom / fit-all\n";
    std::cout << "B           Build Bezier surface (edit mode)\n";
    std::cout << "M           Reset edit surface\n";
    std::cout << "Shift+N     Show/hide surface normals\n";
    std::cout << "X           Toggle twist\n";
    std::cout << "G           Toggle color gradient\n";
    std::cout << "C           Toggle perspective / orthographic\n";
    std::cout << "ESC         Quit\n";
    std::cout << "=====================================\n\n";

    while (!m_myWindow.shouldClose())
    {
        m_myWindow.pollEvents();

        auto  newTime   = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime     = newTime;

        // --- Keyboard camera navigation (navigation mode only) ---
        if (!m_bCreateModel)
        {
            GLFWwindow* win = m_myWindow.glfwWindow();
            const float rotSpeed  = frameTime * 1.2f;
            const float zoomSpeed = frameTime * 0.8f;
            const float panSpeed  = frameTime * 0.4f;
            bool shiftHeld = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
                             glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            float rotH=0, rotV=0, zoom=0, panH=0, panV=0;
            bool L = glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_4) == GLFW_PRESS;
            bool R = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_6) == GLFW_PRESS;
            bool U = glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_8) == GLFW_PRESS;
            bool D = glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_2) == GLFW_PRESS;
            bool ZI = glfwGetKey(win, GLFW_KEY_EQUAL)    == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_ADD)      == GLFW_PRESS;
            bool ZO = glfwGetKey(win, GLFW_KEY_MINUS)    == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
            if (shiftHeld) { if(L) panH-=panSpeed; if(R) panH+=panSpeed; if(U) panV+=panSpeed; if(D) panV-=panSpeed; }
            else           { if(L) rotH-=rotSpeed; if(R) rotH+=rotSpeed; if(U) rotV+=rotSpeed; if(D) rotV-=rotSpeed; }
            if (ZI) zoom -= zoomSpeed;
            if (ZO) zoom += zoomSpeed;
            if (rotH||rotV||zoom||panH||panV)
                m_myCamera.keyStep(rotH, rotV, zoom, panH, panV);
        }

        // Phase 6: animate leaf Bezier control points every frame (navigation mode only)
        if (!m_bCreateModel && m_pSceneRoot)
            _animateLeaves(frameTime);

        // Wind bending is handled entirely by GPU vertex deformation (windPos in GlobalUBO).
        // Scene-graph rotation on leavesGroup is not used — rigid-body rotation causes
        // a seesaw artifact because leaves extend from the pivot in different directions.

        // --- Selected scene graph node spins (HW3 creativity — visual indicator) ---
        if (m_pCurrentNode)
            m_pCurrentNode->transform.rotation.y = glm::mod(
                m_pCurrentNode->transform.rotation.y + 0.015f, glm::two_pi<float>());

        // --- Camera projection ---
        float ar = m_myRenderer.aspectRatio();
        if (m_bPerspectiveProjection)
            m_myCamera.setPerspectiveProjection(glm::radians(50.f), ar, 0.1f, 100.f);
        else
            m_myCamera.setOrthographicProjection(-ar*2.f, ar*2.f, -2.0f, 2.0f, -100.0f, 100.0f);

        if (auto cb = m_myRenderer.beginFrame())
        {
            int fi = m_myRenderer.frameIndex();
            MyFrameInfo frameInfo{ fi, frameTime, cb, m_myCamera,
                                   globalDescriptorSets[fi], glm::vec3(1.f) };

            MyGlobalUBO ubo{};
            ubo.projectionView = m_myCamera.projectionMatrix() * m_myCamera.viewMatrix();
            ubo.windPos        = m_windPos;
            uboBuffers[fi]->writeToBuffer(&ubo);
            uboBuffers[fi]->flush();

            m_myRenderer.beginSwapChainRenderPass(cb);

            // 1. Plant scene (scene graph traversal — HW3 pattern)
            if (m_pSceneRoot)
                simpleRF.renderSceneGraph(frameInfo, m_pSceneRoot);

            // 2 & 3. HW5 overlays + surface preview — edit mode only
            // Navigation mode: built pot is rendered via renderSceneGraph (scene graph node)
            if (m_bCreateModel)
            {
                simpleRF.renderGameObjects(frameInfo, m_vMyGameObjects);

                frameInfo.color = glm::vec3(1.0f, 0.0f, 0.0f);
                pointRF.renderControlPoints(frameInfo, m_vMyGameObjects);
                frameInfo.color = glm::vec3(1.0f, 1.0f, 0.0f);
                lineRF.renderControlPoints(frameInfo, m_vMyGameObjects);
                frameInfo.color = glm::vec3(0.0f, 0.0f, 1.0f);
                lineRF.renderCenterLine(frameInfo, m_vMyGameObjects);
                frameInfo.color = glm::vec3(1.0f, 1.0f, 1.0f);
                lineRF.renderBezierCurve(frameInfo, m_vMyGameObjects);
                if (m_bShowNormals)
                {
                    frameInfo.color = glm::vec3(0.0f, 1.0f, 0.0f);
                    normalRF.renderNormals(frameInfo, m_vMyGameObjects);
                }
            }

            m_myRenderer.endSwapChainRenderPass(cb);
            m_myRenderer.endFrame();
        }
    }
    vkDeviceWaitIdle(m_myDevice.device());
}

// ============================================================
// _makePlantNode: build one Bezier revolution surface and return
// it wrapped in a scene graph leaf node.
// ============================================================
std::shared_ptr<MySceneGraphNode> MyApplication::_makePlantNode(
    const std::string& name,
    const std::vector<glm::vec2>& controlPoints,
    glm::vec3 color,
    int xRes, int rRes, bool dynamic /*= false*/)
{
    auto bezier = std::make_shared<MyBezier>();
    bezier->setColorGradient(false);
    for (auto& cp : controlPoints)
        bezier->addControlPoint(cp.x, cp.y);
    bezier->createRevolutionSurface(xRes, rRes);

    // Tint vertices with the target color
    for (auto& v : bezier->m_vSurface)
        v.color = color;

    static int nodeIDCounter = 100;
    auto node = std::make_shared<MySceneGraphNode>(name, nodeIDCounter++);
    if (dynamic)
    {
        node->model = std::make_shared<MyModel>(
            m_myDevice, bezier->m_vSurface, bezier->m_vIndices, true);
    }
    else
    {
        MyModel::Builder builder;
        builder.vertices = bezier->m_vSurface;
        builder.indices  = bezier->m_vIndices;
        node->model = std::make_shared<MyModel>(m_myDevice, builder);
    }
    return node;
}

// ============================================================
// _makeFlatLeafNode: build a flat leaf mesh and wrap in a scene graph node.
// ============================================================
std::shared_ptr<MySceneGraphNode> MyApplication::_makeFlatLeafNode(
    const std::string& name,
    const std::vector<glm::vec2>& widthCP,
    glm::vec3 color, float camber, float bendAmount,
    int uRes, int vRes)
{
    std::vector<MyModel::Vertex> verts;
    std::vector<uint32_t> idx;
    buildFlatLeafMesh(widthCP, camber, bendAmount, color, uRes, vRes, verts, idx);

    static int leafNodeID = 300;
    auto node = std::make_shared<MySceneGraphNode>(name, leafNodeID++);
    node->model = std::make_shared<MyModel>(m_myDevice, verts, idx, true);
    return node;
}

// ============================================================
// _buildPlantScene  — builds the scene graph hierarchy
//
//   plant_root (group)
//   ├── pot   (leaf)
//   ├── stem  (leaf)
//   └── leaves_group (group)
//       ├── leaf1 (leaf)
//       ├── leaf2 (leaf, rotated 120° around Y)
//       └── leaf3 (leaf, rotated 240° around Y)
//
// Wind updates leaf1/2/3 rotation.z each frame.
// ============================================================
void MyApplication::_buildPlantScene()
{
    static int gID = 10;

    m_pSceneRoot = std::make_shared<MySceneGraphNode>("plant_root", gID++);
    m_pSceneRoot->transform.translation = glm::vec3(0.0f, -0.5f, 0.0f);
    m_pSceneRoot->transform.scale       = glm::vec3(0.80f);

    // ---- Pot: classic terracotta flower-pot profile ----
    // X = axial (left = rim/top after -90° Z rotation, right = base/bottom)
    // Y = radius at each height
    // Shape: wide flared rim → necks in slightly → tapers to narrow base
    std::vector<glm::vec2> potCP = {
        {-0.50f, 0.70f},  // top rim: wide
        {-0.38f, 0.60f},  // neck just below rim
        { 0.00f, 0.52f},  // mid body
        { 0.35f, 0.42f},  // lower body
        { 0.50f, 0.36f},  // base: narrowest
        { 0.50f, 0.36f},  // clamped tangent (flat base)
    };
    auto pot = _makePlantNode("pot", potCP, glm::vec3(0.78f, 0.42f, 0.22f), 60, 48);
    pot->transform.translation = glm::vec3(0.0f, 0.0f, 0.0f);
    pot->transform.rotation    = glm::vec3(0.0f, 0.0f, -glm::half_pi<float>());

    // ---- Stem: gently tapering green cylinder ----
    std::vector<glm::vec2> stemCP = {
        {0.0f,  0.0375f},
        {0.4f,  0.034f},
        {0.9f,  0.030f},
        {1.4f,  0.025f},
        {1.8f,  0.020f},
    };
    auto stem = _makePlantNode("stem", stemCP, glm::vec3(0.14f, 0.54f, 0.10f), 30, 24);
    stem->transform.rotation    = glm::vec3(0.0f, 0.0f, glm::half_pi<float>());
    stem->transform.translation = glm::vec3(0.0f, 0.40f, 0.0f);

    // ---- Leaves group — at stem tip ----
    // Stem tip in plant_root space = translation.y + stemLength = 0.40 + 1.8 = 2.20
    m_pLeavesGroup = std::make_shared<MySceneGraphNode>("leaves_group", gID++);
    auto& leavesGroup = m_pLeavesGroup;
    leavesGroup->transform.translation = glm::vec3(0.0f, 2.20f, 0.0f);

    // Leaf width profile: narrow at root and tip, widest at ~38% along
    std::vector<glm::vec2> leafCP = {
        {0.0f,  0.00f},
        {0.10f, 0.22f},
        {0.38f, 0.30f},
        {0.68f, 0.18f},
        {1.0f,  0.00f},
    };
    m_leafCP = leafCP;

    constexpr float kLeafCamber =  0.07f;  // midrib arch
    constexpr float kLeafBend   = -0.08f;  // initial downward droop

    m_pLeaf1 = _makeFlatLeafNode("leaf1", leafCP, m_leaf1Color, kLeafCamber, kLeafBend, 40, 18);
    m_pLeaf1->transform.scale    = glm::vec3(1.28f);
    m_pLeaf1->transform.rotation = glm::vec3(0.0f, glm::radians(  0.0f), glm::radians(22.0f));

    m_pLeaf2 = _makeFlatLeafNode("leaf2", leafCP, m_leaf2Color, kLeafCamber, kLeafBend, 40, 18);
    m_pLeaf2->transform.scale    = glm::vec3(1.16f);
    m_pLeaf2->transform.rotation = glm::vec3(0.0f, glm::radians(120.0f), glm::radians(22.0f));

    m_pLeaf3 = _makeFlatLeafNode("leaf3", leafCP, m_leaf3Color, kLeafCamber, kLeafBend, 40, 18);
    m_pLeaf3->transform.scale    = glm::vec3(1.05f);
    m_pLeaf3->transform.rotation = glm::vec3(0.0f, glm::radians(240.0f), glm::radians(22.0f));

    leavesGroup->addChild(m_pLeaf1);
    leavesGroup->addChild(m_pLeaf2);
    leavesGroup->addChild(m_pLeaf3);

    // pot is NOT added at startup — user live-builds it in edit mode (press SPACE → click → B)
    // Pre-register the live-built pot node (null model until user builds in edit mode)
    m_pBuiltPotNode = std::make_shared<MySceneGraphNode>("built_pot", gID++);
    m_pBuiltPotNode->transform.scale       = glm::vec3(1.0f / 0.7f);
    m_pBuiltPotNode->transform.translation = glm::vec3(0.0f, 0.5f / 0.7f, 0.0f);
    // m_pBuiltPotNode->transform.rotation    = glm::vec3(0.0f, 0.0f, glm::half_pi<float>());
    // [수정] 이미 메쉬가 서 있으므로 90도 회전(half_pi)을 0으로 바꿉니다.
    m_pBuiltPotNode->transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
    m_pSceneRoot->addChild(m_pBuiltPotNode);

    m_pSceneRoot->addChild(stem);
    m_pSceneRoot->addChild(leavesGroup);

    std::cout << "Plant scene built.\n";
    m_pSceneRoot->printSceneGraph();
}

// ============================================================
// _loadGameObjects — HW5 edit-mode overlay objects
// ============================================================
void MyApplication::_loadGameObjects()
{
    m_pMyBezier = std::make_shared<MyBezier>();

    glm::vec3 sceneMin{-2.f,-2.f,-2.f}, sceneMax{2.f,2.f,2.f};
    m_myCamera.setSceneMinMax(sceneMin, sceneMax);

    auto cpModel = std::make_shared<MyModel>(m_myDevice, 100);
    auto go_cp   = MyGameObject::createGameObject("control_points");
    go_cp.model  = cpModel;
    go_cp.color  = glm::vec3(1,0,0);
    m_vMyGameObjects.push_back(std::move(go_cp));

    auto clModel = std::make_shared<MyModel>(m_myDevice, 2);
    auto go_cl   = MyGameObject::createGameObject("center_line");
    go_cl.model  = clModel;
    go_cl.color  = glm::vec3(0,0,1);
    std::vector<MyModel::PointCurve> cl(2);
    cl[0].position = glm::vec3(0,-2,0); cl[0].color = glm::vec3(0,0,1);
    cl[1].position = glm::vec3(0, 2,0); cl[1].color = glm::vec3(0,0,1);
    clModel->updatePointCurves(cl);
    m_vMyGameObjects.push_back(std::move(go_cl));

    auto bcModel = std::make_shared<MyModel>(m_myDevice, 1000);
    auto go_bc   = MyGameObject::createGameObject("bezier_curve");
    go_bc.model  = bcModel;
    go_bc.color  = glm::vec3(1,1,1);
    m_vMyGameObjects.push_back(std::move(go_bc));

    auto go_bs = MyGameObject::createGameObject("bezier_surface");
    go_bs.color = glm::vec3(1,1,1);
    m_vMyGameObjects.push_back(std::move(go_bs));

    auto go_sn = MyGameObject::createGameObject("surface_normals");
    go_sn.color = glm::vec3(0,1,0);
    m_vMyGameObjects.push_back(std::move(go_sn));

    // Build the plant scene graph
    _buildPlantScene();
}

// ============================================================
// Wind direction
// ============================================================

void MyApplication::setWindPos(float nx, float ny)
{
    constexpr float kWindScale = 0.5f;
    m_windPos = glm::vec2(nx, ny) * kWindScale;
}

// ============================================================
// Phase 6: per-frame Bezier control point animation (flat leaf)
// Animates the widest control point (flutter) and leaf camber (arch sway).
// Wind adds extra arch so leaves droop more under wind load.
// ============================================================
void MyApplication::_animateLeaves(float dt)
{
    m_fElapsedTime += dt;
    float windStrength = glm::length(m_windPos);

    // Each leaf gets independent phase → camber, bend, and width all differ
    auto rebuildLeaf = [&](std::shared_ptr<MySceneGraphNode>& leaf,
                           glm::vec3 color, float phase)
    {
        if (!leaf || !leaf->model) return;

        float camber = 0.07f
                     + sinf(m_fElapsedTime * 0.9f + phase) * 0.020f
                     + windStrength * 0.05f;
        camber = std::max(0.005f, camber);

        float bend = -0.08f
                   + sinf(m_fElapsedTime * 1.3f + phase) * 0.09f
                   - windStrength * 0.12f;

        // Width also breathes subtly — widest CP (index 2)
        auto animCP = m_leafCP;
        if ((int)animCP.size() >= 3)
            animCP[2].y = std::max(0.005f,
                m_leafCP[2].y + sinf(m_fElapsedTime * 0.7f + phase) * 0.04f);

        std::vector<MyModel::Vertex> verts;
        std::vector<uint32_t> idxDiscard;
        buildFlatLeafMesh(animCP, camber, bend, color, 40, 18, verts, idxDiscard);
        leaf->model->updateSurface(verts);
    };

    rebuildLeaf(m_pLeaf1, m_leaf1Color, 0.0f);
    rebuildLeaf(m_pLeaf2, m_leaf2Color, 2.094f);
    rebuildLeaf(m_pLeaf3, m_leaf3Color, 4.189f);
}

// ============================================================
// HW5 camera / mode controls  (unchanged from HW5)
// ============================================================
void MyApplication::switchProjectionMatrix()
{
    m_bPerspectiveProjection = !m_bPerspectiveProjection;
}

void MyApplication::mouseButtonEvent(bool bMouseDown, float posx, float posy)
{
    if (m_bCreateModel && bMouseDown)
    {
        if (posx <= 0.5f) return;                      // left of axis rejected
        float visual_x =  4.0f * posx - 2.0f;         // world X at click (= radius)
        float visual_y = -4.0f * posy + 2.0f;         // world Y at click (= pot height/axial)
        std::cout << "Control point visual(" << visual_x << ", " << visual_y << ")\n";

        // Bezier gets (axial = vertical pos, radius = horizontal dist from axis)
        m_pMyBezier->addControlPoint(visual_y, visual_x);

        // Point renders at the exact click position
        MyModel::PointCurve cp;
        cp.position = glm::vec3(visual_x, visual_y, 0);
        cp.color    = glm::vec3(1,0,1);
        m_vControlPointVertices.push_back(cp);
        if (m_vControlPointVertices.size() >= 100) return;
        if (m_vControlPointVertices.size() >= 2)
            m_pMyBezier->createBezierCurve(200);

        for (auto& obj : m_vMyGameObjects)
        {
            if (obj.name() == "control_points")
            {
                obj.model->updatePointCurves(m_vControlPointVertices);
            }
            else if (obj.name() == "bezier_curve")
            {
                // Bezier curve is in (axial, radius) space — swap to visual (radius, axial)
                auto visualCurve = m_pMyBezier->m_vCurve;
                for (auto& pt : visualCurve)
                {
                    float a = pt.position.x;  // axial
                    float r = pt.position.y;  // radius
                    pt.position.x = r;
                    pt.position.y = a;
                }
                obj.model->updatePointCurves(visualCurve);
            }
        }
    }
    else
    {
        m_bMouseButtonPress = bMouseDown;
        m_myCamera.setButton(m_bMouseButtonPress, posx, posy);
    }
}

void MyApplication::mouseMotionEvent(float posx, float posy)
{
    if (!m_bCreateModel)
        m_myCamera.setMotion(m_bMouseButtonPress, posx, posy);
}

void MyApplication::setCameraNavigationMode(MyCamera::MyCameraMode mode)
{
    m_myCamera.setMode(mode);
}

void MyApplication::switchEditMode()
{
    m_bCreateModel           = !m_bCreateModel;
    m_bPerspectiveProjection = !m_bCreateModel;
    if (m_bPerspectiveProjection)
    {
        std::cout << "Navigation mode [R=rotate P=pan Z=zoom F=fit-all W+mouse=wind]\n";
        // Sync built pot into scene graph so it moves with the plant
        if (m_pBuiltPotNode) m_pBuiltPotNode->model = m_pBuiltPotModel;
        setCameraNavigationMode(MyCamera::MYCAMERA_FITALL);
    }
    else
    {
        std::cout << "Edit mode [click RIGHT of vertical line: up=rim, down=base, right=wider]\n";
        // Hide from scene graph while editing (rendered via game object preview instead)
        if (m_pBuiltPotNode) m_pBuiltPotNode->model = nullptr;
        m_myCamera.setViewYXZ(glm::vec3(0,0,10), glm::vec3(0));
    }
}

void MyApplication::resetSurface()
{
    m_myDevice.waitIdle();
    m_pMyBezier = std::make_shared<MyBezier>();
    m_vControlPointVertices.clear();
    m_vNormalVectors.clear();
    m_bSurfaceExists  = false;
    m_pBuiltPotModel  = nullptr;
    if (m_pBuiltPotNode) m_pBuiltPotNode->model = nullptr;
    std::vector<MyModel::PointCurve> empty;
    for (auto& obj : m_vMyGameObjects)
    {
        if (obj.name() == "control_points" || obj.name() == "bezier_curve")
            obj.model->updatePointCurves(empty);
        else if (obj.name() == "bezier_surface" || obj.name() == "surface_normals")
            obj.model = nullptr;
    }
    std::cout << "Surface reset.\n";
}

void MyApplication::showHideNormalVectors()
{
    m_bShowNormals = !m_bShowNormals;
    std::cout << (m_bShowNormals ? "Show" : "Hide") << " normals.\n";
}

void MyApplication::createBezierRevolutionSurface()
{
    if (m_pMyBezier->numberOfControlPoints() < 2)
    { std::cout << "Need at least 2 control points.\n"; return; }

    m_vNormalVectors.clear();
    m_pMyBezier->createRevolutionSurface(m_iXResolution, m_iRResolution);

    // [중요 수정] 생성된 모든 정점의 X(축)와 Y(반지름)를 스왑하여 
    // X축 회전체를 Y축 회전체로 변환합니다.
    for (auto& v : m_pMyBezier->m_vSurface)
    {
        float oldAxial = v.position.x; // 원래 X축 방향 길이
        float oldRadiusX = v.position.y; // 원래 반지름 Cos 성분
        
        // 좌표 스왑: 높이를 Y로, 반지름을 XZ 평면으로 보냅니다.
        v.position.x = oldRadiusX;   // 반지름 성분이 X로
        v.position.y = oldAxial;     // 높이(Axial) 성분이 Y로 (서 있게 됨)
        
        // 법선(Normal)도 똑같이 스왑해줘야 조명이 제대로 나옵니다.
        float oldNX = v.normal.x;
        float oldNY = v.normal.y;
        v.normal.x = oldNY;
        v.normal.y = oldNX;
    }

    MyModel::Builder builder;
    builder.indices  = m_pMyBezier->m_vIndices;
    builder.vertices = m_pMyBezier->m_vSurface;
    auto surf = std::make_shared<MyModel>(m_myDevice, builder);

    int nv = (int)m_pMyBezier->m_vSurface.size();
    m_vNormalVectors.resize(nv * 2);
    const float ns = 0.05f;
    for (int i = 0; i < nv; i++)
    {
        const auto& sv = m_pMyBezier->m_vSurface[i];
        m_vNormalVectors[i*2+0].position = sv.position;
        m_vNormalVectors[i*2+0].color    = glm::vec3(0,1,0);
        m_vNormalVectors[i*2+1].position = sv.position + sv.normal * ns;
        m_vNormalVectors[i*2+1].color    = glm::vec3(0,1,0);
    }
    auto normModel = std::make_shared<MyModel>(m_myDevice, nv*2);
    normModel->updatePointCurves(m_vNormalVectors);

    for (auto& obj : m_vMyGameObjects)
    {
        if (obj.name() == "bezier_surface") obj.model = surf;
        else if (obj.name() == "surface_normals") obj.model = normModel;
    }
    m_bSurfaceExists  = true;
    m_pBuiltPotModel  = surf;   // kept for scene graph sync on nav-mode return

    glm::vec3 bmin( std::numeric_limits<float>::max());
    glm::vec3 bmax(-std::numeric_limits<float>::max());
    for (const auto& v : m_pMyBezier->m_vSurface)
    { bmin = glm::min(bmin,v.position); bmax = glm::max(bmax,v.position); }
    glm::vec3 pad = (bmax-bmin)*0.1f + glm::vec3(0.01f);
    m_myCamera.setSceneMinMax(bmin-pad, bmax+pad);

    std::cout << "Surface built: " << nv << " vertices.\n";
}

void MyApplication::toggleTwist()
{
    float next = (m_pMyBezier->getTwistAngle() < 0.01f) ? glm::two_pi<float>() : 0.0f;
    m_pMyBezier->setTwistAngle(next);
    std::cout << "Twist " << (next > 0 ? "ON" : "OFF") << "\n";
    if (m_bSurfaceExists) createBezierRevolutionSurface();
}

void MyApplication::toggleColorGradient()
{
    bool next = !m_pMyBezier->getColorGradient();
    m_pMyBezier->setColorGradient(next);
    std::cout << "Color gradient " << (next ? "ON" : "OFF") << "\n";
    if (m_bSurfaceExists) createBezierRevolutionSurface();
}

void MyApplication::_rebuildSurface()
{
    if (m_bSurfaceExists) createBezierRevolutionSurface();
}

// ============================================================
// HW3 scene graph controls
// ============================================================
void MyApplication::traverseNextNode()
{
    if (!m_pSceneRoot) return;
    m_pCurrentNode = m_pSceneRoot->traverseNext(m_pCurrentNode);
    std::cout << "Selected node: " << m_pCurrentNode->getName() << "\n";
    m_pSceneRoot->printSceneGraph();
}

void MyApplication::printSceneGraph()
{
    if (m_pSceneRoot)
    {
        std::cout << "\n--- Scene Graph ---\n";
        m_pSceneRoot->printSceneGraph();
        std::cout << "-------------------\n";
    }
}



