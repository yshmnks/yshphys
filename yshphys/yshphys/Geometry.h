#pragma once
#include "Vec3.h"
#include "Quat.h"
#include "Simplex3D.h"

class Geometry
{
public:
	Geometry();
	virtual ~Geometry();

	dVec3 GetPosition() const;
	dQuat GetRotation() const;

	void SetPosition(const dVec3& pos);
	void SetRotation(const dQuat& rot);

	virtual dVec3 Support(const dVec3& v) const;

	// ptSelf and ptGeoms are the points on the respective Geometries
	// that consititue the smallest separation between the Geometries.
	// separation is the distance between ptSelf and ptGeom (negative if penetrating)
	double ComputeSeparation(const Geometry* geom, dVec3& ptSelf, dVec3& ptGeom) const;

protected:
	double ComputePenetration(const Geometry* geom, dVec3& ptSelf, dVec3& ptGeom, Simplex3D tetrahedron) const;

	dVec3 m_pos; // position
	dQuat m_rot; // rotation
};

