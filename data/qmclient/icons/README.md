# Qm Icon Atlases

This directory is for generated Qm icon atlas files:

- `qm_icons_bold_1x.png` / `qm_icons_bold_1x.json`
- `qm_icons_bold_2x.png` / `qm_icons_bold_2x.json`
- `qm_icons_bold_4x.png` / `qm_icons_bold_4x.json`
- `qm_icons_regular_1x.png` / `qm_icons_regular_1x.json`
- `qm_icons_regular_2x.png` / `qm_icons_regular_2x.json`
- `qm_icons_regular_4x.png` / `qm_icons_regular_4x.json`
- `qm_icons_bold_msdf.png` / `qm_icons_bold_msdf.json`
- `qm_icons_regular_msdf.png` / `qm_icons_regular_msdf.json`

The JSON manifest stores icon IDs and pixel rects. `CQmIconManager` converts
those rects to UV coordinates at runtime and renders from PNG only. The UI
selects one weight family and one matching DPI scale; white and black modes are
runtime tinting, not duplicate texture resources.

The MSDF files are generated directly from the Phosphor SVG sources with
`qmclient_scripts/qm_build_icon_msdf.py` and `msdfgen`. Modern OpenGL and Vulkan
prefer them for independently scalable UI icons. The 1x/2x/4x alpha atlases are
kept as the compatibility fallback for legacy renderers.
