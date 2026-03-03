// ObliqueHiddenLine.h
// Custom hidden-line engine for oblique projections.
// Brute-force approach: no spatial acceleration, just direct
// triangle-by-triangle occlusion testing.
//
// Include AFTER stdafx.h / RhinoSdk.h

#pragma once

// ============================================================================
// Types
// ============================================================================

enum class EHiddenLineVisibility
{
    Visible,
    Hidden
};

struct CClassifiedSegment
{
    ON_Curve* m_curve;
    EHiddenLineVisibility  m_visibility;
    ON_UUID                m_source_object;

    CClassifiedSegment()
        : m_curve(nullptr)
        , m_visibility(EHiddenLineVisibility::Visible)
        , m_source_object(ON_nil_uuid)
    {
    }
};

// ============================================================================
// Projected triangle - stores 2D footprint + depth for occlusion
// ============================================================================

struct CProjectedTriangle
{
    ON_2dPoint  m_v2d[3];
    double      m_depth[3];   // Z in sheared space
    int         m_source_idx; // which object this came from

    CProjectedTriangle() : m_source_idx(-1)
    {
        m_depth[0] = m_depth[1] = m_depth[2] = 0.0;
    }

    // Barycentric point-in-triangle + depth interpolation
    bool TestPoint(const ON_2dPoint& p, double& depth_out) const
    {
        double x0 = m_v2d[0].x, y0 = m_v2d[0].y;
        double x1 = m_v2d[1].x, y1 = m_v2d[1].y;
        double x2 = m_v2d[2].x, y2 = m_v2d[2].y;

        double denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
        if (fabs(denom) < 1e-12)
            return false;

        double u = ((y1 - y2) * (p.x - x2) + (x2 - x1) * (p.y - y2)) / denom;
        double v = ((y2 - y0) * (p.x - x2) + (x0 - x2) * (p.y - y2)) / denom;
        double w = 1.0 - u - v;

        if (u < -1e-9 || v < -1e-9 || w < -1e-9)
            return false;

        depth_out = u * m_depth[0] + v * m_depth[1] + w * m_depth[2];
        return true;
    }
};

// ============================================================================
// Main engine
// ============================================================================

class CObliqueHiddenLineEngine
{
public:

    // -- Config --

    void SetTransform(const ON_Xform& xf) { m_xform = xf; }
    void SetDepthTolerance(double tol) { m_depth_tol = tol; }
    void SetSampleDensity(int n) { m_sample_count = (n < 8) ? 8 : n; }
    void SetEdgeAngleThreshold(double deg) { m_edge_angle_rad = deg * ON_PI / 180.0; }

    // -- Input --

    void AddObjectsFromDoc(CRhinoDoc* doc)
    {
        if (!doc) return;

        CRhinoObjectIterator it(*doc, CRhinoObjectIterator::undeleted_objects);
        it.EnableVisibleFilter(true);
        it.IncludeLights(false);

        for (const CRhinoObject* obj = it.First(); obj; obj = it.Next())
        {
            const ON_Geometry* geo = obj->Geometry();
            if (!geo) continue;

            // -- Brep for edge extraction --
            const ON_Brep* brep = ON_Brep::Cast(geo);
            const ON_Extrusion* extr = ON_Extrusion::Cast(geo);

            if (extr)
            {
                ON_Brep* eb = extr->BrepForm(nullptr);
                if (eb) { m_breps.Append(eb); m_owns_brep.Append(true); }
            }
            else if (brep)
            {
                m_breps.Append(const_cast<ON_Brep*>(brep));
                m_owns_brep.Append(false);
            }

            // -- Render mesh for occlusion --
            // Force creation if not present
            const_cast<CRhinoObject*>(obj)->CreateMeshes(
                ON::render_mesh, ON_MeshParameters::DefaultMesh);

            ON_SimpleArray<const ON_Mesh*> mesh_list;
            obj->GetMeshes(ON::render_mesh, mesh_list);
            for (int mi = 0; mi < mesh_list.Count(); mi++)
            {
                if (mesh_list[mi])
                {
                    m_meshes.Append(const_cast<ON_Mesh*>(mesh_list[mi]));
                    m_owns_mesh.Append(false);
                }
            }

            m_object_ids.Append(obj->Attributes().m_uuid);
        }

        RhinoApp().Print(L"HLD: %d breps, %d meshes collected.\n",
            m_breps.Count(), m_meshes.Count());
    }

    // -- Compute --

    bool Compute()
    {
        m_results.SetCount(0);

        // Phase 1: Extract and transform edges
        ON_SimpleArray<ON_Curve*> edges;
        ON_SimpleArray<int> edge_owner;
        ExtractEdges(edges, edge_owner);

        RhinoApp().Print(L"HLD: %d edges extracted.\n", edges.Count());

        // Phase 2: Project mesh triangles
        ProjectTriangles();

        RhinoApp().Print(L"HLD: %d triangles projected.\n", m_tris.Count());

        // Phase 3: Classify each edge
        for (int i = 0; i < edges.Count(); i++)
        {
            ON_Curve* edge = edges[i];
            if (!edge) continue;

            ON_UUID owner = ON_nil_uuid;
            if (edge_owner[i] >= 0 && edge_owner[i] < m_object_ids.Count())
                owner = m_object_ids[edge_owner[i]];

            ClassifyEdge(edge, owner);
        }

        RhinoApp().Print(L"HLD: %d segments classified.\n", m_results.Count());
        return true;
    }

    // -- Output --

    int ResultCount() const { return m_results.Count(); }
    const CClassifiedSegment& Result(int i) const { return m_results[i]; }

    void DetachResults(ON_SimpleArray<CClassifiedSegment>& out)
    {
        out.Append(m_results.Count(), m_results.Array());
        m_results.SetCount(0);
    }

    // -- Lifecycle --

    CObliqueHiddenLineEngine()
        : m_xform(ON_Xform::IdentityTransformation)
        , m_depth_tol(0.01)
        , m_sample_count(64)
        , m_edge_angle_rad(20.0 * ON_PI / 180.0)
    {
    }

    ~CObliqueHiddenLineEngine() { Cleanup(); }

    void Cleanup()
    {
        for (int i = 0; i < m_breps.Count(); i++)
            if (m_owns_brep[i]) delete m_breps[i];
        m_breps.SetCount(0);
        m_owns_brep.SetCount(0);

        for (int i = 0; i < m_meshes.Count(); i++)
            if (m_owns_mesh[i]) delete m_meshes[i];
        m_meshes.SetCount(0);
        m_owns_mesh.SetCount(0);

        m_object_ids.SetCount(0);
        m_tris.SetCount(0);

        for (int i = 0; i < m_results.Count(); i++)
            delete m_results[i].m_curve;
        m_results.SetCount(0);
    }

private:

    // ================================================================
    // Phase 1: Edge extraction
    // ================================================================

    void ExtractEdges(ON_SimpleArray<ON_Curve*>& edges_out,
        ON_SimpleArray<int>& owner_out)
    {
        // Precompute inverse-transpose for normal transformation
        ON_Xform inv_t = m_xform;
        inv_t.Invert();
        inv_t.Transpose();
        ON_3dVector view_dir(0.0, 0.0, -1.0);

        // A. Brep sharp/boundary edges
        for (int bi = 0; bi < m_breps.Count(); bi++)
        {
            const ON_Brep* brep = m_breps[bi];
            if (!brep) continue;

            for (int ei = 0; ei < brep->m_E.Count(); ei++)
            {
                const ON_BrepEdge& edge = brep->m_E[ei];

                if (edge.m_ti.Count() < 2 || IsSharpEdge(brep, edge))
                {
                    ON_Curve* dup = edge.DuplicateCurve();
                    if (dup)
                    {
                        dup->Transform(m_xform);
                        edges_out.Append(dup);
                        owner_out.Append(bi);
                    }
                }
            }
        }

        // B. Mesh silhouettes (catches curved surfaces like spheres)
        for (int mi = 0; mi < m_meshes.Count(); mi++)
        {
            ON_Mesh* mesh = m_meshes[mi];
            if (!mesh || mesh->FaceCount() == 0) continue;

            mesh->ComputeFaceNormals();
            if (mesh->m_FN.Count() != mesh->FaceCount()) continue;

            const ON_MeshTopology& topo = mesh->Topology();

            for (int ei = 0; ei < topo.m_tope.Count(); ei++)
            {
                const ON_MeshTopologyEdge& te = topo.m_tope[ei];

                bool is_sil = false;

                if (te.m_topf_count < 2)
                {
                    is_sil = true;
                }
                else if (te.m_topf_count >= 2)
                {
                    ON_3dVector n0 = ON_3dVector(mesh->m_FN[te.m_topfi[0]]);
                    ON_3dVector n1 = ON_3dVector(mesh->m_FN[te.m_topfi[1]]);
                    n0 = inv_t * n0;
                    n1 = inv_t * n1;

                    double d0 = n0 * view_dir;
                    double d1 = n1 * view_dir;

                    if ((d0 > 0.0 && d1 < 0.0) || (d0 < 0.0 && d1 > 0.0))
                        is_sil = true;
                }

                if (is_sil)
                {
                    ON_3dPoint p0 = ON_3dPoint(
                        mesh->m_V[topo.m_topv[te.m_topvi[0]].m_vi[0]]);
                    ON_3dPoint p1 = ON_3dPoint(
                        mesh->m_V[topo.m_topv[te.m_topvi[1]].m_vi[0]]);

                    p0 = m_xform * p0;
                    p1 = m_xform * p1;

                    ON_LineCurve* lc = new ON_LineCurve(p0, p1);
                    if (lc)
                    {
                        edges_out.Append(lc);
                        owner_out.Append(mi);
                    }
                }
            }
        }
    }

    bool IsSharpEdge(const ON_Brep* brep, const ON_BrepEdge& edge) const
    {
        if (edge.m_ti.Count() < 2) return true;

        const ON_BrepTrim& trim0 = brep->m_T[edge.m_ti[0]];
        const ON_BrepTrim& trim1 = brep->m_T[edge.m_ti[1]];
        int fi0 = trim0.FaceIndexOf();
        int fi1 = trim1.FaceIndexOf();
        if (fi0 < 0 || fi1 < 0) return true;

        const ON_BrepFace& face0 = brep->m_F[fi0];
        const ON_BrepFace& face1 = brep->m_F[fi1];

        double t_mid = edge.Domain().ParameterAt(0.5);
        ON_3dPoint pt_mid = edge.PointAt(t_mid);

        double u0, v0, u1, v1;
        if (!face0.GetClosestPoint(pt_mid, &u0, &v0) ||
            !face1.GetClosestPoint(pt_mid, &u1, &v1))
            return true;

        ON_3dVector n0 = face0.NormalAt(u0, v0);
        ON_3dVector n1 = face1.NormalAt(u1, v1);
        if (face0.m_bRev) n0 = -n0;
        if (face1.m_bRev) n1 = -n1;

        double dot = n0 * n1;
        if (dot < -1.0) dot = -1.0;
        else if (dot > 1.0) dot = 1.0;

        return acos(dot) > m_edge_angle_rad;
    }

    // ================================================================
    // Phase 2: Project all mesh triangles
    // ================================================================

    void ProjectTriangles()
    {
        m_tris.SetCount(0);

        for (int mi = 0; mi < m_meshes.Count(); mi++)
        {
            const ON_Mesh* mesh = m_meshes[mi];
            if (!mesh) continue;

            int vc = mesh->VertexCount();
            if (vc == 0) continue;

            // Transform all verts once
            ON_SimpleArray<ON_3dPoint> xv;
            xv.SetCapacity(vc);
            xv.SetCount(vc);
            for (int vi = 0; vi < vc; vi++)
                xv[vi] = m_xform * ON_3dPoint(mesh->m_V[vi]);

            int fc = mesh->FaceCount();
            for (int fi = 0; fi < fc; fi++)
            {
                const ON_MeshFace& f = mesh->m_F[fi];
                AddTri(xv, f.vi[0], f.vi[1], f.vi[2], mi);
                if (f.IsQuad())
                    AddTri(xv, f.vi[0], f.vi[2], f.vi[3], mi);
            }
        }
    }

    void AddTri(const ON_SimpleArray<ON_3dPoint>& xv,
        int i0, int i1, int i2, int src)
    {
        int vc = xv.Count();
        if (i0 < 0 || i0 >= vc || i1 < 0 || i1 >= vc || i2 < 0 || i2 >= vc)
            return;

        // Skip degenerate
        double area = fabs(
            (xv[i1].x - xv[i0].x) * (xv[i2].y - xv[i0].y) -
            (xv[i2].x - xv[i0].x) * (xv[i1].y - xv[i0].y));
        if (area < 1e-12)
            return;

        CProjectedTriangle tri;
        tri.m_v2d[0] = ON_2dPoint(xv[i0].x, xv[i0].y);
        tri.m_v2d[1] = ON_2dPoint(xv[i1].x, xv[i1].y);
        tri.m_v2d[2] = ON_2dPoint(xv[i2].x, xv[i2].y);
        tri.m_depth[0] = xv[i0].z;
        tri.m_depth[1] = xv[i1].z;
        tri.m_depth[2] = xv[i2].z;
        tri.m_source_idx = src;
        m_tris.Append(tri);
    }

    // ================================================================
    // Phase 3: Classify edges via brute-force sampling
    // ================================================================

    void ClassifyEdge(ON_Curve* edge, ON_UUID owner_id)
    {
        ON_Interval dom = edge->Domain();

        // Sample visibility along the curve
        ON_SimpleArray<double> params;
        ON_SimpleArray<bool>   vis;
        params.SetCapacity(m_sample_count + 1);
        vis.SetCapacity(m_sample_count + 1);

        for (int s = 0; s <= m_sample_count; s++)
        {
            double t = dom.ParameterAt((double)s / (double)m_sample_count);
            ON_3dPoint pt = edge->PointAt(t);
            params.Append(t);
            vis.Append(IsPointVisible(pt));
        }

        // Walk samples, split at transitions
        int seg_start = 0;
        for (int s = 1; s <= m_sample_count; s++)
        {
            bool changed = (vis[s] != vis[seg_start]);
            bool last = (s == m_sample_count);

            if (changed || last)
            {
                double t_end;
                if (changed)
                    t_end = RefineTransition(edge, params[s - 1], params[s], vis[seg_start]);
                else
                    t_end = params[s];

                ON_Interval sub(params[seg_start], t_end);
                if (sub.Length() > 1e-10)
                {
                    ON_Curve* seg = edge->DuplicateCurve();
                    if (seg && seg->Trim(sub))
                    {
                        CClassifiedSegment cs;
                        cs.m_curve = seg;
                        cs.m_visibility = vis[seg_start]
                            ? EHiddenLineVisibility::Visible
                            : EHiddenLineVisibility::Hidden;
                        cs.m_source_object = owner_id;
                        m_results.Append(cs);
                    }
                    else
                    {
                        delete seg;
                    }
                }

                if (changed)
                {
                    params[s] = t_end;
                    seg_start = s;
                }
            }
        }

        delete edge;
    }

    double RefineTransition(ON_Curve* edge, double t0, double t1,
        bool vis_at_t0, int iterations = 12)
    {
        for (int i = 0; i < iterations; i++)
        {
            double tm = 0.5 * (t0 + t1);
            bool v = IsPointVisible(edge->PointAt(tm));
            if (v == vis_at_t0)
                t0 = tm;
            else
                t1 = tm;
        }
        return 0.5 * (t0 + t1);
    }

    // Brute-force: test point against every triangle
    bool IsPointVisible(const ON_3dPoint& pt) const
    {
        ON_2dPoint p2d(pt.x, pt.y);
        double pt_z = pt.z;

        for (int i = 0; i < m_tris.Count(); i++)
        {
            double tri_z = 0.0;
            if (m_tris[i].TestPoint(p2d, tri_z))
            {
                // Higher Z = closer to camera in top-down -Z view
                if (tri_z > pt_z + m_depth_tol)
                    return false;
            }
        }
        return true;
    }

    // ================================================================
    // Data
    // ================================================================

    ON_Xform  m_xform;
    double    m_depth_tol;
    int       m_sample_count;
    double    m_edge_angle_rad;

    ON_SimpleArray<ON_Brep*>  m_breps;
    ON_SimpleArray<bool>      m_owns_brep;
    ON_SimpleArray<ON_Mesh*>  m_meshes;
    ON_SimpleArray<bool>      m_owns_mesh;
    ON_SimpleArray<ON_UUID>   m_object_ids;

    ON_SimpleArray<CProjectedTriangle>     m_tris;
    ON_SimpleArray<CClassifiedSegment>     m_results;
};