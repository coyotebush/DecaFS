#ifndef __IP_ADDRESS_H__
#define __IP_ADDRESS_H__

#include <stdint.h>
#include <string.h>

#include "net_tcp/connection_to_client.h"
#define NUM_ESPRESSO 4
#define IP_LENGTH 16

struct ip_address {
  char addr[IP_LENGTH];
  ip_address() : addr {'\0'} {};
  ip_address(char *addr) {
    strcpy (this->addr, addr);
  }

  bool operator ==(const ip_address & other) const {
    return (strcmp (this->addr, other.addr) == 0);
  }

  bool operator !=(const ip_address & other) const {
    return !operator==(other);
  }
  
  bool operator <(const ip_address & other) const {
    return (strcmp (this->addr, other.addr) <= 0);
  }
};

struct client {
  struct ip_address ip;
  uint32_t user_id;
  ConnectionToClient *ctc;
  
  client() : ip (ip_address()), user_id (0), ctc (NULL) {};
  client(struct ip_address ip, uint32_t user_id, ConnectionToClient *ctc) :
    ip(ip), user_id (user_id), ctc (ctc) {} 

  bool operator ==(const client & other) const {
    return (this->ip == other.ip &&
            this->user_id == other.user_id &&
            this->ctc == other.ctc);
  }

  bool operator !=(const client & other) const {
    return !operator==(other);
  }
  
  bool operator <(const client & other) const {
    return ((this->ip < other.ip) ? true :
              (this->user_id < other.user_id) ? true :
                 (this->ctc < other.ctc) ? true : false);
  }
};

struct active_nodes {
  uint32_t node_numbers[NUM_ESPRESSO];
  uint32_t active_node_count;
};

bool is_ip_null (struct ip_address ip);
bool is_client_null (struct client client);

#endif
