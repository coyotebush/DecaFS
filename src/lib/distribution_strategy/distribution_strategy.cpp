#include <cassert>
#include "distribution_strategy.h"

extern "C" int put_chunk (uint32_t file_id, char *pathname, uint32_t stripe_id,
                          uint32_t chunk_num) {
  assert(chunk_num <= 3);
  return chunk_num;
}
