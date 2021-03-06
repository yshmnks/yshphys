#pragma once
#include "Geometry.h"

class QuickHull;

class Mesh :
	public Geometry
{
	friend class QuickHull;
public:
	struct HalfEdge;
	struct Face;

	// For meshes, we use indices rather than pointers for the HalfEdge data structure. This is because meshes are static (unchanging),
	// so naturally, all of our vertices, halfedges, and faces will be allocated contiguously as flat arrays. For EPA and Quickhull
	// halfEdges and faces are constantly added and removed, so it is more natural to use pointers, as the memory becomes fragmented.

	struct HalfEdge
	{
		int				iVert; // Our Convention: the vertex at the TIP of the HalfEdge (as opposed to at the TAIL)

		int				iNext;
		int				iPrev;
		int				iTwin;

		int				iFace;

		mutable bool visited;

		HalfEdge() : iVert(-1), iNext(-1), iPrev(-1), iTwin(-1), iFace(-1), visited(false) {}
	};
	struct Face
	{
		dVec3			normal;
		int				iEdge;

		mutable bool visited;

		Face() : normal(0.0, 0.0, 0.0), iEdge(-1), visited(false) {}
	};

	Mesh();
	Mesh(const fVec3* const pointCloud, int nPoints);
	virtual ~Mesh();

	void AllocateMesh(int nEdges, int nFaces, int nVerts);

	virtual dVec3 SupportLocal(const dVec3& v) const;

	dVec3 CenterOfMassLocal_Solid(double& volume) const;

	dMat33 InertiaLocal_Solid(double kilogramPerCubicMeter, double& mass) const;

	void ShiftToCenterOfMass_Solid();

	struct FaceList
	{
		friend class Mesh;
	public:
		struct Face
		{
			std::vector<fVec3> verts;
			dVec3 normal;
		};

		int GetNumFaces() const;
		FaceList::Face GetFace(int i) const;

		void AddFace(const fVec3* verts, int nVerts, const dVec3& normal);

	private:
		struct FacePartition
		{
			int iFirstVert;
			dVec3 normal;
		};
		std::vector<fVec3> m_verts;
		std::vector<FacePartition> m_faces;
	};
	struct PolygonalFace
	{
		std::vector<fVec3> verts;
		dVec3 normal;
	};

	FaceList GetFaces() const;

protected:

	// entryEdge is the starting point of our steepest descent search.
	// The returned HalfEdge's vert is the support point.
	int ISupportEdgeLocal(const dVec3& v, int iEntryEdge) const;
	void InitCardinalEdges();

	HalfEdge*	m_halfEdges;
	int			m_nHalfEdges;
	Face*		m_faces;
	int			m_nFaces;
	fVec3*		m_verts;
	int			m_nVerts;

	// m_iCardinalEdge[i][0] corresponds to the edge pointing to the vertex that is minimal in the ith-direction
	// m_iCardinalEdge[i][1] corresponds to the edge pointing to the vertex that is maximal in the ith-direction
	// i=0 corresponds to the x direction
	// i=1 corresponds to the y direction
	// i=2 corresponds to the z direction
	// (This is to slightly accelerate the Support function)

	int			m_iCardinalEdges[3][2];
};

