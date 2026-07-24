#include "trrojan/ospray/plugin.h"

#include <Windows.h>

#include "trrojan/ospray/environment.h"

#include "trrojan/ospray/sphere_benchmark.h"

/// <summary>
/// Handle of the plugin DLL.
/// </summary>
static HINSTANCE hTrrojanDll = NULL;


/// <summary>
/// Entry point of the DLL.
/// </summary>
BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            ::DisableThreadLibraryCalls(hDll);
            ::hTrrojanDll = hDll;
            break;

        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

/// <summary>
/// Gets a new instance of the plugin descriptor.
/// </summary>
extern "C" TRROJANOSPRAY_API trrojan::plugin_base *get_trrojan_plugin(void) {
    return new trrojan::ospray::plugin();
}

/*
 * trrojan::ospray::plugin::~plugin
 */
trrojan::ospray::plugin::~plugin(void) { }


/*
 * trrojan::ospray::plugin::create_benchmarks
 */
size_t trrojan::ospray::plugin::create_benchmarks(benchmark_list& dst) const {
    dst.emplace_back(std::make_shared<trrojan::ospray::sphere_benchmark>());
    return 1;
}


/*
 * trrojan::ospray::plugin::create_environments
 */
size_t trrojan::ospray::plugin::create_environments(
        environment_list& dst) const {
    dst.emplace_back(std::make_shared<trrojan::ospray::environment>());
    return 1;
}
