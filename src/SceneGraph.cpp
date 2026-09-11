#include "SceneGraph.h"

#include <cassert>

NodeHandle SceneGraph::CreateNode(NodeHandle Parent)
{
    const bool bHasParent = IsValid(Parent);
    const uint32_t Depth = bHasParent ? Slots[Parent.Index].Node.Depth + 1 : 0;

    uint32_t Index;
    if(!FreeIndices.empty())
    {
        Index = FreeIndices.back();
        FreeIndices.pop_back();
    }
    else
    {
        Index = (uint32_t)Slots.size();
        Slots.emplace_back();
    }

    FSlot& Slot = Slots[Index];
    Slot.Node = SceneNode();
    Slot.Node.Parent = bHasParent ? Parent : NodeHandle{};
    Slot.Node.Depth = Depth;
    Slot.bAlive = true;
    //Slot.Generation is already correct here: 0 for a freshly-appended slot, or already bumped
    //by the DestroyNode() call that freed this slot for reuse.

    const NodeHandle NewHandle{ Index, Slot.Generation };

    AddToDepthBucket(NewHandle, Depth);

    if(bHasParent)
    {
        Slots[Parent.Index].Node.Children.push_back(NewHandle);
    }

    return NewHandle;
}

void SceneGraph::DestroyNode(NodeHandle Handle)
{
    if(!IsValid(Handle)) return;

    //copy first - destroying a child mutates this node's Children vector via RemoveFromParentsChildren
    const std::vector<NodeHandle> ChildrenCopy = Slots[Handle.Index].Node.Children;
    for(NodeHandle Child : ChildrenCopy)
    {
        DestroyNode(Child);
    }

    RemoveFromParentsChildren(Handle);
    RemoveFromDepthBucket(Handle, Slots[Handle.Index].Node.Depth);

    FSlot& Slot = Slots[Handle.Index];
    Slot.bAlive = false;
    Slot.Generation++;
    FreeIndices.push_back(Handle.Index);
}

void SceneGraph::Reparent(NodeHandle Handle, NodeHandle NewParent)
{
    if(!IsValid(Handle) || Handle == NewParent) return;

    const bool bHasNewParent = IsValid(NewParent);
    const uint32_t NewDepth = bHasNewParent ? Slots[NewParent.Index].Node.Depth + 1 : 0;

    RemoveFromParentsChildren(Handle);
    MoveSubtreeToDepth(Handle, NewDepth);

    Slots[Handle.Index].Node.Parent = bHasNewParent ? NewParent : NodeHandle{};
    if(bHasNewParent)
    {
        Slots[NewParent.Index].Node.Children.push_back(Handle);
    }
}

Transform& SceneGraph::GetLocalTransform(NodeHandle Handle)
{
    SceneNode* Node = GetNodeMutable(Handle);
    assert(Node && "GetLocalTransform called with an invalid/stale NodeHandle");
    return Node->LocalTransform;
}

glm::quat SceneGraph::GetWorldRotation(NodeHandle Handle) const
{
    const SceneNode* Node = GetNode(Handle);
    assert(Node && "GetWorldRotation called with an invalid/stale NodeHandle");

    const glm::quat ParentRotation = Node->GetParent().IsValid() ? GetWorldRotation(Node->GetParent()) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    return glm::normalize(ParentRotation * Node->LocalTransform.GetRotation());
}

glm::mat4 SceneGraph::GetWorldMatrix(NodeHandle Handle) const
{
    const SceneNode* Node = GetNode(Handle);
    assert(Node && "GetWorldMatrix called with an invalid/stale NodeHandle");
    return Node->WorldMatrix;
}

const SceneNode* SceneGraph::GetNode(NodeHandle Handle) const
{
    return IsValid(Handle) ? &Slots[Handle.Index].Node : nullptr;
}

bool SceneGraph::IsValid(NodeHandle Handle) const
{
    return Handle.IsValid()
        && Handle.Index < Slots.size()
        && Slots[Handle.Index].bAlive
        && Slots[Handle.Index].Generation == Handle.Generation;
}

void SceneGraph::UpdateWorldTransforms()
{
    for(uint32_t Depth = 0; Depth < DepthBuckets.size(); Depth++)
    {
        for(NodeHandle Handle : DepthBuckets[Depth])
        {
            SceneNode& Node = Slots[Handle.Index].Node;
            const glm::mat4 Local = Node.LocalTransform.GetMatrix();
            Node.WorldMatrix = Node.Parent.IsValid() ? Slots[Node.Parent.Index].Node.WorldMatrix * Local : Local;
        }
    }
}

SceneNode* SceneGraph::GetNodeMutable(NodeHandle Handle)
{
    return IsValid(Handle) ? &Slots[Handle.Index].Node : nullptr;
}

void SceneGraph::AddToDepthBucket(NodeHandle Handle, uint32_t Depth)
{
    if(Depth >= DepthBuckets.size())
    {
        DepthBuckets.resize(Depth + 1);
    }
    DepthBuckets[Depth].push_back(Handle);
}

void SceneGraph::RemoveFromDepthBucket(NodeHandle Handle, uint32_t Depth)
{
    std::vector<NodeHandle>& Bucket = DepthBuckets[Depth];
    for(size_t i = 0; i < Bucket.size(); i++)
    {
        if(Bucket[i] == Handle)
        {
            Bucket[i] = Bucket.back();
            Bucket.pop_back();
            break;
        }
    }
}

void SceneGraph::RemoveFromParentsChildren(NodeHandle Handle)
{
    const SceneNode* Node = GetNode(Handle);
    if(!Node || !IsValid(Node->Parent)) return;

    std::vector<NodeHandle>& Siblings = Slots[Node->Parent.Index].Node.Children;
    for(size_t i = 0; i < Siblings.size(); i++)
    {
        if(Siblings[i] == Handle)
        {
            Siblings[i] = Siblings.back();
            Siblings.pop_back();
            break;
        }
    }
}

void SceneGraph::MoveSubtreeToDepth(NodeHandle Handle, uint32_t NewDepth)
{
    SceneNode& Node = Slots[Handle.Index].Node;
    const uint32_t OldDepth = Node.Depth;

    if(OldDepth != NewDepth)
    {
        RemoveFromDepthBucket(Handle, OldDepth);
        Node.Depth = NewDepth;
        AddToDepthBucket(Handle, NewDepth);
    }

    //copy - recursing further below reallocates other slots' Children vectors, not this one, but
    //copying keeps this loop safe regardless
    const std::vector<NodeHandle> ChildrenCopy = Node.Children;
    for(NodeHandle Child : ChildrenCopy)
    {
        MoveSubtreeToDepth(Child, NewDepth + 1);
    }
}
