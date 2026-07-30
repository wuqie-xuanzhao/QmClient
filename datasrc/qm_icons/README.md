# Qm Icon Sources

Place supported SVG source files in `datasrc/qm_icons/tabler/`. The directory
name is retained for compatibility with the existing build target, but the
atlas may contain attributed sources from more than one icon family.

The `satellite-mute`, `satellite-swap`, and `satellite-check` icons are from
Lucide. Their license is stored in `datasrc/qm_icons/LICENSE_LUCIDE.txt`.

The `eye`, `satellite-swap-incoming`, `satellite-swap-outgoing`, and
`satellite-switch` icons use the Phosphor Icons fill variants `eyes`,
`arrow-circle-left`, `arrow-circle-right`, and `clock-countdown`. Their license
is stored in `datasrc/qm_icons/LICENSE_PHOSPHOR.txt`.

The `satellite-spectator-eye` and `satellite-spectator-eye-closed` icons are
QmClient hand-drawn sources used only by the Dynamic Island spectator capsule.

SVG files are build-time inputs only. Runtime code must load the generated
`data/qmclient/icons/qm_icons_*x.png` atlas and matching JSON manifest through
`CQmIconManager`; do not parse SVG in the client.

The atlas builder prefers `resvg`, `inkscape`, or `magick` when available. If
none is installed, it can render the SVG command and paint subset used by the
bundled Qm icon sources with Pillow, keeping the fallback build-time only.

Generate atlases with:

```sh
python qmclient_scripts/qm_build_icon_atlas.py --source datasrc/qm_icons/tabler --output data/qmclient/icons --sizes 1 2 4
```
