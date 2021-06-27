//
// Created by smdupor on 6/25/21.
//

#ifndef INCLUDE_MFTPSERVER_H
#define INCLUDE_MFTPSERVER_H



#include "UDP_Communicator.h"

class MftpServer : public UDP_Communicator {

private:
   std::list<LogItem> local_time_logs;
   struct sockaddr_in *remote_sock_addr;
   std::string filename;
   int inbound_socket, system_port;
   int loss_probability;
   uint_fast32_t loss_count;
   int bytes_written;
   uint_fast64_t packet_count;

   bool valid_seq_num();
   bool valid_checksum();
   bool valid_pkt_type();
   bool probability_not_dropped();

public:
   MftpServer(std::string &file_path, std::string &logfile, int port, bool verbose, float loss_probability);
   ~MftpServer() override;
   void rdt_receive();
   void system_report();
};


#endif //INCLUDE_MFTPSERVER_H
