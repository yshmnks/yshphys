#pragma once

class PhysicsScene;
class PhysicsObject;

class PhysicsNode
{
	friend class PhysicsScene;
public:
	PhysicsObject* GetPhysicsObject() const;
	PhysicsNode* GetPrev() const;
	PhysicsNode* GetNext() const;
private:
	PhysicsNode();
	virtual ~PhysicsNode();

	void BindPhysicsObject(PhysicsObject* physicsObject);
	void Remove();
	void AppendTo(PhysicsNode* prev);
	void PrependTo(PhysicsNode* next);

	PhysicsObject* m_physicsObject;

	PhysicsNode* m_next;
	PhysicsNode* m_prev;
};

class FreedPhysicsNode
{
public:
	FreedPhysicsNode();
	virtual ~FreedPhysicsNode();

	PhysicsNode* m_node;
	PhysicsNode* m_precedingNode;
};

