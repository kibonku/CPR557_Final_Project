#include "my_game_object.h"
#include <iostream>

// ============================================================
// TransformComponent  (from HW5 — unchanged)
// ============================================================

glm::mat4 TransformComponent::mat4()
{
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);

    return glm::mat4{
        { scale.x*(c1*c3+s1*s2*s3), scale.x*(c2*s3), scale.x*(c1*s2*s3-c3*s1), 0.0f },
        { scale.y*(c3*s1*s2-c1*s3), scale.y*(c2*c3), scale.y*(c1*c3*s2+s1*s3), 0.0f },
        { scale.z*(c2*s1),          scale.z*(-s2),    scale.z*(c1*c2),           0.0f },
        { translation.x, translation.y, translation.z, 1.0f }
    };
}

glm::mat3 TransformComponent::normalMatrix()
{
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    const glm::vec3 inv = 1.0f / scale;

    return glm::mat3{
        { inv.x*(c1*c3+s1*s2*s3), inv.x*(c2*s3), inv.x*(c1*s2*s3-c3*s1) },
        { inv.y*(c3*s1*s2-c1*s3), inv.y*(c2*c3), inv.y*(c1*c3*s2+s1*s3) },
        { inv.z*(c2*s1),          inv.z*(-s2),    inv.z*(c1*c2)           }
    };
}

// ============================================================
// MySceneGraphNode  (from HW3)
// ============================================================

void MySceneGraphNode::addChild(std::shared_ptr<MySceneGraphNode>& child)
{
    m_vChildren.push_back(child);
}

void MySceneGraphNode::_collectNodes(std::vector<MySceneGraphNode*>& nodes)
{
    nodes.push_back(this);
    for (auto& c : m_vChildren)
        c->_collectNodes(nodes);
}

MySceneGraphNode* MySceneGraphNode::traverseNext(MySceneGraphNode* pCurrent)
{
    std::vector<MySceneGraphNode*> nodes;
    _collectNodes(nodes);

    // Clear current highlight on all nodes
    for (auto* n : nodes) n->setCurrent(false);

    if (nodes.empty()) return nullptr;

    // Find pCurrent in the flat list and advance one step
    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        if (nodes[i] == pCurrent)
        {
            int next = (i + 1) % (int)nodes.size();
            nodes[next]->setCurrent(true);
            return nodes[next];
        }
    }

    // pCurrent was nullptr or not found — start at first node
    nodes[0]->setCurrent(true);
    return nodes[0];
}

void MySceneGraphNode::_printSceneGraph(int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << "  ";
    std::cout << (m_bIsCurrent ? "* " : "  ") << getName() << "\n";
    for (auto& c : m_vChildren)
        c->_printSceneGraph(indent + 1);
}

void MySceneGraphNode::printSceneGraph()
{
    _printSceneGraph(0);
}
