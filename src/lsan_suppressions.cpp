// Suppresses a false-positive leak: Breeze lazily inits Wayland compositor
// proxies as process-lifetime singletons that are never freed by design.
extern "C" const char* __lsan_default_suppressions() {
    return "leak:Breeze::ShadowHelper\n";
}
