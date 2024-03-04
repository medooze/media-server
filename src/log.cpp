#include "log.h"

// Global logger instance
// Note: This is not an inline in the header as we want an instance per node plugin not a single global instance
// When put into the header inline function gcc will produce a "u" symbol that is a special extension used by GNU
// to ELF to allow a single global to be obtained from multiple symbols on load time.
Logger Logger::instance;

