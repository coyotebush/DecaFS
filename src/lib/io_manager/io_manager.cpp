#include "io_manager.h"

IO_Manager::IO_Manager() {

}

void IO_Manager::init(char *metadata_path) {
  std::string node_metadata_file = std::string(metadata_path) +
                                   std::string(node_metadata_filename);
  std::string replica_metadata_file = std::string(metadata_path) +
                                      std::string(replica_metadata_filename);
  chunk_to_node.open (node_metadata_file.c_str());
  chunk_to_replica_node.open (replica_metadata_file.c_str());
}

uint32_t IO_Manager::process_read_stripe (uint32_t request_id, uint32_t file_id,
                                          char *pathname, uint32_t stripe_id,
                                          uint32_t stripe_size, uint32_t chunk_size,
                                          const void *buf, int offset,
                                          size_t count) {
  uint32_t chunk_id, bytes_read = 0, read_size = 0, num_chunks = 0;
  int chunk_offset, chunk_result, node_id;
  
  assert (((int)count - offset) <= (int)stripe_size);
  
  printf ("\n(BARISTA) Process Read Stripe\n");

  get_first_chunk (&chunk_id, chunk_size, &chunk_offset, offset);
  
  while (bytes_read < count) {
    struct file_chunk cur_chunk = {file_id, stripe_id, chunk_id};

    if (!chunk_exists (cur_chunk)) {
      // Current chunk does not exist. Report and error and stop the read.
      fprintf (stderr, "Could only read %d bytes (out of %d requested.\n",
                  (int)bytes_read, (int)count);
      break;
    }

    // The chunk exists, so set the node_id
    node_id = chunk_to_node[cur_chunk];

    // Determine how much data to read from the current chunk
    if (count - bytes_read > chunk_size - chunk_offset) {
      read_size = chunk_size - chunk_offset;
    }
    else {
      read_size = count - bytes_read;
    }
    
    printf ("\tprocessing chunk %d (sending to node %d)\n", chunk_id, node_id);
    printf ("\t\toffset: %d, size: %d\n", chunk_offset, read_size);
    // Send the read to the node
                   // ADD FD HERE
    if (is_node_up(node_id)) {
      chunk_result = process_read_chunk (request_id, 0, file_id, node_id, stripe_id,
                                        chunk_id, chunk_offset,
                                        (uint8_t *)buf + bytes_read,
                                        read_size);
      printf ("\t\treceived %d from network call.\n", chunk_result);
    }
    else {
      // RAID invariants:
      // - For all chunks, node_id == chunk_num.
      // - Parity chunk is guaranteed to exist if any chunk in the stripe does.
      // - chunk_to_node maps every primary and parity chunk to its node.
      // - chunk_to_replica_node maps every primary chunk to the node storing
      //   its parity chunk.

      printf("RAID reconstructing chunk %d,%d,%d since node %d is DOWN\n",
             file_id, stripe_id, chunk_id, node_id);
      uint32_t raid_request_id = get_new_request_id();
      int parity_node_id = chunk_to_replica_node[cur_chunk];

      int read_count = 0;
      struct file_chunk raid_chunk = cur_chunk;
      for (raid_chunk.chunk_num = 1; raid_chunk.chunk_num <= get_num_espressos();
           raid_chunk.chunk_num++) {
        if (raid_chunk != cur_chunk && chunk_exists(raid_chunk)) {
          chunk_result = process_read_chunk(raid_request_id, 0,
              file_id, raid_chunk.chunk_num, stripe_id, raid_chunk.chunk_num,
              chunk_offset, nullptr, read_size);
          printf("\tRAID reading chunk %d,%d,%d from node %d got %d\n",
                 file_id, stripe_id, raid_chunk.chunk_num,
                 raid_chunk.chunk_num, chunk_result);
          read_count++;
        }
      }
      assert(read_count > 0);
      chunk_changes.insert(make_pair(raid_request_id,
          ChunkViking(request_id, cur_chunk, read_count,
                      vector<uint8_t>(read_size, 0))));
    }
    // The read suceeded, so move on
    // update counters
    chunk_offset = 0;
    bytes_read += read_size;
    chunk_id++;
    num_chunks++;
  }
  return num_chunks;
}

void IO_Manager::process_write_stripe (uint32_t request_id,
                                       uint32_t replica_request_id,
                                       uint32_t *chunks_written,
                                       uint32_t *replica_chunks_written,
                                       uint32_t file_id, char *pathname,
                                       uint32_t stripe_id, uint32_t stripe_size,
                                       uint32_t chunk_size, const void *buf,
                                       int offset, size_t count) {
  uint32_t chunk_id, bytes_written = 0, write_size = 0;
  int chunk_offset, node_id, parity_node_id, chunk_result;

  assert (((int)count - offset) <= (int)stripe_size);
  printf ("\n(BARISTA) Process Write Stripe\n");
  
  get_first_chunk (&chunk_id, chunk_size, &chunk_offset, offset);

  while (bytes_written < count) {
    struct file_chunk cur_chunk = {file_id, stripe_id, chunk_id};
    parity_node_id = put_replica (file_id, pathname, stripe_id, chunk_id);
    struct file_chunk parity_chunk = {file_id, stripe_id, parity_node_id};
    bool chunk_already_exists = chunk_exists(cur_chunk);
    bool parity_chunk_already_exists = chunk_exists(parity_chunk);
    
    // If the chunk does not exists, create it
    if (!chunk_already_exists) {
      node_id = put_chunk (file_id, pathname, stripe_id, chunk_id);
      printf ("\tchunk doesn't exist. Preparing to send chunk to node %d\n", node_id);
      chunk_to_node[cur_chunk] = node_id;
      chunk_to_replica_node[cur_chunk] = parity_node_id;
    }

    if (!parity_chunk_already_exists) {
      printf ("\tparity chunk doesn't exist. Preparing to send chunk to node %d\n", parity_node_id);
      chunk_to_node[parity_chunk] = parity_node_id;
    }

    // Ensure that we have the proper node id's to send data to
    node_id = chunk_to_node[cur_chunk];
    parity_node_id = chunk_to_replica_node[cur_chunk];
    uint32_t raid_request_id = get_new_request_id();
    uint32_t ignore_request_id = get_new_request_id();
    int read_count = 0;

    // Determine the size of the write
    if (count - bytes_written > chunk_size - chunk_offset) {
      write_size = chunk_size - chunk_offset;
    }
    else {
      write_size = count - bytes_written;
    }

    if (is_node_up(parity_node_id)) {
      if (is_node_up(node_id)) {
        printf("RAID sending chunk %d,%d,%d to node %d\n",
               file_id, stripe_id, chunk_id, node_id);

        // Read chunk
        if (chunk_already_exists) {
          chunk_result = process_read_chunk(raid_request_id, 0, file_id,
                                            node_id, stripe_id,
                                            chunk_id, chunk_offset,
                                            nullptr, write_size);
          printf("\tRAID reading original chunk %d,%d,%d from node %d got %d\n",
                 file_id, stripe_id, chunk_id,
                 node_id, chunk_result);
          read_count++;
        }

        // Read parity
        if (parity_chunk_already_exists) {
          chunk_result = process_read_chunk(raid_request_id, 0, file_id,
                                            parity_node_id, stripe_id,
                                            parity_node_id, chunk_offset,
                                            nullptr, write_size);
          printf("\tRAID reading parity chunk %d,%d,%d from node %d got %d\n",
                 file_id, stripe_id, parity_node_id,
                 parity_node_id, chunk_result);
          read_count++;
        }

        // Write chunk
        printf ("\tprocessing chunk %d (sending to node %d)\n", chunk_id, node_id);
        chunk_result = process_write_chunk (ignore_request_id, 0, file_id, node_id, stripe_id,
                                            chunk_id, chunk_offset, (uint8_t *)buf
                                            + bytes_written, write_size);
        ignorable_request_ids.insert(ignore_request_id);
      }

      else {
        // Parity is up, primary is down
        printf("RAID updating stripe parity for chunk %d,%d,%d since node %d is DOWN\n",
               file_id, stripe_id, chunk_id, node_id);

        struct file_chunk raid_chunk = cur_chunk;
        for (raid_chunk.chunk_num = 1; raid_chunk.chunk_num <= get_num_espressos();
             raid_chunk.chunk_num++) {
          if (raid_chunk != cur_chunk && chunk_exists(raid_chunk) &&
              raid_chunk.chunk_num != parity_node_id) {
            chunk_result = process_read_chunk(raid_request_id, 0,
                file_id, raid_chunk.chunk_num, stripe_id, raid_chunk.chunk_num,
                chunk_offset, nullptr, write_size);
            printf("\tRAID reading chunk %d,%d,%d from node %d got %d\n",
                   file_id, stripe_id, raid_chunk.chunk_num,
                   raid_chunk.chunk_num, chunk_result);
            read_count++;
          }
        }

        printf("\tqueueing write of chunk %d to node %d which is DOWN\n", chunk_id, node_id);
        dirty_chunks.insert(make_pair(node_id, cur_chunk));
      }

      ChunkViking viking(request_id, parity_chunk, cur_chunk, read_count,
                         chunk_offset,
                         vector<uint8_t>((uint8_t *)buf + bytes_written,
                                         (uint8_t *)buf + bytes_written + write_size));
      chunk_changes.insert(make_pair(raid_request_id, viking));
      if (read_count == 0) {
        raid_finalize(viking, request_id);
      }
    }
    else {
      // Parity is down
      printf ("\tprocessing chunk %d (sending to node %d)\n", chunk_id, node_id);
      chunk_result = process_write_chunk (request_id, 0, file_id, node_id, stripe_id,
                                          chunk_id, chunk_offset, (uint8_t *)buf
                                          + bytes_written, write_size);

      printf("\tqueueing write of parity chunk %d to node %d which is DOWN\n", parity_node_id, parity_node_id);
      dirty_chunks.insert(make_pair(parity_node_id, parity_chunk));
    }

    // update counters
    (*chunks_written)++;
    chunk_offset = 0;
    bytes_written += write_size;
    chunk_id++;
  }
}

uint32_t IO_Manager::process_delete_file (uint32_t request_id, uint32_t file_id) {
  std::vector<struct file_chunk> chunks = get_all_chunks (file_id); 
  uint32_t num_chunks = 0;

  for (std::vector<struct file_chunk>::iterator it = chunks.begin();
       it != chunks.end(); it++) {
    if (chunk_exists (*it)) {
      int chunk_node = get_node_id (file_id, (*it).stripe_id, (*it).chunk_num);
      if (is_node_up (chunk_node)) {
        process_delete_chunk (request_id, file_id, chunk_node, (*it).stripe_id, (*it).chunk_num);
        chunk_to_node.erase (*it);
        num_chunks++;
      }
    }
  }
  return num_chunks;
}

void IO_Manager::flush_chunks(int node_id) {
  printf("RAID flushing chunks for node %d\n", node_id);

  auto range = dirty_chunks.equal_range(node_id);
  for (auto it = range.first; it != range.second; it++) {
    int read_count = 0;
    struct file_chunk cur_chunk = it->second;
    struct file_chunk raid_chunk = cur_chunk;
    uint32_t raid_request_id = get_new_request_id();
    uint32_t ignore_request_id = get_new_request_id();

    for (raid_chunk.chunk_num = 1; raid_chunk.chunk_num <= get_num_espressos();
         raid_chunk.chunk_num++) {
      if (raid_chunk != cur_chunk && chunk_exists(raid_chunk)) {
        int chunk_result = process_read_chunk(raid_request_id, 0,
            raid_chunk.file_id, raid_chunk.chunk_num, raid_chunk.stripe_id,
            raid_chunk.chunk_num, 0, nullptr, get_chunk_size());
        printf("\tRAID reading chunk %d,%d,%d from node %d got %d\n",
               raid_chunk.file_id, raid_chunk.stripe_id, raid_chunk.chunk_num,
               raid_chunk.chunk_num, chunk_result);
        read_count++;
      }
    }

    ignorable_request_ids.insert(ignore_request_id);
    chunk_changes.insert(make_pair(raid_request_id,
          ChunkViking(ignore_request_id, cur_chunk, cur_chunk, read_count, 0,
                      vector<uint8_t>(get_chunk_size(), 0))));
  }
  dirty_chunks.erase(node_id);
}

bool IO_Manager::read_response_handler(ReadChunkResponse *read_response) {
  auto it = chunk_changes.find(read_response->id);
  if (it == chunk_changes.end()) {
    return false;
  }

  auto &viking = it->second;
  if (read_response->count > 0) {
    assert(viking.buffer.size() == read_response->count);
    for (size_t i = 0; i < viking.buffer.size(); i++) {
      viking.buffer[i] ^= read_response->data_buffer[i];
    }
  }
  if (!--viking.node_count) {
    raid_finalize(viking, read_response->id);
  }
  return true;
}

void IO_Manager::raid_finalize(ChunkViking &viking, uint32_t request_id) {
  if (viking.write_to_parity) {
    int chunk_result = process_write_chunk (viking.client_request_id, 0,
        viking.parity_chunk.file_id, viking.parity_chunk.chunk_num,
        viking.parity_chunk.stripe_id, viking.parity_chunk.chunk_num,
        viking.offset, viking.buffer.data(), viking.buffer.size());
    printf("RAID wrote parity chunk %d,%d,%d to node %d, got %d\n",
           viking.parity_chunk.file_id, viking.parity_chunk.stripe_id,
           viking.parity_chunk.chunk_num, viking.parity_chunk.chunk_num,
           chunk_result);
  }
  else {
    // Send to client
    read_chunk_handler(viking.client_request_id, viking.client_chunk,
        new read_buffer(viking.buffer.size(), viking.buffer.data()));
  }
  chunk_changes.erase(request_id);
}

bool IO_Manager::write_response_handler(WriteChunkResponse *write_response) {
  return ignorable_request_ids.erase(write_response->id);
}

bool IO_Manager::delete_response_handler(DeleteChunkResponse *delete_response) {
  return ignorable_request_ids.erase(delete_response->id);
}
    
char * IO_Manager::process_file_storage_stat (struct decafs_file_stat file_info) {
  stringstream storage_info;
  std::vector<struct file_chunk> chunks = get_all_chunks (file_info.file_id); 
  int last_stripe = -1;
  bool first_stripe = true;

  // Print basic file information
  storage_info << "{\n  \"file_id\": ";
  storage_info << file_info.file_id;
  storage_info << "\n  \"stripe_size\": ";
  storage_info << file_info.stripe_size;
  storage_info << "\n  \"chunk_size\": ";
  storage_info << file_info.chunk_size;
  storage_info << "\n  \"stripes\": [";

  for (std::vector<struct file_chunk>::iterator it = chunks.begin();
       it != chunks.end(); it++) {
    if (chunk_exists (*it)) {
      if ((int)(*it).stripe_id > last_stripe) {
        last_stripe = (*it).stripe_id;
        if (first_stripe) {
          first_stripe = false;
        }
        else {
          storage_info << "\n      ]\n    }";
        }
        storage_info << "\n    {\n      \"stripe_id\": ";
        storage_info << last_stripe;
        storage_info << "\n      \"chunks\": [";
      }
      storage_info << "\n        {";
      storage_info << "\n          \"chunk_num\": ";
      storage_info << (*it).chunk_num;
      storage_info << "\n          \"node\": ";
      storage_info << chunk_to_node[*it];
      if (chunk_replica_exists (*it)) {
        storage_info << "\n          \"replica_node\": ";
        storage_info << chunk_to_replica_node[*it];
      }
      storage_info << "\n        }";
    }
  }

  // end json
  if (!first_stripe) {
    storage_info << "\n      ]\n    }";
  }

  storage_info << "\n  ]\n}\n";

  return strdup(storage_info.str().c_str());
}

int IO_Manager::set_node_id (uint32_t file_id, uint32_t stripe_id,
                             uint32_t chunk_num, uint32_t node_id) {
  struct file_chunk chunk = {file_id, stripe_id, chunk_num};
  chunk_to_node[chunk] = node_id;
  return node_id;
}

int IO_Manager::get_node_id (uint32_t file_id, uint32_t stripe_id,
                             uint32_t chunk_num) {
  struct file_chunk chunk = {file_id, stripe_id, chunk_num};
  if (chunk_exists (chunk)) {
    return chunk_to_node[chunk];
  }
  return CHUNK_NOT_FOUND;
}

int IO_Manager::set_replica_node_id (uint32_t file_id, uint32_t stripe_id,
                                     uint32_t chunk_num, uint32_t node_id) {
  struct file_chunk chunk = {file_id, stripe_id, chunk_num};
  chunk_to_replica_node[chunk] = node_id;
  return node_id;
}

int IO_Manager::get_replica_node_id (uint32_t file_id, uint32_t stripe_id,
                                     uint32_t chunk_num) {
  struct file_chunk chunk = {file_id, stripe_id, chunk_num};
  if (chunk_replica_exists (chunk)) {
    return chunk_to_replica_node[chunk];
  }
  return REPLICA_CHUNK_NOT_FOUND;
}

// These 4 functions should be moved up to Barista Core
// this layer is stripe level not file level
int IO_Manager::stat_file_name (char *pathname, struct decafs_file_stat *buf) {
  return 0;
}

int IO_Manager::stat_file_id (uint32_t file_id, struct decafs_file_stat *buf) {
  return 0;
}

int IO_Manager::stat_replica_name (char *pathname, struct decafs_file_stat *buf) {
  return 0;
}

int IO_Manager::stat_replica_id (uint32_t file_id, struct decafs_file_stat *buf) {
  return 0;
}

void IO_Manager::sync () {
  // Intentionally left blank, in this implementation, all file data is written
  // to disk when "write" completes.
}

bool IO_Manager::chunk_exists (struct file_chunk chunk) {
  return (chunk_to_node.find (chunk) != chunk_to_node.end());
}

bool IO_Manager::chunk_replica_exists (struct file_chunk chunk) {
  return (chunk_to_replica_node.find (chunk) != chunk_to_replica_node.end());
}

std::vector<struct file_chunk> IO_Manager::get_all_chunks (uint32_t file_id) {
  std::vector <struct file_chunk> chunks;
  
  for (PersistentMap<struct file_chunk, int>::iterator it = chunk_to_node.begin();
  //for (map<struct file_chunk, int>::iterator it = chunk_to_node.begin();
         it != chunk_to_node.end(); it++) {
    struct file_chunk cur = it->first;
    if (cur.file_id == file_id) {
      chunks.push_back (cur);
    }
  }
  return chunks;
}

void IO_Manager::get_first_chunk (uint32_t *id, uint32_t chunk_size, int *chunk_offset, int offset) {
  *id = CHUNK_ID_INIT;
  while (offset > (int)chunk_size) {
    (*id)++;
    offset -= chunk_size;
  }
  *chunk_offset = offset;
}
