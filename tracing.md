# Tracing

The Medooze media server is being instrumented with [Perfetto](https://perfetto.dev), a production grade tracing SDK.
Tracing can help you find bottlenecks in the media server pipeline as well as debug a range of issues.

At the time of this writing, Perfetto seems to be aimed at application binaries and there isn't a clear, documented way for libraries to provide instrumentation.  
However it [is supported to some degree](https://github.com/google/perfetto/blob/12541607c370d7168b9ad4ab62a81120f723b774/include/perfetto/tracing/track_event.h#L119). Therefore, the current approach is:

 1. The media server itself doesn't expose any tracing APIs. The code is *only* instrumented with the appropriate macros to:
    - record events (`TRACE_EVENT` and siblings)
    - register categories and its static storage, but in a different namespace so that the application can also define their own (see above)
      - all categories have a `medooze.` prefix (can be overriden by defining `MEDOOZE_TRACING_PREFIX`)

 2. Such code is hidden behind a `MEDOOZE_TRACING` define.

 3. Applications wishing to use tracing need to:
    - build Medooze with the define in place, supplying the [tracing SDK](https://perfetto.dev/docs/instrumentation/tracing-sdk) as an include directory (so that `perfetto.h` can be included).
    - add `perfetto.cc` from that directory to the source list
    - initialize the tracing SDK
    - register Medooze's TrackEvent data source by calling `MedoozeTrackEventRegister()` from the `MedoozeTracing.h` header

This gives the most control to final applications and lets them add their own events into the trace as well.

## Guidelines

Unfortunately we are currently unable to guarantee stability with respect to the instrumentation code.
In practice, the main user for the tracing is going to be the Node.js addon, see [Tracing](https://github.com/medooze/media-server-node/blob/master/tracing.md) there for more info.

There's some set of guidelines that should be respected for consistency:

 - For events that span (almost) a whole function, event names are the method path (i.e. `EventLoop::Run` if it's the `Run` method in class `EventLoop`)
 - For events that span part of a function, append another token to the path (i.e. `EventLoop::Run::ProcessIn`) but just one, no nesting (if an event spans part of the `ProcessIn` slice, it would be `EventLoop::Run::DequeueIn` not `EventLoop::Run::ProcessIn::Dequeue`)
