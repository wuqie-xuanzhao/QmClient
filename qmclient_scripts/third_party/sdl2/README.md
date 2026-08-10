# QmClient SDL2 patch

QmClient uses SDL 2.32.10 in UI-less IME mode on Windows so the game can draw
its own candidate list. Microsoft Pinyin can block the SDL message pump while
SDL synchronously enumerates TSF `ITfUIElement` candidate and reading data.

The patch adds the private hint
`SDL_QM_IME_SKIP_UILESS_UIELEMENT_PROCESSING`. When enabled, SDL still tells
TSF to hide native UI and keeps its normal `WM_IME_COMPOSITION` handling, but
does not query candidate or reading UI elements. QmClient continues to obtain
candidate data through its existing `WM_IME_NOTIFY`/IMM path.

Build and install the pinned Win64 dependency from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File qmclient_scripts/third_party/sdl2/build_windows_x64.ps1 -Install
```

The script checks out the exact SDL 2.32.10 commit, applies the checked-in
patch, builds the shared Release target, and replaces the bundled Win64 DLL
while retaining DDNet's existing import library. Other architectures remain
unchanged until reproduced and tested on those targets.
