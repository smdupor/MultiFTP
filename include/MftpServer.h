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
  // std::vector<RemoteHost> remote_hosts;
   struct sockaddr_in *remote_sock_addr;

   int inbound_socket, system_port;
   float loss_probability;

   char buffer[1500];

   bool valid_seq_num(char *buffer);
   bool valid_checksum(char *buffer);
   bool valid_pkt_type(char *buffer);
   bool probability_not_dropped();

public:
   MftpServer(std::string &file_path, std::string &logfile, int port, bool verbose, float loss_probability);
   ~MftpServer() override;
   void start();
   void rdt_receive();


};


#endif //MULTIFTP_MFTPSERVER_H
