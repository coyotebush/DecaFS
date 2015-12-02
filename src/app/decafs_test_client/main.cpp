
#include "network_core/barista_server.h"
#include "network_core/espresso_client.h"
#include "../decafs_barista/decafs_barista.h"
#include "network_core/decafs_client.h"

#include <stdio.h>
#include <thread>
#include "network_core/barista_network_helper.h"

int main(int argc, char** argv) {
  int port = 3899;
  char filename[] = "testfile";
  DecafsClient client("127.0.0.1", port, 2);
  client.openConnection();

  sleep(1);

  // OPEN
  int fd = client.open(filename, O_RDWR);
  std::cout << "open returned: " << fd << std::endl;
  sleep(1);

  // WRITE
  char *chunks[] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWX",
    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwx",
    "tHE QUICK BROWN fOX JUMPS OVER THE LAZY DOG_ tHE QUICK BROWN fOX JUMPS OVER THE LAZY DOG_ tHE QUICK BROWN fOX JUMPS OVER THE LAZ"
  };
  int bytes_written;
  for (int i = 0; i <3; i++) {
    bytes_written = client.write(fd, chunks[i], 128);
    std::cout << "write returned: " << bytes_written << std::endl;
    sleep(1);
  }
  
  // FILE STORAGE STAT
  client.file_storage_stat(filename);
  sleep(1);

  std::cout << "\nPlease take the espresso responsible for chunk 1 offline (probably node 1)\n";
  sleep(10);

  // SEEK
  int offset = client.lseek(fd, 0, SEEK_SET);
  std::cout << "seek returned: " << offset << std::endl;
  sleep(1);

  // READ
  char testread[1000];
  int bytes_read = client.read(fd, testread, 8);
  std::cout << "read returned: " << bytes_read << std::endl;
  sleep(1);
  
  // WRITE
  char char_write[] = "  UPDATED CHUNK  ";
  bytes_written = client.write(fd, char_write, strlen(char_write));
  std::cout << "write returned: " << bytes_written << std::endl;
  sleep(1);

  // SEEK
  offset = client.lseek(fd, 0, SEEK_SET);
  std::cout << "seek returned: " << offset << std::endl;
  sleep(1);

  // READ
  bytes_read = client.read(fd, testread, 384);
  std::cout << "read returned: " << bytes_read << std::endl;
  sleep(1);

  std::cout << "\nPlease put the espresso back online\n";
  sleep(10);
  
  // SEEK
  offset = client.lseek(fd, 0, SEEK_SET);
  std::cout << "seek returned: " << offset << std::endl;
  sleep(1);

  // READ
  bytes_read = client.read(fd, testread, 384);
  std::cout << "read returned: " << bytes_read << std::endl;
  sleep(1);
  
  // CLOSE
  int close = client.close(fd);
  std::cout << "close returned: " << close << std::endl;

  return 0;
}
