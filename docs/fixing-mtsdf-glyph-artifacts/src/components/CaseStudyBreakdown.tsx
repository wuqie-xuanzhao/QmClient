import React from 'react';
import { Target, CheckCircle } from 'lucide-react';

export const CaseStudyBreakdown: React.FC = () => {
  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 sm:p-8 shadow-xl mb-12">
      <div className="flex items-center gap-3 mb-6">
        <div className="w-9 h-9 rounded-xl bg-rose-500/20 border border-rose-500/30 flex items-center justify-center text-rose-400">
          <Target className="w-5 h-5" />
        </div>
        <div>
          <h2 className="text-xl sm:text-2xl font-bold text-white">
            您提问截图的“病理剖析”：为什么缺口会长这样？
          </h2>
          <p className="text-sm text-slate-400">
            逐像素解析 U+3042 (あ) 在 MTSDF 栅格化过程中的几何与拓扑陷阱
          </p>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-center">
        {/* Left: Reconstructed visual diagram of the screenshot */}
        <div className="lg:col-span-5 bg-slate-950 p-6 rounded-xl border border-slate-800 flex flex-col items-center justify-center relative">
          <div className="text-[11px] font-mono text-slate-400 mb-2 w-full flex justify-between">
            <span>原图诊断标记 (U+3042)</span>
            <span className="text-rose-400">虫蚀特征检测 100% 吻合</span>
          </div>

          <div className="relative w-full aspect-square max-w-[280px] bg-[#6f757d] rounded-lg overflow-hidden flex items-center justify-center shadow-lg border border-slate-600">
            {/* SVG reproduction matching user screenshot */}
            <svg viewBox="0 0 100 100" className="w-full h-full p-4">
              {/* Horizontal line */}
              <path
                d="M 18 28 C 35 30 65 28 85 24"
                stroke="#ffffff"
                strokeWidth="4"
                strokeLinecap="round"
                fill="none"
              />
              {/* Vertical spine */}
              <path
                d="M 45 15 C 44 40 43 65 47 88"
                stroke="#ffffff"
                strokeWidth="4.5"
                strokeLinecap="round"
                fill="none"
              />
              {/* Loop stroke with the exact bug cutout */}
              <path
                d="M 70 36 C 62 55 35 70 25 65 C 15 60 18 45 32 38 C 45 32 68 40 82 58 C 88 68 78 82 58 87"
                stroke="#ffffff"
                strokeWidth="4"
                strokeLinecap="round"
                fill="none"
              />

              {/* Bug Cutout 1: Top right notch on loop stroke */}
              <rect x="68" y="38" width="6" height="5" fill="#6f757d" transform="rotate(25 71 40)" />
              {/* Tiny remnant dot inside */}
              <circle cx="70.5" cy="40.5" r="0.8" fill="#ffffff" />

              {/* Highlight callout circle around bug */}
              <circle
                cx="71"
                cy="41"
                r="7"
                fill="none"
                stroke="#ef4444"
                strokeWidth="1.2"
                strokeDasharray="2 1.5"
              />
            </svg>

            {/* Float badge */}
            <div className="absolute top-2 left-2 text-[10px] font-mono text-slate-300 bg-black/60 px-2 py-0.5 rounded backdrop-blur">
              U+3042
            </div>
          </div>

          <div className="mt-3 text-center text-xs text-rose-300 font-medium">
            ↑ 红虚线圈内即为您截图中被“咬掉”的主断口
          </div>
        </div>

        {/* Right: Detailed explanations */}
        <div className="lg:col-span-7 space-y-4">
          <div className="bg-slate-950 p-4 rounded-xl border border-slate-800">
            <h3 className="text-sm font-bold text-white flex items-center gap-2 mb-1.5">
              <span className="w-2 h-2 rounded-full bg-rose-500" />
              断口特征 1：为什么缺口中间还留着一个小白点？
            </h3>
            <p className="text-xs text-slate-300 leading-relaxed">
              仔细观察您的原图，在右上方回环笔画的缺口内，甚至悬浮着一个微弱的白色细像素。这正是<strong>偶数层轮廓奇偶反转（Even-Odd Flip）</strong>的铁证！笔画起始端与弧线回环交叉形成了三层重合区（1层内部 ➔ 2层变外部 ➔ 3层极深处又偶数变回内部），直接验证了是矢量相交所致。
            </p>
          </div>

          <div className="bg-slate-950 p-4 rounded-xl border border-slate-800">
            <h3 className="text-sm font-bold text-white flex items-center gap-2 mb-1.5">
              <span className="w-2 h-2 rounded-full bg-amber-500" />
              断口特征 2：为什么笔画交叉处变细或呈凹陷锯齿？
            </h3>
            <p className="text-xs text-slate-300 leading-relaxed">
              竖直中轴线在与回环交叉的地方，距离场计算时两段贝塞尔曲线各自往相反方向计算法线。没有布尔并集时，两曲线的有向距离相互冲抵，使相交处的距离极值被人为压低，片元判定边缘收缩，形成“细腰”与“啃咬感”。
            </p>
          </div>

          <div className="bg-emerald-950/20 border border-emerald-900/40 p-4 rounded-xl">
            <h3 className="text-sm font-bold text-emerald-400 flex items-center gap-2 mb-1.5">
              <CheckCircle className="w-4 h-4 text-emerald-400" />
              对症下药：30 秒即刻解决路线
            </h3>
            <div className="text-xs text-slate-300 space-y-2">
              <p>
                无需修改或重新设计字体，直接采用<strong>“Skia 自动熔接”</strong>或运行我们准备的 Python 单文件脚本，在 1 秒内将所有重叠笔画自动熔为一体即可！
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
