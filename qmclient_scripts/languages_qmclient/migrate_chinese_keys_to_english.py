#!/usr/bin/env python3
"""Legacy helper to report QmClient Chinese localization keys.

The active QmClient localization model uses English source keys and stores
translations only under data/languages/. This script is retained only for
cleanup if old or newly introduced Chinese source keys need to be migrated.

The migration map comes from generate_all.py's legacy Chinese-to-English seed
tables, EXTRA_TRANSLATIONS below, and static notification rules.

It no longer rewrites source files. It only reports:
- direct Localize("中文") keys
- remaining bare Chinese literals
- suggested English-key mappings when known

Unknown keys are reported for manual follow-up.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import OrderedDict, defaultdict
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
SIMPLIFIED_CHINESE = PROJECT_ROOT / "data" / "languages" / "simplified_chinese.txt"
BASE_SIMPLIFIED_CHINESE = PROJECT_ROOT / "data" / "languages" / "simplified_chinese.txt"
REPORT_PATH = PROJECT_ROOT / "tmp" / "qmclient_language_migration_report.txt"
STATIC_RULES_HEADER = (
    PROJECT_ROOT
    / "src"
    / "game"
    / "client"
    / "components"
    / "qmclient"
    / "hud_notification_static_rules.h"
)

LOCALIZE_LITERAL_RE = re.compile(r'Localize\(\s*"((?:[^"\\]|\\.)*)"')
STRING_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
STATIC_RULE_RE = re.compile(r'X\("((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\)')

sys.path.insert(0, str(SCRIPT_DIR))
try:
    from generate_all import EN_TRANSLATIONS as LEGACY_EN_TRANSLATIONS
except Exception:
    LEGACY_EN_TRANSLATIONS = {}

EXTRA_TRANSLATIONS = {
    "你现在处于单人区域": "You are now in a solo part",
    "你现在已离开单人区域": "You are now out of the solo part",
    "'%s' 加入了 0 队": "'%s' joined team 0",
    "队伍存档进行中，之后可以用 '/load %s' 载入": "Team save in progress. You'll be able to load with '/load %s'",
    "队伍存档进行中，成功后可用 '/load %s' 载入，失败时可用 '/load %s' 载入": "Team save in progress. You'll be able to load with '/load %s' if save is successful or with '/load %s' if it fails",
    "队伍已由 %s 成功存档。数据库连接失败，因此改用生成的存档码避免冲突。用 '/load %s' 继续": "Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' to continue",
    "队伍已由 %s 成功存档。数据库连接失败，因此改用生成的存档码避免冲突。请在 %s 上用 '/load %s' 继续": "Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' on %s to continue",
    "'%s' 关闭了你们队伍的练习模式": "'%s' disabled practice mode for your team",
    "'%s' 锁定了你们的队伍": "'%s' locked your team.",
    "'%s' 锁定了你们的队伍。比赛开始后，任何人 kill 都会导致整队死亡": "'%s' locked your team. After the race starts, killing will kill everyone in your team.",
    "'%s' 解锁了你们的队伍": "'%s' unlocked your team.",
    "这个队伍已经达到最大人数上限 %s": "This team already has the maximum allowed size of %s players",
    "无法关闭 team 0 模式。该队伍人数已超过普通队伍允许上限 %s": "Can't disable team 0 mode. This team exceeds the maximum allowed size of %s players for regular team",
    "'%s' 关闭了 team 0 模式": "'%s' disabled team 0 mode.",
    "'%s' 开启了 team 0 模式。你们的队伍现在会按 team 0 规则运作": "'%s' enabled team 0 mode. This will make your team behave like team 0.",
    "你当前在 %s 队，队伍里有 %s 人": "You are in team %s having %s players",
    "'%s' 加入了 %s 队": "'%s' joined team %s",
    "'%s' 邀请你加入 %s 队。输入 /team %s 即可加入": "'%s' invited you to team %s. Use /team %s to join",
    "'%s' 邀请了 '%s' 加入你们的队伍": "'%s' invited '%s' to your team.",
    "这个队伍已无法完赛，因为 '%s' 在碰到起点前离开了队伍": "This team cannot finish anymore because '%s' left the team before hitting the start",
    "你已经向 %s 发过交换请求了": "You have already requested to swap with %s.",
    "你已向 %s 发出交换请求。输入 /cancelswap 可取消": "You have requested to swap with %s. Use /cancelswap to cancel the request.",
    "%s 请求与你交换。请等待 %s 秒后输入 /swap %s 完成": "%s has requested to swap with you. To complete the swap process please wait %s seconds and then type /swap %s.",
    "%s 请求与 %s 交换位置": "%s has requested to swap with %s",
    "你还需要等待 %s 秒才能交换": "You have to wait %s seconds until you can swap",
    "你现在可以跳跃 %s 次": "You can now jump %s times",
    "救援模式已切换为 %s": "Rescue mode switched to %s",
    "当前救援模式：%s": "Current rescue mode: %s",
    "你的交换请求已在 %s 秒前超时，请重新输入 /swap 发起": "Your swap request timed out %s seconds ago, use /swap again to request",
    "%s 与 %s 已完成交换": "%s and %s have swapped",
    "你已取消与 %s 的交换": "You canceled the swap with %s",
    "%s 已取消与你的交换": "%s canceled the swap with you",
    "%s 已取消与 %s 的交换": "%s canceled the swap with %s",
    "自杀": "killed themselves",
    "死亡": "died",
    "你的锁队全员被处死，因为 '%s' %s 了": "Your locked team was killed because '%s' %s",
    "'%s' 发起了%s队伍练习模式投票。当前票数 %s/%s": "'%s' called vote to %s practice mode for your team. Current votes %s/%s",
    "开启": "enable",
    "'%s' 不是本服务器可用的投票选项": "'%s' is not a valid option on this server",
    "'%s' 发起了服务器选项投票：%s": "'%s' called vote to change server option: %s",
    "'%s' 发起了服务器选项投票：%s（原因：%s）": "'%s' called vote to change server option: %s (reason: %s)",
    "'%s' 发起了踢出 '%s' 的投票（原因：%s）": "'%s' called for vote to kick '%s' (reason: %s)",
    "'%s' 发起了禁言 '%s' 的投票（原因：%s）": "'%s' called for vote to mute '%s' (reason: %s)",
    "'%s' 发起了暂停 '%s' %s 秒的投票（原因：%s）": "'%s' called for vote to force-pause '%s' for %s seconds (reason: %s)",
    "'%s' 发起了将 '%s' 移到旁观的投票（原因：%s）": "'%s' called for vote to move '%s' to spectators (reason: %s)",
    "每名玩家两次踢人投票之间需要间隔 %s 秒，请再等待 %s 秒": "There's a %s second wait time between kick votes for each player please wait %s second(s)",
    "踢人投票至少需要 %s 名玩家": "Kick voting requires %s players",
    "授权玩家强制将当前投票设为 '%s'": "Authorized player forced vote '%s'",
    "两次换图投票之间需要间隔 %s 秒，请再等待 %s 秒": "There's a %s second delay between map-votes, please wait %s seconds.",
    "'%s' 发起了针对你的踢人投票": "'%s' called for vote to kick you",
    "'%s' 发起了针对你的旁观投票": "'%s' called for vote to move you to spectators",
    "首次发起投票前还需要等待 %s 秒": "You must wait %s seconds before making your first vote.",
    "再次发起投票前还需要等待 %s 秒": "You must wait %s seconds before making another vote.",
    "你接下来 %s 秒内不能发起投票": "You are not permitted to vote for the next %s seconds.",
    "本服务器启用了初始发言延迟，你还需要等待 %s 秒才能说话": "This server has an initial chat delay, you will be able to talk in %s seconds.",
    "你接下来 %s 秒内不能发言": "You are not permitted to talk for the next %s seconds.",
    "计时器当前显示在 %s": "Timer is displayed in %s",
    "距离下次切换队伍还需等待：%s": "Time to wait before changing team: %s",
    "你正处于强制暂停状态，还需等待 %s 秒": "You are force-paused for %s seconds.",
    "你当前的比赛用时是 %s": "Your current race time is %s",
    "%s 当前的比赛用时是 %s": "%s current race time is %s",
    "正在显示 '%s' 的 checkpoint 时间，当前成绩为 %s": "Showing the checkpoint times for '%s' with a race time of %s",
    "'%s' 原本会超时掉线，但现在可以使用超时保护": "'%s' would have timed out, but can use timeout protection now",
    "'%s' 被强制暂停了 %s 秒": "'%s' was force-paused for %s seconds",
    "栖梦客户端概览": "QmClient overview",
    "使用顶部标签按分类浏览栖梦功能": "Use the top tabs to browse QmClient features by category",
    "概览卡展示客户端与页面结构的轻量说明": "Overview cards show a lightweight guide to the client and page structure",
    "视觉页包含外观和渲染相关选项": "The Visuals tab contains appearance and rendering options",
    "功能页包含工具、自动化和游戏辅助": "The Functions tab contains tools, automation, and gameplay helpers",
    "页面指南": "Page guide",
    "每个标签页都有明确职责": "Each tab has a clear purpose",
    "HUD 页收集叠加层、计数器、语音显示和顶部组件": "The HUD tab collects overlays, counters, voice display, and top components",
    "配置页复用栖梦里的客户端配置浏览器": "The Config tab reuses QmClient's client config browser",
    "社区链接、更新与赞助名单已移到贡献者页": "Community links, updates, and sponsors moved to the Contributors tab",
    "拖拽、折叠、搜索和使用历史会在每个分类中保留": "Dragging, collapsing, search, and usage history are preserved per category",
    "栖梦社区": "QmClient Community",
    "官方社区链接": "Official community links",
    "QQ群: 1076765929（点击复制）": "QQ group: 1076765929 (click to copy)",
    "点击复制QQ群号": "Click to copy QQ group number",
    "加入QQ群": "Join QQ group",
    "赞助支持": "Sponsor support",
    "感谢支持栖梦客户端": "Thanks for supporting QmClient",
    "隐藏赞助码": "Hide sponsor QR code",
    "显示赞助码": "Show sponsor QR code",
    "无法加载支持二维码。请检查 Base64 内容": "Could not load sponsor QR code. Check the Base64 data",
    "支持二维码的 Base64 内容未配置": "Sponsor QR code Base64 data is not configured",
    "查看最新更新": "View latest updates",
    "栖梦客户端": "QmClient",
    "开发以及赞助者": "Developers and sponsors",
    "赞助者:": "Sponsors:",
    "皮肤切换": "Skin transition",
    "调整锤中偷皮和换皮动画": "Configure hammer skin steal and skin transition animations",
    "调试图表": "Debug graph",
    "调试性能图表面板": "Debug performance graph panel",
    "持续时间": "Duration",
    "字体大小": "Font size",
    "自动切换快速输入": "Auto-toggle fast input",
    "自动切换快速输入（其他人）": "Auto-toggle fast input others",
    "主动断开": "Active disconnect",
    "显示Qm标识": "Show Qm badge",
    "新版IME": "New IME",
    "换皮动画类型": "Skin transition type",
    "残影弹出": "Afterimage pop",
    "柔和淡变": "Smooth fade",
    "向左滑切": "Slide left",
    "旋转弹出": "Spin pop",
    "明暗切换": "Brightness shift",
    "换皮动画时长": "Skin transition duration",
    "刷新间隔": "Refresh interval",
    "看起来已经是目标语言的消息会跳过自动翻译": "Messages that already look like the target language will skip auto-translate",
    "纯数字消息会直接跳过": "Numeric-only messages will be skipped",
    "地域": "Region",
    "API 密钥": "API key",
    "智谱 API 密钥": "Zhipu API key",
    "DeepSeek API 密钥": "DeepSeek API key",
    "OpenAI API 密钥": "OpenAI API key",
    "自定义 API 密钥": "Custom API key",
    "启用思考模式需要使用推理模型": "Thinking mode requires a reasoning model",
    "确保后端支持 OpenAI 兼容的思考参数": "Make sure the backend supports OpenAI-compatible thinking parameters",
    "自动回复冷却时间": "Auto reply cooldown",
    "UI 比例": "UI scale",
    "提及": "Mention",
    "复制皮肤": "Copy skin",
    "切换": "Switch",
    "仅自己": "Self only",
    "本地": "Local",
    "所有人": "All players",
    "动画范围": "Animation range",
    "激光风格": "Laser style",
    "激光效果增强": "Laser effect enhancement",
    "脉冲速度": "Pulse speed",
    "脉冲幅度": "Pulse amplitude",
    "玩家数据": "Player data",
    "玩家统计数据和信息显示": "Player stats and info display",
    "显示玩家统计数据HUD": "Show player stats HUD",
    "地图进度条": "Map progress bar",
    "竖直位置": "Vertical position",
    "未知": "Unknown",
    "古典.easy": "Classic easy",
    "古典.next": "Classic next",
    "古典.pro": "Classic pro",
    "古典.nut": "Classic nut",
    "古典": "Classic",
    "简单": "Novice",
    "中阶": "Moderate",
    "高阶": "Brutal",
    "疯狂": "Insane",
    "单人": "Solo",
    "传统": "Oldschool",
    "竞速": "Race",
    "娱乐": "Fun",
    "活动": "Event",
    "从收藏中移除": "Remove from favorites",
    "解冻时自动取消旁观": "Auto unspec on unfreeze",
    "速通倒计时器": "Speedrun countdown timer",
    "小时": "Hours",
    "分钟": "Minutes",
    "秒": "Seconds",
    "毫秒": "Milliseconds",
    "时间到时自动禁用": "Auto disable when time expires",
    "全局开关键": "Global toggle key",
    "面板不透明度": "Panel opacity",
    "输入叠加": "Input overlay",
    "输入叠加显示": "Input overlay display",
    "配置文件: data/input_overlay.json": "Config file: data/input_overlay.json",
    "外部保存后自动热重载": "Auto hot-reload after external saves",
    "通知栏": "Notifications",
    "把 Echo 和需要关注的系统提示移到右侧弹出显示": "Move Echo and important system prompts to right-side popups",
    "服务器系统提示改走通知栏（黑名单除外）": "Route server system prompts to notifications (except blacklist)",
    "Echo 消息改走通知栏": "Route Echo messages to notifications",
    "其他服务器的类似提示也尝试改走通知栏（例如自定义单人区域提示，按黑名单排除）": "Also route similar prompts from other servers to notifications (for example custom solo prompts; blacklist still applies)",
    "通知栏背景颜色": "Notification background",
    "系统提示文字颜色": "System prompt text",
    "Echo 跟随聊天里当时的实际颜色": "Echo follows the original chat color",
    "Echo 不跟随聊天颜色时的文字颜色": "Echo text color when not inheriting chat color",
    "通知文字大小": "Notification text size",
    "每条显示多久": "Notification hold time",
    "弹出动画": "Popup animation",
    "淡入滑入": "Fade and slide",
    "仅淡入": "Fade only",
    "无动画": "No animation",
    "动画持续多久": "Animation duration",
    "最多同时显示几条": "Max visible notifications",
    "粒子辉光": "Particle glow",
    "粒子脉冲": "Particle pulse",
    "运行 chai 脚本": "Run chai script",
    "无效的 ID": "Invalid ID",
    "该 ID 未连接": "This ID is not connected",
    "没有可翻译的消息": "No chat message to translate",
    "不翻译服务器消息": "Do not translate server messages",
    "无效的翻译后端": "Invalid translation backend",
    "%s 正在翻译为 %s": "%s translating to %s",
    "翻译任务过多": "Too many translation tasks",
    "%s 正在发送前翻译为 %s": "%s translating to %s before send",
    "%s 翻译为 %s 失败: %s": "%s translating to %s failed: %s",
    "FTAPI 自动翻译已禁用以避免服务过载。需要时可在设置中启用。": "FTAPI auto-translate is disabled to prevent overload. Enable in settings if needed.",
    "翻译队列已满，发送原文": "Translation queue full, sending original",
    "翻译后端无效，发送原文": "Translation backend invalid, sending original",
    "正在翻译为 %s...": "Translating to %s...",
    "按钮 - 禁用": "Button - Disabled",
    "按钮 - 启用": "Button - Enabled",
    "菜单背景": "Menu background",
    "菜单选项 - 选中": "Menu option - selected",
    "菜单选项 - 普通": "Menu option - normal",
    "自由旁观光标不透明度": "Freeview cursor opacity",
    "旁观选择中显示颜色": "Show colors in spectator selection",
    "a = 视角角度": "a = View angle",
    "p = Ping 延迟": "p = Ping latency",
    "d = 预测延迟": "d = Prediction latency",
    "c = 玩家坐标": "c = Player position",
    "l = 本地时间": "l = Local time",
    "r = 比赛时间": "r = Race time",
    "f = 帧率": "f = Frame rate",
    "v = 速度": "v = Velocity",
    "z = 缩放": "z = Zoom",
    "u = 快照延迟": "u = Snapshot latency",
    "n = 预测延迟": "n = Prediction latency",
    "j = 延迟抖动": "j = Latency jitter",
    "k = 重发丢包率": "k = Resend loss",
    "i = 接收速率": "i = Receive rate",
    "o = 发送速率": "o = Send rate",
    "q = 连接质量": "q = Connection quality",
    "x = DDNet CPU% / 总 CPU%": "x = DDNet CPU% / total CPU%",
    "y = DDNet 内存占用": "y = DDNet memory usage",
    "_ 或 ' ' = 空白间隔": "_ or ' ' = blank spacer",
    "栖梦": "QmClient",
    "正在尝试 Axiom 自动登录": "Trying Axiom auto login",
    "Axiom 自动登录成功": "Axiom auto login succeeded",
    "Axiom 自动登录失败，正在重试": "Axiom auto login failed, retrying",
    "Axiom 自动登录失败": "Axiom auto login failed",
}


def contains_han(text: str) -> bool:
    return any("\u3400" <= ch <= "\u4dbf" or "\u4e00" <= ch <= "\u9fff" for ch in text)


def detect_newline(data: bytes) -> str:
    return (
        "\r\n"
        if data.count(b"\r\n") > data.count(b"\n") - data.count(b"\r\n")
        else "\n"
    )


def read_text_preserve(path: Path) -> tuple[str, str, bool]:
    data = path.read_bytes()
    has_bom = data.startswith(b"\xef\xbb\xbf")
    newline = detect_newline(data)
    encoding = "utf-8-sig" if has_bom else "utf-8"
    return data.decode(encoding), newline, has_bom


def cpp_unescape_key(raw: str) -> str:
    result: list[str] = []
    i = 0
    while i < len(raw):
        ch = raw[i]
        if ch != "\\" or i + 1 >= len(raw):
            result.append(ch)
            i += 1
            continue
        nxt = raw[i + 1]
        if nxt == "n":
            result.append("\n")
        elif nxt == "t":
            result.append("\t")
        elif nxt == "r":
            result.append("\r")
        elif nxt == '"':
            result.append('"')
        elif nxt == "\\":
            result.append("\\")
        else:
            result.append("\\" + nxt)
        i += 2
    return "".join(result)


def cpp_escape_key(text: str) -> str:
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def read_language_pairs(path: Path) -> list[tuple[str, str]]:
    text, _, _ = read_text_preserve(path)
    lines = text.splitlines()
    pairs: list[tuple[str, str]] = []
    i = 0
    while i < len(lines):
        key = lines[i]
        i += 1
        if (
            not key
            or key.startswith("#")
            or key.startswith("[")
            or key.startswith("== ")
        ):
            continue
        if i >= len(lines) or not lines[i].startswith("== "):
            continue
        pairs.append((key, lines[i][3:]))
        i += 1
    return pairs


def read_existing_simplified(path: Path) -> OrderedDict[str, str]:
    entries: OrderedDict[str, str] = OrderedDict()
    if not path.exists():
        return entries
    for key, value in read_language_pairs(path):
        if key != value:
            entries[key] = value
    return entries


def iter_source_files() -> list[Path]:
    import source_keys

    return source_keys.iter_source_files()


def merge_translation(
    cn_to_en: OrderedDict[str, str],
    en_to_cn: OrderedDict[str, str],
    collisions: dict[str, list[str]],
    chinese: str,
    english: str,
) -> None:
    if not chinese or not english or not contains_han(chinese):
        return
    if chinese not in cn_to_en:
        cn_to_en[chinese] = english
    if english not in en_to_cn:
        en_to_cn[english] = chinese
    elif en_to_cn[english] != chinese:
        collisions[english].append(chinese)


def build_maps(
    pairs: list[tuple[str, str]],
) -> tuple[OrderedDict[str, str], OrderedDict[str, str], dict[str, list[str]]]:
    cn_to_en: OrderedDict[str, str] = OrderedDict()
    en_to_cn: OrderedDict[str, str] = OrderedDict()
    collisions: dict[str, list[str]] = defaultdict(list)
    for chinese, english in pairs:
        merge_translation(cn_to_en, en_to_cn, collisions, chinese, english)
    for chinese, english in LEGACY_EN_TRANSLATIONS.items():
        merge_translation(cn_to_en, en_to_cn, collisions, chinese, english)
    for chinese, english in EXTRA_TRANSLATIONS.items():
        merge_translation(cn_to_en, en_to_cn, collisions, chinese, english)
    if STATIC_RULES_HEADER.exists():
        text, _, _ = read_text_preserve(STATIC_RULES_HEADER)
        for match in STATIC_RULE_RE.finditer(text):
            english = cpp_unescape_key(match.group(1))
            chinese = cpp_unescape_key(match.group(2))
            if contains_han(chinese) and not contains_han(english):
                merge_translation(cn_to_en, en_to_cn, collisions, chinese, english)
    return cn_to_en, en_to_cn, collisions


def migrate_static_rules(
    path: Path,
    dry_run: bool,
) -> int:
    if not path.exists():
        return 0
    text, _, _ = read_text_preserve(path)
    replacements = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal replacements
        english = cpp_unescape_key(match.group(1))
        chinese = cpp_unescape_key(match.group(2))
        if contains_han(chinese) and not contains_han(english):
            replacements += 1
            return f'X("{cpp_escape_key(english)}", "{cpp_escape_key(english)}")'
        return match.group(0)

    STATIC_RULE_RE.sub(replace, text)
    return replacements


def migrate_source_file(
    path: Path,
    cn_to_en: OrderedDict[str, str],
    dry_run: bool,
) -> tuple[int, list[tuple[int, str]], list[tuple[int, str]]]:
    text, _, _ = read_text_preserve(path)
    replacements = 0
    unknown: list[tuple[int, str]] = []

    def replace(match: re.Match[str]) -> str:
        nonlocal replacements
        raw_key = match.group(1)
        key = cpp_unescape_key(raw_key)
        if not contains_han(key):
            return match.group(0)
        english = cn_to_en.get(key)
        if english is None:
            unknown.append((line_number(text, match.start(1)), key))
            return match.group(0)
        replacements += 1
        return (
            match.group(0)[: match.start(1) - match.start(0)]
            + cpp_escape_key(english)
            + match.group(0)[match.end(1) - match.start(0) :]
        )

    updated = LOCALIZE_LITERAL_RE.sub(replace, text)
    remaining: list[tuple[int, str]] = []
    for match in LOCALIZE_LITERAL_RE.finditer(updated):
        key = cpp_unescape_key(match.group(1))
        if contains_han(key):
            remaining.append((line_number(updated, match.start(1)), key))

    return replacements, unknown, remaining


def collect_bare_chinese_strings(path: Path) -> list[tuple[int, str]]:
    text, _, _ = read_text_preserve(path)
    results: list[tuple[int, str]] = []
    for match in STRING_LITERAL_RE.finditer(text):
        key = cpp_unescape_key(match.group(1))
        if contains_han(key):
            results.append((line_number(text, match.start(1)), key))
    return results


def write_simplified_chinese(
    existing: OrderedDict[str, str],
    en_to_cn: OrderedDict[str, str],
    dry_run: bool,
) -> tuple[int, int]:
    base_keys = (
        {key for key, _ in read_language_pairs(BASE_SIMPLIFIED_CHINESE)}
        if BASE_SIMPLIFIED_CHINESE.exists()
        else set()
    )
    merged: OrderedDict[str, str] = OrderedDict()
    skipped_base = 0
    for english, chinese in en_to_cn.items():
        if english in base_keys:
            skipped_base += 1
            continue
        if english != chinese:
            merged[english] = chinese
    for english, chinese in existing.items():
        if english not in merged and english != chinese:
            merged[english] = chinese

    return len(merged), skipped_base


def write_report(
    file_replacements: dict[Path, int],
    unknown: dict[Path, list[tuple[int, str]]],
    remaining: dict[Path, list[tuple[int, str]]],
    bare: dict[Path, list[tuple[int, str]]],
    collisions: dict[str, list[str]],
    simplified_count: int,
    skipped_base_count: int,
    dry_run: bool,
) -> None:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = []
    lines.append("QmClient language key migration report")
    lines.append(f"dry_run: {dry_run}")
    lines.append(f"simplified_chinese_entries: {simplified_count}")
    lines.append(f"skipped_base_simplified_entries: {skipped_base_count}")
    lines.append("")
    lines.append("Replacements:")
    for path, count in file_replacements.items():
        if count:
            lines.append(f"  {path.relative_to(PROJECT_ROOT)}: {count}")
    if not any(file_replacements.values()):
        lines.append("  none")
    lines.append("")
    lines.append("English key collisions in migration maps:")
    if collisions:
        for english, chinese_keys in collisions.items():
            lines.append(f"  {english}: {', '.join(chinese_keys)}")
    else:
        lines.append("  none")
    lines.append("")
    lines.append("Unknown direct Localize Chinese keys:")
    if unknown:
        for path, entries in unknown.items():
            for line, key in entries:
                lines.append(f"  {path.relative_to(PROJECT_ROOT)}:{line}: {key}")
    else:
        lines.append("  none")
    lines.append("")
    lines.append("Remaining direct Localize Chinese keys:")
    if remaining:
        for path, entries in remaining.items():
            for line, key in entries:
                lines.append(f"  {path.relative_to(PROJECT_ROOT)}:{line}: {key}")
    else:
        lines.append("  none")
    lines.append("")
    lines.append("Bare Chinese string literals after migration:")
    if bare:
        for path, entries in bare.items():
            for line, key in entries:
                lines.append(f"  {path.relative_to(PROJECT_ROOT)}:{line}: {key}")
    else:
        lines.append("  none")
    with REPORT_PATH.open("w", encoding="utf-8", newline="\n") as file:
        file.write("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=True,
        help="Report only; retained for backward compatibility",
    )
    args = parser.parse_args()

    pairs = read_language_pairs(BASE_SIMPLIFIED_CHINESE)
    cn_to_en, en_to_cn, collisions = build_maps(pairs)
    existing_simplified = read_existing_simplified(SIMPLIFIED_CHINESE)

    file_replacements: dict[Path, int] = OrderedDict()
    unknown: dict[Path, list[tuple[int, str]]] = OrderedDict()
    remaining: dict[Path, list[tuple[int, str]]] = OrderedDict()

    for path in iter_source_files():
        count, file_unknown, file_remaining = migrate_source_file(
            path, cn_to_en, args.dry_run
        )
        file_replacements[path] = count
        if file_unknown:
            unknown[path] = file_unknown
        if file_remaining:
            remaining[path] = file_remaining

    static_replacements = migrate_static_rules(STATIC_RULES_HEADER, args.dry_run)
    if static_replacements:
        file_replacements[STATIC_RULES_HEADER] = (
            file_replacements.get(STATIC_RULES_HEADER, 0) + static_replacements
        )

    simplified_count, skipped_base_count = write_simplified_chinese(
        existing_simplified, en_to_cn, args.dry_run
    )

    bare: dict[Path, list[tuple[int, str]]] = OrderedDict()
    for path in iter_source_files():
        entries = collect_bare_chinese_strings(path)
        if entries:
            bare[path] = entries

    write_report(
        file_replacements,
        unknown,
        remaining,
        bare,
        collisions,
        simplified_count,
        skipped_base_count,
        args.dry_run,
    )

    print(f"Source files scanned: {len(file_replacements)}")
    print(f"Direct Localize replacements: {sum(file_replacements.values())}")
    print(f"Simplified Chinese entries: {simplified_count}")
    print(f"Skipped base Simplified Chinese entries: {skipped_base_count}")
    print(f"Report: {REPORT_PATH.relative_to(PROJECT_ROOT)}")
    if unknown or remaining:
        print("Manual follow-up remains; see report.")


if __name__ == "__main__":
    main()
