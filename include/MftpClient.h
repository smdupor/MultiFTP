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


/**
 * Encapsulate a datapoint for an accurate time value.
 */
struct LogItem {
   // On creation, log the immediate (steady) time and the input quantity
   explicit LogItem(size_t qty) {
         this->qty = qty;
         this->time = std::chrono::steady_clock::now();
   }
   std::chrono::steady_clock::time_point time;
   size_t qty;
};

struct RemoteClient {
   explicit RemoteClient(sockaddr_in *addr) {
      this->address = addr;
      segment_num = -1;
      ack_num = -1;
   }
   sockaddr_in *address;
   int segment_num, ack_num;
};

class MftpClient : public UDP_Communicator {

private:
   std::string hostname, path_prefix;
   std::list<LogItem> local_time_logs;
   std::vector<RemoteClient> remote_hosts;

   int inbound_socket, system_port;

   // Inline these for performance optimization
   inline void check_files();

   void write_time_log();
   //void downloader_backoff(size_t past_local_qty, int &backoff_time);
   void shutdown_system();

public:
   MftpClient(std::list<std::string> &server_list, std::string &logfile, int port, bool verbose);
   ~MftpClient() override;
   void start();
   void start_reversed();
   bool get_system_on();

};

#endif /* INCLUDE_MFTPCLIENT_H_ */
