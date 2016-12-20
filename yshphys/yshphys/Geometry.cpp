#include "stdafx.h"
#include "Geometry.h"
#include "MathUtils.h"
#include "Point.h"
#include "Ray.h"

#define MIN_SUPPORT_SQR 0.0001
#define GJK_TERMINATION_RATIO 0.0001
#define DEGENERATE_SIMPLEX_RATIO 0.01

Geometry::Geometry() : m_pos(0.0, 0.0, 0.0), m_rot(0.0, 0.0, 0.0, 1.0)
{
}


Geometry::~Geometry()
{
}

dVec3 Geometry::GetPosition() const
{
	return m_pos;
}
dQuat Geometry::GetRotation() const
{
	return m_rot;
}
BoundingBox Geometry::GetLocalOOBB() const
{
	return m_localOOBB;
}

void Geometry::SetPosition(const dVec3& pos)
{
	m_pos = pos;
}

void Geometry::SetRotation(const dQuat& rot)
{
	m_rot = rot;
}

SupportPolygon Geometry::Support(const dVec3& x, const dQuat& q, const dVec3& v) const
{
	SupportPolygon poly = SupportLocal((-q).Transform(v));
	for (int i = 0; i < poly.nVertices; ++i)
	{
		poly.vertices[i] = x + q.Transform(poly.vertices[i]);
	}
	return poly;
}

SupportPolygon Geometry::SupportLocal(const dVec3& v) const
{
	SupportPolygon poly;
	poly.nVertices = 0;
	return poly;
}

bool Geometry::RayIntersect(const dVec3& pos, const dQuat& rot, const Ray& ray, dVec3& hit) const
{
	Simplex3D simplex;
	hit = ray.GetOrigin();
	const dVec3 rayDir = ray.GetDirection();

	Point pt;

	dVec3 closest;
	dVec3 dummy;

	while (true)
	{
		double sep = Geometry::ComputeSeparation(&pt, hit, dQuat::Identity(), dummy, this, pos, rot, closest, simplex);

		const dVec3 hit2closest = closest - hit;

		if (sep < 0.001)
		{
			return true;
		}

		if (hit2closest.Dot(hit2closest) < 0.001*0.001)
		{
			return true;
		}
		else if (hit2closest.Dot(rayDir) / sqrt(hit2closest.Dot(hit2closest)*rayDir.Dot(rayDir)) < 0.001)
		{
			// if the closest point is perpendicular to or behind the ray direction, then the ray missed (or will miss)
			return false;
		}

		// march the ray forward, and shift the simplex for the next GJK pass
		const dVec3 dHit = rayDir.Scale(hit2closest.Dot(rayDir) / rayDir.Dot(rayDir));
		hit = hit + dHit;

		dVec3 vertices[4];
		const int nVertices = simplex.GetVertices(vertices);
		for (int i = 0; i < nVertices; ++i)
		{
			vertices[i] = vertices[i] + dHit;
		}
		simplex.SetVertices(nVertices, vertices);
	}

}

double Geometry::ComputePenetration(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0, dVec3& pt0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1, dVec3& pt1,
	Simplex3D& tetrahedron)
{
	dVec3 simplex[4];
	if (tetrahedron.GetVertices(simplex) == 4)
	{
		int nIter = 0;

		while (nIter < 16)
		{
			nIter++;

			int iFace = 0;
			dVec3 nFace(0.0, 0.0, 0.0);
			double dFace = 88888888.0;

			for (int i = 0; i < 4; ++i)
			{
				const dVec3& a = simplex[(i + 0) % 4];
				const dVec3& b = simplex[(i + 1) % 4];
				const dVec3& c = simplex[(i + 2) % 4];
				const dVec3& d = simplex[(i + 3) % 4];

				dVec3 n = (c - b).Cross(d - b);
				const dVec3 asdf = b - a;
				const double qwer = n.Dot(asdf);
				n = n.Scale(MathUtils::sgn(n.Dot(b - a)));
				n.Scale(1.0 / sqrt(n.Dot(n)));

				double dist = b.Dot(n);

				if (dist > 0.0 && dist < dFace)
				{
					iFace = i;
					nFace = n;
					dFace = dist;
				}
			}
			assert(dFace < 8888.0f);

			if (dFace < MIN_SUPPORT_SQR)
			{
				tetrahedron.SetVertices(4, simplex);
				return 0.0;
			}

			SupportPolygon poly0, poly1;
			poly0 = geom0->Support(pos0, rot0, nFace);
			poly1 = geom1->Support(pos1, rot1, -nFace);
			SupportPolygon::ComputeSeparation(poly0, pt0, poly1, pt1);
			simplex[iFace] = pt0 - pt1;
			if (abs((simplex[iFace].Dot(nFace) - dFace) / dFace) < GJK_TERMINATION_RATIO)
			{
				tetrahedron.SetVertices(4, simplex);
				return sqrt(simplex[iFace].Dot(simplex[iFace]));
			}
		}
	}
	tetrahedron.SetVertices(4, simplex);
	return 0.0;
}
double Geometry::ComputeSeparation(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0, dVec3& pt0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1, dVec3& pt1)
{
	Simplex3D simplex;
	simplex.SetVertices(0, nullptr);

	return Geometry::ComputeSeparation(
		geom0, pos0, rot0, pt0,
		geom1, pos1, rot1, pt1,
		simplex);
}
double Geometry::ComputeSeparation(
	const Geometry* geom0, const dVec3& pos0, const dQuat& rot0, dVec3& pt0,
	const Geometry* geom1, const dVec3& pos1, const dQuat& rot1, dVec3& pt1,
	Simplex3D& simplex)
{
	SupportPolygon poly0, poly1;
	if (simplex.GetNumVertices() == 0)
	{
		// First pass is special. Don't check for termination since v is just a guess.
		dVec3 v(pos0 - pos1);
		v = dVec3(1.0, 0.0, 0.0);
		poly0 = geom0->Support(pos0, rot0, -v);
		poly1 = geom1->Support(pos1, rot1, v);
		SupportPolygon::ComputeSeparation(poly0, pt0, poly1, pt1);
		dVec3 newSimplexPt(pt0 - pt1);
		simplex.SetVertices(1, &newSimplexPt);
	}

	int nIter = 0;

	while (nIter < 16)
	{
		nIter++;
		// Get the closest point on the convex hull of the simplex, set it to the new support direction "v"
		// and discard any existing points on the simplex that are not needed to express "v"
		Simplex3D closestFeature;
		dVec3 v = simplex.ClosestPoint(dVec3(0.0, 0.0, 0.0), closestFeature);

		dVec3 featureVertices[4];
		const int nFeatureVertices = closestFeature.GetVertices(featureVertices);

		if (nFeatureVertices == 4)
		{
			return ComputePenetration(geom0, pos0, rot0, pt0, geom1, pos1, rot1, pt1, simplex);
		}
		else
		{
			dVec3 newSimplexPt;
			double vSqr = v.Dot(v);
			if (vSqr < MIN_SUPPORT_SQR)
			{
				dVec3 simplexVertices[4];
				const int nSimplexVertices = simplex.GetVertices(simplexVertices);
				switch (nSimplexVertices)
				{
				case 4:
					return ComputePenetration(geom0, pos0, rot0, pt0, geom1, pos1, rot1, pt1, simplex);
				case 3:
				{
					const dVec3 perp = (simplexVertices[1] - simplexVertices[0]).Cross(simplexVertices[2] - simplexVertices[0]);
					poly0 = geom0->Support(pos0, rot0, -perp);
					poly1 = geom1->Support(pos1, rot1, perp);
					SupportPolygon::ComputeSeparation(poly0, pt0, poly1, pt1);
					newSimplexPt = pt0 - pt1;
					simplex.AddVertex(newSimplexPt);
					return ComputePenetration(geom0, pos0, rot0, pt0, geom1, pos1, rot1, pt1, simplex);
				}
				case 2:
					return 0.0;
				case 1:
					return 0.0;
				default:
					assert(false);
				}
			}
			poly0 = geom0->Support(pos0, rot0, -v);
			poly1 = geom1->Support(pos1, rot1, v);
			SupportPolygon::ComputeSeparation(poly0, pt0, poly1, pt1);
			newSimplexPt = pt0 - pt1;
			const double dSqr(newSimplexPt.Dot(newSimplexPt));

			if (dSqr < MIN_SUPPORT_SQR)
			{
				return 0.0;
			}
			else if (fabs(newSimplexPt.Dot(v) - vSqr) / vSqr < GJK_TERMINATION_RATIO)
			{
				return sqrt(dSqr);
			}
#if 0
			else if (nVertices == 3)
			{
				const dVec3 area = (vertices[1] - vertices[0]).Cross(vertices[2] - vertices[0]);
				const double proj =(newSimplexPt - vertices[0]).Dot(area);
				const double areaSqr = area.Dot(area);

				const double numerator = proj*proj*proj*proj;
				const double denominator = areaSqr*areaSqr*areaSqr;

				const double degeneracyRatio = DEGENERATE_SIMPLEX_RATIO*DEGENERATE_SIMPLEX_RATIO*DEGENERATE_SIMPLEX_RATIO*DEGENERATE_SIMPLEX_RATIO;

				if (numerator / denominator < degeneracyRatio)
				{
//					const dVec3 perp = area.Scale(proj / areaSqr);
//					if (degen0)
//					{
//						pt0 = pt1 + perp;
//					}
//					else if (degen1)
//					{
//						pt1 = pt0 - perp;
//					}
					return sqrt(vSqr);
				}
			}
			else if (nVertices == 2)
			{
				const dVec3 line = vertices[1] - vertices[0];
				const dVec3 u = (newSimplexPt - vertices[0]);
				const dVec3 perp = u - line.Scale(u.Dot(line) / line.Dot(line));

				const double degeneracyRatio = DEGENERATE_SIMPLEX_RATIO*DEGENERATE_SIMPLEX_RATIO;

				if (perp.Dot(perp) / line.Dot(line) < degeneracyRatio)
				{
//					if (degen0)
//					{
//						pt0 = pt1 + perp;
//					}
//					else if (degen1)
//					{
//						pt1 = pt0 - perp;
//					}
					return sqrt(vSqr);
				}
			}
#endif

			// Add the newly found support point to the simplex
			simplex = closestFeature;
			simplex.AddVertex(newSimplexPt);
		}
	}
//	assert(0);
	return 88888888.0;
}