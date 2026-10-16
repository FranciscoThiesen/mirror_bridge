// Built with -DMIRROR_BRIDGE_BATCH_METHODS by test_batch.sh. Without the flag
// nothing here is generated, which the test also checks.
#include "python/mirror_bridge_python.hpp"
#include "batched.hpp"

MIRROR_BRIDGE_MODULE(batched, mirror_bridge::bind_class<Batched>(m, "Batched");)
