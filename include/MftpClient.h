/**
 */

#ifndef INCLUDE_MFTPCLIENT_H_
#define INCLUDE_MFTPCLIENT_H_

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <ctime>
#include <algorithm>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include "UDP_Communicator.h"

class MftpClient : public UDP_Communicator {

private:
   std::string hostname, path_prefix;
   std::list<LogItem> local_time_logs;
   std::vector<RemoteHost> remote_hosts;
   uint16_t MSS, byte_index;
   int sockfd, system_port;

   // Inline these for performance optimization
   inline void check_files();

   void write_time_log();
   //void downloader_backoff(size_t past_local_qty, int &backoff_time);
   void shutdown_system();

public:
   MftpClient(std::list<std::string> &server_list, std::string &logfile, int port, bool verbose,
              uint16_t max_seg_size);
   ~MftpClient() override;
   void start();
   void rdt_send(char data);
   bool get_system_on();
   void shutdown();

};

#endif /* INCLUDE_MFTPCLIENT_H_ */
