// ObliqueConduit.h
// Display conduit that applies an oblique shear to all drawing passes.
// Include AFTER stdafx.h / RhinoSdk.h

#pragma once

class CObliqueConduit : public CRhinoDisplayConduit
{
public:
    CObliqueConduit()
        : CRhinoDisplayConduit(
            CSupportChannels::SC_CALCBOUNDINGBOX |
            CSupportChannels::SC_PREDRAWOBJECTS |
            CSupportChannels::SC_DRAWOBJECT |
            CSupportChannels::SC_POSTDRAWOBJECTS |
            CSupportChannels::SC_DRAWFOREGROUND |
            CSupportChannels::SC_DRAWOVERLAY)
        , m_angle_deg(45.0)
        , m_scale(1.0)
        , m_pushCount(0)
        , m_viewport_id(ON_nil_uuid)
    {
        UpdateShearMatrix();
    }

    void SetObliqueParams(double angle_deg, double scale)
    {
        m_angle_deg = angle_deg;
        m_scale = scale;
        UpdateShearMatrix();
    }

    void SetViewportId(ON_UUID id)
    {
        m_viewport_id = id;
    }

    bool ExecConduit(CRhinoDisplayPipeline& dp, UINT nChannel, bool& bTerminate) override
    {
        // Only apply to our target viewport
        if (ON_UuidCompare(m_viewport_id, ON_nil_uuid) != 0)
        {
            const CRhinoViewport* vp = dp.GetRhinoVP();
            if (vp && ON_UuidCompare(vp->ViewportId(), m_viewport_id) != 0)
                return true;
        }

        switch (nChannel)
        {
        case CSupportChannels::SC_CALCBOUNDINGBOX:
        {
            // Expand scene bbox to account for the shear so nothing gets clipped
            ON_BoundingBox bbox = m_pChannelAttrs->m_BoundingBox;
            if (bbox.IsValid())
            {
                ON_BoundingBox sheared_bbox;
                for (int i = 0; i < 8; i++)
                {
                    ON_3dPoint corner = bbox.Corner(i & 1, (i >> 1) & 1, (i >> 2) & 1);
                    ON_3dPoint sheared = m_shear * corner;
                    if (i == 0)
                        sheared_bbox.Set(sheared, true);
                    else
                        sheared_bbox.Set(sheared, false);
                }
                m_pChannelAttrs->m_BoundingBox.Union(sheared_bbox);
            }
            break;
        }

        // --- Drawing passes: push shear at the start of each ---

        case CSupportChannels::SC_PREDRAWOBJECTS:
            dp.PushModelTransform(m_shear);
            m_pushCount++;
            break;

        case CSupportChannels::SC_DRAWOBJECT:
            // Pipeline may reset model transform between objects.
            // If our shear got popped, re-push it.
            if (m_pushCount == 0)
            {
                dp.PushModelTransform(m_shear);
                m_pushCount++;
            }
            break;

        case CSupportChannels::SC_POSTDRAWOBJECTS:
            // Pop the PREDRAWOBJECTS push
            if (m_pushCount > 0)
            {
                dp.PopModelTransform();
                m_pushCount--;
            }
            break;

        case CSupportChannels::SC_DRAWFOREGROUND:
            // Selected/highlighted wireframes and selection dots
            dp.PushModelTransform(m_shear);
            m_pushCount++;
            break;

        case CSupportChannels::SC_DRAWOVERLAY:
            // Pop the foreground push if it's still on the stack
            if (m_pushCount > 0)
            {
                dp.PopModelTransform();
                m_pushCount--;
            }
            // Push fresh for overlay drawing (dynamic draw, gumballs, etc.)
            dp.PushModelTransform(m_shear);
            m_pushCount++;
            break;
        }

        return true;
    }

    void NotifyConduit(EConduitNotifiers notify, CRhinoDisplayPipeline& dp) override
    {
        // Clean up any remaining pushes at frame end to keep the stack balanced
        if (notify == CN_PIPELINECLOSED || notify == CN_FRAMESIZECHANGED)
        {
            while (m_pushCount > 0)
            {
                dp.PopModelTransform();
                m_pushCount--;
            }
        }
    }

private:
    void UpdateShearMatrix()
    {
        double alpha = m_angle_deg * ON_PI / 180.0;
        double shx = m_scale * cos(alpha);
        double shy = m_scale * sin(alpha);

        m_shear = ON_Xform::IdentityTransformation;
        // For top-down camera (-Z):  new_x = x + shx*z,  new_y = y + shy*z
        m_shear.m_xform[0][2] = shx;
        m_shear.m_xform[1][2] = shy;
    }

    double   m_angle_deg;
    double   m_scale;
    ON_Xform m_shear;
    int      m_pushCount;
    ON_UUID  m_viewport_id;
};

// ─────────────────────────────────────────────────────────────────────────────
// CCavalierConduit
// Display conduit for cavalier / cabinet oblique projections.
// Accepts an externally computed shear matrix so any elevation plane works.
// ─────────────────────────────────────────────────────────────────────────────

class CCavalierConduit : public CRhinoDisplayConduit
{
public:
    CCavalierConduit()
        : CRhinoDisplayConduit(
            CSupportChannels::SC_CALCBOUNDINGBOX |
            CSupportChannels::SC_PREDRAWOBJECTS  |
            CSupportChannels::SC_DRAWOBJECT      |
            CSupportChannels::SC_POSTDRAWOBJECTS |
            CSupportChannels::SC_DRAWFOREGROUND  |
            CSupportChannels::SC_DRAWOVERLAY)
        , m_pushCount(0)
        , m_viewport_id(ON_nil_uuid)
    {
        m_shear = ON_Xform::IdentityTransformation;
    }

    void SetShearMatrix(const ON_Xform& xform) { m_shear = xform; }
    void SetViewportId(ON_UUID id)              { m_viewport_id = id; }

    bool ExecConduit(CRhinoDisplayPipeline& dp, UINT nChannel, bool& bTerminate) override
    {
        if (ON_UuidCompare(m_viewport_id, ON_nil_uuid) != 0)
        {
            const CRhinoViewport* vp = dp.GetRhinoVP();
            if (vp && ON_UuidCompare(vp->ViewportId(), m_viewport_id) != 0)
                return true;
        }

        switch (nChannel)
        {
        case CSupportChannels::SC_CALCBOUNDINGBOX:
        {
            ON_BoundingBox bbox = m_pChannelAttrs->m_BoundingBox;
            if (bbox.IsValid())
            {
                ON_BoundingBox sheared;
                for (int i = 0; i < 8; i++)
                {
                    ON_3dPoint c = bbox.Corner(i & 1, (i >> 1) & 1, (i >> 2) & 1);
                    ON_3dPoint s = m_shear * c;
                    if (i == 0) sheared.Set(s, true);
                    else        sheared.Set(s, false);
                }
                m_pChannelAttrs->m_BoundingBox.Union(sheared);
            }
            break;
        }

        case CSupportChannels::SC_PREDRAWOBJECTS:
            dp.PushModelTransform(m_shear);
            m_pushCount++;
            break;

        case CSupportChannels::SC_DRAWOBJECT:
            if (m_pushCount == 0)
            {
                dp.PushModelTransform(m_shear);
                m_pushCount++;
            }
            break;

        case CSupportChannels::SC_POSTDRAWOBJECTS:
            if (m_pushCount > 0) { dp.PopModelTransform(); m_pushCount--; }
            break;

        case CSupportChannels::SC_DRAWFOREGROUND:
            dp.PushModelTransform(m_shear);
            m_pushCount++;
            break;

        case CSupportChannels::SC_DRAWOVERLAY:
            if (m_pushCount > 0) { dp.PopModelTransform(); m_pushCount--; }
            dp.PushModelTransform(m_shear);
            m_pushCount++;
            break;
        }
        return true;
    }

    void NotifyConduit(EConduitNotifiers notify, CRhinoDisplayPipeline& dp) override
    {
        if (notify == CN_PIPELINECLOSED || notify == CN_FRAMESIZECHANGED)
        {
            while (m_pushCount > 0)
            {
                dp.PopModelTransform();
                m_pushCount--;
            }
        }
    }

private:
    ON_Xform m_shear;
    int      m_pushCount;
    ON_UUID  m_viewport_id;
};