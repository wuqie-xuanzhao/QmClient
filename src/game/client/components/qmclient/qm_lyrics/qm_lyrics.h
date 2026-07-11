#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_H

#include "qm_lyrics_clock.h"
#include "qm_lyrics_model.h"
#include "qm_lyrics_source.h"

#include <base/color.h>

#include <game/client/component.h>

#include <cstddef>
#include <memory>
#include <string>

namespace QmLyrics
{
	class CCacheIndex;
}

class CUIRect;

// 歌词 HUD 主组件。
//
// 数据流：
//   每帧 OnUpdate → 读 CSystemMediaControls.GetStateSnapshot()
//       → 检测曲目变化 → 触发数据源 QueryAsync（异步，下一帧拿结果）
//       → 用 CClockInterpolator 算当前 PositionMs
//       → 二分查找活动行
//   OnRender → 只绘制已经同步的歌词状态
//
// 状态机：
//   IDLE → FETCHING → READY → IDLE (新曲)
//
// 配置：全部 qm_lyrics_* 来自 config_variables_qmclient.h。
// HUD 编辑器接入：复用 EHudEditorElement::Lyrics token + T0a 通用贴边 helpers。
class CQmLyrics : public CComponent
{
public:
	CQmLyrics();
	~CQmLyrics() override;

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnReset() override;
	void OnUpdate() override;
	void OnRender() override;

	bool GetMediaIslandText(char *pBuf, size_t BufSize, ColorRGBA *pColor) const;
	bool RenderMediaIslandLine(const CUIRect &Rect, float FontSize, float Alpha);

	struct SImpl;

private:
	std::unique_ptr<SImpl> m_pImpl;

	void TickStateMachine();
	void RenderHud();
};

#endif
