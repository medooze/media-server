#include "tracing.h"
#include "MedoozeTracing.h"

#ifdef MEDOOZE_TRACING

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

bool MedoozeTrackEventRegister() {
	return medooze::TrackEvent::Register();
}

#endif
