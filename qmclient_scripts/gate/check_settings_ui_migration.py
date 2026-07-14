#!/usr/bin/env python3
"""Audit whether P5 settings pages use the unified UI stack exclusively."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


PAGE_STABLE_IDS = {
    "general": ("deck:general-game", "deck:general-language", "deck:general-client", "deck:general-recording"),
    "player": ("deck:player-identity", "deck:player-country"),
    "tee": ("deck:tee-identity",),
    "graphics": ("deck:graphics-display", "deck:graphics-visual", "deck:graphics-backend", "deck:graphics-modes"),
    "sound": ("deck:sound-toggle", "deck:sound-volume", "deck:sound-audio-pack"),
    "ddnet": ("deck:ddnet-demo", "deck:ddnet-gameplay", "deck:ddnet-background", "deck:ddnet-miscellaneous"),
    "appearance": (
        "deck:appearance-hud-main", "deck:appearance-hud-ddrace",
        "deck:appearance-chat-settings", "deck:appearance-chat-messages", "deck:appearance-chat-preview",
        "deck:appearance-name-plate-settings", "deck:appearance-name-plate-preview",
        "deck:appearance-hook-collision-main", "deck:appearance-hook-collision-preview",
        "deck:appearance-info-messages", "deck:appearance-laser-enhanced",
        "deck:appearance-laser-colors", "deck:appearance-laser-preview",
    ),
    "controls": (
        "deck:controls-movement", "deck:controls-weapon", "deck:controls-voting",
        "deck:controls-chat", "deck:controls-dummy", "deck:controls-miscellaneous",
        "deck:controls-custom", "deck:controls-mouse", "deck:controls-controller",
    ),
}

PAGE_FUNCTIONS = {
    "general": ("CMenus::RenderSettingsGeneral",),
    "player": ("CMenus::RenderSettingsTeeIdentity", "CMenus::RenderSettingsPlayer"),
    "tee": ("CMenus::RenderSettingsTee(CUIRect MainView)",),
    "graphics": ("CMenus::RenderSettingsGraphics",),
    "sound": ("CMenus::RenderSettingsSound",),
    "ddnet": ("CMenus::RenderSettingsDDNet",),
    "appearance": ("CMenus::RenderSettingsAppearance",),
    "controls": ("CMenusSettingsControls::Render",),
}

PAGE_ROUTE_TABS = {
    "general": ("general",),
    "player": ("player",),
    "tee": ("tee",),
    "graphics": ("graphics",),
    "sound": ("sound",),
    "ddnet": ("ddnet",),
    "appearance": (
        "appearance-hud", "appearance-chat", "appearance-name-plate",
        "appearance-hook-collision", "appearance-info-messages", "appearance-laser",
    ),
    "controls": ("controls",),
}

COMMON_REQUIRED = (
    "ResolveSettingsPageLayout(", "SSettingsPageLayoutFrame", "SSettingsCardDefinition",
    "m_SettingsCardDeck.Render(", "SSettingsCardDeckResult", ".State()", "CQmScrollState", "QmResolveScrollPolicy(",
)
COMMON_FORBIDDEN = (
    "SettingsCard(",
    "BeginSettingsCardDeck(", "BeginSettingsCardDeckCard(",
    "DoSettingsScrollbarOption(", "DoSettingsSliderInputField(",
    "ui_widget::TextFieldEx(", "ui_widget::SearchFieldEx(",
    "ui_widget::ClearableTextFieldEx(", "ui_widget::IconTextFieldEx(",
    "ui_widget::LegacyTextFieldEx(",
    "ui_widget::TextField(", "ui_widget::SearchField(",
    "ui_widget::ClearableTextField(", "ui_widget::IconTextField(",
    "Ui()->DoEditBox(", "Ui()->DoScrollbarH(",
)
PAGE_REQUIRED = {
    "general": ("ui_widget::NumericField(",),
    "player": ("ui_widget::InputField(",),
    "tee": ("ui_widget::InputField(", "ui_widget::NumericField("),
    "graphics": ("ui_widget::NumericField(",),
    "sound": ("ui_widget::NumericField(",),
    "ddnet": ("ui_widget::InputField(", "ui_widget::NumericField("),
    "appearance": ("ui_widget::NumericField(",),
    "controls": ("ui_widget::InputField(", "ui_widget::NumericField("),
}
PAGE_FORBIDDEN = {
    "graphics": ("s_GraphicsSettingsScrollRegion",),
    "sound": ("s_SoundSettingsScrollRegion",),
    "ddnet": ("s_DDNetSettingsScrollRegion",),
    "appearance": (
        "BeginAppearanceCard", "s_ChatSettingsScrollRegion",
        "s_NamePlateSettingsScrollRegion", "s_LaserSettingsScrollRegion",
    ),
    "controls": ("RenderSettingsBlock",),
}

_PAGE_SOURCE = {
    "controls": Path("src/game/client/components/menus_settings_controls.cpp"),
}
_DEFAULT_SOURCE = Path("src/game/client/components/menus_settings.cpp")
_REGISTRY_SOURCE = Path("src/game/client/QmUi/QmCardRegistry.cpp")
_NAVIGATION_SOURCE = Path("src/game/client/components/qmclient/menus_qmclient.cpp")


def _extract_function_body(source: str, symbol: str) -> str | None:
    """Return a C++ function body using a small brace-aware scanner."""
    symbol_pos = source.find(symbol)
    if symbol_pos < 0:
        return None
    open_brace = source.find("{", symbol_pos)
    if open_brace < 0:
        return None

    depth = 0
    index = open_brace
    quote = ""
    line_comment = False
    block_comment = False
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 1
        elif quote:
            if char == "\\":
                index += 1
            elif char == quote:
                quote = ""
        elif char == "/" and next_char == "/":
            line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            block_comment = True
            index += 1
        elif char in ('"', "'"):
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[symbol_pos:index + 1]
        index += 1
    return None


def _read(root: Path, relative: Path) -> str:
    path = root / relative
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


def audit_page(repo_root: Path, page: str) -> list[str]:
    if page not in PAGE_STABLE_IDS:
        raise ValueError(f"unknown settings page: {page}")

    errors: list[str] = []
    source = _read(repo_root, _PAGE_SOURCE.get(page, _DEFAULT_SOURCE))
    bodies: list[str] = []
    for symbol in PAGE_FUNCTIONS[page]:
        body = _extract_function_body(source, symbol)
        if body is None:
            errors.append(f"{page}: {symbol}: function body missing")
        else:
            bodies.append(body)
    page_source = "\n".join(bodies)
    if page == "controls":
        # Controls keeps small render helpers outside Render; inspect them as part of the page contract.
        page_source += "\n" + source

    for token in COMMON_REQUIRED + PAGE_REQUIRED[page]:
        if token not in page_source:
            errors.append(f"{page}: {token}: required unified contract missing")
    for token in COMMON_FORBIDDEN + PAGE_FORBIDDEN.get(page, ()):
        if token in page_source:
            errors.append(f"{page}: {token}: legacy path remains")

    registry = _read(repo_root, _REGISTRY_SOURCE)
    navigation = _read(repo_root, _NAVIGATION_SOURCE)
    for stable_id in PAGE_STABLE_IDS[page]:
        if stable_id not in registry:
            errors.append(f"{page}: {stable_id}: registry/navigation entry missing")
    for route in PAGE_ROUTE_TABS[page]:
        if route not in navigation:
            errors.append(f"{page}: {route}: registry/navigation entry missing")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--page", choices=tuple(PAGE_STABLE_IDS))
    group.add_argument("--all", action="store_true")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()

    pages = tuple(PAGE_STABLE_IDS) if args.all else (args.page,)
    errors = [error for page in pages for error in audit_page(args.repo_root, page)]
    if errors:
        print("P5 设置页迁移结构清单失败：")
        for error in errors:
            print(f"- {error}")
        return 1
    for page in pages:
        print(f"{page}: clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
