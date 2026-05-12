#ifndef __MY_APPLICATION_H__
#define __MY_APPLICATION_H__

#include "my_descriptors.h"
#include "my_window.h"
#include "my_device.h"
#include "my_renderer.h"
#include "my_game_object.h"
#include "my_camera.h"
#include "my_bezier_curve_surface.h"

#include <memory>
#include <vector>

class MyApplication
{
public:
    static constexpr int WIDTH  = 1000;
    static constexpr int HEIGHT = 1000;

    MyApplication();
    ~MyApplication();
    void run();

    // ---- HW5 controls (edit mode) ----
    void switchProjectionMatrix();
    void mouseButtonEvent(bool bMouseDown, float posx, float posy);
    void mouseMotionEvent(float posx, float posy);
    void setCameraNavigationMode(MyCamera::MyCameraMode mode);
    void switchEditMode();
    void resetSurface();
    void showHideNormalVectors();
    void createBezierRevolutionSurface();
    void toggleTwist();
    void toggleColorGradient();

    // ---- HW3 controls (scene graph) ----
    void traverseNextNode();   // key N
    void printSceneGraph();    // key P

    // ---- Final project: wind ----
    // GPU vertex deformation — sends windPos into GlobalUBO each frame
    void setWindPos(float nx, float ny);

private:
    void _loadGameObjects();
    void _rebuildSurface();
    void _buildPlantScene();

    // Helper: create one Bezier revolution surface and wrap it in a
    // MySceneGraphNode with the given name.  Returns a shared_ptr to the node.
    std::shared_ptr<MySceneGraphNode> _makePlantNode(
        const std::string& name,
        const std::vector<glm::vec2>& controlPoints,
        glm::vec3 color,
        int xRes, int rRes, bool dynamic = false);

    // Flat parametric leaf mesh (position.x = axial 0→1, position.z = ±halfWidth)
    std::shared_ptr<MySceneGraphNode> _makeFlatLeafNode(
        const std::string& name,
        const std::vector<glm::vec2>& widthCP,
        glm::vec3 color, float camber, float bendAmount,
        int uRes, int vRes);

    // ---- Vulkan / window objects ----
    MyWindow   m_myWindow{ WIDTH, HEIGHT, "Final Project – Interactive Plant Modeler" };
    MyDevice   m_myDevice{ m_myWindow };
    MyRenderer m_myRenderer{ m_myWindow, m_myDevice };

    std::unique_ptr<MyDescriptorPool> m_pMyGlobalPool{};
    MyCamera                          m_myCamera{};

    // ---- HW5 edit mode state ----
    bool m_bPerspectiveProjection = true;
    bool m_bMouseButtonPress      = false;
    bool m_bCreateModel           = false;   // true = edit mode
    bool m_bShowNormals           = false;
    bool m_bSurfaceExists         = false;
    int  m_iXResolution           = 50;
    int  m_iRResolution           = 50;

    std::vector<MyModel::PointCurve> m_vControlPointVertices;
    std::vector<MyModel::PointCurve> m_vNormalVectors;
    std::shared_ptr<MyBezier>        m_pMyBezier;

    // HW5 flat game-object list (edit-mode overlays only)
    std::vector<MyGameObject> m_vMyGameObjects;

    // ---- HW3 scene graph (plant scene) ----
    std::shared_ptr<MySceneGraphNode> m_pSceneRoot;    // plant_root
    MySceneGraphNode*                 m_pCurrentNode = nullptr;

    // Pointers into the plant hierarchy for per-frame wind updates
    std::shared_ptr<MySceneGraphNode> m_pLeavesGroup;
    std::shared_ptr<MySceneGraphNode> m_pLeaf1;
    std::shared_ptr<MySceneGraphNode> m_pLeaf2;
    std::shared_ptr<MySceneGraphNode> m_pLeaf3;

    // ---- Final project: wind ----
    glm::vec2 m_windPos{ 0.0f, 0.0f }; // GPU vertex deformation vector (scaled)

    // Live-built pot (edit mode → scene graph on navigation return)
    std::shared_ptr<MySceneGraphNode> m_pBuiltPotNode;
    std::shared_ptr<MyModel>          m_pBuiltPotModel;

    // ---- Phase 6: Bezier control point animation ----
    std::vector<glm::vec2> m_leafCP;
    glm::vec3 m_leaf1Color{ 0.22f, 0.78f, 0.15f };
    glm::vec3 m_leaf2Color{ 0.28f, 0.82f, 0.12f };
    glm::vec3 m_leaf3Color{ 0.18f, 0.72f, 0.20f };
    float     m_fElapsedTime = 0.0f;
    void _animateLeaves(float dt);

    // ---- Phase 7: Texture Mapping ----
    VkImage        m_potTexImage    = VK_NULL_HANDLE;
    VkDeviceMemory m_potTexMemory   = VK_NULL_HANDLE;
    VkImageView    m_potTexView     = VK_NULL_HANDLE;
    VkSampler      m_potTexSampler  = VK_NULL_HANDLE;

    void _createTexture(const std::string& path,
                        VkImage& image, VkDeviceMemory& memory,
                        VkImageView& view, VkSampler& sampler);
    void _transitionImageLayout(VkImage image,
                                VkImageLayout oldLayout, VkImageLayout newLayout);
    void _copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h);
};

#endif
