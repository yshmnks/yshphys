#pragma once
#include "MouseMotionHandler.h"
#include "KeyHandler.h"
#include "Viewport.h"

class Camera : public MouseMotionHandler, public KeyHandler
{
public:
	enum KeyActions
	{
		MOVE_FORWARD = 0,
		MOVE_BACKWARD,
		MOVE_LEFTWARD,
		MOVE_RIGHTWARD,
		MOVE_UPWARD,
		MOVE_DOWNWARD,

		N_KEY_ACTIONS
	};

	Camera();
	virtual ~Camera();

	void PanUp(float pitch);
	void PanRight(float yaw);

	void UpdateView();
	
	virtual void ProcessKeyStates(KeyState* keyStates, int dt_ms);

	void SetViewport(Viewport* viewport);

private:

	virtual void ProcessMouseRelativeMotion(int xRel, int yRel);
	virtual unsigned int GetNumMappedKeys() const;

	// We use physics conventions. m_theta is the angle from the z-axis. m_phi is the polar angle from the x-axis
	float m_theta;
	float m_phi;
	Viewport* m_viewport;

	fVec3 m_pos;

	fVec3 m_moveSpeed; // meters per second
};

