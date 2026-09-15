import React from 'react';
import { AlertTriangle, CheckCircle2, Zap, ArrowRight, ShieldCheck, HelpCircle } from 'lucide-react';

interface QuickAnswerCardProps {
  onJumpToSolution: (id: string) => void;
}

export const QuickAnswerCard: React.FC<QuickAnswerCardProps> = ({ onJumpToSolution }) => {
  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 sm:p-8 shadow-2xl relative overflow-hidden text-white">
      {/* Background glow accent */}
      <div className="absolute -top-24 -right-24 w-80 h-80 bg-red-500/10 rounded-full blur-3xl pointer-events-none" />
      <div className="absolute -bottom-24 -left-24 w-80 h-80 bg-emerald-500/10 rounded-full blur-3xl pointer-events-none" />

      <div className="relative z-10">
        <div className="flex flex-wrap items-center gap-3 mb-4">
          <span className="inline-flex items-center gap-1.5 px-3 py-1 rounded-full text-xs font-semibold bg-amber-500/20 text-amber-300 border border-amber-500/30">
            <AlertTriangle className="w-3.5 h-3.5" /> 故障诊断：轮廓相交 / 环绕数翻转
          </span>
          <span className="inline-flex items-center gap-1.5 px-3 py-1 rounded-full text-xs font-semibold bg-blue-500/20 text-blue-300 border border-blue-500/30">
            针对字符：U+3042 (あ) 及复合笔画字形
          </span>
        </div>

        <h1 className="text-2xl sm:text-3xl lg:text-4xl font-bold tracking-tight text-white mb-4">
          为什么 MTSDF 会出现<span className="text-rose-400">“虫蚀状 / 咬痕”</span>缺口？如何彻底解决？
        </h1>

        <p className="text-slate-300 text-base sm:text-lg leading-relaxed max-w-4xl mb-6">
          如图中所示，日文假名 <code className="bg-slate-800 text-amber-300 px-2 py-0.5 rounded font-mono font-bold">あ (U+3042)</code> 在横竖笔画与右侧圆弧环相交的节点处被硬生生挖出了缺口。这并不是分辨率不足，而是由于原字体的<strong>笔画轮廓自相交（Self-intersecting Contours / Overlapping Contours）</strong>，破坏了有向距离场的符号内外判定与多通道中值滤波规则。
        </p>

        {/* 30-second core solution cards */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6">
          <div className="bg-slate-800/80 hover:bg-slate-800 border border-emerald-500/30 rounded-xl p-4.5 transition-all">
            <div className="flex items-center gap-2.5 text-emerald-400 font-semibold mb-2">
              <CheckCircle2 className="w-5 h-5 flex-shrink-0" />
              <span>方案一：开启 Skia 预处理（最推荐）</span>
            </div>
            <p className="text-slate-300 text-sm leading-relaxed mb-3">
              现代 <code className="text-emerald-300 font-mono">msdf-atlas-gen</code> 已经内置了 Google Skia 库进行路径布尔合并。确保你使用的版本编译了 Skia，且<strong>千万不要加 <code className="text-rose-300 font-mono">-nopreprocess</code></strong>。
            </p>
            <button
              onClick={() => onJumpToSolution('skia-cli')}
              className="text-xs text-emerald-400 font-semibold hover:text-emerald-300 flex items-center gap-1"
            >
              查看 CLI 命令与编译配置 <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>

          <div className="bg-slate-800/80 hover:bg-slate-800 border border-cyan-500/30 rounded-xl p-4.5 transition-all">
            <div className="flex items-center gap-2.5 text-cyan-400 font-semibold mb-2">
              <ShieldCheck className="w-5 h-5 flex-shrink-0" />
              <span>方案二：字体源头消除重叠（最稳健）</span>
            </div>
            <p className="text-slate-300 text-sm leading-relaxed mb-3">
              在输入 MTSDF 生成器前，用 Python 或 FontForge 对字体执行一次 <code className="text-cyan-300 font-mono">removeOverlap()</code>，将交叉笔画焊死成单一轮廓，一劳永逸。
            </p>
            <button
              onClick={() => onJumpToSolution('fontforge-script')}
              className="text-xs text-cyan-400 font-semibold hover:text-cyan-300 flex items-center gap-1"
            >
              复制 Python 自动化清洗脚本 <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>

          <div className="bg-slate-800/80 hover:bg-slate-800 border border-purple-500/30 rounded-xl p-4.5 transition-all">
            <div className="flex items-center gap-2.5 text-purple-400 font-semibold mb-2">
              <Zap className="w-5 h-5 flex-shrink-0" />
              <span>方案三：应急命令行开关</span>
            </div>
            <p className="text-slate-300 text-sm leading-relaxed mb-3">
              若在老版工具或无 Skia 环境下，向 CLI 追加参数 <code className="text-purple-300 font-mono">-overlap</code>（重叠轮廓模式）及 <code className="text-purple-300 font-mono">-scanline</code>（扫描线符号纠偏）。
            </p>
            <button
              onClick={() => onJumpToSolution('cli-flags')}
              className="text-xs text-purple-400 font-semibold hover:text-purple-300 flex items-center gap-1"
            >
              获取完整参数组合 <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>

        {/* Quick diagnosis banner */}
        <div className="flex items-center justify-between flex-wrap gap-4 pt-4 border-t border-slate-800/80 text-sm text-slate-400">
          <div className="flex items-center gap-2">
            <HelpCircle className="w-4 h-4 text-slate-500" />
            <span>受影响字体类型：中日韩手写/毛笔字、连笔花体、未合并导出的 OpenType / TTF 字体</span>
          </div>
          <span className="text-xs text-slate-500 font-mono">Bug Signature: MSDF Contour Self-Intersection &amp; Winding Inversion</span>
        </div>
      </div>
    </div>
  );
};
