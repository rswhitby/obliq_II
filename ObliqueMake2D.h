// ObliqueMake2D.h
// Utility for projecting geometry through an oblique shear and flattening to 2D.
// Include AFTER stdafx.h / RhinoSdk.h

#pragma once

class CObliqueMake2D
{
public:
    CObliqueMake2D()
        : m_angle_deg(45.0)
        , m_scale(1.0)
    {
        UpdateTransforms();
    }

    void SetParams(double angle_deg, double scale)
    {
        m_angle_deg = angle_deg;
        m_scale = scale;
        UpdateTransforms();
    }

    double AngleDeg() const { return m_angle_deg; }
    double Scale() const { return m_scale; }

    // The combined transform: shear then flatten to Z=0
    const ON_Xform& ProjectionXform() const { return m_combined; }

    // Just the shear (same as the conduit uses)
    const ON_Xform& ShearXform() const { return m_shear; }

    // Just the flatten-to-Z=0
    const ON_Xform& FlattenXform() const { return m_flatten; }

    // Project a single curve: duplicate, transform, return new curve.
    // Caller owns the returned pointer.
    ON_Curve* ProjectCurve(const ON_Curve* crv) const
    {
        if (!crv)
            return nullptr;

        ON_Curve* dup = crv->DuplicateCurve();
        if (!dup)
            return nullptr;

        if (!dup->Transform(m_combined))
        {
            delete dup;
            return nullptr;
        }
        return dup;
    }

    // Project a single point
    ON_3dPoint ProjectPoint(const ON_3dPoint& pt) const
    {
        return m_combined * pt;
    }

private:
    void UpdateTransforms()
    {
        double alpha = m_angle_deg * ON_PI / 180.0;
        double shx = m_scale * cos(alpha);
        double shy = m_scale * sin(alpha);

        // Shear: new_x = x + shx*z, new_y = y + shy*z  (top-down, -Z camera)
        m_shear = ON_Xform::IdentityTransformation;
        m_shear.m_xform[0][2] = shx;
        m_shear.m_xform[1][2] = shy;

        // Flatten: zero out the Z row so everything lands on Z=0
        m_flatten = ON_Xform::IdentityTransformation;
        m_flatten.m_xform[2][0] = 0.0;
        m_flatten.m_xform[2][1] = 0.0;
        m_flatten.m_xform[2][2] = 0.0;
        m_flatten.m_xform[2][3] = 0.0;

        // Combined = flatten * shear  (shear first, then squash Z)
        m_combined = m_flatten * m_shear;
    }

    double   m_angle_deg;
    double   m_scale;
    ON_Xform m_shear;
    ON_Xform m_flatten;
    ON_Xform m_combined;
};