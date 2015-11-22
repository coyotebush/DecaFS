#include "monitored_strategy.h"
#include "monitored_strategy_c_api.h"

static int num_nodes_down;

extern "C" void strategy_startup() {
  // Register custom monitoring here given a specified timeout.
  // register_monitor_module (&monitor, TIMEOUT);

  num_nodes_down = get_num_espressos();
  printf("Starting with all %d nodes down\n", num_nodes_down);

  register_node_failure_handler (&node_failure_handler_func);
  register_node_up_handler (&node_up_handler_func);
}

extern "C" void monitor_func () {
  // Do custom monitoring here. This function will be called every TIMEOUT
  // seconds after registration above.
}

extern "C" void node_failure_handler_func (uint32_t node_number) {
  printf ("Handling node failure...\n");
  // Add custom processing to be done when node node_number goes down.
  if (num_nodes_down++) {
    printf("WARNING: More than one node is down (%d are down). "
        "Client operations may fail catastrophically!\n", num_nodes_down);
  }
}

extern "C" void node_up_handler_func (uint32_t node_number) {
  printf ("Handling node coming online...\n");
  // Add custom processing to be done when node node_number goes up.
  num_nodes_down--;
  flush_chunks(node_number);
  printf ("Done handling node coming online...\n");
}

