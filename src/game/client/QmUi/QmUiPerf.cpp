#include "QmUiPerf.h"

#include <base/system.h>

#include <game/client/components/qmclient/perf_logging.h>

#include <cinttypes>

void QmLogMenuUiFramePerf(const SQmMenuUiFramePerf &Frame, const IClient *pClient)
{
	if(!QmPerfEnabled())
		return;

	char pPayload[768];
	str_format(pPayload, sizeof(pPayload), "event=menu_ui_frame page=%s operation=%s frame=%" PRIu64 " items_total=%d items_visible=%d items_processed=%d items_skipped=%d ui_ms=%.3f layout_ms=%.3f text_ms=%.3f heap_allocs=%d cache_hits=%d cache_misses=%d cache_evictions=%d source=qm_ui_perf",
		Frame.m_pPage != nullptr ? Frame.m_pPage : "unknown",
		Frame.m_pOperation != nullptr ? Frame.m_pOperation : "unknown",
		pClient != nullptr ? pClient->PerfFrame() : 0,
		Frame.m_ItemsTotal,
		Frame.m_ItemsVisible,
		Frame.m_ItemsProcessed,
		Frame.m_ItemsSkipped,
		Frame.m_UiMs,
		Frame.m_LayoutMs,
		Frame.m_TextMs,
		Frame.m_HeapAllocs,
		Frame.m_CacheHits,
		Frame.m_CacheMisses,
		Frame.m_CacheEvictions);
	QmPerfLogPayload("perf/menu-ui", pPayload, pClient);
}
