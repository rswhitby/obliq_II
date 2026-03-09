> **This is a fork of [criticalsoftware-lab/obliq](https://github.com/criticalsoftware-lab/obliq).** The original military oblique viewport and Make2D commands were built by Critical Software Lab. This fork extends that work with cavalier oblique projection support (selectable elevation plane, angle, and scale).

# obliq_II
Obliq_II plug-in for Rhinoceros®. Provides utilities for working with oblique (military, cabinet, etc...) projections in a non-destructive way.

![Project Screenshot](screenshot.PNG)

# Installation (windows)
1. Download `Obliq.rhp` from the [latest release](https://github.com/rswhitby/obliq/releases/tag/v0.3.0-beta)
2. Make sure you have the latest Rhino 8 service release.
3. Drag the RHP file into the Rhino viewport.

# Usage
**Commands:**

**Obliq** → Creates a floating viewport showing a military (plan) oblique projection. The camera looks straight down (-Z) and height is sheared diagonally into the view.

**ObliqueMake2D** → Generates a flat 2D hidden-line drawing of all scene geometry using the military oblique projection. Outputs visible and hidden curves to dedicated layers. May take some time depending on scene complexity.

**ObliqueCurveMake2D** → Takes a selection of curves and projects them through the oblique shear, flattening them onto Z=0 as new geometry. Supports Angle and Scale options.

---

**ObliqCavalier** → Creates a floating viewport showing a cavalier oblique projection. Unlike the military oblique, you choose which world face is the elevation plane (Front, Right, Back, or Left). The elevation plane is displayed true-shape and depth recedes at the specified angle and scale.

Options:
- `Plane` — elevation face: Front / Right / Back / Left
- `Angle` — angle of receding lines in degrees (default 45°)
- `Scale` — depth scale factor: 1.0 = cavalier (full depth), 0.5 = cabinet (half depth)

**CavalierMake2D** → Generates a flat 2D hidden-line drawing using the cavalier oblique projection. Uses the same Plane, Angle, and Scale options as ObliqCavalier. Outputs visible (black) and hidden (grey, dashed) curves to named layers. The output curves lie in the chosen elevation plane.
