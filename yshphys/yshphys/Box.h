#pragma once
#include "Geometry.h"
class Box :
	public Geometry
{
public:
	Box();
	virtual ~Box();

	void GetDimensions(double& halfDimX, double& halfDimY, double& halfDimZ) const;
	void SetDimensions(double halfDimX, double halDimY, double halfDimZ);

	virtual dVec3 SupportLocal(const dVec3& v) const;

	virtual bool RayIntersect(const dVec3& pos, const dQuat& rot, const Ray& ray, dVec3& hitPt) const;

	virtual Polygon IntersectPlaneLocal(const dVec3& planeOrigin, const dVec3& planeNormal, const dVec3& xAxis, const dVec3& yAxis) const;

	virtual EGeomType GetType() const { return EGeomType::BOX; }

protected:
	dVec3 m_halfDim;
};

