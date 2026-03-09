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
    //         name       cam_offset      look_dir        screen_right
    { L"Front", { 0,-1, 0}, { 0, 1, 0}, { 1, 0, 0} },
    { L"Right", { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0} },
    { L"Back",  { 0, 1, 0}, { 0,-1, 0}, {-1, 0, 0} },
    { L"Left",  {-1, 0, 0}, { 1, 0, 0}, { 0,-1, 0} },
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
