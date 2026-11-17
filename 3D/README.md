# Minimal enclosure

[Back to project](../README.md) · [Printing and assembly guide](../docs/assembly.md)

Three printable parts for the Waveshare 2.9-inch black-and-white module and an
ESP32-C3 SuperMini. The assembled size is **93 × 42 × 18.4 mm**. All dimensions
are in millimetres.

The published CAD contains **our enclosure parts only**. Third-party board STEP
models, full electronic reference assemblies, and the old reference-dependent
generation/viewer workflow are not included. The editable FreeCAD document is
self-contained and retains its Parameters sheet and native construction history.

## Download the parts

| Part | Print file | Editable interchange file |
| --- | --- | --- |
| Case | [01_case.stl](STL/01_case.stl) | [01_case.step](output/parts-step/01_case.step) |
| Insulating tray | [02_insulator_tray.stl](STL/02_insulator_tray.stl) | [02_insulator_tray.step](output/parts-step/02_insulator_tray.step) |
| Rear lid | [03_rear_lid.stl](STL/03_rear_lid.stl) | [03_rear_lid.step](output/parts-step/03_rear_lid.step) |

For the assembled enclosure:

- [FreeCAD document](output/assembly/SEO_Dashboard_Minimal_V8.FCStd) — editable
  dimensions and construction history, prepared with FreeCAD 1.1.4.
- [STEP assembly](output/assembly/SEO_Dashboard_Minimal_V8_printed_parts.step) —
  the three enclosure solids in assembly coordinates.

STL parts are positioned for printing with their lowest Z at zero. Individual
STEP parts use assembly coordinates. Print the three STL files separately,
not the assembly as one fused object.

## Compatible set and revisions

The current set is **V7 case + V6 tray + V8 lid**. Each revision identifies the
last geometry change to that part; use the current files listed above together.

- The case retains the tray seats raised from **6.6 to 8.6 mm**, with matching
  USB and lid interfaces. Its maximum depth is **17.4 mm** before the lid.
- The tray retains its raised ESP32 supports and four T-shaped mounting keys.
  These keys locate it in the case; they are not button travel stops.
- The rear lid is **1 mm thick**. The previous 1.6 mm version also fits this
  case, but increases assembled depth from 18.4 to 19 mm.
- The V8 thickness reduction removed material from the outside face. The lid's
  inner face, capture stops, actuator tips, and engagement positions stayed fixed.

Removing board references from the published assembly did not change the three
printed solids. The published STL and individual enclosure STEP files are
unchanged from the printed set.

## Mounting

The display mounts independently to the case through its four existing PCB
holes using short **M2 × 4 mm screws**. The nominal head envelope is 3.7 mm in
diameter and 1.4 mm high. The design provides about 2.4 mm of plastic thread
engagement and 0.2 mm clearance below the screw tip. Check the actual screws
and avoid transmitting tightening force to the glass or ribbon.

The tray slides into four guides. Its floor is 1 mm thick and its complete
assembly position is 2 mm farther from the display than the earlier case.
The ESP32 PCB underside is nominally at **Z = 12.4 mm**.

The lid uses **two fixed hooks and two releasable latches**, without lid screws.
Internal stops keep the tray and bare ESP32 PCB from lifting. The display stays
attached when the lid is opened. Follow the [photographic assembly guide](../docs/assembly.md#assembly)
for wire routing, closing, and releasing the lid.

## Board and USB fit assumptions

The design fits a bare ESP32 PCB of **22.8 × 18 × 1.01 mm**, without pin headers.
The Waveshare pin connector is absent and eight wires are soldered directly.
The display-to-PCB ribbon remains fitted.

The USB-facing PCB end has a nominal 0.15 mm gap to the inner wall. PCB-edge
stops react cable insertion/extraction load. The USB socket itself is not the
mechanical stop. Its end is approximately **0.45 mm behind the outside wall**.

The case's USB opening is 12 mm wide and open to the rear rim. A small lid lip
forms its upper edge, avoiding an unsupported case bridge. The assembled
opening is nominally Z = 11.83–17.20 mm. Check your cable's plug housing and
your board variant before closing the case.

Two 1.4 × 1.4 mm PCB capture columns sit 16.8 mm from the upper/lower outside
edges and 24 mm from the USB-end outside edge, measured to their nearest faces.
Their height is 3.84 mm. Component reliefs keep them clear of the antenna region;
the columns are PCB capture stops, not antenna pressure pads.

## BOOT and RST buttons

Viewed from the rear with USB-C on the right, actuator centres are approximately
10.55 mm from the right outside edge. Their separation is 5.6 mm. BOOT is about
18.20 mm from the upper edge, and RST about 18.20 mm from the lower edge.

The integral leaves are **16 × 3.6 × 1 mm**, with 0.6 mm U slots and rounded
slot ends. Each 1.4 mm diameter actuator has a nominal 0.3 mm free gap above
the switch. The old T-shaped button travel limiters are absent. Use gentle
travel; no actuation-force or fatigue-cycle rating is provided.

## Printing

The case prints front face down, the tray floor down, and the lid outside face
down. Use the supplied STL orientations, PLA, a 0.4 mm nozzle, and 0.2 mm layers
as a starting point. All three parts are intended to print **without supports**.

The two latch springs are 0.8 mm wide and lie within the lid plate. Their
nominal inward release travel is 0.6 mm for 0.45 mm engagement. Keep the narrow
slots free of brim, stray extrusion, and elephant-foot expansion. Six small
side vents have pointed roofs to avoid horizontal support bridges.

The maintainer has printed and assembled the set on an Anet A8. Fit still
depends on printer calibration and actual board dimensions. Inspect your slicer
preview and check button movement and latch engagement before final assembly.
Use your own printer/filament profile; no machine-specific G-code is provided.

## Editing in FreeCAD

Open the native FCStd file to edit the enclosure without any external board
files. The Assembly group contains the three finished parts. The Construction
group retains their native operations, and the Parameters spreadsheet stores
dimensions and expressions. Hide intermediate geometry when inspecting the fit.

`LidT` controls lid plate thickness and integral flexures. `BodyD` is derived as:

```text
OriginalBodyD + BodyDepthIncrease + LidT - OriginalLidT
```

This keeps the case height and inner lid face fixed when the lid is thinned.
For the current set, `BodyD - LidT` is 17.4 mm. Change `LidT`, not the 1.6 mm
reference datum `OriginalLidT`, for another plate-thickness adjustment.

Some hook/relief profiles are fixed geometry. A parameter change does not
guarantee that every clearance or interface remains valid. Recompute the model,
inspect all three parts and the real boards, and review the sliced result.
Export only the finished part, in a suitable print orientation, when creating
a replacement STL. Original enclosure design is covered by the project
[MIT license](../LICENSE).
