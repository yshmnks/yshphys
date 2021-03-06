#include "stdafx.h"
#include "BVTree.h"
#include "DebugRenderer.h"

BVTree::BVTree() : m_iRoot(INVALID_BVNODEINDEX), m_iFirst(INVALID_BVNODEINDEX)
{
	for (int i = 0; i < MAX_BV_NODES; ++i)
	{
		m_nodes[i].m_tree = this;
		m_nodes[i].m_index = i;
	}
	for (int i = MAX_BV_NODES - 1; i > 0; --i)
	{
		FreedBVNode freed;
		freed.m_iNode = i;
		freed.m_iPrecedingNode = i - 1;

		m_iFreeNodes.push(freed);
	}
	FreedBVNode freed;
	freed.m_iNode = 0;
	freed.m_iPrecedingNode = INVALID_BVNODEINDEX;
	m_iFreeNodes.push(freed);
}

const BVNode* BVTree::Root() const
{
	return &m_nodes[m_iRoot];
}

void BVTree::PopFreeNode()
{
	FreedBVNode freed = m_iFreeNodes.top();

	const unsigned int& iCurr = freed.m_iNode;
	BVNode& curr = m_nodes[iCurr];

	const unsigned int& iPrev = freed.m_iPrecedingNode;
	curr.m_iPrev = iPrev;

	if (iPrev == INVALID_BVNODEINDEX) // The popped node is the first element in the linked list of active nodes
	{
		if (m_iFirst != INVALID_BVNODEINDEX)
		{
			m_nodes[m_iFirst].m_iPrev = iCurr;
			curr.m_iNext = m_iFirst;
		}
		m_iFirst = iCurr;
	}
	else
	{
		BVNode& prev = m_nodes[iPrev];
		const unsigned int& iNext = prev.m_iNext;
		curr.m_iNext = iNext;
		prev.m_iNext = iCurr;

		if (iNext != INVALID_BVNODEINDEX)
		{
			BVNode& next = m_nodes[iNext];
			next.m_iPrev = iCurr;
		}
	}
	m_iFreeNodes.pop();
}
void BVTree::PushFreeNode(unsigned int iCurr)
{
	FreedBVNode freed;

	BVNode& curr = m_nodes[iCurr];
	const unsigned int iPrev = curr.m_iPrev;
	const unsigned int iNext = curr.m_iNext;

	curr.m_iNext = INVALID_BVNODEINDEX;
	curr.m_iPrev = INVALID_BVNODEINDEX;

	if (iPrev != INVALID_BVNODEINDEX)
	{
		BVNode& prev = m_nodes[iPrev];
		prev.m_iNext = iNext;
	}
	if (iNext != INVALID_BVNODEINDEX)
	{
		BVNode& next = m_nodes[iNext];
		next.m_iPrev = iPrev;
	}

	freed.m_iNode = iCurr;
	freed.m_iPrecedingNode = iPrev;
	m_iFreeNodes.push(freed);
}

BVTree::~BVTree()
{
}

bool BVTree::LeftInsertNewLeaf(const AABB& aabb, BVNodeContent* content)
{
	if (m_iFreeNodes.empty() || content == nullptr)
	{
		return false;
	}

	const unsigned int iNewLeaf = m_iFreeNodes.top().m_iNode;
	PopFreeNode();
	const unsigned int iNewRoot = m_iFreeNodes.top().m_iNode;
	PopFreeNode();

	BVNode* newRoot = &m_nodes[iNewRoot];
	BVNode* newLeaf = &m_nodes[iNewLeaf];
	
	BVNode* oldRoot = &m_nodes[m_iRoot];

	oldRoot->m_parent = newRoot;
	newLeaf->m_parent = newRoot;
	
	newLeaf->m_left = nullptr;
	newLeaf->m_right = nullptr;

	// NOTE: newLeaf must be inserted on the LEFT, since we traverse from the leftmost leaf when checking for collisions.
	// If we were to put it on the right and the new leaf's AABB is actually enclosed in the oldRoot's AABB, then we will miss collisions.
	newRoot->m_left = newLeaf;
	newRoot->m_right = oldRoot;

	newLeaf->m_AABB = aabb;
	newLeaf->SetContent(content);
	newRoot->m_AABB = newRoot->m_AABB.Aggregate(aabb);

	m_iRoot = iNewRoot;
	return true;
}

bool BVTree::DeepInsertNewLeaf(const AABB& aabb, BVNodeContent* content)
{
	if (m_iFreeNodes.empty() || content == nullptr)
	{
		return false;
	}

	const unsigned int iNewLeaf = m_iFreeNodes.top().m_iNode;
	PopFreeNode();

	BVNode* newLeaf = &m_nodes[iNewLeaf];

	newLeaf->m_left = nullptr;
	newLeaf->m_right = nullptr;

	if (m_iRoot == INVALID_BVNODEINDEX)
	{
		newLeaf->m_parent = nullptr;
		m_iRoot = iNewLeaf;
	}
	else
	{
		BVNode* fork = &m_nodes[m_iRoot];

		bool forkIsL = false;

		while (!fork->IsLeaf())
		{
			fork->m_AABB = fork->m_AABB.Aggregate(aabb);

			BVNode* L = fork->m_left;
			BVNode* R = fork->m_right;

			double A_mergeL = L->m_AABB.Aggregate(aabb).Area() + R->m_AABB.Area();
			double A_mergeR = R->m_AABB.Aggregate(aabb).Area() + L->m_AABB.Area();

			if (A_mergeL < A_mergeR)
			{
				fork = L;
				forkIsL = true;
			}
			else
			{
				fork = R;
				forkIsL = false;
			}
		}

		const unsigned int iNewParent = m_iFreeNodes.top().m_iNode;
		PopFreeNode();

		BVNode* newParent = &m_nodes[iNewParent];

		BVNode* grandparent = fork->m_parent;
		if (grandparent == nullptr)
		{
			m_iRoot = iNewParent;
		}
		else
		{
			if (forkIsL)
			{
				grandparent->m_left = newParent;
			}
			else
			{
				grandparent->m_right = newParent;
			}
		}
		newParent->m_left = fork;
		newParent->m_right = newLeaf;
		newParent->m_parent = grandparent;

		fork->m_parent = newParent;
		newLeaf->m_parent = newParent;

		newParent->m_AABB = fork->m_AABB.Aggregate(aabb);
	}

	newLeaf->m_AABB = aabb;
	newLeaf->SetContent(content);

	return true;
}

void BVTree::DebugDraw(DebugRenderer* renderer) const
{
	const BVNode* root = Root();
	if (!root)
	{
		return;
	}

	std::stack<const BVNode*> nodeStack;
	nodeStack.push(Root());
	while (!nodeStack.empty())
	{
		const BVNode* node = nodeStack.top();
		nodeStack.pop();

		const AABB aabb = node->GetAABB();

		const fVec3 aabbCenter(
			float(aabb.max.x + aabb.min.x)*0.5f,
			float(aabb.max.y + aabb.min.y)*0.5f,
			float(aabb.max.z + aabb.min.z)*0.5f);

		renderer->DrawBox(
			float(aabb.max.x - aabb.min.x)*0.5f,
			float(aabb.max.y - aabb.min.y)*0.5f,
			float(aabb.max.z - aabb.min.z)*0.5f,
			aabbCenter, fQuat::Identity(), fVec3(1.0f, 1.0f, 1.0f), true
		);

		if (!node->IsLeaf())
		{
			nodeStack.push(node->GetLeftChild());
			nodeStack.push(node->GetRightChild());
		}
	}
}