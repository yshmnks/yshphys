#pragma once
#include "Geometry.h"
class Cylinder :
	public Geometry
{
public:
	Cylinder();
	~Cylinder();

	void SetHalfHeight(double halfHeight);
	void SetRadius(double radius);

	virtual dVec3 SupportLocal(const dVec3& v) const;

	virtual Polygon IntersectPlaneLocal(const dVec3& planeOrigin, const dVec3& planeNormal, const dVec3& xAxis, const dVec3& yAxis) const;

protected:

	double m_halfHeight;
	double m_radius;
};

