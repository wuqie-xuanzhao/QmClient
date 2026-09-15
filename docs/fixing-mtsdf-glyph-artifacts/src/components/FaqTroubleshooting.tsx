import React, { useState } from 'react';
import { HelpCircle, ChevronDown } from 'lucide-react';

interface FaqItem {
  q: string;
  a: React.ReactNode;
  tag: string;
}

export const FaqTroubleshooting: React.FC = () => {
  const [openIdx, setOpenIdx] = useState<number | null>(0);

  const faqs: FaqItem[] = [
    {
      tag: '原理差异',
      q: '为什么传统单通道 SDF 没这么严重，换成 MSDF / MTSDF 后咬痕特别明显？',
      a: (
        <div className="space-y-2 text-slate-300 text-sm leading-relaxed">
          <p>
            因为 MSDF 的核心在于<strong>“多通道边缘着色（Edge Coloring）”</strong>与片元着色器的<strong>“中值滤波（Median Filter）”</strong>：
          </p>
          <ul className="list-disc list-inside space-y-1 text-xs text-slate-400">
            <li>
              传统单通道 SDF 只计算到最近边缘的单一距离，相交处即使符号有局部扰动，通常只呈现为微小的灰度变暗或轻微缩水。
            </li>
            <li>
              而在 MSDF 中，生成器会将形状的各个拐角轮廓分配给 R、G、B 通道。当笔画自相交时，相交点既属于轮廓 A 又属于轮廓 B，算法在三色分配时会产生剧烈的<strong>“通道颜色竞争与割裂”</strong>。
            </li>
            <li>
              在 GPU 端，着色器执行 <code className="text-amber-300">median(r, g, b)</code>。只要两个通道在相交处判定为负数或距离低于 0.5，中值就会被立刻下拉到背景阈值之下，导致该区域像素被片元着色器直接剔除，形成如同被老鼠啃掉一般的硬缺口！
            </li>
          </ul>
        </div>
      ),
    },
    {
      tag: '字体特性',
      q: '为什么英文 ABC 没问题，日文假名（如 あ、ぬ、め）和汉字就频繁中招？',
      a: (
        <div className="space-y-2 text-slate-300 text-sm leading-relaxed">
          <p>
            这与<strong>字体的制作工艺</strong>息息相关：
          </p>
          <p className="text-xs text-slate-400">
            大多数拉丁英文字体（特别是无衬线体 Helvetica、Arial 等）由几何外轮廓精确勾勒，笔画在设计时即为一个完整的闭合路径；
            而中日韩（CJK）字体、假名、连笔花体或手写毛笔字体，字库厂商为了制作方便，通常采用<strong>“组合笔画拼装法”</strong>——先画一横，再放一竖，最后搭上一个大回环笔画，且导出时未勾选布尔合并（Remove Overlap）。这导致字形在矢量层面上是多个互相穿插的独立闭环。
          </p>
        </div>
      ),
    },
    {
      tag: '参数迷思',
      q: '提高分辨率（例如从 32px 改为 128px）或者增大 pxRange 能修复虫蚀吗？',
      a: (
        <div className="space-y-2 text-slate-300 text-sm leading-relaxed">
          <p className="font-semibold text-rose-300">
            结论：不能！单纯提高分辨率或 pxRange 无法解决该问题。
          </p>
          <p className="text-xs text-slate-400">
            因为这是<strong>几何拓扑层面的符号判定错误</strong>，而非采样精度不足。提高分辨率只会让那个被咬掉的缺口边缘变得“更清晰、更锐利”，但缺口本身依然存在。唯有通过 <strong>Skia 几何布尔并集</strong> 或 <strong>字体 Remove Overlap</strong>，重构拓扑流形，才能根除。
          </p>
        </div>
      ),
    },
    {
      tag: '引擎排查',
      q: '在 Unity TextMeshPro (TMP) 中生成字体资产也遇到了咬痕，该怎么解决？',
      a: (
        <div className="space-y-2 text-slate-300 text-sm leading-relaxed">
          <p>
            Unity 的 Font Asset Creator 内部使用的是 FreeType + 自研 SDF 光栅化：
          </p>
          <ol className="list-decimal list-inside space-y-1.5 text-xs text-slate-400">
            <li>
              <strong>最稳定方案</strong>：使用方案二中的 Python 脚本或 FontForge 执行 <code className="text-emerald-300">removeOverlap()</code>，将清洗后的字体导入 Unity 再行烘焙，100% 解决。
            </li>
            <li>
              <strong>TMP 窗口设置</strong>：在 Font Asset Creator 中，确保 Render Mode 选择为 <strong>SDFAA</strong> 或 <strong>SDF32</strong>，将 Padding 适当放大至 5~7。
            </li>
          </ol>
        </div>
      ),
    },
    {
      tag: '通道进阶',
      q: 'MTSDF 的 4 个通道中，Alpha 通道为什么是真 SDF？如何利用它？',
      a: (
        <div className="space-y-2 text-slate-300 text-sm leading-relaxed">
          <p>
            <strong>MTSDF = Multi-channel Signed Distance Field with True Distance Field</strong>
          </p>
          <p className="text-xs text-slate-400">
            RGB 通道保存的是 MSDF（用于保证锐利拐角不被圆角化），而 Alpha 通道保存的是标准的单通道 Signed Distance Field（真有向距离）。
            在高级着色器中，可以通过以下逻辑进行防错融合：当检测到 RGB 距离差值过大（发生颜色翻转伪影）时，回退到 A 通道的真实距离，达到兼具 MSDF 锐利拐角与 SDF 平滑稳定性的最佳效果。
          </p>
        </div>
      ),
    },
  ];

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 sm:p-8 shadow-xl mb-12">
      <div className="flex items-center gap-3 mb-6">
        <div className="w-9 h-9 rounded-xl bg-amber-500/20 border border-amber-500/30 flex items-center justify-center text-amber-400">
          <HelpCircle className="w-5 h-5" />
        </div>
        <div>
          <h2 className="text-xl sm:text-2xl font-bold text-white">
            常见疑难与衍生问题深度 FAQ
          </h2>
          <p className="text-sm text-slate-400">
            关于 CJK 字体兼容性、分辨率迷思、着色器调优与 Unity / WebGL 引擎实战解答
          </p>
        </div>
      </div>

      <div className="space-y-3">
        {faqs.map((item, idx) => {
          const isOpen = openIdx === idx;
          return (
            <div
              key={idx}
              className="bg-slate-950 border border-slate-800/80 rounded-xl overflow-hidden transition-all"
            >
              <button
                onClick={() => setOpenIdx(isOpen ? null : idx)}
                className="w-full text-left p-4.5 flex items-center justify-between gap-4 hover:bg-slate-900/50 transition-colors"
              >
                <div className="flex items-center gap-3">
                  <span className="text-[11px] font-mono px-2 py-0.5 rounded bg-slate-800 text-slate-400 border border-slate-700 flex-shrink-0">
                    {item.tag}
                  </span>
                  <span className="text-sm font-bold text-slate-200">{item.q}</span>
                </div>
                <ChevronDown
                  className={`w-4 h-4 text-slate-400 transition-transform flex-shrink-0 ${
                    isOpen ? 'rotate-180 text-white' : ''
                  }`}
                />
              </button>

              {isOpen && (
                <div className="px-5 pb-5 pt-1 border-t border-slate-800/50">
                  {item.a}
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
};
