// WARNING: This header is internal to Medooze. It must only be
// included directly from Medooze .cpp, never from other headers
// or in user code.
//
// If you're a user, include MedoozeTracing.h instead.

#ifndef __MEDOOZE_TRACING_H
#define __MEDOOZE_TRACING_H

// This is the central header from where we control tracing.
// See tracing.md for details. If MEDOOZE_TRACING isn't defined,
// then we just define empty macros to remove instrumentation.

#ifdef MEDOOZE_TRACING

// unless overriden, set our default prefix
#ifndef MEDOOZE_TRACING_PREFIX
#define MEDOOZE_TRACING_PREFIX "medooze."
#endif

// before including Perfetto, define a non-default namespace to put
// Perfetto events and categories inside (affects the whole translation
// unit, hence this header is private and shouln't be included from any
// of the public headers)
// More info: https://github.com/google/perfetto/blob/12541607c370d7168b9ad4ab62a81120f723b774/include/perfetto/tracing/track_event.h#L119
#define PERFETTO_TRACK_EVENT_NAMESPACE medooze

// this must be our only include of Perfetto. any other point in
// the codebase needs to `#include "tracing.h"` instead of perfetto.h,
// and it needs to be the first include, otherwise it seems to often fail
// (haven't looked into why, probably because our public headers pollute
// the root namespace)
#include <perfetto.h>

// now, define our categories as usual
// FIXME: tracing prefix here and in the code
PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("eventloop")
        .SetDescription("Event loop operation (scheduling, I/O, timers, sync)"),
    perfetto::Category("transport")
        .SetDescription("ICE transport layer (managing candidates)"),
    perfetto::Category("dtls")
        .SetDescription("DTLS session"),
    perfetto::Category("srtp")
        .SetDescription("SRTP layer"),
    perfetto::Category("rtp")
        .SetDescription("RTP session (includes RTCP)"),
    perfetto::Category("datachannel")
        .SetDescription("Data channels / SCTP session"));

#else

// disable the macros
#define TRACE_EVENT(...)
#define TRACE_EVENT_BEGIN(...)
#define TRACE_EVENT_END(...)
#define TRACE_EVENT_CATEGORY_ENABLED(...)

#endif /* MEDOOZE_TRACING */

#endif /* __MEDOOZE_TRACING_H */
