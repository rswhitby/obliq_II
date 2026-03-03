// cmdObliqueMake2D.cpp : command file
//
// Runs the custom oblique hidden-line engine and adds the resulting
// visible / hidden curves to the document on dedicated layers.
//

#include "stdafx.h"
#include "ObliqPlugIn.h"
#include "ObliqueConduit.h"
#include "ObliqueHiddenLine.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// BEGIN ObliqueMake2D command
//

#pragma region ObliqueMake2D command

class CCommandObliqueMake2D : public CRhinoCommand
{
public:
    CCommandObliqueMake2D() = default;
    ~CCommandObliqueMake2D() = default;

    UUID CommandUUID() override
    {
        // {EC82F5A2-F35E-4EC5-A85E-EDA8789E6E35}
        static const GUID ObliqueMake2D_UUID =
        { 0xec82f5a2, 0xf35e, 0x4ec5, { 0xa8, 0x5e, 0xed, 0xa8, 0x78, 0x9e, 0x6e, 0x35 } };

        return ObliqueMake2D_UUID;
    }

    const wchar_t* EnglishCommandName() override { return L"ObliqueMake2D"; }

    CRhinoCommand::result RunCommand(const CRhinoCommandContext& context) override;

private:
    // Helpers
    int  FindOrCreateLayer(CRhinoDoc* doc, const wchar_t* name, COLORREF color);
    void FlattenCurveTo2D(ON_Curve* crv);
};

static class CCommandObliqueMake2D theObliqueMake2DCommand;


// ============================================================================
// RunCommand
// ============================================================================

CRhinoCommand::result CCommandObliqueMake2D::RunCommand(const CRhinoCommandContext& context)
{
    CRhinoDoc* doc = context.Document();
    if (!doc)
        return CRhinoCommand::failure;

    // ------------------------------------------------------------------
    // 1. Build the oblique shear transform (same params as the conduit)
    // ------------------------------------------------------------------
    double angle_deg = 90.0;
    double scale = 1.0;

    double alpha = angle_deg * ON_PI / 180.0;
    double shx = scale * cos(alpha);
    double shy = scale * sin(alpha);

    ON_Xform shear = ON_Xform::IdentityTransformation;
    shear.m_xform[0][2] = shx;   // new_x = x + shx*z
    shear.m_xform[1][2] = shy;   // new_y = y + shy*z

    // ------------------------------------------------------------------
    // 2. Configure and run the hidden-line engine
    // ------------------------------------------------------------------
    RhinoApp().Print(L"ObliqueMake2D: Collecting geometry...\n");

    CObliqueHiddenLineEngine hld;
    hld.SetTransform(shear);
    hld.SetSampleDensity(128);         // samples per edge curve
    hld.SetDepthTolerance(1e-3);       // Z tolerance for occlusion
    hld.SetEdgeAngleThreshold(9.0);   // degrees � edges sharper than this are included
    hld.AddObjectsFromDoc(doc);

    RhinoApp().Print(L"ObliqueMake2D: Computing hidden lines...\n");

    if (!hld.Compute())
    {
        RhinoApp().Print(L"ObliqueMake2D: Computation failed.\n");
        return CRhinoCommand::failure;
    }

    int total = hld.ResultCount();
    if (total == 0)
    {
        RhinoApp().Print(L"ObliqueMake2D: No edges found.\n");
        return CRhinoCommand::nothing;
    }

    RhinoApp().Print(L"ObliqueMake2D: %d segments computed.\n", total);

    // ------------------------------------------------------------------
    // 3. Prepare output layers
    // ------------------------------------------------------------------
    int layer_visible = FindOrCreateLayer(doc, L"ObliqueMake2D::Visible", RGB(0, 0, 0));
    int layer_hidden = FindOrCreateLayer(doc, L"ObliqueMake2D::Hidden", RGB(128, 128, 128));

    // ------------------------------------------------------------------
    // 4. Add result curves to the document
    // ------------------------------------------------------------------
    ON_SimpleArray<CClassifiedSegment> results;
    hld.DetachResults(results);

    int count_vis = 0, count_hid = 0;

    for (int i = 0; i < results.Count(); i++)
    {
        CClassifiedSegment& seg = results[i];
        if (!seg.m_curve)
            continue;

        // Flatten to XY plane (drop Z) for a clean 2D drawing
        FlattenCurveTo2D(seg.m_curve);

        // Build object attributes
        ON_3dmObjectAttributes attrs;
        attrs.m_uuid = ON_nil_uuid;  // let Rhino assign

        if (seg.m_visibility == EHiddenLineVisibility::Visible)
        {
            attrs.m_layer_index = layer_visible;
            attrs.m_color = RGB(0, 0, 0);
            count_vis++;
        }
        else
        {
            attrs.m_layer_index = layer_hidden;
            attrs.m_color = RGB(128, 128, 128);
            // Set a dashed linetype if one exists
            int dash_idx = doc->m_linetype_table.FindLinePatternFromName(L"Dashed", true, -1);
            if (dash_idx >= 0)
                attrs.m_linetype_index = dash_idx;
            count_hid++;
        }

        attrs.SetColorSource(ON::color_from_object);
        attrs.SetLinetypeSource(ON::linetype_from_object);

        // Add curve to doc � Rhino takes ownership of the curve
        doc->AddCurveObject(*seg.m_curve, &attrs);
        delete seg.m_curve;
        seg.m_curve = nullptr;
    }

    doc->Redraw();

    RhinoApp().Print(L"ObliqueMake2D: Done � %d visible, %d hidden segments.\n",
        count_vis, count_hid);

    return CRhinoCommand::success;
}


// ============================================================================
// Helpers
// ============================================================================

int CCommandObliqueMake2D::FindOrCreateLayer(CRhinoDoc* doc, const wchar_t* name, COLORREF color)
{
    int idx = doc->m_layer_table.FindLayerFromName(name, false, false, -1, -1);

    if (idx >= 0)
        return idx;

    ON_Layer layer;
    layer.SetName(name);
    layer.SetColor(color);
    idx = doc->m_layer_table.AddLayer(layer);
    return idx;
}

void CCommandObliqueMake2D::FlattenCurveTo2D(ON_Curve* crv)
{
    if (!crv) return;

    // Project curve to the XY plane by setting Z = 0
    // For polyline curves / NURBS, we transform control points directly.
    ON_NurbsCurve* nc = nullptr;
    if (crv->IsKindOf(&ON_NurbsCurve::m_ON_NurbsCurve_class_rtti))
    {
        nc = static_cast<ON_NurbsCurve*>(crv);
    }
    else
    {
        // Try getting a nurbs form
        nc = crv->NurbsCurve();
        // Note: this creates a new curve, but since we're modifying in-place
        // we handle it differently. For simplicity, use a transform.
    }

    // Safest approach: use a projection transform
    ON_Xform flatten = ON_Xform::IdentityTransformation;
    flatten.m_xform[2][2] = 0.0;  // Z = 0
    crv->Transform(flatten);
}

#pragma endregion

//
// END ObliqueMake2D command
//
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////