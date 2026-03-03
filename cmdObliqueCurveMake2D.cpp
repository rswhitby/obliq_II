// cmdObliqueCurveMake2D.cpp : Oblique Make2D command
//
// Projects selected curves through the oblique shear matrix
// and flattens them to Z=0, adding the results as new geometry.
//

#include "stdafx.h"
#include "ObliqPlugIn.h"
#include "ObliqueMake2D.h"

#pragma region ObliqueCurveMake2D command

class CCommandObliqueCurveMake2D : public CRhinoCommand
{
public:
    CCommandObliqueCurveMake2D() = default;
    ~CCommandObliqueCurveMake2D() = default;

    UUID CommandUUID() override
    {
        // {8460F999-4677-4015-AB0C-71D1024E8E0D}
        static const GUID uuid =
        { 0x8460f999, 0x4677, 0x4015, { 0xab, 0xc, 0x71, 0xd1, 0x2, 0x4e, 0x8e, 0xd } };

        return uuid;
    }

    const wchar_t* EnglishCommandName() override { return L"ObliqueCurveMake2D"; }

    CRhinoCommand::result RunCommand(const CRhinoCommandContext& context) override;
};

static class CCommandObliqueCurveMake2D theObliqueCurveMake2DCommand;

CRhinoCommand::result CCommandObliqueCurveMake2D::RunCommand(const CRhinoCommandContext& context)
{
    CRhinoDoc* doc = context.Document();
    if (!doc)
        return CRhinoCommand::failure;

    // ── 1. Select curves ──────────────────────────────────────
    CRhinoGetObject go;
    go.SetCommandPrompt(L"Select curves to project");
    go.SetGeometryFilter(CRhinoGetObject::curve_object);
    go.EnableSubObjectSelect(false);
    go.GetObjects(1, 0);

    if (go.CommandResult() != CRhinoCommand::success)
        return go.CommandResult();

    const int count = go.ObjectCount();
    if (count == 0)
    {
        RhinoApp().Print(L"ObliqueCurveMake2D: No curves selected.\n");
        return CRhinoCommand::nothing;
    }

    // ── 2. Gather options via CRhinoGetOption ─────────────────
    // Defaults match the conduit: 90° angle, 1.0 scale
    double angle_deg = 90.0;
    double scale = 1.0;

    CRhinoGetOption getOpt;
    getOpt.SetCommandPrompt(L"Oblique projection options. Press Enter to accept");
    getOpt.AcceptNothing(true);

    for (;;)
    {
        getOpt.ClearCommandOptions();

        ON_wString angle_str, scale_str;
        angle_str.Format(L"%.1f", angle_deg);
        scale_str.Format(L"%.2f", scale);

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
            if (!opt)
                continue;

            if (opt->m_option_index == ix_angle)
            {
                CRhinoGetNumber gn;
                gn.SetCommandPrompt(L"Shear angle in degrees");
                gn.SetDefaultNumber(angle_deg);
                gn.SetLowerLimit(0.0, false);
                gn.SetUpperLimit(360.0, false);
                if (gn.GetNumber() == CRhinoGet::number)
                    angle_deg = gn.Number();
            }
            else if (opt->m_option_index == ix_scale)
            {
                CRhinoGetNumber gn;
                gn.SetCommandPrompt(L"Shear scale factor");
                gn.SetDefaultNumber(scale);
                gn.SetLowerLimit(0.001, false);
                gn.SetUpperLimit(100.0, false);
                if (gn.GetNumber() == CRhinoGet::number)
                    scale = gn.Number();
            }
            continue;
        }
        break;
    }

    // ── 3. Build the projection ───────────────────────────────
    CObliqueMake2D projector;
    projector.SetParams(angle_deg, scale);

    // ── 4. Create a layer for the output ──────────────────────
    ON_wString layer_name;
    layer_name.Format(L"ObliqueCurveMake2D%.0f_%.2f", angle_deg, scale);

    int layer_index = doc->m_layer_table.FindLayerFromFullPathName(layer_name, -1);
    if (layer_index < 0)
    {
        ON_Layer layer;
        layer.SetName(layer_name);
        layer.SetColor(ON_Color(0, 0, 0));
        layer_index = doc->m_layer_table.AddLayer(layer);
    }

    // ── 5. Project each selected curve ────────────────────────
    int added = 0;
    for (int i = 0; i < count; i++)
    {
        const CRhinoObjRef& ref = go.Object(i);
        const ON_Curve* crv = ref.Curve();
        if (!crv)
            continue;

        ON_Curve* projected = projector.ProjectCurve(crv);
        if (!projected)
            continue;

        ON_3dmObjectAttributes attrs;
        attrs.m_layer_index = layer_index;

        // AddCurveObject copies the curve, so we delete our copy after
        const CRhinoCurveObject* crv_obj = doc->AddCurveObject(*projected, &attrs);
        delete projected;

        if (crv_obj)
            added++;
    }

    doc->Redraw();

    RhinoApp().Print(L"ObliqueCurveMake2D: Projected %d curve(s) to layer \"%ls\"\n",
        added, static_cast<const wchar_t*>(layer_name));

    return (added > 0) ? CRhinoCommand::success : CRhinoCommand::nothing;
}

#pragma endregion