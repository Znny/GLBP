#pragma once

#include <cstdint>
#include <vector>

#include "Transform.h"

struct NodeHandle
{
    static constexpr uint32_t InvalidIndex = 0xFFFFFFFFu;

    uint32_t Index = InvalidIndex;
    uint32_t Generation = 0;

    bool IsValid() const { return Index != InvalidIndex; }
    bool operator==(const NodeHandle& Other) const { return Index == Other.Index && Generation == Other.Generation; }
};

class SceneNode
{
public:
    Transform LocalTransform;

    NodeHandle GetParent() const { return Parent; }
    const std::vector<NodeHandle>& GetChildren() const { return Children; }
    uint32_t GetDepth() const { return Depth; }
    const glm::mat4& GetWorldMatrix() const { return WorldMatrix; }

private:
    friend class SceneGraph;

    NodeHandle Parent;
    std::vector<NodeHandle> Children;
    uint32_t Depth = 0;
    glm::mat4 WorldMatrix = glm::mat4(1.0f);
};

//handle-based scene hierarchy: nodes live in a slotmap (stale handles are detected via a
//per-slot generation counter rather than silently aliasing a recycled slot), and are grouped
//into per-depth buckets purely for update ordering - a node's depth is always parent.Depth+1,
//so processing buckets in ascending depth order guarantees every parent's world matrix is
//computed before any of its children need it, without requiring a full topological sort or any
//particular ordering within a bucket. UpdateWorldTransforms() recomputes every node
//unconditionally each call (no dirty-flag short-circuiting) - deliberately simple since scenes
//here are tiny; the depth-bucket layout is what would let that optimization be added later
//without changing this public API.
class SceneGraph
{
public:
    NodeHandle CreateNode(NodeHandle Parent = {});
    void DestroyNode(NodeHandle Handle);
    void Reparent(NodeHandle Handle, NodeHandle NewParent);

    Transform& GetLocalTransform(NodeHandle Handle);
    glm::mat4 GetWorldMatrix(NodeHandle Handle) const;
    glm::quat GetWorldRotation(NodeHandle Handle) const; //product of ancestor local rotations, root-to-node

    const SceneNode* GetNode(NodeHandle Handle) const;
    bool IsValid(NodeHandle Handle) const;

    void UpdateWorldTransforms();

private:
    struct FSlot
    {
        SceneNode Node;
        uint32_t Generation = 0;
        bool bAlive = false;
    };

    SceneNode* GetNodeMutable(NodeHandle Handle);
    void AddToDepthBucket(NodeHandle Handle, uint32_t Depth);
    void RemoveFromDepthBucket(NodeHandle Handle, uint32_t Depth);
    void RemoveFromParentsChildren(NodeHandle Handle);
    void MoveSubtreeToDepth(NodeHandle Handle, uint32_t NewDepth);

    std::vector<FSlot> Slots;
    std::vector<uint32_t> FreeIndices;
    std::vector<std::vector<NodeHandle>> DepthBuckets; //DepthBuckets[0] doubles as the root list
};
