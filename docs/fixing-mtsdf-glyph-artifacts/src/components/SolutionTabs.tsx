import React, { useState } from 'react';
import { Terminal, Code2, Cpu, Wrench, Copy, Check, Download } from 'lucide-react';

export const SolutionTabs: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'cli' | 'python' | 'gui' | 'shader'>('cli');
  const [copiedId, setCopiedId] = useState<string | null>(null);

  const copyToClipboard = (text: string, id: string) => {
    navigator.clipboard.writeText(text);
    setCopiedId(id);
    setTimeout(() => setCopiedId(null), 2000);
  };

  const cliCodeSkia = `# 推荐标准命令（msdf-atlas-gen v1.8+ 默认已集成 Skia 预处理）
# 注意：切勿添加 -nopreprocess 参数！
msdf-atlas-gen \\
  -font "MyJapaneseFont.ttf" \\
  -type mtsdf \\
  -charset "charset.txt" \\
  -size 2048 2048 \\
  -dimensions 64 64 \\
  -pxrange 4 \\
  -imageout "font_atlas.png" \\
  -jsonout "font_atlas.json"`;

  const cliCodeFallback = `# 如果你使用的版本未链接 Skia（如精简构建或旧版 msdfgen）
# 必须显式开启 -overlap（支持重叠轮廓）与 -scanline（扫描线符号纠偏）
msdf-atlas-gen \\
  -font "MyJapaneseFont.ttf" \\
  -type mtsdf \\
  -overlap \\
  -scanline \\
  -pxrange 4 \\
  -imageout "font_atlas.png" \\
  -jsonout "font_atlas.json"

# 单字形 msdfgen 测试命令（测试 U+3042 'あ'）
msdfgen mtsdf -font "MyJapaneseFont.ttf" 'あ' -o test_a.png -overlap -scanline`;

  const cmakeConfig = `# 重新编译 msdfgen / msdf-atlas-gen 开启 Skia 预处理
cmake -B build \\
  -DMSDFGEN_USE_SKIA=ON \\
  -DMSDFGEN_BUILD_MSDFGEN=ON \\
  -DMSDF_ATLAS_GEN_BUILD_STANDALONE=ON
cmake --build build --config Release`;

  const pythonScript = `#!/usr/bin/env python3
"""
字体轮廓布尔合并脚本 (Font Overlap Remover)
作用：遍历字体中所有字形，自动执行 removeOverlap() 与 correctDirection()，
彻底清除导致 MTSDF 虫蚀缺口的自相交笔画。
依赖：fontforge (可通过 pip install fontforge 或 brew/apt 安装)
"""

import os
import sys

try:
    import fontforge
except ImportError:
    print("错误：请先安装 FontForge Python 绑定：")
    print("Ubuntu/Debian: sudo apt-get install python3-fontforge")
    print("macOS: brew install fontforge")
    sys.exit(1)

def clean_font_overlaps(input_path, output_path=None):
    if output_path is None:
        name, ext = os.path.splitext(input_path)
        output_path = f"{name}_cleaned{ext}"

    print(f"正在加载字体: {input_path} ...")
    font = fontforge.open(input_path)

    total_glyphs = 0
    fixed_glyphs = 0

    for glyph in font.glyphs():
        total_glyphs += 1
        # 针对每个字形消除轮廓重叠
        try:
            glyph.removeOverlap()
            glyph.correctDirection()
            glyph.round()
            fixed_glyphs += 1
        except Exception as e:
            pass

    print(f"已处理 {total_glyphs} 个字形，成功熔接优化 {fixed_glyphs} 个轮廓！")
    print(f"正在导出修复后的字体: {output_path} ...")
    font.generate(output_path)
    font.close()
    print("完成！现在将该字体输入 MTSDF 生成器，虫蚀缺口将彻底消失。")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python clean_font.py <字体文件.ttf/.otf> [输出文件.ttf]")
    else:
        in_font = sys.argv[1]
        out_font = sys.argv[2] if len(sys.argv) > 2 else None
        clean_font_overlaps(in_font, out_font)
`;

  const glslShaderCode = `// WebGL / Three.js MTSDF 片元着色器 (Fragment Shader)
precision highp float;

uniform sampler2D u_fontTexture;
uniform vec4 u_textColor;
uniform float u_pxRange; // 比如生成时指定的 4.0

varying vec2 v_uv;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// 动态屏幕像素距离（实现任意缩放不模糊、不锯齿）
float screenPxRange() {
    vec2 unitRange = vec2(u_pxRange) / vec2(textureSize(u_fontTexture, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(v_uv);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    // 采样 MTSDF: RGB 是多通道 MSDF, A 是真单通道 SDF
    vec4 mtsdf = texture2D(u_fontTexture, v_uv);
    
    // 计算 MSDF 中值
    float sd = median(mtsdf.r, mtsdf.g, mtsdf.b);
    
    // 💡 防护机制：如果遇到个别极端边缘冲突，可以用 A 通道(真SDF)做兜底校正：
    // float trueSdf = mtsdf.a;
    // sd = mix(sd, trueSdf, ...);

    float screenPxDistance = screenPxRange() * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    // 输出最终文字颜色
    gl_FragColor = vec4(u_textColor.rgb, u_textColor.a * opacity);
}
`;

  return (
    <div id="solutions-section" className="bg-slate-900 border border-slate-800 rounded-2xl p-6 sm:p-8 shadow-xl mb-12">
      <div className="flex flex-wrap items-center justify-between gap-4 mb-6">
        <div className="flex items-center gap-3">
          <div className="w-9 h-9 rounded-xl bg-emerald-500/20 border border-emerald-500/30 flex items-center justify-center text-emerald-400">
            <Wrench className="w-5 h-5" />
          </div>
          <div>
            <h2 className="text-xl sm:text-2xl font-bold text-white">
              全流程解决方案速查与代码库
            </h2>
            <p className="text-sm text-slate-400">
              从工具链生成参数、字体源文件批处理，到着色器（Shader）配置的完整应对策略
            </p>
          </div>
        </div>
      </div>

      {/* Tabs */}
      <div className="flex flex-wrap gap-2 border-b border-slate-800 pb-3 mb-6">
        <button
          onClick={() => setActiveTab('cli')}
          className={`flex items-center gap-2 px-4 py-2 rounded-xl text-xs sm:text-sm font-semibold transition-all ${
            activeTab === 'cli'
              ? 'bg-emerald-600 text-white shadow-lg shadow-emerald-600/20'
              : 'bg-slate-800/80 text-slate-400 hover:text-white hover:bg-slate-800'
          }`}
        >
          <Terminal className="w-4 h-4" />
          <span>1. CLI 命令行参数 (即刻解决)</span>
        </button>

        <button
          onClick={() => setActiveTab('python')}
          className={`flex items-center gap-2 px-4 py-2 rounded-xl text-xs sm:text-sm font-semibold transition-all ${
            activeTab === 'python'
              ? 'bg-emerald-600 text-white shadow-lg shadow-emerald-600/20'
              : 'bg-slate-800/80 text-slate-400 hover:text-white hover:bg-slate-800'
          }`}
        >
          <Code2 className="w-4 h-4" />
          <span>2. Python 自动化字体清洗 (一劳永逸)</span>
        </button>

        <button
          onClick={() => setActiveTab('gui')}
          className={`flex items-center gap-2 px-4 py-2 rounded-xl text-xs sm:text-sm font-semibold transition-all ${
            activeTab === 'gui'
              ? 'bg-emerald-600 text-white shadow-lg shadow-emerald-600/20'
              : 'bg-slate-800/80 text-slate-400 hover:text-white hover:bg-slate-800'
          }`}
        >
          <Cpu className="w-4 h-4" />
          <span>3. FontForge 图形化消除重叠</span>
        </button>

        <button
          onClick={() => setActiveTab('shader')}
          className={`flex items-center gap-2 px-4 py-2 rounded-xl text-xs sm:text-sm font-semibold transition-all ${
            activeTab === 'shader'
              ? 'bg-emerald-600 text-white shadow-lg shadow-emerald-600/20'
              : 'bg-slate-800/80 text-slate-400 hover:text-white hover:bg-slate-800'
          }`}
        >
          <Terminal className="w-4 h-4" />
          <span>4. MTSDF Shader 与防穿孔规范</span>
        </button>
      </div>

      {/* Tab 1: CLI Flags */}
      {activeTab === 'cli' && (
        <div id="skia-cli" className="space-y-6">
          <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <div className="flex items-center justify-between mb-2">
              <span className="text-xs font-mono font-bold text-emerald-400 flex items-center gap-1.5">
                <Terminal className="w-3.5 h-3.5" /> 首选：现代 msdf-atlas-gen 标准调用
              </span>
              <button
                onClick={() => copyToClipboard(cliCodeSkia, 'cli-skia')}
                className="text-xs text-slate-400 hover:text-white flex items-center gap-1 bg-slate-800 px-2.5 py-1 rounded-md"
              >
                {copiedId === 'cli-skia' ? (
                  <>
                    <Check className="w-3.5 h-3.5 text-emerald-400" /> 已复制
                  </>
                ) : (
                  <>
                    <Copy className="w-3.5 h-3.5" /> 复制命令
                  </>
                )}
              </button>
            </div>
            <pre className="text-xs font-mono text-slate-200 overflow-x-auto p-3 bg-slate-900 rounded-lg">
              {cliCodeSkia}
            </pre>
            <div className="mt-3 text-xs text-slate-400 leading-relaxed">
              💡 <strong>核心排查点</strong>：自 msdfgen 1.8 开始，官方整合了 Google Skia 库进行路径预处理（自动布尔并集自相交笔画）。如果你曾经为了加速加上了 <code className="text-rose-400 bg-slate-900 px-1 py-0.5 rounded">-nopreprocess</code>，请立即<strong>删掉该参数</strong>！
            </div>
          </div>

          <div id="cli-flags" className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <div className="flex items-center justify-between mb-2">
              <span className="text-xs font-mono font-bold text-amber-400 flex items-center gap-1.5">
                <Terminal className="w-3.5 h-3.5" /> 备选：无 Skia 依赖下的 -overlap 与 -scanline 组合
              </span>
              <button
                onClick={() => copyToClipboard(cliCodeFallback, 'cli-fallback')}
                className="text-xs text-slate-400 hover:text-white flex items-center gap-1 bg-slate-800 px-2.5 py-1 rounded-md"
              >
                {copiedId === 'cli-fallback' ? (
                  <>
                    <Check className="w-3.5 h-3.5 text-emerald-400" /> 已复制
                  </>
                ) : (
                  <>
                    <Copy className="w-3.5 h-3.5" /> 复制命令
                  </>
                )}
              </button>
            </div>
            <pre className="text-xs font-mono text-slate-200 overflow-x-auto p-3 bg-slate-900 rounded-lg">
              {cliCodeFallback}
            </pre>
            <div className="mt-3 grid grid-cols-1 md:grid-cols-2 gap-3 text-xs">
              <div className="bg-slate-900 p-3 rounded-lg border border-slate-800">
                <span className="font-bold text-amber-300 font-mono">-overlap</span>
                <p className="text-slate-400 mt-1">切换到支持重叠轮廓的生成模式，避免将笔画交叉区域直接视作字形外部。</p>
              </div>
              <div className="bg-slate-900 p-3 rounded-lg border border-slate-800">
                <span className="font-bold text-amber-300 font-mono">-scanline</span>
                <p className="text-slate-400 mt-1">追加一轮扫描线测试（Scanline pass），强制校准自相交区域的正负距离符号。</p>
              </div>
            </div>
          </div>

          <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <div className="flex items-center justify-between mb-2">
              <span className="text-xs font-mono font-bold text-blue-400 flex items-center gap-1.5">
                <Cpu className="w-3.5 h-3.5" /> 开发者自编译：确保开启 MSDFGEN_USE_SKIA
              </span>
              <button
                onClick={() => copyToClipboard(cmakeConfig, 'cmake')}
                className="text-xs text-slate-400 hover:text-white flex items-center gap-1 bg-slate-800 px-2.5 py-1 rounded-md"
              >
                {copiedId === 'cmake' ? (
                  <>
                    <Check className="w-3.5 h-3.5 text-emerald-400" /> 已复制
                  </>
                ) : (
                  <>
                    <Copy className="w-3.5 h-3.5" /> 复制 CMake 指令
                  </>
                )}
              </button>
            </div>
            <pre className="text-xs font-mono text-slate-200 overflow-x-auto p-3 bg-slate-900 rounded-lg">
              {cmakeConfig}
            </pre>
          </div>
        </div>
      )}

      {/* Tab 2: Python Script */}
      {activeTab === 'python' && (
        <div id="fontforge-script" className="space-y-4">
          <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <div className="flex items-center justify-between mb-2">
              <span className="text-xs font-mono font-bold text-emerald-400 flex items-center gap-1.5">
                <Code2 className="w-3.5 h-3.5" /> clean_font.py：批量字形轮廓焊接脚本
              </span>
              <div className="flex items-center gap-2">
                <button
                  onClick={() => {
                    const blob = new Blob([pythonScript], { type: 'text/plain;charset=utf-8' });
                    const url = URL.createObjectURL(blob);
                    const link = document.createElement('a');
                    link.href = url;
                    link.download = 'clean_font_overlaps.py';
                    link.click();
                  }}
                  className="text-xs text-slate-300 hover:text-white flex items-center gap-1 bg-slate-800 px-2.5 py-1 rounded-md"
                >
                  <Download className="w-3.5 h-3.5" /> 下载 .py 脚本
                </button>
                <button
                  onClick={() => copyToClipboard(pythonScript, 'py-script')}
                  className="text-xs text-slate-300 hover:text-white flex items-center gap-1 bg-indigo-600 px-2.5 py-1 rounded-md font-medium"
                >
                  {copiedId === 'py-script' ? (
                    <>
                      <Check className="w-3.5 h-3.5 text-white" /> 已复制
                    </>
                  ) : (
                    <>
                      <Copy className="w-3.5 h-3.5" /> 复制代码
                    </>
                  )}
                </button>
              </div>
            </div>
            <pre className="text-xs font-mono text-slate-200 overflow-x-auto p-3 bg-slate-900 rounded-lg max-h-[380px]">
              {pythonScript}
            </pre>
            <div className="mt-4 p-3 bg-slate-900 rounded-lg border border-slate-800 text-xs text-slate-300">
              <span className="font-bold text-amber-300">为什么此方案最根本？</span>
              <p className="mt-1 text-slate-400 leading-relaxed">
                无论是使用 MSDF、SDF 还是传统矢量光栅化，未合并重叠笔画的字体在各类渲染引擎（包括 Unity TextMeshPro、WebGL、Canvas）中都有可能偶发闪烁或挖孔。通过该脚本导出的清洗字体，从根本上消除了自相交线，后续无论用何种生成工具都不会再出现虫蚀咬痕。
              </p>
            </div>
          </div>
        </div>
      )}

      {/* Tab 3: FontForge GUI */}
      {activeTab === 'gui' && (
        <div className="space-y-4">
          <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <h3 className="text-sm font-bold text-white mb-3 flex items-center gap-2">
              <Cpu className="w-4 h-4 text-cyan-400" />
              使用开源字体编辑器 FontForge 3 步消除重叠（无需写代码）
            </h3>

            <div className="grid grid-cols-1 md:grid-cols-3 gap-4 my-4">
              <div className="bg-slate-900 p-4 rounded-xl border border-slate-800">
                <span className="text-xs font-mono bg-cyan-950 text-cyan-300 border border-cyan-800 px-2 py-0.5 rounded">
                  步骤 01
                </span>
                <h4 className="font-bold text-slate-200 text-sm mt-2 mb-1">全选字形</h4>
                <p className="text-xs text-slate-400 leading-relaxed">
                  在 FontForge 中打开原字体文件，按下 <kbd className="bg-slate-800 px-1.5 py-0.5 rounded text-amber-300">Ctrl + A</kbd>（Mac: Cmd + A）全选所有字形。
                </p>
              </div>

              <div className="bg-slate-900 p-4 rounded-xl border border-slate-800">
                <span className="text-xs font-mono bg-cyan-950 text-cyan-300 border border-cyan-800 px-2 py-0.5 rounded">
                  步骤 02
                </span>
                <h4 className="font-bold text-slate-200 text-sm mt-2 mb-1">执行 Remove Overlap</h4>
                <p className="text-xs text-slate-400 leading-relaxed">
                  点击顶部菜单栏：<br />
                  <strong className="text-emerald-400">Element</strong> ➔ <strong className="text-emerald-400">Overlap</strong> ➔ <strong className="text-emerald-400">Remove Overlap</strong><br />
                  （快捷键: <kbd className="bg-slate-800 px-1.5 py-0.5 rounded text-amber-300">Ctrl + Shift + O</kbd>）
                </p>
              </div>

              <div className="bg-slate-900 p-4 rounded-xl border border-slate-800">
                <span className="text-xs font-mono bg-cyan-950 text-cyan-300 border border-cyan-800 px-2 py-0.5 rounded">
                  步骤 03
                </span>
                <h4 className="font-bold text-slate-200 text-sm mt-2 mb-1">校正方向并重新生成</h4>
                <p className="text-xs text-slate-400 leading-relaxed">
                  点击 <strong className="text-emerald-400">Element</strong> ➔ <strong className="text-emerald-400">Correct Direction</strong>（快捷键 <kbd className="bg-slate-800 px-1.5 py-0.5 rounded text-amber-300">Ctrl + Shift + D</kbd>），然后点击 <strong className="text-emerald-400">File ➔ Generate Fonts</strong> 保存为新 TTF/OTF。
                </p>
              </div>
            </div>

            <div className="p-3 bg-cyan-950/30 border border-cyan-800/40 rounded-lg text-xs text-cyan-300">
              💡 <strong>小窍门</strong>：在 Glyphs 或 FontLab 等商业字体软件中，同样可以在导出面板（Export Dialog）中直接勾选 <strong>“Remove Overlap”</strong>，导出时会自动将重叠笔画打平。
            </div>
          </div>
        </div>
      )}

      {/* Tab 4: Shader */}
      {activeTab === 'shader' && (
        <div className="space-y-4">
          <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <div className="flex items-center justify-between mb-2">
              <span className="text-xs font-mono font-bold text-purple-400 flex items-center gap-1.5">
                <Terminal className="w-3.5 h-3.5" /> WebGL / Three.js MTSDF 片元着色器标准实现
              </span>
              <button
                onClick={() => copyToClipboard(glslShaderCode, 'glsl-code')}
                className="text-xs text-slate-400 hover:text-white flex items-center gap-1 bg-slate-800 px-2.5 py-1 rounded-md"
              >
                {copiedId === 'glsl-code' ? (
                  <>
                    <Check className="w-3.5 h-3.5 text-emerald-400" /> 已复制
                  </>
                ) : (
                  <>
                    <Copy className="w-3.5 h-3.5" /> 复制 Shader
                  </>
                )}
              </button>
            </div>
            <pre className="text-xs font-mono text-slate-200 overflow-x-auto p-3 bg-slate-900 rounded-lg max-h-[360px]">
              {glslShaderCode}
            </pre>
            <div className="mt-4 grid grid-cols-1 md:grid-cols-2 gap-3 text-xs text-slate-400">
              <div className="bg-slate-900 p-3 rounded-lg border border-slate-800">
                <span className="font-bold text-white">MTSDF 中 Alpha 通道的作用</span>
                <p className="mt-1">
                  MTSDF = MSDF (RGB) + True SDF (Alpha)。当字形在极其尖锐的拐角或复杂的相交区域出现中值失真时，可以通过 Shader 将 RGB 的 MSDF 与 A 通道的 SDF 融合，从而避免穿孔破洞。
                </p>
              </div>
              <div className="bg-slate-900 p-3 rounded-lg border border-slate-800">
                <span className="font-bold text-white">动态 screenPxRange 计算</span>
                <p className="mt-1">
                  使用硬件内置的导数函数 <code className="text-indigo-300">fwidth()</code> 或 <code className="text-indigo-300">dFdx / dFdy</code>，保证文字在从极远到极近缩放时，边缘抗锯齿宽度恒定为 1 个物理屏幕像素。
                </p>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
