/**
 */

#ifndef INCLUDE_P2PCLIENT_H_
#define INCLUDE_P2PCLIENT_H_

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <ctime>
#include <algorithm>
#include <vector>
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
 * Encapsulate a datapoint for a time value when a qty of files have been downloaded.
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


/**
 * P2P Client subclass header.
 */
class MftpClient : public UDP_Communicator {

private:
   std::string hostname, path_prefix;
   std::list<LogItem> local_time_logs;

   // Inline these for performance optimization
   inline void check_files();

   void write_time_log();
   void downloader_backoff(size_t past_local_qty, int &backoff_time);
   void shutdown_system();

public:
   MftpClient(std::string &addr_reg_server, std::string &logfile, bool verbose);
   ~MftpClient() override;
   void start(std::string config_file);
   bool get_system_on();

};

#endif /* INCLUDE_P2PCLIENT_H_ */
