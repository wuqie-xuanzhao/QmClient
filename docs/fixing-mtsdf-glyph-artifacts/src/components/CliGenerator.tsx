import React, { useState } from 'react';
import { Sliders, Copy, Check, Sparkles, Terminal } from 'lucide-react';

export const CliGenerator: React.FC = () => {
  const [tool, setTool] = useState<'msdf-atlas-gen' | 'msdfgen' | 'msdf-bmfont-xml'>('msdf-atlas-gen');
  const [fontFile, setFontFile] = useState<string>('SourceHanSansSC-Bold.otf');
  const [charPreset, setCharPreset] = useState<string>('hiragana');
  const [customChars, setCustomChars] = useState<string>('あいうえおかきくけこさしすせそ');
  const [atlasSize, setAtlasSize] = useState<string>('2048 2048');
  const [glyphDim, setGlyphDim] = useState<number>(48);
  const [pxRange, setPxRange] = useState<number>(4);
  const [outputType, setOutputType] = useState<'mtsdf' | 'msdf' | 'sdf'>('mtsdf');

  // Fix switches
  const [useSkiaPreprocess, setUseSkiaPreprocess] = useState<boolean>(true);
  const [enableOverlap, setEnableOverlap] = useState<boolean>(false);
  const [enableScanline, setEnableScanline] = useState<boolean>(false);
  const [guessWinding, setGuessWinding] = useState<boolean>(false);

  const [copied, setCopied] = useState<boolean>(false);

  // Generate the command string
  const generateCommand = () => {
    if (tool === 'msdf-atlas-gen') {
      const parts = ['msdf-atlas-gen'];
      parts.push(`-font "${fontFile}"`);
      parts.push(`-type ${outputType}`);

      if (charPreset === 'hiragana') {
        parts.push('-charset "hiragana_charset.txt"');
      } else if (charPreset === 'ascii') {
        parts.push('-charset "ascii.txt"');
      } else {
        parts.push(`-chars "${customChars}"`);
      }

      parts.push(`-size ${atlasSize}`);
      parts.push(`-dimensions ${glyphDim} ${glyphDim}`);
      parts.push(`-pxrange ${pxRange}`);

      // Overlap & bug fix flags
      if (!useSkiaPreprocess) {
        parts.push('-nopreprocess');
      }
      if (enableOverlap) {
        parts.push('-overlap');
      }
      if (enableScanline) {
        parts.push('-scanline');
      }
      if (guessWinding) {
        parts.push('-guesswinding');
      }

      parts.push('-imageout "atlas.png"');
      parts.push('-jsonout "atlas.json"');

      return parts.join(' \\\n  ');
    } else if (tool === 'msdfgen') {
      const parts = ['msdfgen', outputType];
      parts.push(`-font "${fontFile}"`);
      parts.push(`'${customChars.charAt(0) || 'あ'}'`);
      parts.push(`-size ${glyphDim} ${glyphDim}`);
      parts.push(`-pxrange ${pxRange}`);

      if (!useSkiaPreprocess) {
        parts.push('-nopreprocess');
      }
      if (enableOverlap) {
        parts.push('-overlap');
      }
      if (enableScanline) {
        parts.push('-scanline');
      }
      if (guessWinding) {
        parts.push('-guesswinding');
      }

      parts.push('-o "glyph_out.png"');
      return parts.join(' \\\n  ');
    } else {
      // msdf-bmfont-xml
      return `msdf-bmfont -f json -m ${atlasSize.split(' ')[0]},${atlasSize.split(' ')[1]} -s ${glyphDim} -r ${pxRange} -t ${outputType} "${fontFile}"`;
    }
  };

  const handleCopy = () => {
    navigator.clipboard.writeText(generateCommand());
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 sm:p-8 shadow-xl mb-12">
      <div className="flex items-center gap-3 mb-6">
        <div className="w-9 h-9 rounded-xl bg-blue-500/20 border border-blue-500/30 flex items-center justify-center text-blue-400">
          <Sliders className="w-5 h-5" />
        </div>
        <div>
          <h2 className="text-xl sm:text-2xl font-bold text-white">
            定制生成命令：一键配齐防虫蚀参数
          </h2>
          <p className="text-sm text-slate-400">
            针对您的项目字体、图集规格和目标引擎，自动生成无缺陷的 MTSDF 生成器命令行
          </p>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Settings form (Left) */}
        <div className="lg:col-span-6 space-y-4">
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">生成工具链</label>
              <select
                value={tool}
                onChange={(e) => setTool(e.target.value as any)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-3 py-2 text-xs text-white focus:outline-none focus:border-indigo-500"
              >
                <option value="msdf-atlas-gen">msdf-atlas-gen (官方图集推荐)</option>
                <option value="msdfgen">msdfgen (单字形调试)</option>
                <option value="msdf-bmfont-xml">msdf-bmfont-xml (Node.js)</option>
              </select>
            </div>

            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">输出格式</label>
              <select
                value={outputType}
                onChange={(e) => setOutputType(e.target.value as any)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-3 py-2 text-xs text-white focus:outline-none focus:border-indigo-500"
              >
                <option value="mtsdf">MTSDF (RGB多通道 + A单通道真SDF)</option>
                <option value="msdf">MSDF (仅RGB多通道)</option>
                <option value="sdf">SDF (单通道传统距离场)</option>
              </select>
            </div>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">字体文件名</label>
              <input
                type="text"
                value={fontFile}
                onChange={(e) => setFontFile(e.target.value)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-3 py-2 text-xs text-white font-mono focus:outline-none focus:border-indigo-500"
                placeholder="font.ttf"
              />
            </div>

            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">字符集范围</label>
              <select
                value={charPreset}
                onChange={(e) => setCharPreset(e.target.value)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-3 py-2 text-xs text-white focus:outline-none focus:border-indigo-500"
              >
                <option value="hiragana">日文平假名/片假名 (包含 あ 等易重叠字符)</option>
                <option value="ascii">常用 ASCII 英文</option>
                <option value="custom">指定字符/单字测试</option>
              </select>
            </div>
          </div>

          {charPreset === 'custom' && (
            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">自定义字符 (包含 U+3042)</label>
              <input
                type="text"
                value={customChars}
                onChange={(e) => setCustomChars(e.target.value)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-3 py-2 text-xs text-white focus:outline-none focus:border-indigo-500 font-mono"
              />
            </div>
          )}

          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">图集尺寸</label>
              <select
                value={atlasSize}
                onChange={(e) => setAtlasSize(e.target.value)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-2 text-xs text-white focus:outline-none focus:border-indigo-500 font-mono"
              >
                <option value="1024 1024">1024x1024</option>
                <option value="2048 2048">2048x2048</option>
                <option value="4096 4096">4096x4096</option>
              </select>
            </div>

            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">单字像素</label>
              <input
                type="number"
                value={glyphDim}
                onChange={(e) => setGlyphDim(parseInt(e.target.value) || 32)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-2 text-xs text-white focus:outline-none focus:border-indigo-500 font-mono"
              />
            </div>

            <div>
              <label className="text-xs font-semibold text-slate-300 block mb-1.5">pxRange (距离)</label>
              <input
                type="number"
                value={pxRange}
                onChange={(e) => setPxRange(parseInt(e.target.value) || 4)}
                className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-2 text-xs text-white focus:outline-none focus:border-indigo-500 font-mono"
              />
            </div>
          </div>

          {/* Critical fix toggles */}
          <div className="pt-2 border-t border-slate-800 space-y-2">
            <span className="text-xs font-bold text-slate-400 block mb-1">
              防虫蚀与几何修正核心开关：
            </span>

            <label className="flex items-center justify-between p-2.5 rounded-lg bg-slate-950 border border-slate-800 cursor-pointer hover:border-slate-700">
              <div>
                <span className="text-xs font-semibold text-emerald-400 block">
                  启用 Skia 几何预处理（默认保持开启）
                </span>
                <span className="text-[11px] text-slate-400">
                  自动简化自相交笔画，消除虫蚀根源（取消即添加 -nopreprocess）
                </span>
              </div>
              <input
                type="checkbox"
                checked={useSkiaPreprocess}
                onChange={(e) => setUseSkiaPreprocess(e.target.checked)}
                className="w-4 h-4 accent-emerald-500"
              />
            </label>

            <label className="flex items-center justify-between p-2.5 rounded-lg bg-slate-950 border border-slate-800 cursor-pointer hover:border-slate-700">
              <div>
                <span className="text-xs font-semibold text-amber-400 font-mono block">
                  -overlap (重叠轮廓模式)
                </span>
                <span className="text-[11px] text-slate-400">
                  非 Skia 环境下的必要后备参数，激活自交保护
                </span>
              </div>
              <input
                type="checkbox"
                checked={enableOverlap}
                onChange={(e) => setEnableOverlap(e.target.checked)}
                className="w-4 h-4 accent-amber-500"
              />
            </label>

            <label className="flex items-center justify-between p-2.5 rounded-lg bg-slate-950 border border-slate-800 cursor-pointer hover:border-slate-700">
              <div>
                <span className="text-xs font-semibold text-purple-400 font-mono block">
                  -scanline (扫描线正负纠偏)
                </span>
                <span className="text-[11px] text-slate-400">
                  执行额外光栅扫描线测试，校正笔画重叠区的距离正负号
                </span>
              </div>
              <input
                type="checkbox"
                checked={enableScanline}
                onChange={(e) => setEnableScanline(e.target.checked)}
                className="w-4 h-4 accent-purple-500"
              />
            </label>

            <label className="flex items-center justify-between p-2.5 rounded-lg bg-slate-950 border border-slate-800 cursor-pointer hover:border-slate-700">
              <div>
                <span className="text-xs font-semibold text-blue-400 font-mono block">
                  -guesswinding (智能推断轮廓顺逆时针)
                </span>
                <span className="text-[11px] text-slate-400">
                  自动纠正字体文件内由于方向反转造成的内外反相
                </span>
              </div>
              <input
                type="checkbox"
                checked={guessWinding}
                onChange={(e) => setGuessWinding(e.target.checked)}
                className="w-4 h-4 accent-blue-500"
              />
            </label>
          </div>
        </div>

        {/* Command Output preview (Right) */}
        <div className="lg:col-span-6 flex flex-col justify-between bg-slate-950 border border-slate-800 rounded-xl p-5">
          <div>
            <div className="flex items-center justify-between mb-3">
              <span className="text-xs font-mono font-bold text-slate-300 flex items-center gap-1.5">
                <Terminal className="w-3.5 h-3.5 text-indigo-400" /> 最终生成命令 (Ready to Run)
              </span>
              <button
                onClick={handleCopy}
                className="text-xs bg-indigo-600 hover:bg-indigo-500 text-white font-medium px-3 py-1.5 rounded-md flex items-center gap-1.5 transition-colors shadow"
              >
                {copied ? (
                  <>
                    <Check className="w-3.5 h-3.5" /> 已复制到剪贴板
                  </>
                ) : (
                  <>
                    <Copy className="w-3.5 h-3.5" /> 复制命令
                  </>
                )}
              </button>
            </div>

            <div className="bg-slate-900 rounded-lg p-4 font-mono text-xs text-indigo-200 overflow-x-auto border border-slate-800 max-h-[300px]">
              <pre>{generateCommand()}</pre>
            </div>
          </div>

          {/* Quick status advice */}
          <div className="mt-4 p-3 bg-slate-900/90 rounded-lg border border-slate-800 text-xs space-y-1.5">
            <div className="flex items-center gap-1.5 font-bold text-white">
              <Sparkles className="w-3.5 h-3.5 text-amber-400" />
              <span>当前配置防护等级评定：</span>
            </div>
            <p className="text-slate-400 leading-relaxed">
              {useSkiaPreprocess ? (
                <span className="text-emerald-400">
                  <strong>已就绪</strong>：使用了 Skia 路径布尔简化（默认），U+3042 等日文和复杂汉字字形将平滑生成，不会产生虫蚀咬痕。
                </span>
              ) : enableOverlap && enableScanline ? (
                <span className="text-amber-400">
                  <strong>已加固后备模式</strong>：关闭了 Skia 预处理，但启用了 -overlap 与 -scanline，能够抑制大部分轮廓重叠翻转。
                </span>
              ) : (
                <span className="text-rose-400">
                  <strong>高风险</strong>：未开启预处理且未加 -overlap/-scanline，重叠笔画大概率会复现提问中的虫蚀空洞！
                </span>
              )}
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};
