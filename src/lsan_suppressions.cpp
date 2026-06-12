// Suppress known false-positive leaks from KDE/Wayland system libraries.
// When a QMenu pops up, the Breeze style lazily initializes Wayland compositor
// protocol proxies (ContrastManager, SlideManager, ShadowManager, etc.) via
// KWaylandPlugin. These are process-lifetime singletons that are never freed by
// design. They are not bugs in our code.
extern "C" const char* __lsan_default_suppressions() {
    return "leak:Breeze::ShadowHelper\n";
}
