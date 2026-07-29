#include "python/mirror_bridge_python.hpp"
#include "worker.hpp"

MIRROR_BRIDGE_MODULE(gil_worker,
    mirror_bridge::bind_class<Worker>(m, "Worker").release_gil();
)
