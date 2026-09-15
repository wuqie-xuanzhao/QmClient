import { Header } from './components/Header';
import { QuickAnswerCard } from './components/QuickAnswerCard';
import { CaseStudyBreakdown } from './components/CaseStudyBreakdown';
import { InteractiveArtifactSimulator } from './components/InteractiveArtifactSimulator';
import { DeepDiveExplanation } from './components/DeepDiveExplanation';
import { SolutionTabs } from './components/SolutionTabs';
import { CliGenerator } from './components/CliGenerator';
import { FaqTroubleshooting } from './components/FaqTroubleshooting';
import { ArrowUp, BookOpen, Terminal, Code2 } from 'lucide-react';

export default function App() {
  const scrollTo = (id: string) => {
    const el = document.getElementById(id);
    if (el) {
      el.scrollIntoView({ behavior: 'smooth' });
    }
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 selection:bg-indigo-500 selection:text-white font-sans antialiased">
      {/* Header Navigation */}
      <Header onScrollTo={scrollTo} />

      {/* Main Container */}
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 sm:py-10 space-y-12">
        {/* Hero Quick Answer */}
        <section id="quick-answer">
          <QuickAnswerCard onJumpToSolution={scrollTo} />
        </section>

        {/* User Screenshot Breakdown / Diagnosis */}
        <section id="case-study">
          <CaseStudyBreakdown />
        </section>

        {/* Interactive Simulator: Canvas reproducing U+3042 bug and boolean fix */}
        <section id="simulator">
          <InteractiveArtifactSimulator />
        </section>

        {/* Deep Dive Theory */}
        <section id="deep-dive">
          <DeepDiveExplanation />
        </section>

        {/* Solutions & Code Snippets */}
        <section id="solutions-section">
          <SolutionTabs />
        </section>

        {/* Interactive Command Line Generator */}
        <section id="cli-generator">
          <CliGenerator />
        </section>

        {/* FAQ */}
        <section id="faq">
          <FaqTroubleshooting />
        </section>
      </main>

      {/* Footer */}
      <footer className="border-t border-slate-900 bg-slate-950/80 py-12 text-slate-500 text-xs">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 flex flex-col md:flex-row items-center justify-between gap-6">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded-lg bg-indigo-600 flex items-center justify-center text-white font-bold">
              あ
            </div>
            <div>
              <p className="text-slate-300 font-semibold text-sm">MTSDF 字形伪影修复知识库</p>
              <p className="text-slate-500 text-xs">针对有向距离场轮廓自相交与中值中空缺陷的系统级解决方案</p>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-6">
            <span className="text-slate-400">相关技术标准与开源库：</span>
            <a
              href="https://github.com/Chlumsky/msdfgen"
              target="_blank"
              rel="noreferrer"
              className="text-slate-400 hover:text-indigo-400 transition-colors flex items-center gap-1"
            >
              <Code2 className="w-3.5 h-3.5" /> Chlumsky/msdfgen
            </a>
            <a
              href="https://github.com/Chlumsky/msdf-atlas-gen"
              target="_blank"
              rel="noreferrer"
              className="text-slate-400 hover:text-indigo-400 transition-colors flex items-center gap-1"
            >
              <Terminal className="w-3.5 h-3.5" /> msdf-atlas-gen
            </a>
            <a
              href="https://skia.org/docs/user/api/skpathops/"
              target="_blank"
              rel="noreferrer"
              className="text-slate-400 hover:text-indigo-400 transition-colors flex items-center gap-1"
            >
              <BookOpen className="w-3.5 h-3.5" /> Skia PathOps
            </a>
          </div>

          <button
            onClick={() => window.scrollTo({ top: 0, behavior: 'smooth' })}
            className="p-2 rounded-lg bg-slate-900 hover:bg-slate-800 text-slate-400 hover:text-white border border-slate-800 transition-colors"
            title="回到顶部"
          >
            <ArrowUp className="w-4 h-4" />
          </button>
        </div>
      </footer>
    </div>
  );
}
