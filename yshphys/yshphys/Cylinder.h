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

	dVec3 Support(const dVec3& v) const;

protected:

	double m_halfHeight;
	double m_radius;
};

