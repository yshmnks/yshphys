#include "stdafx.h"
#include "Geometry.h"
#include "MathUtils.h"
#include "Point.h"
#include "Ray.h"
#include "EPAHull.h"
#include "Sphere.h"

#define MIN_SUPPORT_SQR 0.0001
#define GJK_TERMINATION_RATIO 0.01

Geometry::Geometry() : m_material(Material::Type::WOOD)
{
}

Geometry::~Geometry()
{
}

void Geometry::UpdateLocalOOBB()
{
	m_localOOBB.min.x = SupportLocal(dVec3(-1.0, 0.0, 0.0)).x;
	m_localOOBB.min.y = SupportLocal(dVec3(0.0, -1.0, 0.0)).y;
	m_localOOBB.min.z = SupportLocal(dVec3(0.0, 0.0, -1.0)).z;

	m_localOOBB.max.x = SupportLocal(dVec3(1.0, 0.0, 0.0)).x;
	m_localOOBB.max.y = SupportLocal(dVec3(0.0, 1.0, 0.0)).y;
	m_localOOBB.max.z = SupportLocal(dVec3(0.0, 0.0, 1.0)).z;
}

BoundingBox Geometry::GetLocalOOBB() const
{
	return m_localOOBB;
}

dVec3 Geometry::QuantizeDirection(const dVec3& v)
{
	return v;

	const dVec3 r = v.Scale(1.0 / sqrt(v.Dot(v)));
	const double cosTheta = r.z;
	const double theta = acos(cosTheta);
	const double phi = atan2(v.y, v.x);
	
	const int nTheta = 256;
	const int nPhi = 64;

	const double dTheta = dPI / (nTheta - 1);
	const double dPhi = 2.0*dPI / nPhi;

	double theta_quantized = (double)std::round(theta / dTheta) * dTheta;
	double phi_quantized = (double)std::round(phi / dPhi) * dPhi;

	assert(0.0 <= theta_quantized && theta_quantized <= dPI);
	assert(-dPI <= phi_quantized && phi_quantized <= dPI);

	const dVec3 r_quantized(sin(theta_quantized)*cos(phi_quantized), sin(theta_quantized)*sin(phi_quantized), cos(theta_quantized));

	assert(r.Dot(r_quantized) > 0.9);

	return r_quantized;
}

dVec3 Geometry::Support(const dVec3& x, const dQuat& q, const dVec3& v_) const
{
	assert(abs(v_.x) < 100000.0);
	assert(abs(v_.y) < 100000.0);
	assert(abs(v_.z) < 100000.0);

	const dVec3 v = QuantizeDirection(v_);

	return x + q.Transform(SupportLocal((-q).Transform(v)));
}

dVec3 Geometry::SupportLocal(const dVec3& v) const
{
	return dVec3(0.0, 0.0, 0.0);
}

Material::Type Geometry::GetMaterialLocal(const dVec3& x) const
{
	return m_material;
}

Polygon Geometry::IntersectPlane(const dVec3& pos, const dQuat& rot, const dVec3& planeOrigin, const dVec3& planeNormal, const dVec3& xAxis, const dVec3& yAxis) const
{
	dMat33 planeOrientationT;
	planeOrientationT.SetRow(0, xAxis);
	planeOrientationT.SetRow(1, yAxis);
	planeOrientationT.SetRow(2, planeNormal);
	dMat33 localPlaneOrientationT = planeOrientationT*dMat33(rot);
	return IntersectPlaneLocal((-rot).Transform(planeOrigin - pos),
		localPlaneOrientationT.GetRow(2),
		localPlaneOrientationT.GetRow(0),
		localPlaneOrientationT.GetRow(1)
	);
}

Polygon Geometry::IntersectPlaneLocal(const dVec3& planeOrigin, const dVec3& planeNormal, const dVec3& xAxis, const dVec3& yAxis) const
{
	return Polygon();
}

void Geometry::SetUniformMaterial(Material::Type material)
{
	m_material = material;
}

bool Geometry::RayIntersect(const dVec3& pos, const dQuat& rot, const Ray& ray, dVec3& hit) const
{
	GJKSimplex simplex;
	hit = ray.GetOrigin();
	const dVec3 rayDir = ray.GetDirection();

	Point pt;

	dVec3 closest;

	while (true)
	{
		Geometry::Intersect(&pt, hit, dQuat::Identity(), dVec3(), dVec3(), this, pos, rot, closest, dVec3(), simplex, true);

		const dVec3 hit2closest = closest - hit;

		const double dSqr = hit2closest.Dot(hit2closest);

		if (dSqr < 0.00001)
		{
			return true;
		}
		else if (hit2closest.Dot(rayDir) / sqrt(dSqr*rayDir.Dot(rayDir)) < 0.001)
		{
			// if the closest point is perpendicular to or behind the ray direction, then the ray missed (or will miss)
			return false;
		}

		// march the ray forward, and shift the simplex for the next GJK pass
		const dVec3 dHit = rayDir.Scale(hit2closest.Dot(rayDir) / rayDir.Dot(rayDir));
		hit = hit + dHit;

		for (int i = 0; i < simplex.m_nPts; ++i)
		{
			simplex.m_pts[i].m_MinkDif = simplex.m_pts[i].m_MinkDif + dHit;
			simplex.m_pts[i].m_MinkSum = simplex.m_pts[i].m_MinkSum + dHit;
		}
	}
	return false;
}

void CompleteSimplex3(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1,
	GJKSimplex& simplex)
{
	const dVec3& A = simplex.m_pts[0].m_MinkDif;
	const dVec3& B = simplex.m_pts[1].m_MinkDif;
	const dVec3& C = simplex.m_pts[2].m_MinkDif;
	dVec3 n = (B - A).Cross(C - A);
	n = n.Scale(1.0 / sqrt(n.Dot(n)));
	dMinkowskiPoint newSimplexPt;
	dVec3 p0 = geom0->Support(pos0, rot0, -n);
	dVec3 p1 = geom1->Support(pos1, rot1, n);
	dVec3 d = (p0 - p1) - A;
	if (abs(d.Dot(n)) < (double)FLT_EPSILON)
	{
		n = -n;
		p0 = geom0->Support(pos0, rot0, -n);
		p1 = geom1->Support(pos1, rot1, n);
		d = (p0 - p1) - A;
		assert(abs(d.Dot(n)) > (double)FLT_EPSILON);
	}
	newSimplexPt.m_MinkDif = p0 - p1;
	newSimplexPt.m_MinkSum = p0 + p1;
	simplex.AddPoint(newSimplexPt);
}
void CompleteSimplex2(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1,
	GJKSimplex& simplex)
{
	const dVec3& A = simplex.m_pts[0].m_MinkDif;
	const dVec3& B = simplex.m_pts[1].m_MinkDif;
	const dVec3 AB = B - A;
	const dVec3 AB_AB = AB.Times(AB);
	dVec3 n;
	for (int i = 0; i < 3; ++i)
	{
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;
		if (AB_AB[i] <= AB_AB[j] && AB_AB[i] <= AB_AB[k])
		{
			const double invNorm = 1.0 / sqrt(AB[j] * AB[j] + AB[k] * AB[k]);
			n[i] = 0.0;
			n[j] = AB[k] * invNorm;
			n[k] = -AB[j] * invNorm;

			dMinkowskiPoint newSimplexPt;
			dVec3 p0 = geom0->Support(pos0, rot0, -n);
			dVec3 p1 = geom1->Support(pos1, rot1, n);
			dVec3 d = (p0 - p1) - A;
			if (abs(d.Dot(n)) < (double)FLT_EPSILON)
			{
				n = -n;
				p0 = geom0->Support(pos0, rot0, -n);
				p1 = geom1->Support(pos1, rot1, n);
				d = (p0 - p1) - A;
				if (abs(d.Dot(n)) < (double)FLT_EPSILON)
				{
					n = AB.Cross(n);
					n = n.Scale(1.0 / sqrt(n.Dot(n)));
					p0 = geom0->Support(pos0, rot0, -n);
					p1 = geom1->Support(pos1, rot1, n);
					d = (p0 - p1) - A;
					if (abs(d.Dot(n)) < (double)FLT_EPSILON)
					{
						n = -n;
						p0 = geom0->Support(pos0, rot0, -n);
						p1 = geom1->Support(pos1, rot1, n);
						d = (p0 - p1) - A;
						assert(abs(d.Dot(n)) > (double)FLT_EPSILON);
					}
				}
			}
			newSimplexPt.m_MinkDif = p0 - p1;
			newSimplexPt.m_MinkSum = p0 + p1;
			simplex.AddPoint(newSimplexPt);
			CompleteSimplex3(geom0, pos0, rot0, geom1, pos1, rot1, simplex);
			return;
		}
	}
}
void CompleteSimplex1(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1,
	GJKSimplex& simplex, const dVec3& v)
{
	dVec3 p0 = geom0->Support(pos0, rot0, -v);
	dVec3 p1 = geom1->Support(pos1, rot1, v);
	dMinkowskiPoint newSimplexPt;
	newSimplexPt.m_MinkDif = p0 - p1;
	newSimplexPt.m_MinkSum = p0 + p1;
	simplex.AddPoint(newSimplexPt);
	CompleteSimplex2(geom0, pos0, rot0, geom1, pos1, rot1, simplex);
}
bool Geometry::Intersect(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0, dVec3& pt0, dVec3& n0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1, dVec3& pt1, dVec3& n1)
{
	if (geom0->GetType() == EGeomType::SPHERE && geom1->GetType() == EGeomType::SPHERE)
	{
		Sphere* sphere0 = (Sphere*)geom0;
		Sphere* sphere1 = (Sphere*)geom1;

		const dVec3 d = pos1 - pos0;
		const double dd = d.Dot(d);
		const double r0 = sphere0->GetRadius();
		const double r1 = sphere1->GetRadius();
		const double r = r0 + r1;
		const double rr = r*r;

		if (dd < rr)
		{
			n0 = (pos1 - pos0);
			n0 = n0.Scale(1.0 / sqrt(n0.Dot(n0)));
			n1 = -n0;
			pt0 = pos0 + n0.Scale(r0);
			pt1 = pos1 + n1.Scale(r1);
			return true;
		}
		else
		{
			return false;
		}
	}


	GJKSimplex simplex;

	return Geometry::Intersect(
		geom0, pos0, rot0, pt0, n0,
		geom1, pos1, rot1, pt1, n1,
		simplex);
}
bool Geometry::Intersect(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0, dVec3& pt0, dVec3& n0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1, dVec3& pt1, dVec3& n1,
	GJKSimplex& simplex, bool bypassPenetration)
{
	dVec3 v;

	if (simplex.GetNumPoints() == 0)
	{
		v = pos0 - pos1;
		pt0 = geom0->Support(pos0, rot0, -v);
		pt1 = geom1->Support(pos1, rot1, v);
		dMinkowskiPoint newSimplexPt;
		newSimplexPt.m_MinkDif = pt0 - pt1;
		newSimplexPt.m_MinkSum = pt0 + pt1;
		simplex.AddPoint(newSimplexPt);
	}

	int nIter = 0;

	dMinkowskiPoint closestSimplexPt;

	while (nIter < 16)
	{
		nIter++;
		// Get the closest point on the convex hull of the simplex, set it to the new support direction "v"
		// and discard any existing points on the simplex that are not needed to express "v"
		GJKSimplex closestFeature;
		closestSimplexPt = simplex.ClosestPointToOrigin(closestFeature);

		if (closestFeature.GetNumPoints() == 4)
		{
			EPAHull hull(geom0, pos0, rot0, geom1, pos1, rot1, simplex);
			return hull.ComputeIntersection(pt0, n0, pt1, n1);
		}
		else
		{
			const dVec3 vPrev = v;
			auto CheckPenetration = [&]()
			{
				switch (simplex.GetNumPoints())
				{
				case 1:
					CompleteSimplex1(geom0, pos0, rot0, geom1, pos1, rot1, simplex, -vPrev);
					break;
				case 2:
					CompleteSimplex2(geom0, pos0, rot0, geom1, pos1, rot1, simplex);
					break;
				case 3:
					CompleteSimplex3(geom0, pos0, rot0, geom1, pos1, rot1, simplex);
					break;
				}
				EPAHull hull(geom0, pos0, rot0, geom1, pos1, rot1, simplex);
				return hull.ComputeIntersection(pt0, n0, pt1, n1);
			};

			v = closestSimplexPt.m_MinkDif;
			double vSqr = v.Dot(v);
			if (vSqr < MIN_SUPPORT_SQR)
			{
				if (bypassPenetration)
				{
					pt0 = (closestSimplexPt.m_MinkSum + closestSimplexPt.m_MinkDif).Scale(0.5);
					pt1 = (closestSimplexPt.m_MinkSum - closestSimplexPt.m_MinkDif).Scale(0.5);
					return false;
				}
				else
				{
					return CheckPenetration();
				}
			}
			pt0 = geom0->Support(pos0, rot0, -v);
			pt1 = geom1->Support(pos1, rot1, v);
			dMinkowskiPoint newSimplexPt;
			newSimplexPt.m_MinkDif = pt0 - pt1;
			newSimplexPt.m_MinkSum = pt0 + pt1;
			const double dSqr = newSimplexPt.m_MinkDif.Dot(newSimplexPt.m_MinkDif);

			if (dSqr < MIN_SUPPORT_SQR)
			{
				return CheckPenetration();
			}
			else
			{
				const double vNorm = sqrt(vSqr);
				const dVec3 vHat = v.Scale(1.0 / vNorm);
				const double dCloser = fabs(newSimplexPt.m_MinkDif.Dot(vHat) - vNorm);
				if (dCloser < 0.001 || dCloser / vNorm < GJK_TERMINATION_RATIO)
				{
					pt0 = (closestSimplexPt.m_MinkSum + closestSimplexPt.m_MinkDif).Scale(0.5);
					pt1 = (closestSimplexPt.m_MinkSum - closestSimplexPt.m_MinkDif).Scale(0.5);
					n0 = pt0 - pt1;
					n1 = -n0;
					return false;
				}
			}
			// Add the newly found support point to the simplex
			simplex = closestFeature;
			simplex.AddPoint(newSimplexPt);
		}
	}
	pt0 = (closestSimplexPt.m_MinkSum + closestSimplexPt.m_MinkDif).Scale(0.5);
	pt1 = (closestSimplexPt.m_MinkSum - closestSimplexPt.m_MinkDif).Scale(0.5);
	n0 = pt0 - pt1;
	n1 = -n0;
	return false;
}