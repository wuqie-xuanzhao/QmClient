# Qm Icon Sources

Qm UI atlas sources are Phosphor Icons. The source directories contain the
official SVG variants used at runtime:

- `phosphor_thin/` builds the thin-weight atlas.
- `phosphor_regular/` builds the regular-weight atlas.
- `phosphor_bold/` builds the default bold-weight atlas.
- `phosphor_fill/` builds the fill-weight atlas.

All variants use the Phosphor Icons MIT license in
`datasrc/qm_icons/LICENSE_PHOSPHOR.txt`.

SVG files are build-time inputs only. Runtime code must load the generated
`data/qmclient/icons/qm_icons_*x.png` atlas and matching JSON manifest through
`CQmIconManager`; do not parse SVG in the client.

The atlas builder prefers `resvg`, `inkscape`, or `magick` when available. If
none is installed, it can render the SVG command and paint subset used by the
bundled Qm icon sources with Pillow, keeping the fallback build-time only.

Generate all atlas families with:

```sh
python qmclient_scripts/qm_build_icon_atlas.py --source datasrc/qm_icons/phosphor_thin --output data/qmclient/icons --atlas-name qm_icons_thin --sizes 1 2 4
python qmclient_scripts/qm_build_icon_atlas.py --source datasrc/qm_icons/phosphor_bold --output data/qmclient/icons --atlas-name qm_icons_bold --sizes 1 2 4
python qmclient_scripts/qm_build_icon_atlas.py --source datasrc/qm_icons/phosphor_fill --output data/qmclient/icons --atlas-name qm_icons_fill --sizes 1 2 4
python qmclient_scripts/qm_build_icon_atlas.py --source datasrc/qm_icons/phosphor_regular --output data/qmclient/icons --atlas-name qm_icons_regular --sizes 1 2 4
```
