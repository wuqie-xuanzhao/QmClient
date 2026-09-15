import React, { useRef, useEffect, useState } from 'react';
import { Layers, ZoomIn, ZoomOut, AlertCircle, CheckCircle, Sparkles } from 'lucide-react';

export type ViewChannel = 'final' | 'rgb' | 'r' | 'g' | 'b' | 'a' | 'wireframe';

export const InteractiveArtifactSimulator: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [fixedMode, setFixedMode] = useState<boolean>(false);
  const [channelView, setChannelView] = useState<ViewChannel>('final');
  const [zoom, setZoom] = useState<number>(1.2);
  const [threshold, setThreshold] = useState<number>(0.5);
  const [showAnnotations, setShowAnnotations] = useState<boolean>(true);
  const [activeGlyph, setActiveGlyph] = useState<'A' | 'NU' | 'EIGHT'>('A');

  const canvasWidth = 600;
  const canvasHeight = 600;

  // Draw the simulation
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Set pixel density
    const dpr = window.devicePixelRatio || 1;
    canvas.width = canvasWidth * dpr;
    canvas.height = canvasHeight * dpr;
    ctx.scale(dpr, dpr);

    // Clear background (Dark gray like user's screenshot)
    ctx.fillStyle = '#6b7280'; // Gray matching user screenshot
    ctx.fillRect(0, 0, canvasWidth, canvasHeight);

    ctx.save();
    // Center & zoom transform
    ctx.translate(canvasWidth / 2, canvasHeight / 2);
    ctx.scale(zoom, zoom);
    ctx.translate(-canvasWidth / 2, -canvasHeight / 2);

    // Draw coordinate grid if wireframe
    if (channelView === 'wireframe') {
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
      ctx.lineWidth = 1;
      const step = 40;
      for (let x = 0; x < canvasWidth; x += step) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvasHeight);
        ctx.stroke();
      }
      for (let y = 0; y < canvasHeight; y += step) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(canvasWidth, y);
        ctx.stroke();
      }
    }

    if (activeGlyph === 'A') {
      renderHiraganaA(ctx, fixedMode, channelView, threshold);
    } else if (activeGlyph === 'NU') {
      renderHiraganaNu(ctx, fixedMode, channelView);
    } else {
      renderFigureEight(ctx, fixedMode, channelView);
    }

    ctx.restore();

    // Draw annotations on top in final screen space
    if (showAnnotations && !fixedMode && activeGlyph === 'A') {
      drawArtifactAnnotations(ctx, zoom);
    }
  }, [fixedMode, channelView, zoom, threshold, showAnnotations, activeGlyph]);

  // Drawing Hiragana 'あ'
  const renderHiraganaA = (
    ctx: CanvasRenderingContext2D,
    fixed: boolean,
    mode: ViewChannel,
    thresh: number
  ) => {
    // scale stroke slightly with threshold
    const strokeWidth = 22 * (1 + (0.5 - thresh) * 0.5);

    if (mode === 'wireframe') {
      // Draw vector outlines and show self-intersection problem
      ctx.lineWidth = 2;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';

      // Stroke 1: Horizontal top bar
      ctx.strokeStyle = '#60a5fa'; // Blue
      ctx.beginPath();
      ctx.moveTo(160, 200);
      ctx.bezierCurveTo(280, 210, 420, 200, 480, 185);
      ctx.stroke();

      // Stroke 2: Vertical center spine
      ctx.strokeStyle = '#34d399'; // Emerald
      ctx.beginPath();
      ctx.moveTo(290, 140);
      ctx.bezierCurveTo(280, 280, 275, 420, 305, 520);
      ctx.stroke();

      // Stroke 3: Loop stroke
      ctx.strokeStyle = fixed ? '#a855f7' : '#f87171'; // Red if bug, purple if fixed
      ctx.beginPath();
      ctx.moveTo(420, 250);
      ctx.bezierCurveTo(360, 360, 230, 470, 180, 450);
      ctx.bezierCurveTo(130, 430, 140, 340, 220, 280);
      ctx.bezierCurveTo(300, 230, 420, 280, 500, 390);
      ctx.bezierCurveTo(530, 450, 480, 520, 360, 545);
      ctx.stroke();

      // Draw intersections indicators
      if (!fixed) {
        ctx.fillStyle = '#ef4444';
        const intersectPoints = [
          { x: 420, y: 268 }, // top right loop break
          { x: 275, y: 310 }, // spine cross
          { x: 282, y: 460 }, // lower loop cross
        ];
        intersectPoints.forEach((pt) => {
          ctx.beginPath();
          ctx.arc(pt.x, pt.y, 14, 0, Math.PI * 2);
          ctx.strokeStyle = '#f87171';
          ctx.lineWidth = 2;
          ctx.stroke();
          ctx.fillStyle = 'rgba(239, 68, 68, 0.3)';
          ctx.fill();
        });
      }
      return;
    }

    if (mode === 'rgb' || mode === 'r' || mode === 'g' || mode === 'b' || mode === 'a') {
      // Simulate MSDF / MTSDF distance field texture
      drawDistanceFieldSim(ctx, fixed, mode);
      return;
    }

    // FINAL RENDER MODE: Simulate reconstructed glyph with or without worm-eaten artifact
    // Base strokes drawing
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';

    // 1. Horizontal top stroke
    ctx.lineWidth = strokeWidth * 0.95;
    ctx.strokeStyle = '#ffffff';
    ctx.beginPath();
    ctx.moveTo(155, 200);
    ctx.bezierCurveTo(280, 212, 420, 200, 485, 185);
    ctx.stroke();

    // 2. Vertical center spine
    ctx.lineWidth = strokeWidth * 1.1;
    ctx.beginPath();
    ctx.moveTo(290, 140);
    ctx.bezierCurveTo(280, 280, 275, 420, 305, 520);
    ctx.stroke();

    // 3. Loop stroke
    ctx.lineWidth = strokeWidth;
    ctx.beginPath();
    ctx.moveTo(425, 248);
    ctx.bezierCurveTo(360, 360, 230, 470, 175, 445);
    ctx.bezierCurveTo(128, 420, 140, 340, 220, 280);
    ctx.bezierCurveTo(300, 230, 420, 280, 500, 390);
    ctx.bezierCurveTo(535, 450, 480, 520, 360, 545);
    ctx.stroke();

    // Now, if NOT fixed, simulate the exact worm-eaten / bitten-out artifacts caused by MSDF winding inversion
    if (!fixed) {
      // Background eraser color
      ctx.fillStyle = '#6b7280';
      ctx.strokeStyle = '#6b7280';

      // Hole 1: The exact notched bite at top right of loop (U+3042 characteristic artifact from image!)
      ctx.save();
      ctx.beginPath();
      // Cut a slot/notch where stroke contour self-intersects
      ctx.translate(418, 266);
      ctx.rotate(0.4);
      // Cutout notch resembling the screenshot:
      ctx.clearRect(-16, -10, 32, 20);
      ctx.fillRect(-16, -10, 32, 20);

      // Add the tiny residual fleck inside the hole like the original screenshot
      ctx.fillStyle = '#ffffff';
      ctx.beginPath();
      ctx.arc(1, -2, 2.5, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();

      // Hole 2: Worm-eaten notch at middle intersection where loop intersects vertical spine
      ctx.save();
      ctx.fillStyle = '#6b7280';
      ctx.beginPath();
      ctx.ellipse(278, 305, 8, 14, 0.2, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();

      // Hole 3: Erosion notch at lower spine intersection
      ctx.save();
      ctx.fillStyle = '#6b7280';
      ctx.beginPath();
      ctx.ellipse(284, 455, 7, 12, -0.3, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    }
  };

  const renderHiraganaNu = (
    ctx: CanvasRenderingContext2D,
    fixed: boolean,
    mode: ViewChannel
  ) => {
    // Similar demonstration for Japanese 'ぬ' (U+3062) which has an internal loop knot
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 22;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';

    // Left stroke
    ctx.beginPath();
    ctx.moveTo(220, 160);
    ctx.bezierCurveTo(240, 320, 180, 460, 150, 520);
    ctx.stroke();

    // Main sweeping body and knot
    ctx.beginPath();
    ctx.moveTo(170, 240);
    ctx.bezierCurveTo(260, 220, 430, 230, 420, 380);
    ctx.bezierCurveTo(410, 500, 260, 520, 250, 430);
    ctx.bezierCurveTo(240, 350, 360, 360, 430, 420);
    // Knot
    ctx.bezierCurveTo(470, 460, 490, 490, 465, 520);
    ctx.bezierCurveTo(440, 540, 420, 510, 435, 470);
    ctx.bezierCurveTo(450, 440, 490, 450, 520, 475);
    ctx.stroke();

    if (!fixed && mode === 'final') {
      ctx.fillStyle = '#6b7280';
      // Bite at knot intersection
      ctx.beginPath();
      ctx.arc(442, 478, 14, 0, Math.PI * 2);
      ctx.fill();
      // Bite at body crossing
      ctx.beginPath();
      ctx.arc(260, 400, 10, 0, Math.PI * 2);
      ctx.fill();
    }
  };

  const renderFigureEight = (
    ctx: CanvasRenderingContext2D,
    fixed: boolean,
    mode: ViewChannel
  ) => {
    // Number '8' self-intersecting lemniscate
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 28;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';

    // Top loop
    ctx.beginPath();
    ctx.ellipse(300, 230, 90, 80, 0, 0, Math.PI * 2);
    ctx.stroke();

    // Bottom loop (overlapping)
    ctx.beginPath();
    ctx.ellipse(300, 390, 120, 100, 0, 0, Math.PI * 2);
    ctx.stroke();

    if (!fixed && mode === 'final') {
      ctx.fillStyle = '#6b7280';
      // Center junction bite
      ctx.beginPath();
      ctx.ellipse(300, 305, 18, 14, 0, 0, Math.PI * 2);
      ctx.fill();
    }
  };

  // Draw simulated MSDF / MTSDF multi-channel distance field
  const drawDistanceFieldSim = (
    ctx: CanvasRenderingContext2D,
    fixed: boolean,
    mode: ViewChannel
  ) => {
    const offscreen = document.createElement('canvas');
    offscreen.width = 160;
    offscreen.height = 160;
    const offCtx = offscreen.getContext('2d');
    if (!offCtx) return;

    // Background depending on channel
    if (mode === 'rgb') {
      offCtx.fillStyle = '#111827';
      offCtx.fillRect(0, 0, 160, 160);

      // Create colorful MSDF edge coloring (Red, Green, Blue along different tangent angles)
      offCtx.lineWidth = 14;
      offCtx.lineCap = 'round';
      offCtx.lineJoin = 'round';

      // Top bar in Red & Yellow
      offCtx.strokeStyle = 'rgba(255, 60, 60, 0.9)';
      offCtx.beginPath();
      offCtx.moveTo(40, 50);
      offCtx.bezierCurveTo(70, 52, 110, 50, 125, 46);
      offCtx.stroke();

      // Vertical spine in Green & Cyan
      offCtx.strokeStyle = 'rgba(60, 255, 120, 0.9)';
      offCtx.beginPath();
      offCtx.moveTo(76, 35);
      offCtx.bezierCurveTo(73, 70, 71, 110, 80, 136);
      offCtx.stroke();

      // Loop stroke in Blue & Magenta
      offCtx.strokeStyle = 'rgba(80, 120, 255, 0.9)';
      offCtx.beginPath();
      offCtx.moveTo(110, 62);
      offCtx.bezierCurveTo(90, 90, 60, 120, 48, 115);
      offCtx.bezierCurveTo(35, 110, 38, 90, 60, 75);
      offCtx.bezierCurveTo(80, 60, 110, 75, 130, 102);
      offCtx.bezierCurveTo(138, 118, 125, 136, 95, 142);
      offCtx.stroke();

      if (!fixed) {
        // Artifact in RGB: Colors clash and zero-out at intersection!
        offCtx.fillStyle = '#000000';
        offCtx.beginPath();
        offCtx.arc(110, 66, 6, 0, Math.PI * 2);
        offCtx.fill();

        offCtx.beginPath();
        offCtx.arc(73, 78, 5, 0, Math.PI * 2);
        offCtx.fill();
      }
    } else if (mode === 'a') {
      // True SDF (Alpha Channel): smooth grayscale distance field
      offCtx.fillStyle = '#000000';
      offCtx.fillRect(0, 0, 160, 160);

      offCtx.lineWidth = 16;
      offCtx.lineCap = 'round';
      offCtx.lineJoin = 'round';
      offCtx.strokeStyle = '#ffffff';

      // Top bar
      offCtx.beginPath();
      offCtx.moveTo(40, 50);
      offCtx.bezierCurveTo(70, 52, 110, 50, 125, 46);
      offCtx.stroke();

      // Vertical spine
      offCtx.beginPath();
      offCtx.moveTo(76, 35);
      offCtx.bezierCurveTo(73, 70, 71, 110, 80, 136);
      offCtx.stroke();

      // Loop stroke
      offCtx.beginPath();
      offCtx.moveTo(110, 62);
      offCtx.bezierCurveTo(90, 90, 60, 120, 48, 115);
      offCtx.bezierCurveTo(35, 110, 38, 90, 60, 75);
      offCtx.bezierCurveTo(80, 60, 110, 75, 130, 102);
      offCtx.bezierCurveTo(138, 118, 125, 136, 95, 142);
      offCtx.stroke();

      if (!fixed) {
        // Even in naive true SDF, winding cancellation cuts a hole
        offCtx.fillStyle = '#000000';
        offCtx.beginPath();
        offCtx.arc(110, 66, 5, 0, Math.PI * 2);
        offCtx.fill();
      }
    } else {
      // Single channel (R, G, or B)
      offCtx.fillStyle = '#000000';
      offCtx.fillRect(0, 0, 160, 160);
      offCtx.lineWidth = 16;
      offCtx.lineCap = 'round';
      const tint = mode === 'r' ? '#ef4444' : mode === 'g' ? '#10b981' : '#3b82f6';
      offCtx.strokeStyle = tint;

      // Draw stroke active for this edge color
      offCtx.beginPath();
      if (mode === 'r') {
        offCtx.moveTo(40, 50);
        offCtx.bezierCurveTo(70, 52, 110, 50, 125, 46);
      } else if (mode === 'g') {
        offCtx.moveTo(76, 35);
        offCtx.bezierCurveTo(73, 70, 71, 110, 80, 136);
      } else {
        offCtx.moveTo(110, 62);
        offCtx.bezierCurveTo(90, 90, 60, 120, 48, 115);
        offCtx.bezierCurveTo(35, 110, 38, 90, 60, 75);
        offCtx.bezierCurveTo(80, 60, 110, 75, 130, 102);
      }
      offCtx.stroke();
    }

    // Draw magnified to canvas
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(offscreen, 60, 60, 480, 480);
  };

  // Draw informative callout pins on top of the canvas
  const drawArtifactAnnotations = (ctx: CanvasRenderingContext2D, currentZoom: number) => {
    // Transform coordinates based on zoom
    const transformPt = (x: number, y: number) => {
      const cx = canvasWidth / 2;
      const cy = canvasHeight / 2;
      return {
        x: cx + (x - cx) * currentZoom,
        y: cy + (y - cy) * currentZoom,
      };
    };

    const target1 = transformPt(418, 266);
    const target2 = transformPt(278, 305);

    // Pin 1: User's primary screenshot bite (at top right loop intersection)
    drawPin(
      ctx,
      target1.x,
      target1.y,
      '虫蚀咬痕 (Worm-eaten Notch)',
      '笔画自相交处符号反相，中值截断为背景',
      '#ef4444'
    );

    // Pin 2: Secondary bite at vertical cross
    drawPin(
      ctx,
      target2.x,
      target2.y,
      '笔画交叉撕裂',
      '重叠区域 Winding 数失真',
      '#f97316'
    );
  };

  const drawPin = (
    ctx: CanvasRenderingContext2D,
    x: number,
    y: number,
    title: string,
    subtitle: string,
    color: string
  ) => {
    // Pulsing circle
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(x, y, 16, 0, Math.PI * 2);
    ctx.stroke();

    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(x, y, 4, 0, Math.PI * 2);
    ctx.fill();

    // Connecting callout line
    const labelX = x + 40;
    const labelY = y - 35;

    ctx.beginPath();
    ctx.moveTo(x + 12, y - 10);
    ctx.lineTo(labelX - 10, labelY);
    ctx.lineTo(labelX + 160, labelY);
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Label card background
    ctx.fillStyle = 'rgba(15, 23, 42, 0.9)';
    ctx.fillRect(labelX - 5, labelY - 22, 175, 42);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
    ctx.strokeRect(labelX - 5, labelY - 22, 175, 42);

    // Text
    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 11px sans-serif';
    ctx.fillText(title, labelX + 2, labelY - 6);

    ctx.fillStyle = '#94a3b8';
    ctx.font = '9px sans-serif';
    ctx.fillText(subtitle, labelX + 2, labelY + 12);

    ctx.restore();
  };

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl overflow-hidden shadow-2xl mb-12">
      {/* Header bar */}
      <div className="px-6 py-4 bg-slate-950/60 border-b border-slate-800 flex flex-wrap items-center justify-between gap-4">
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 rounded-lg bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400">
            <Layers className="w-4 h-4" />
          </div>
          <div>
            <h2 className="text-base font-bold text-white flex items-center gap-2">
              实时仿真实验台：虫蚀现象复现与修复对比
            </h2>
            <p className="text-xs text-slate-400">
              精准模拟字形 <span className="font-mono text-amber-300">U+3042 (あ)</span> 重叠轮廓在距离场生成中的撕裂机理
            </p>
          </div>
        </div>

        {/* Character selector */}
        <div className="flex items-center gap-1.5 bg-slate-800/80 p-1 rounded-lg border border-slate-700 text-xs">
          <span className="text-slate-400 px-2">测试字符:</span>
          <button
            onClick={() => setActiveGlyph('A')}
            className={`px-2.5 py-1 rounded font-medium transition-colors ${
              activeGlyph === 'A'
                ? 'bg-indigo-600 text-white shadow'
                : 'text-slate-300 hover:text-white hover:bg-slate-700'
            }`}
          >
            あ (原图故障字符)
          </button>
          <button
            onClick={() => setActiveGlyph('NU')}
            className={`px-2.5 py-1 rounded font-medium transition-colors ${
              activeGlyph === 'NU'
                ? 'bg-indigo-600 text-white shadow'
                : 'text-slate-300 hover:text-white hover:bg-slate-700'
            }`}
          >
            ぬ (死结交叉)
          </button>
          <button
            onClick={() => setActiveGlyph('EIGHT')}
            className={`px-2.5 py-1 rounded font-medium transition-colors ${
              activeGlyph === 'EIGHT'
                ? 'bg-indigo-600 text-white shadow'
                : 'text-slate-300 hover:text-white hover:bg-slate-700'
            }`}
          >
            8 (闭环相交)
          </button>
        </div>
      </div>

      {/* Main interactive area */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-0">
        {/* Canvas viewport (Left / Center) */}
        <div className="lg:col-span-7 bg-slate-950 p-4 sm:p-6 flex flex-col items-center justify-center relative select-none">
          {/* Status Indicator Banner */}
          <div className="w-full flex items-center justify-between mb-3 text-xs">
            <div className="flex items-center gap-2">
              <span className="text-slate-400 font-mono">U+3042 HIRAGANA LETTER A</span>
              <span
                className={`px-2 py-0.5 rounded-full font-bold flex items-center gap-1 ${
                  fixedMode
                    ? 'bg-emerald-500/20 text-emerald-400 border border-emerald-500/30'
                    : 'bg-rose-500/20 text-rose-400 border border-rose-500/30'
                }`}
              >
                {fixedMode ? (
                  <>
                    <CheckCircle className="w-3 h-3" /> 已应用布尔并集 / Skia 预处理
                  </>
                ) : (
                  <>
                    <AlertCircle className="w-3 h-3" /> 存在轮廓重叠虫蚀故障 (未修复)
                  </>
                )}
              </span>
            </div>

            <div className="flex items-center gap-2 text-slate-400">
              <button
                onClick={() => setZoom((z) => Math.max(0.8, Number((z - 0.2).toFixed(1))))}
                className="p-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-300"
                title="缩小"
              >
                <ZoomOut className="w-3.5 h-3.5" />
              </button>
              <span className="font-mono w-10 text-center">{Math.round(zoom * 100)}%</span>
              <button
                onClick={() => setZoom((z) => Math.min(2.5, Number((z + 0.2).toFixed(1))))}
                className="p-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-300"
                title="放大"
              >
                <ZoomIn className="w-3.5 h-3.5" />
              </button>
            </div>
          </div>

          {/* Canvas container */}
          <div className="relative border border-slate-800 rounded-xl overflow-hidden shadow-inner bg-[#6b7280] max-w-full">
            <canvas
              ref={canvasRef}
              style={{ width: '100%', maxWidth: '520px', aspectRatio: '1/1' }}
              className="block cursor-crosshair"
            />

            {/* Quick toggle pill on bottom left inside canvas */}
            <div className="absolute bottom-3 left-3 bg-slate-900/90 backdrop-blur border border-slate-700 px-3 py-1.5 rounded-lg text-xs text-slate-300 flex items-center gap-2">
              <span className="font-medium">当前视图:</span>
              <span className="text-amber-400 font-mono capitalize">
                {channelView === 'final'
                  ? '最终字形渲染 (Shader)'
                  : channelView === 'rgb'
                  ? 'MSDF RGB 纹理'
                  : channelView === 'a'
                  ? 'True SDF (Alpha 通道)'
                  : channelView === 'wireframe'
                  ? '矢量轮廓相交诊断'
                  : `${channelView.toUpperCase()} 通道`}
              </span>
            </div>
          </div>

          <div className="mt-3 text-center text-xs text-slate-500">
            * 提示：灰底与白色笔画完全复现了您提问截屏中的渲染环境，缺口位于环状笔画起点交叉处。
          </div>
        </div>

        {/* Control and inspection panel (Right) */}
        <div className="lg:col-span-5 bg-slate-900/60 p-5 sm:p-6 border-t lg:border-t-0 lg:border-l border-slate-800 flex flex-col justify-between space-y-6">
          <div className="space-y-6">
            {/* Primary Toggle: Bug vs Fixed */}
            <div>
              <label className="text-xs font-semibold text-slate-400 uppercase tracking-wider block mb-2.5">
                核心状态对比
              </label>
              <div className="grid grid-cols-2 gap-2 bg-slate-950 p-1.5 rounded-xl border border-slate-800">
                <button
                  onClick={() => setFixedMode(false)}
                  className={`flex items-center justify-center gap-2 py-2.5 px-3 rounded-lg text-xs font-bold transition-all ${
                    !fixedMode
                      ? 'bg-rose-500/20 text-rose-300 border border-rose-500/40 shadow'
                      : 'text-slate-400 hover:text-slate-200'
                  }`}
                >
                  <AlertCircle className="w-4 h-4 text-rose-400" />
                  复现虫蚀故障
                </button>
                <button
                  onClick={() => setFixedMode(true)}
                  className={`flex items-center justify-center gap-2 py-2.5 px-3 rounded-lg text-xs font-bold transition-all ${
                    fixedMode
                      ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/40 shadow'
                      : 'text-slate-400 hover:text-slate-200'
                  }`}
                >
                  <Sparkles className="w-4 h-4 text-emerald-400" />
                  布尔修复效果
                </button>
              </div>
            </div>

            {/* Channels & Diagnostics switcher */}
            <div>
              <label className="text-xs font-semibold text-slate-400 uppercase tracking-wider block mb-2.5">
                通道拆解与诊断视图
              </label>
              <div className="grid grid-cols-3 gap-1.5">
                <button
                  onClick={() => setChannelView('final')}
                  className={`p-2 rounded-lg text-xs font-medium border text-center transition-all ${
                    channelView === 'final'
                      ? 'bg-indigo-600 border-indigo-500 text-white shadow'
                      : 'bg-slate-800/80 border-slate-700/60 text-slate-300 hover:bg-slate-800'
                  }`}
                >
                  最终合成
                </button>
                <button
                  onClick={() => setChannelView('rgb')}
                  className={`p-2 rounded-lg text-xs font-medium border text-center transition-all ${
                    channelView === 'rgb'
                      ? 'bg-indigo-600 border-indigo-500 text-white shadow'
                      : 'bg-slate-800/80 border-slate-700/60 text-slate-300 hover:bg-slate-800'
                  }`}
                >
                  MSDF (RGB图)
                </button>
                <button
                  onClick={() => setChannelView('a')}
                  className={`p-2 rounded-lg text-xs font-medium border text-center transition-all ${
                    channelView === 'a'
                      ? 'bg-indigo-600 border-indigo-500 text-white shadow'
                      : 'bg-slate-800/80 border-slate-700/60 text-slate-300 hover:bg-slate-800'
                  }`}
                >
                  True SDF (A通道)
                </button>
                <button
                  onClick={() => setChannelView('r')}
                  className={`p-2 rounded-lg text-xs font-medium border text-center transition-all ${
                    channelView === 'r'
                      ? 'bg-red-900/60 border-red-500 text-red-200'
                      : 'bg-slate-800/80 border-slate-700/60 text-slate-300 hover:bg-slate-800'
                  }`}
                >
                  R (红边)
                </button>
                <button
                  onClick={() => setChannelView('g')}
                  className={`p-2 rounded-lg text-xs font-medium border text-center transition-all ${
                    channelView === 'g'
                      ? 'bg-emerald-900/60 border-emerald-500 text-emerald-200'
                      : 'bg-slate-800/80 border-slate-700/60 text-slate-300 hover:bg-slate-800'
                  }`}
                >
                  G (绿边)
                </button>
                <button
                  onClick={() => setChannelView('wireframe')}
                  className={`p-2 rounded-lg text-xs font-medium border text-center transition-all ${
                    channelView === 'wireframe'
                      ? 'bg-purple-600 border-purple-500 text-white'
                      : 'bg-slate-800/80 border-slate-700/60 text-slate-300 hover:bg-slate-800'
                  }`}
                >
                  矢量轮廓相交线
                </button>
              </div>
            </div>

            {/* Explanatory Callout Box based on current state */}
            <div
              className={`p-4 rounded-xl text-xs leading-relaxed border ${
                fixedMode
                  ? 'bg-emerald-950/30 border-emerald-500/30 text-emerald-200'
                  : 'bg-rose-950/30 border-rose-500/30 text-rose-200'
              }`}
            >
              <div className="font-bold mb-1 flex items-center gap-1.5">
                {fixedMode ? (
                  <>
                    <CheckCircle className="w-4 h-4 text-emerald-400" />
                    已修复：布尔并集消除了自相交点
                  </>
                ) : (
                  <>
                    <AlertCircle className="w-4 h-4 text-rose-400" />
                    故障机理剖析：为什么缺口是虫蚀状？
                  </>
                )}
              </div>
              <p>
                {fixedMode
                  ? '通过 Skia PathOps 或字体 Remove Overlap 操作，笔画相交处的重叠几何被熔铸为统一的外轮廓。此时无论使用 Non-Zero 还是 Even-Odd 规则，内外距离场判定均 100% 正确，边缘平滑无损。'
                  : '日文假名“あ”由 3 笔组成。字体作者在绘制右下角的大回环时，笔画直接搭在横竖交叉线上且自相重叠。MSDF 生成器在计算点到轮廓的带符号距离时，相交区域的环绕数发生奇偶抵消，中值判定认为该处位于“字形外部”，在 Shader 中直接被剔除，呈现出穿孔般的咬痕。'}
              </p>
            </div>

            {/* Fine-tuning parameters */}
            <div className="space-y-4 pt-2 border-t border-slate-800">
              <div>
                <div className="flex justify-between text-xs mb-1">
                  <span className="text-slate-400">距离阈值 (Distance Threshold):</span>
                  <span className="font-mono text-white">{threshold.toFixed(2)}</span>
                </div>
                <input
                  type="range"
                  min="0.3"
                  max="0.7"
                  step="0.02"
                  value={threshold}
                  onChange={(e) => setThreshold(parseFloat(e.target.value))}
                  className="w-full accent-indigo-500 bg-slate-800 rounded-lg h-1.5"
                />
                <div className="flex justify-between text-[10px] text-slate-500 mt-0.5">
                  <span>笔画膨胀 (0.3)</span>
                  <span>标准值 (0.5)</span>
                  <span>笔画收缩 (0.7)</span>
                </div>
              </div>

              <div className="flex items-center justify-between text-xs pt-1">
                <span className="text-slate-400">显示故障位置标注图钉</span>
                <button
                  onClick={() => setShowAnnotations(!showAnnotations)}
                  className={`w-11 h-6 rounded-full transition-colors relative ${
                    showAnnotations ? 'bg-indigo-600' : 'bg-slate-700'
                  }`}
                >
                  <div
                    className={`w-4 h-4 rounded-full bg-white absolute top-1 transition-transform ${
                      showAnnotations ? 'left-6' : 'left-1'
                    }`}
                  />
                </button>
              </div>
            </div>
          </div>

          <div className="text-[11px] text-slate-500 border-t border-slate-800 pt-3">
            💡 切换到 <strong className="text-slate-300">“矢量轮廓相交线”</strong>{' '}
            视图，可以清晰看到字体原始笔画在红圈处发生重叠相交。
          </div>
        </div>
      </div>
    </div>
  );
};
