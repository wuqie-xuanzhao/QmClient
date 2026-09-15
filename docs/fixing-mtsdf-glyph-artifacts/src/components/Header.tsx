import React from 'react';
import { Type, Sparkles, HelpCircle, Layers, Wrench, Sliders } from 'lucide-react';

interface HeaderProps {
  onScrollTo: (id: string) => void;
}

export const Header: React.FC<HeaderProps> = ({ onScrollTo }) => {
  return (
    <header className="sticky top-0 z-50 backdrop-blur-md bg-slate-950/80 border-b border-slate-800">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 h-16 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-tr from-indigo-600 via-indigo-500 to-emerald-400 p-0.5 shadow-lg shadow-indigo-500/20">
            <div className="w-full h-full bg-slate-950 rounded-[10px] flex items-center justify-center text-white">
              <Type className="w-5 h-5 text-indigo-400" />
            </div>
          </div>
          <div>
            <div className="flex items-center gap-2">
              <span className="font-bold text-white text-base tracking-tight">MTSDF 字形伪影修复指南</span>
              <span className="text-[10px] font-mono px-2 py-0.5 rounded-full bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 font-bold">
                U+3042 Fix
              </span>
            </div>
            <p className="text-[11px] text-slate-400 hidden sm:block">
              Multi-channel Signed Distance Field · 轮廓自相交与中值虫蚀诊断系统
            </p>
          </div>
        </div>

        <nav className="flex items-center gap-1 sm:gap-2 text-xs">
          <button
            onClick={() => onScrollTo('simulator')}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800/80 transition-colors"
          >
            <Layers className="w-3.5 h-3.5 text-indigo-400" />
            <span className="hidden md:inline">交互式</span>复现台
          </button>

          <button
            onClick={() => onScrollTo('deep-dive')}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800/80 transition-colors"
          >
            <Sparkles className="w-3.5 h-3.5 text-purple-400" />
            <span className="hidden md:inline">原理</span>拆解
          </button>

          <button
            onClick={() => onScrollTo('solutions-section')}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800/80 transition-colors"
          >
            <Wrench className="w-3.5 h-3.5 text-emerald-400" />
            解决<span className="hidden md:inline">方案库</span>
          </button>

          <button
            onClick={() => onScrollTo('cli-generator')}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800/80 transition-colors"
          >
            <Sliders className="w-3.5 h-3.5 text-blue-400" />
            命令<span className="hidden md:inline">生成器</span>
          </button>

          <button
            onClick={() => onScrollTo('faq')}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800/80 transition-colors"
          >
            <HelpCircle className="w-3.5 h-3.5 text-amber-400" />
            FAQ
          </button>
        </nav>
      </div>
    </header>
  );
};
