#ifndef _DECAFS_CLIENT_H_
#define _DECAFS_CLIENT_H_

#include <iostream>

#include "net_tcp/tcp_client.h"
#include "network_packets.h"
#include "open_packet.h"
#include "open_packet_response.h"
#include "espresso_packet_processor.h"

class DecafsClient : public TcpClient {

  private:
    uint32_t user_id;

  public:
    DecafsClient(std::string hostname, unsigned short port, uint32_t user_id);

    void connectionClosed();
    void connectionEstablished();
    void handleMessageFromServer(int socket);

    int open(const char* pathname, int flags);
    //ssize_t read(int fd, void* buf, ssize_t count);
    //ssize_t write(int fd, const void buf*, ssize_t count);
    //int clode(fd);
    //void delete_file(char* pathname);
    //void sync();
    //int file_stat(const char* path, struct stat *buf);
    //int file_fstat(int fd, struct stat *buf);
    //void statfs(char *pathname, struct *statvfs);
    //int mkdir(const char* dirname);
    //DIR* opendir(const char* name);
    //struct dirent* readdir(DIR *dirp);
};

#endif // _ESPRESSO_CLIENT_H_