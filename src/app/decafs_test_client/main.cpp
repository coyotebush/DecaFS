
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
  char filename2[] = "testfile2";
  DecafsClient client("127.0.0.1", port, 2);
  client.openConnection();

  sleep(1);

  // OPEN
  int fd = client.open(filename, O_RDWR);
  std::cout << "open returned: " << fd << std::endl;
  char testwrite[] = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 100";
  int bytes_written = client.write(fd, testwrite, strlen(testwrite));
  std::cout << "write returned: " << bytes_written << std::endl;
  client.file_storage_stat(filename);
  client.lseek(fd, 0, SEEK_SET);
  char testread[1000];
  int bytes_read = client.read(fd, testread, strlen(testwrite));
  std::cout << bytes_read << ' ' << testread << std::endl;
  client.close(fd);

  int fd2 = client.open(filename2, O_RDWR);
  std::cout << "open returned: " << fd2 << std::endl;

  // WRITE
  bytes_written = client.write(fd2, testwrite, strlen(testwrite));
  std::cout << "write returned: " << bytes_written << std::endl;
  
  // FILE STORAGE STAT
  client.file_storage_stat(filename2);

  client.lseek(fd2, 0, SEEK_SET);
  bytes_read = client.read(fd2, testread, strlen(testwrite));
  std::cout << bytes_read << ' ' << testread << std::endl;
  exit(0);

  sleep(10);

  // SEEK
  int offset = client.lseek(fd, 8, SEEK_SET);
  std::cout << "seek returned: " << offset << std::endl;
  sleep(1);

  // WRITE
  char char_write[] = "  UPDATED CHUNK  ";
  bytes_written = client.write(fd, char_write, strlen(char_write));
  std::cout << "write returned: " << bytes_written << std::endl;
  sleep(1);

  std::cout << "\nPlease put the espresso back online\n";
  sleep(10);
  
  // SEEK
  offset = client.lseek(fd, 0, SEEK_SET);
  std::cout << "seek returned: " << offset << std::endl;
  sleep(1);

  // READ
  /* char testread[1000]; */
  bytes_read = client.read(fd, testread, strlen(testwrite));
  std::cout << "read returned: " << bytes_read << std::endl;
  sleep(1);
  
  // CLOSE
  int close = client.close(fd);
  std::cout << "close returned: " << close << std::endl;

  return 0;
}
