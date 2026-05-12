#ifndef __MY_GAMEOBJECT_H__
#define __MY_GAMEOBJECT_H__

#include "my_model.h"
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <vector>
#include <string>

// ============================================================
// TransformComponent  (unchanged from HW5)
// ============================================================
struct TransformComponent
{
    glm::vec3 translation{ 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3 rotation{ 0.0f, 0.0f, 0.0f };

    glm::mat4 mat4();
    glm::mat3 normalMatrix();
};

// ============================================================
// MyGameObject  (HW5 base — unchanged)
// ============================================================
class MyGameObject
{
public:
    using id_t = unsigned int;

    static MyGameObject createGameObject(std::string name)
    {
        static id_t currentID = 0;
        return MyGameObject{ currentID++, name };
    }

    MyGameObject(const MyGameObject&) = delete;
    MyGameObject& operator=(const MyGameObject&) = delete;
    MyGameObject(MyGameObject&&) = default;
    MyGameObject& operator=(MyGameObject&&) = default;

    id_t                     getID()   const { return m_iID; }
    std::string              name()    const { return m_sName; }
    // HW3 compatibility alias
    std::string              getName() const { return m_sName; }

    std::shared_ptr<MyModel> model{};
    glm::vec3                color{};
    TransformComponent       transform{};

protected:
    MyGameObject(id_t objID, std::string name) : m_iID{ objID }, m_sName{ name } {}
    id_t        m_iID;
    std::string m_sName;
};

// ============================================================
// MySceneGraphNode  (from HW3 — drives the plant hierarchy)
//
// Hierarchy for the final project:
//
//   plant_root  (group — no model, just a transform)
//   ├── pot     (leaf — Bezier revolution surface)
//   ├── stem    (leaf — Bezier revolution surface)
//   └── leaves  (group — no model)
//       ├── leaf1  (leaf — Bezier revolution surface)
//       ├── leaf2  (leaf — rotated 120°)
//       └── leaf3  (leaf — rotated 240°)
//
// Wind response: leaf rotation is updated every frame before rendering.
// ============================================================
class MySceneGraphNode : public MyGameObject
{
public:
    // Factory used in HW3 style: name + explicit id
    MySceneGraphNode(std::string name, int id)
        : MyGameObject(static_cast<id_t>(id), name) {}

    // Add a child node
    void addChild(std::shared_ptr<MySceneGraphNode>& child);

    // In-order DFS traversal — returns the next node after pCurrentNode,
    // wrapping around to root when the end is reached.
    MySceneGraphNode* traverseNext(MySceneGraphNode* pCurrentNode);

    // Print scene graph to stdout (indented)
    void printSceneGraph();

    // Accessors used by the renderer
    bool isCurrent() const { return m_bIsCurrent; }
    void setCurrent(bool b) { m_bIsCurrent = b; }
    const std::vector<std::shared_ptr<MySceneGraphNode>>& getChildren() const { return m_vChildren; }

private:
    void _printSceneGraph(int indent);
    void _collectNodes(std::vector<MySceneGraphNode*>& nodes);

    std::vector<std::shared_ptr<MySceneGraphNode>> m_vChildren;
    bool m_bIsCurrent = false;
};

#endif
