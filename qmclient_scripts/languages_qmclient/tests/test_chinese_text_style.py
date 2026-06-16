import unittest

from qmclient_scripts.languages_qmclient import chinese_text_style, i18n_store


class ChineseTextStyleTest(unittest.TestCase):
    def test_adds_spacing_between_cjk_and_ascii_terms(self):
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text("加入QQ群"),
            "加入 QQ 群",
        )
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text("聊天翻译按钮UI"),
            "聊天翻译按钮 UI",
        )

    def test_adds_spacing_around_numbers_and_placeholders(self):
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text(
                "您在这张图有%d个存档！"
            ),
            "您在这张图有 %d 个存档！",
        )
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text(
                "宽和高需要在%d到%d内。"
            ),
            "宽和高需要在 %d 到 %d 内。",
        )

    def test_uses_chinese_punctuation_in_chinese_context(self):
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text(
                "示例: 名字1|名字2|名字3"
            ),
            "示例：名字 1|名字 2|名字 3",
        )
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text(
                "喜报! 请更新QmClient!!!"
            ),
            "喜报！请更新 QmClient！！！",
        )
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text("正在加载资源..."),
            "正在加载资源……",
        )

    def test_preserves_command_syntax_and_chinese_slash_pairs(self):
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text(
                "/times ?s?i 显示服务器最近5次成绩"
            ),
            "/times ?s?i 显示服务器最近 5 次成绩",
        )
        self.assertEqual(
            chinese_text_style.normalize_simplified_chinese_text("启用/禁用键盘快捷键"),
            "启用/禁用键盘快捷键",
        )

    def test_i18n_store_normalizes_simplified_chinese_only(self):
        self.assertEqual(
            i18n_store.normalize_translation("simplified_chinese", "加入QQ群"),
            "加入 QQ 群",
        )
        self.assertEqual(
            i18n_store.normalize_translation("traditional_chinese", "加入QQ群"),
            "加入QQ群",
        )


if __name__ == "__main__":
    unittest.main()
