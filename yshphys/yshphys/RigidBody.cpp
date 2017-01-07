#include "stdafx.h"
#include "RigidBody.h"
#include "Force.h"

#define AABB_QUANTIZATION 0.25

void QuantizeAABB(AABB& aabb)
{
	if (AABB_QUANTIZATION != 0.0)
	{
		aabb.min.x = std::floor(aabb.min.x / AABB_QUANTIZATION) * AABB_QUANTIZATION;
		aabb.min.y = std::floor(aabb.min.y / AABB_QUANTIZATION) * AABB_QUANTIZATION;
		aabb.min.z = std::floor(aabb.min.z / AABB_QUANTIZATION) * AABB_QUANTIZATION;
		aabb.max.x = std::ceil(aabb.max.x / AABB_QUANTIZATION) * AABB_QUANTIZATION;
		aabb.max.y = std::ceil(aabb.max.y / AABB_QUANTIZATION) * AABB_QUANTIZATION;
		aabb.max.z = std::ceil(aabb.max.z / AABB_QUANTIZATION) * AABB_QUANTIZATION;
	}
}

RigidBody::RigidBody() :
	m_geometry(nullptr),
	m_nForces(0)
{
	m_state.P = dVec3(0.0, 0.0, 0.0);
	m_state.x = dVec3(0.0, 0.0, 0.0);

	m_state.L = dVec3(0.0, 0.0, 0.0);
	m_state.q = dQuat::Identity();

	m_v = dVec3(0.0, 0.0, 0.0);
	m_w = dVec3(0.0, 0.0, 0.0);

	m_inertia.m = 0.0;
	m_inertia.minv = 0.0;

	m_inertia.Ibody.SetRow(0, dVec3(0.0, 0.0, 0.0));
	m_inertia.Ibody.SetRow(1, dVec3(0.0, 0.0, 0.0));
	m_inertia.Ibody.SetRow(2, dVec3(0.0, 0.0, 0.0));

	m_inertia.Ibodyinv.SetRow(0, dVec3(0.0, 0.0, 0.0));
	m_inertia.Ibodyinv.SetRow(1, dVec3(0.0, 0.0, 0.0));
	m_inertia.Ibodyinv.SetRow(2, dVec3(0.0, 0.0, 0.0));

	m_Iinv.SetRow(0, dVec3(0.0, 0.0, 0.0));
	m_Iinv.SetRow(1, dVec3(0.0, 0.0, 0.0));
	m_Iinv.SetRow(2, dVec3(0.0, 0.0, 0.0));
}

RigidBody::~RigidBody()
{
}

double RigidBody::GetMass(double& inverseMass) const
{
	inverseMass = m_inertia.minv;
	return m_inertia.m;
}

dVec3 RigidBody::GetPosition() const
{
	return m_state.x;
}
dQuat RigidBody::GetRotation() const
{
	return m_state.q;
}
dVec3 RigidBody::GetLinearVelocity() const
{
	return m_v;
}
dVec3 RigidBody::GetAngularVelocity() const
{
	return m_w;
}
Geometry* RigidBody::GetGeometry() const
{
	return m_geometry;
}
void RigidBody::SetPosition(const dVec3& x)
{
	m_state.x = x;
	UpdateAABB();
}
void RigidBody::SetRotation(const dQuat& q)
{
	m_state.q = q;
	UpdateDependentStateVariables();
	UpdateAABB();
}
void RigidBody::SetGeometry(Geometry* geometry)
{
	m_geometry = geometry;
	UpdateAABB();
}
void RigidBody::SetMass(double m)
{
	if (m < 64.0*(double)FLT_EPSILON)
	{
		m_inertia.m = 0.0;
		m_inertia.minv = 0.0;
	}
	else
	{
		m_inertia.m = m;
		m_inertia.minv = 1.0 / m_inertia.m;
	}
}
void RigidBody::SetInertia(const dMat33& Ibody)
{
	m_inertia.Ibody = Ibody;

	m_inertia.Ibody(1, 0) = m_inertia.Ibody(0, 1);
	m_inertia.Ibody(2, 0) = m_inertia.Ibody(0, 2);
	m_inertia.Ibody(2, 1) = m_inertia.Ibody(1, 2);

	m_inertia.Ibodyinv = m_inertia.Ibody.Inverse();

	UpdateDependentStateVariables();
}

void RigidBody::ApplyForce(Force* force)
{
	m_forces[m_nForces++] = force;
}

void RigidBody::UpdateAABB()
{
	const dVec3 x = m_state.x;
	const dQuat q = m_state.q;

	const BoundingBox oobb = m_geometry->GetLocalOOBB();
	const dVec3 oobbCenter = (oobb.min + oobb.max).Scale(0.5); // center of the OOBB in geom's local frame
	const dVec3 oobbSpan = (oobb.max - oobb.min).Scale(0.5); // span of the OOBB

	const dVec3 aabbCenter = x + q.Transform(m_geometry->GetPosition() + oobbCenter);
	const dQuat geomRot = q*m_geometry->GetRotation();

	const dVec3 aabbSpan = dMat33(geomRot).Abs().Transform(oobbSpan);

	m_AABB.min = aabbCenter - aabbSpan;
	m_AABB.max = aabbCenter + aabbSpan;

	QuantizeAABB(m_AABB);

	if (m_bvNode != nullptr)
	{
		m_bvNode->SetAABB(m_AABB);
	}
}

void RigidBody::Compute_qDot(const dQuat& q, const dVec3& L, dQuat& qDot) const
{
	const dMat33 R(q);
	const dMat33 Iinv = R*m_inertia.Ibodyinv*R.Transpose();
	const dVec3 w = Iinv.Transform(L);

	dQuat wQuat;
	wQuat.x = w.x;
	wQuat.y = w.y;
	wQuat.z = w.z;
	wQuat.w = 0.0;

	qDot = wQuat*q;
	qDot.x *= 0.5;
	qDot.y *= 0.5;
	qDot.z *= 0.5;
	qDot.w *= 0.5;
}
void RigidBody::Compute_xDot(const dVec3& P, dVec3& xDot) const
{
	xDot = P.Scale(m_inertia.minv);
}

void RigidBody::Step(double dt)
{
	if (!m_awake)
	{
		return;
	}

	auto NormalizeQuat = [](dQuat& q)
	{
		if (abs(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z - 1.0) > FLT_EPSILON)
		{
			q = q.Normalize();
		}
	};

	auto ZeroState = [](RigidBody::State& state)
	{
		state.x = dVec3(0.0, 0.0, 0.0);
		state.P = dVec3(0.0, 0.0, 0.0);
		state.L = dVec3(0.0, 0.0, 0.0);
		state.q = dQuat(0.0, 0.0, 0.0, 0.0);
	};
	auto ScaleState = [](const RigidBody::State& state, double scale)
	{
		RigidBody::State out = state;

		out.q.x = state.q.x * scale;
		out.q.y = state.q.y * scale;
		out.q.z = state.q.z * scale;
		out.q.w = state.q.w * scale;

		out.L = out.L.Scale(scale);

		out.x = out.x.Scale(scale);
		out.P = out.P.Scale(scale);

		return out;
	};
	auto AddStates = [](const RigidBody::State& state0, const RigidBody::State& state1)
	{
		auto AddQuats = [](const dQuat& q0, const dQuat& q1)
		{
			dQuat sum;
			sum.x = q0.x + q1.x;
			sum.y = q0.y + q1.y;
			sum.z = q0.z + q1.z;
			sum.w = q0.w + q1.w;
			return sum;
		};

		RigidBody::State state;
		state.q = AddQuats(state0.q, state1.q);
		state.L = state0.L + state1.L;
		state.x = state0.x + state1.x;
		state.P = state0.P + state1.P;

		return state;
	};

	RigidBody::State state = m_state;

	RigidBody::State stateDerivatives[4];
	const double dtRK4[3] = { 0.5*dt, 0.5*dt, 1.0*dt };

	int i = 0;

	while (true)
	{
		RigidBody::State& stateDerivative = stateDerivatives[i];
		ZeroState(stateDerivative);

		Compute_xDot(state.P, stateDerivative.x);
		Compute_qDot(state.q, state.L, stateDerivative.q);

		for (int j = 0; j < m_nForces; ++j)
		{
			dVec3 F, T;
			m_forces[j]->ComputeForceAndTorque(m_inertia, m_state, F, T);
			stateDerivative.P = stateDerivative.P + F;
			stateDerivative.L = stateDerivative.L + T;
		}

		if (i == 3)
		{
			break;
		}

		state = AddStates(m_state, ScaleState(stateDerivative, dtRK4[i]));
		NormalizeQuat(state.q);

		i++;
	}

	const double cRK4[4] = { 1.0 / 6.0, 2.0 / 6.0, 2.0 / 6.0, 1.0 / 6.0 };
	RigidBody::State dState;
	ZeroState(dState);
	for (int i = 0; i < 4; ++i)
	{
		dState = AddStates(dState, ScaleState(stateDerivatives[i], cRK4[i]));
	}
	m_state = AddStates(m_state, ScaleState(dState, dt));

	NormalizeQuat(m_state.q);

	UpdateDependentStateVariables();

	UpdateAABB();

	for (int i = 0; i < m_nForces; ++i)
	{
		delete m_forces[i];
	}
	m_nForces = 0;
}
