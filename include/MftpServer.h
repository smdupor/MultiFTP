//
// Created by smdupor on 6/25/21.
//

#ifndef MULTIFTP_MFTPSERVER_H
#define MULTIFTP_MFTPSERVER_H

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

class MftpServer : public UDP_Communicator {

private:
   std::list<LogItem> local_time_logs;
   std::vector<RemoteHost> remote_hosts;

   int inbound_socket, system_port;
public:
   MftpServer(std::list<std::string> &server_list, std::string &logfile, int port, bool verbose);
   ~MftpServer() override;
   void start();
};


#endif //MULTIFTP_MFTPSERVER_H
