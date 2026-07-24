#pragma once

#include "trrojan/plugin.h"

#include "trrojan/ospray/export.h"

namespace trrojan {
namespace ospray {

    /// <summary>
    /// Descriptor for the OSPRay benchmark plugin.
    /// </summary>
    class TRROJANOSPRAY_API plugin : public trrojan::plugin_base {

    public:

        typedef trrojan::plugin_base::benchmark_list benchmark_list;
        typedef trrojan::plugin_base::environment_list environment_list;

        inline plugin(void) : trrojan::plugin_base("ospray") { }

        virtual ~plugin(void);

        virtual size_t create_benchmarks(benchmark_list& dst) const;

        virtual size_t create_environments(environment_list& dst) const;

    };

}
}
