// cmdObliqCavalier.cpp
//
// Creates a floating viewport showing a cavalier oblique projection.
// The user picks which world face is the elevation plane (Front/Right/Back/Left)
// then sets the receding angle (default 45 deg) and scale (default 1.0 = cavalier,
// 0.5 = cabinet).
//
// Math: the shear matrix is M = I + outer(v, look_dir) where
//   v = scale*(cos(angle)*screen_right + sin(angle)*screen_up)
// This adds the depth component of each point along the receding direction,
// producing a correct cavalier oblique in the parallel viewport.
//

#include "stdafx.h"
#include "ObliqPlugIn.h"
#include "ObliqueConduit.h"
#include "ObliqueHiddenLine.h"

#pragma region ObliqCavalier command

// ── Plane presets ──────────────────────────────────────────────────────────
// Each entry defines:
//   cam_offset   : unit direction from scene origin to camera
//   look_dir     : into the scene (= -cam_offset)
//   screen_right : camera's right axis in world space (= cross(look_dir, cam_up))
//   cam_up       : camera up (always world +Z for the four cardinal views)

struct CavalierPlane
{
    const wchar_t* name;
    ON_3dVector    cam_offset;   // unit vector, camera sits at cam_offset * dist
    ON_3dVector    look_dir;     // into the scene
    ON_3dVector    screen_right; // camera right in world
};

static const CavalierPlane s_planes[] =
{
    //         name       cam_offset                  look_dir                   screen_right
    { L"Front", ON_3dVector( 0,-1, 0), ON_3dVector( 0, 1, 0), ON_3dVector( 1, 0, 0) },
    { L"Right", ON_3dVector( 1, 0, 0), ON_3dVector(-1, 0, 0), ON_3dVector( 0, 1, 0) },
    { L"Back",  ON_3dVector( 0, 1, 0), ON_3dVector( 0,-1, 0), ON_3dVector(-1, 0, 0) },
    { L"Left",  ON_3dVector(-1, 0, 0), ON_3dVector( 1, 0, 0), ON_3dVector( 0,-1, 0) },
};
static const int s_plane_count = 4;

// ── Shear matrix builder ───────────────────────────────────────────────────
// Builds M = I + outer(v, look_dir) where v = cs*right + ss*up.
// screen_up is always world +Z for all standard cavalier views.

static ON_Xform BuildCavalierShear(
    const ON_3dVector& look_dir,
    const ON_3dVector& screen_right,
    double angle_deg,
    double scale)
{
    static const ON_3dVector screen_up(0.0, 0.0, 1.0);

    const double a  = angle_deg * ON_PI / 180.0;
    const double cs = scale * cos(a);
    const double ss = scale * sin(a);

    // v = cs * screen_right + ss * screen_up
    const ON_3dVector v(
        cs * screen_right.x + ss * screen_up.x,
        cs * screen_right.y + ss * screen_up.y,
        cs * screen_right.z + ss * screen_up.z);

    ON_Xform xf = ON_Xform::IdentityTransformation;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            xf.m_xform[i][j] += v[i] * look_dir[j];

    return xf;
}

// ── Command ────────────────────────────────────────────────────────────────

class CCommandObliqCavalier : public CRhinoCommand
{
public:
    CCommandObliqCavalier() = default;
    ~CCommandObliqCavalier() = default;

    UUID CommandUUID() override
    {
        // {C3A5F2B8-9E47-4D61-B023-71A3C94E8F12}
        static const GUID uuid =
        { 0xc3a5f2b8, 0x9e47, 0x4d61, { 0xb0, 0x23, 0x71, 0xa3, 0xc9, 0x4e, 0x8f, 0x12 } };
        return uuid;
    }

    const wchar_t* EnglishCommandName() override { return L"ObliqCavalier"; }

    CRhinoCommand::result RunCommand(const CRhinoCommandContext& context) override;
};

static class CCommandObliqCavalier theObliqCavalierCommand;

CRhinoCommand::result CCommandObliqCavalier::RunCommand(const CRhinoCommandContext& context)
{
    CRhinoDoc* doc = context.Document();
    if (!doc)
        return CRhinoCommand::failure;

    // ── 1. Options loop ────────────────────────────────────────────────────
    int    plane_idx = 0;   // Front
    double angle_deg = 45.0;
    double scale     = 1.0; // 1.0 = cavalier, 0.5 = cabinet

    CRhinoGetOption getOpt;
    getOpt.SetCommandPrompt(L"Cavalier oblique options. Press Enter to create viewport");
    getOpt.AcceptNothing(true);

    for (;;)
    {
        getOpt.ClearCommandOptions();

        ON_wString angle_str, scale_str;
        angle_str.Format(L"%.1f", angle_deg);
        scale_str.Format(L"%.2f", scale);

        int ix_plane = getOpt.AddCommandOption(
            RHCMDOPTNAME(L"Plane"),
            RHCMDOPTVALUE(s_planes[plane_idx].name));
        int ix_angle = getOpt.AddCommandOption(
            RHCMDOPTNAME(L"Angle"),
            RHCMDOPTVALUE(angle_str));
        int ix_scale = getOpt.AddCommandOption(
            RHCMDOPTNAME(L"Scale"),
            RHCMDOPTVALUE(scale_str));

        CRhinoGet::result res = getOpt.GetOption();

        if (res == CRhinoGet::nothing)
            break;
        if (res == CRhinoGet::cancel)
            return CRhinoCommand::cancel;

        if (res == CRhinoGet::option)
        {
            const CRhinoCommandOption* opt = getOpt.Option();
            if (!opt) continue;

            if (opt->m_option_index == ix_plane)
            {
                // Sub-prompt: pick the elevation plane
                CRhinoGetOption gp;
                gp.SetCommandPrompt(L"Select elevation plane");
                gp.AcceptNothing(true);
                int ix[4];
                ix[0] = gp.AddCommandOption(RHCMDOPTNAME(L"Front"), RHCMDOPTVALUE(L""));
                ix[1] = gp.AddCommandOption(RHCMDOPTNAME(L"Right"), RHCMDOPTVALUE(L""));
                ix[2] = gp.AddCommandOption(RHCMDOPTNAME(L"Back"),  RHCMDOPTVALUE(L""));
                ix[3] = gp.AddCommandOption(RHCMDOPTNAME(L"Left"),  RHCMDOPTVALUE(L""));

                if (gp.GetOption() == CRhinoGet::option)
                {
                    int oi = gp.Option()->m_option_index;
                    for (int i = 0; i < s_plane_count; i++)
                        if (oi == ix[i]) { plane_idx = i; break; }
                }
            }
            else if (opt->m_option_index == ix_angle)
            {
                CRhinoGetNumber gn;
                gn.SetCommandPrompt(L"Receding angle in degrees (0-360)");
                gn.SetDefaultNumber(angle_deg);
                gn.SetLowerLimit(0.0, false);
                gn.SetUpperLimit(360.0, false);
                if (gn.GetNumber() == CRhinoGet::number)
                    angle_deg = gn.Number();
            }
            else if (opt->m_option_index == ix_scale)
            {
                CRhinoGetNumber gn;
                gn.SetCommandPrompt(L"Receding scale (1.0=cavalier, 0.5=cabinet)");
                gn.SetDefaultNumber(scale);
                gn.SetLowerLimit(0.001, false);
                gn.SetUpperLimit(10.0, false);
                if (gn.GetNumber() == CRhinoGet::number)
                    scale = gn.Number();
            }
            continue;
        }
        break;
    }

    // ── 2. Record existing viewport IDs ────────────────────────────────────
    ON_SimpleArray<ON_UUID> existing_ids;
    ON_SimpleArray<CRhinoView*> view_list;
    doc->GetViewList(view_list, CRhinoView::ViewTypeFilter::Model);
    for (int i = 0; i < view_list.Count(); i++)
        if (view_list[i])
            existing_ids.Append(view_list[i]->ActiveViewportID());
    view_list.Empty();

    // ── 3. Create new view ─────────────────────────────────────────────────
    doc->NewView(ON_3dmView(), true);

    // Find the new view by diffing against the old ID list
    CRhinoView* new_view = nullptr;
    doc->GetViewList(view_list, CRhinoView::ViewTypeFilter::Model);
    for (int i = 0; i < view_list.Count(); i++)
    {
        CRhinoView* v = view_list[i];
        if (v && existing_ids.Search(v->ActiveViewportID()) < 0)
        {
            new_view = v;
            break;
        }
    }

    if (!new_view)
    {
        RhinoApp().Print(L"ObliqCavalier: Failed to find new viewport.\n");
        return CRhinoCommand::failure;
    }

    // ── 4. Configure viewport camera ───────────────────────────────────────
    const CavalierPlane& plane = s_planes[plane_idx];
    static const double cam_dist = 1000.0;
    static const double hw = 30.0, hh = 30.0;
    static const ON_3dVector cam_up(0.0, 0.0, 1.0);

    ON_Viewport onvp = new_view->ActiveViewport().VP();
    onvp.SetProjection(ON::parallel_view);

    ON_3dPoint  cam_loc  = ON_3dPoint(0, 0, 0) + plane.cam_offset * cam_dist;
    ON_3dVector cam_dir  = plane.look_dir;

    onvp.SetCameraLocation(cam_loc);
    onvp.SetCameraDirection(cam_dir);
    onvp.SetCameraUp(cam_up);
    onvp.SetFrustum(-hw, hw, -hh, hh, 0.1, cam_dist * 2.0);
    onvp.SetTargetPoint(ON_3dPoint(0.0, 0.0, 0.0));

    new_view->ActiveViewport().SetVP(onvp, true);

    ON_wString view_name;
    view_name.Format(L"Cavalier%ls%.0f_%.2f", plane.name, angle_deg, scale);
    ON_3dmView v3dm = new_view->ActiveViewport().View();
    v3dm.m_name = view_name;
    new_view->ActiveViewport().SetView(v3dm);
    new_view->FloatRhinoView(true);

    // ── 5. Build and attach the conduit ────────────────────────────────────
    static CCavalierConduit s_conduit;
    s_conduit.SetShearMatrix(
        BuildCavalierShear(plane.look_dir, plane.screen_right, angle_deg, scale));
    s_conduit.SetViewportId(new_view->ActiveViewportID());
    s_conduit.Bind(new_view->ActiveViewport().VP());
    s_conduit.Enable(doc->RuntimeSerialNumber());

    new_view->Redraw();

    RhinoApp().Print(
        L"ObliqCavalier: Created \"%ls\" (plane=%ls angle=%.1f scale=%.2f)\n",
        static_cast<const wchar_t*>(view_name),
        plane.name, angle_deg, scale);

    return CRhinoCommand::success;
}

#pragma endregion

// ─────────────────────────────────────────────────────────────────────────────
// CavalierMake2D command
//
// Runs the hidden-line engine using a combined R * T_shear transform so the
// engine's coordinate space always has screen = XY and depth = Z (higher Z =
// closer to camera), regardless of which elevation plane is chosen.
//
// Reorientation matrix R:
//   row 0 = screen_right   → engine +X
//   row 1 = screen_up(+Z)  → engine +Y
//   row 2 = -look_dir      → engine +Z  (closer to camera)
// ─────────────────────────────────────────────────────────────────────────────

#pragma region CavalierMake2D command

static ON_Xform BuildReorientation(
    const ON_3dVector& screen_right,
    const ON_3dVector& screen_up,
    const ON_3dVector& look_dir)
{
    ON_Xform R = ON_Xform::IdentityTransformation;
    R.m_xform[0][0] =  screen_right.x;  R.m_xform[0][1] =  screen_right.y;  R.m_xform[0][2] =  screen_right.z;
    R.m_xform[1][0] =  screen_up.x;     R.m_xform[1][1] =  screen_up.y;     R.m_xform[1][2] =  screen_up.z;
    R.m_xform[2][0] = -look_dir.x;      R.m_xform[2][1] = -look_dir.y;      R.m_xform[2][2] = -look_dir.z;
    return R;
}

static int CavFindOrCreateLayer(CRhinoDoc* doc, const wchar_t* name, COLORREF color)
{
    int idx = doc->m_layer_table.FindLayerFromName(name, false, false, -1, -1);
    if (idx >= 0) return idx;
    ON_Layer layer;
    layer.SetName(name);
    layer.SetColor(color);
    return doc->m_layer_table.AddLayer(layer);
}

class CCommandCavalierMake2D : public CRhinoCommand
{
public:
    CCommandCavalierMake2D() = default;
    ~CCommandCavalierMake2D() = default;

    UUID CommandUUID() override
    {
        // {B2D4F6E8-1A3C-4D5E-9078-FEDCBA987654}
        static const GUID uuid =
        { 0xb2d4f6e8, 0x1a3c, 0x4d5e, { 0x90, 0x78, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54 } };
        return uuid;
    }

    const wchar_t* EnglishCommandName() override { return L"CavalierMake2D"; }

    CRhinoCommand::result RunCommand(const CRhinoCommandContext& context) override;
};

static class CCommandCavalierMake2D theCavalierMake2DCommand;

CRhinoCommand::result CCommandCavalierMake2D::RunCommand(const CRhinoCommandContext& context)
{
    CRhinoDoc* doc = context.Document();
    if (!doc)
        return CRhinoCommand::failure;

    // ── 1. Options (same as ObliqCavalier) ────────────────────────────────
    int    plane_idx = 0;
    double angle_deg = 45.0;
    double scale     = 1.0;

    CRhinoGetOption getOpt;
    getOpt.SetCommandPrompt(L"Cavalier Make2D options. Press Enter to compute");
    getOpt.AcceptNothing(true);

    for (;;)
    {
        getOpt.ClearCommandOptions();

        ON_wString angle_str, scale_str;
        angle_str.Format(L"%.1f", angle_deg);
        scale_str.Format(L"%.2f", scale);

        int ix_plane = getOpt.AddCommandOption(RHCMDOPTNAME(L"Plane"), RHCMDOPTVALUE(s_planes[plane_idx].name));
        int ix_angle = getOpt.AddCommandOption(RHCMDOPTNAME(L"Angle"), RHCMDOPTVALUE(angle_str));
        int ix_scale = getOpt.AddCommandOption(RHCMDOPTNAME(L"Scale"), RHCMDOPTVALUE(scale_str));

        CRhinoGet::result res = getOpt.GetOption();
        if (res == CRhinoGet::nothing) break;
        if (res == CRhinoGet::cancel)  return CRhinoCommand::cancel;

        if (res == CRhinoGet::option)
        {
            const CRhinoCommandOption* opt = getOpt.Option();
            if (!opt) continue;

            if (opt->m_option_index == ix_plane)
            {
                CRhinoGetOption gp;
                gp.SetCommandPrompt(L"Select elevation plane");
                gp.AcceptNothing(true);
                int ix[4];
                ix[0] = gp.AddCommandOption(RHCMDOPTNAME(L"Front"), RHCMDOPTVALUE(L""));
                ix[1] = gp.AddCommandOption(RHCMDOPTNAME(L"Right"), RHCMDOPTVALUE(L""));
                ix[2] = gp.AddCommandOption(RHCMDOPTNAME(L"Back"),  RHCMDOPTVALUE(L""));
                ix[3] = gp.AddCommandOption(RHCMDOPTNAME(L"Left"),  RHCMDOPTVALUE(L""));
                if (gp.GetOption() == CRhinoGet::option)
                {
                    int oi = gp.Option()->m_option_index;
                    for (int i = 0; i < s_plane_count; i++)
                        if (oi == ix[i]) { plane_idx = i; break; }
                }
            }
            else if (opt->m_option_index == ix_angle)
            {
                CRhinoGetNumber gn;
                gn.SetCommandPrompt(L"Receding angle in degrees");
                gn.SetDefaultNumber(angle_deg);
                gn.SetLowerLimit(0.0, false);
                gn.SetUpperLimit(360.0, false);
                if (gn.GetNumber() == CRhinoGet::number)
                    angle_deg = gn.Number();
            }
            else if (opt->m_option_index == ix_scale)
            {
                CRhinoGetNumber gn;
                gn.SetCommandPrompt(L"Receding scale (1.0=cavalier, 0.5=cabinet)");
                gn.SetDefaultNumber(scale);
                gn.SetLowerLimit(0.001, false);
                gn.SetUpperLimit(10.0, false);
                if (gn.GetNumber() == CRhinoGet::number)
                    scale = gn.Number();
            }
            continue;
        }
        break;
    }

    // ── 2. Build combined engine transform: R * T_shear ────────────────────
    // R reorients so engine XY = screen plane, engine Z = depth (higher = closer).
    // The existing CObliqueHiddenLineEngine then works unchanged.
    static const ON_3dVector screen_up(0.0, 0.0, 1.0);
    const CavalierPlane& plane = s_planes[plane_idx];

    ON_Xform T_shear = BuildCavalierShear(plane.look_dir, plane.screen_right, angle_deg, scale);
    ON_Xform R       = BuildReorientation(plane.screen_right, screen_up, plane.look_dir);
    ON_Xform engine_xform = R * T_shear;

    // ── 3. Run hidden-line engine ──────────────────────────────────────────
    RhinoApp().Print(L"CavalierMake2D: Collecting geometry...\n");

    CObliqueHiddenLineEngine hld;
    hld.SetTransform(engine_xform);
    hld.SetSampleDensity(128);
    hld.SetDepthTolerance(1e-3);
    hld.SetEdgeAngleThreshold(9.0);
    hld.AddObjectsFromDoc(doc);

    RhinoApp().Print(L"CavalierMake2D: Computing hidden lines...\n");

    if (!hld.Compute())
    {
        RhinoApp().Print(L"CavalierMake2D: Computation failed.\n");
        return CRhinoCommand::failure;
    }

    int total = hld.ResultCount();
    if (total == 0)
    {
        RhinoApp().Print(L"CavalierMake2D: No edges found.\n");
        return CRhinoCommand::nothing;
    }

    // ── 4. Output layers ───────────────────────────────────────────────────
    ON_wString layer_base;
    layer_base.Format(L"CavalierMake2D::%ls%.0f_%.2f", plane.name, angle_deg, scale);

    ON_wString vis_name = layer_base + L"::Visible";
    ON_wString hid_name = layer_base + L"::Hidden";

    int layer_visible = CavFindOrCreateLayer(doc, vis_name, RGB(0, 0, 0));
    int layer_hidden  = CavFindOrCreateLayer(doc, hid_name, RGB(128, 128, 128));

    // ── 5. Add result curves ───────────────────────────────────────────────
    ON_SimpleArray<CClassifiedSegment> results;
    hld.DetachResults(results);

    // Flatten to Z=0 in engine space (engine XY = screen plane)
    ON_Xform flatten = ON_Xform::IdentityTransformation;
    flatten.m_xform[2][2] = 0.0;

    int count_vis = 0, count_hid = 0;

    for (int i = 0; i < results.Count(); i++)
    {
        CClassifiedSegment& seg = results[i];
        if (!seg.m_curve) continue;

        seg.m_curve->Transform(flatten);

        ON_3dmObjectAttributes attrs;
        attrs.m_uuid = ON_nil_uuid;

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
            int dash_idx = doc->m_linetype_table.FindLinePatternFromName(L"Dashed", true, -1);
            if (dash_idx >= 0)
                attrs.m_linetype_index = dash_idx;
            count_hid++;
        }

        attrs.SetColorSource(ON::color_from_object);
        attrs.SetLinetypeSource(ON::linetype_from_object);

        doc->AddCurveObject(*seg.m_curve, &attrs);
        delete seg.m_curve;
        seg.m_curve = nullptr;
    }

    doc->Redraw();

    RhinoApp().Print(
        L"CavalierMake2D: Done - %d visible, %d hidden segments (plane=%ls angle=%.1f scale=%.2f)\n",
        count_vis, count_hid, plane.name, angle_deg, scale);

    return CRhinoCommand::success;
}

#pragma endregion
