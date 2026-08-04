# Qm Icon Atlases

This directory is for generated Qm icon atlas files:

- `qm_icons_{thin,regular,bold,fill}_{1x,2x,4x}.png` with matching JSON manifests
- `qm_icons_{thin,regular,bold,fill}_msdf.png` with matching JSON manifests

The JSON manifest stores icon IDs and pixel rects. `CQmIconManager` converts
those rects to UV coordinates at runtime and renders from PNG only. The UI
selects one weight family and one matching DPI scale; white, black, custom, and
rainbow modes are runtime tinting, not duplicate texture resources.

The MSDF files are generated directly from the Phosphor SVG sources with
`qmclient_scripts/qm_build_icon_msdf.py` and `msdfgen`. Modern OpenGL and Vulkan
prefer them for independently scalable UI icons. The 1x/2x/4x alpha atlases are
kept as the compatibility fallback for legacy renderers.
