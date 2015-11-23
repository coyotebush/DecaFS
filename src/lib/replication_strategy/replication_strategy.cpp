#include "replication_strategy.h"

extern "C" int put_replica (uint32_t file_id, char *pathname,
                            uint32_t stripe_id, uint32_t chunk_num) {
  // TODO RAID 5?
  return PARITY_NODE;
}

