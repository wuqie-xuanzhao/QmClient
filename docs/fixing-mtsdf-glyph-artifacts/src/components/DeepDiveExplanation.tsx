import React, { useState } from 'react';
import { BookOpen, Zap, AlertCircle, CheckCircle2 } from 'lucide-react';

export const DeepDiveExplanation: React.FC = () => {
  const [activeStep, setActiveStep] = useState<number>(0);

  const steps = [
    {
      title: '步骤 1：字体字形包含“未合并的重叠笔画”',
      desc: '中日韩汉字与假名（如“あ”）在字体设计时，设计师常常分笔画绘制。横折、竖笔和大回环在交叉处直接叠加在一起，没有在导出时做布尔并集（Boolean Union）。',
      highlight: '笔画物理重合，存在两条独立的交叉贝塞尔轮廓',
      tag: '字体设计源头',
    },
    {
      title: '步骤 2：环绕数（Winding Number）计算冲突',
      desc: '生成器需要通过扫描线或射线法计算内外性。当自相交轮廓的环绕方向相反、或生成器采用奇偶规则（Even-Odd Rule）时，两条重合区域会被误判为“字形外部”。',
      highlight: '符号距离 Sign 发生取反：内部区域的有向距离变成了负数！',
      tag: '几何拓扑失效',
    },
    {
      title: '步骤 3：MSDF 多通道边缘着色（Edge Coloring）撕裂',
      desc: 'MSDF 依赖将不同方向的轮廓切分赋予 RGB 三种颜色。但在相交交叉点，生成器无法理清哪条边属于哪个通道，导致通道间距离不连续。',
      highlight: 'RGB 三个通道在交叉区域的值被破坏',
      tag: '多通道着色混乱',
    },
    {
      title: '步骤 4：片元着色器 median(r, g, b) 剔除形成“虫蚀缺口”',
      desc: '在 GPU 端，着色器执行 float sd = median(msdf.r, msdf.g, msdf.b); 当至少两个通道因为上述原因被翻转或变为负值时，中值跌破 0.5 阈值，片元直接被丢弃（透明），形成镂空咬痕。',
      highlight: '像素被 Shader 丢弃，形成视觉上的“虫子咬了一口”缺口',
      tag: '渲染端断裂',
    },
  ];

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 sm:p-8 shadow-xl mb-12">
      <div className="flex items-center gap-3 mb-6">
        <div className="w-9 h-9 rounded-xl bg-purple-500/20 border border-purple-500/30 flex items-center justify-center text-purple-400">
          <BookOpen className="w-5 h-5" />
        </div>
        <div>
          <h2 className="text-xl sm:text-2xl font-bold text-white">
            原理深度拆解：为什么重叠轮廓会导致“虫蚀缺口”？
          </h2>
          <p className="text-sm text-slate-400">
            从字体矢量轮廓、数学环绕数，到 MSDF 边缘着色与 Shader 中值滤波的完整链路分析
          </p>
        </div>
      </div>

      {/* 4-Step Interactive Pipeline */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-3 mb-8">
        {steps.map((s, idx) => (
          <button
            key={idx}
            onClick={() => setActiveStep(idx)}
            className={`text-left p-4 rounded-xl border transition-all relative ${
              activeStep === idx
                ? 'bg-slate-800 border-indigo-500 shadow-lg shadow-indigo-500/10'
                : 'bg-slate-950/60 border-slate-800 hover:bg-slate-850 hover:border-slate-700'
            }`}
          >
            <div className="flex items-center justify-between mb-2">
              <span
                className={`text-xs font-mono font-bold px-2 py-0.5 rounded ${
                  activeStep === idx
                    ? 'bg-indigo-600 text-white'
                    : 'bg-slate-800 text-slate-400'
                }`}
              >
                0{idx + 1}
              </span>
              <span className="text-[10px] text-slate-500 uppercase tracking-wider">{s.tag}</span>
            </div>
            <h3 className="text-xs font-bold text-slate-200 line-clamp-1 mb-1">{s.title.split('：')[1]}</h3>
            <p className="text-[11px] text-slate-400 line-clamp-2">{s.highlight}</p>

            {activeStep === idx && (
              <div className="absolute -bottom-1.5 left-1/2 -translate-x-1/2 w-3 h-3 bg-indigo-500 rotate-45" />
            )}
          </button>
        ))}
      </div>

      {/* Detail Showcase of the active step */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl p-5 sm:p-6 mb-8">
        <div className="flex items-start justify-between flex-wrap gap-4 mb-4">
          <div>
            <span className="text-xs font-mono text-indigo-400 uppercase tracking-wider">
              阶段 0{activeStep + 1} / 04 · {steps[activeStep].tag}
            </span>
            <h3 className="text-lg font-bold text-white mt-0.5">{steps[activeStep].title}</h3>
          </div>
          <span className="text-xs bg-rose-500/20 text-rose-300 border border-rose-500/30 px-2.5 py-1 rounded-full font-medium flex items-center gap-1.5">
            <AlertCircle className="w-3.5 h-3.5" /> 故障核心触发机制
          </span>
        </div>

        <p className="text-slate-300 text-sm leading-relaxed mb-4">
          {steps[activeStep].desc}
        </p>

        <div className="bg-slate-900/90 border border-slate-800 rounded-lg p-3 text-xs text-amber-300 flex items-center gap-2">
          <Zap className="w-4 h-4 flex-shrink-0 text-amber-400" />
          <span>关键现象：<strong>{steps[activeStep].highlight}</strong></span>
        </div>
      </div>

      {/* Comparison Grid: Problem vs Solution Math */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* Left: The Flawed Mechanism */}
        <div className="bg-rose-950/20 border border-rose-900/40 rounded-xl p-5">
          <div className="flex items-center gap-2 text-rose-400 font-bold text-sm mb-3">
            <AlertCircle className="w-4.5 h-4.5" />
            <span>未做布尔合并时（产生虫蚀）</span>
          </div>

          <div className="bg-slate-950 rounded-lg p-4 font-mono text-xs text-slate-300 space-y-2 border border-rose-900/30 mb-4">
            <div className="text-slate-500">// 相交区域的采样伪代码</div>
            <div>float dist_stroke1 = +1.2; <span className="text-slate-500">// 属于笔画1内部</span></div>
            <div>float dist_stroke2 = +1.4; <span className="text-slate-500">// 属于笔画2内部</span></div>
            <div className="text-rose-400 font-bold">
              // 错误：简单距离求和或奇偶翻转导致符变
            </div>
            <div>sign = isInsideOddEven(pt) ? +1.0 : -1.0; <span className="text-rose-400">// 翻转为 -1.0!</span></div>
            <div>vec3 msdf = vec3(-0.8, 0.9, -0.6);</div>
            <div className="text-amber-300">
              float final_sd = median(msdf.r, msdf.g, msdf.b); <span className="text-rose-400">// = -0.6 &lt; 0.5</span>
            </div>
            <div className="text-rose-400 font-bold">=&gt; alpha = smoothstep(...) = 0.0 (空洞缺口)</div>
          </div>

          <ul className="text-xs text-slate-300 space-y-1.5 list-disc list-inside">
            <li>轮廓重叠处环绕数（Winding Number）发生抵消或翻转</li>
            <li>MSDF 边缘着色算法将相邻重叠边缘识别为“内外冲突”</li>
            <li>字形内部本应是实体，却被判定为“距离边界负无穷远处”</li>
          </ul>
        </div>

        {/* Right: The Solution Mechanism */}
        <div className="bg-emerald-950/20 border border-emerald-900/40 rounded-xl p-5">
          <div className="flex items-center gap-2 text-emerald-400 font-bold text-sm mb-3">
            <CheckCircle2 className="w-4.5 h-4.5" />
            <span>启用 Skia / 布尔并集后（平滑完美）</span>
          </div>

          <div className="bg-slate-950 rounded-lg p-4 font-mono text-xs text-slate-300 space-y-2 border border-emerald-900/30 mb-4">
            <div className="text-slate-500">// Skia PathOps: Simplify(Stroke1 ∪ Stroke2)</div>
            <div className="text-emerald-400">SkPath merged;</div>
            <div className="text-emerald-400">Op(stroke1, stroke2, kUnion_SkPathOp, &merged);</div>
            <div className="text-emerald-400">Simplify(merged); <span className="text-slate-500">// 消除一切自相交</span></div>
            <div>float dist_to_boundary = +2.6; <span className="text-emerald-300">// 纯净的正距离</span></div>
            <div>vec3 msdf = vec3(0.85, 0.88, 0.82);</div>
            <div className="text-emerald-300">
              float final_sd = median(msdf.r, msdf.g, msdf.b); <span className="text-emerald-400">// = 0.85 &gt; 0.5</span>
            </div>
            <div className="text-emerald-400 font-bold">=&gt; alpha = 1.0 (完整实体填充，无缝衔接)</div>
          </div>

          <ul className="text-xs text-slate-300 space-y-1.5 list-disc list-inside">
            <li>几何轮廓在生成前被融合成严格的单一流形外轮廓</li>
            <li>消除一切内部冗余线段与自相交点</li>
            <li>MSDF 可以正确把外轮廓分割并染上红绿蓝三色，中值滤波坚挺</li>
          </ul>
        </div>
      </div>
    </div>
  );
};
