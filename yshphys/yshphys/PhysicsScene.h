#pragma once
#include "BVTree.h"
#include "RigidBody.h"
#include "PhysicsObject.h"
#include "PhysicsNode.h"

#define MAX_PHYSICS_NODES 1024

class PhysicsScene
{
public:
	PhysicsScene();
	virtual ~PhysicsScene();

	void AddPhysicsObject(PhysicsObject* physicsObject);
	void RemovePhysicsObject(PhysicsObject* physicsObject);

	void Step(double dt);

protected:
	std::stack<FreedPhysicsNode> m_freedNodeStack;
	PhysicsNode m_physicsNodes[MAX_PHYSICS_NODES];
	PhysicsNode* m_firstNode;

	BVTree m_bvTree;
};

