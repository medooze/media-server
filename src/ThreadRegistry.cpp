#include "ThreadRegistry.h"

std::unordered_map<std::thread::id, std::function<std::future<void>()>> ThreadRegistry::callbacks;
bool ThreadRegistry::closed = false;
std::mutex ThreadRegistry::callbackMutex;