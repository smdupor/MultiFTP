/**
 * MftpServer.h class inherits all member functions from the UDP_Communicator superclass, and implements the rdt_receive()
 * (Reliable Data Transfer Receive) API which receives packets from a remote client, checksums the packets for
 * validity and in-orderness, and returns ACKs to the client when appropriate. The class also implements a
 * probabilistic loss service to simulate lossy connections for performance experiments.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#ifndef INCLUDE_MFTPSERVER_H
#define INCLUDE_MFTPSERVER_H

#include "UDP_Communicator.h"

class MftpServer : public UDP_Communicator {
private:
   // Communication Variables
   struct sockaddr_in *remote_sock_addr;
   std::string filename;
   int inbound_socket, system_port;
   int loss_probability;
   int bytes_written;

   // Communication Functions
   bool valid_seq_num();
   bool valid_checksum();
   bool valid_pkt_type();
   bool probability_not_dropped();

   // Utility Variables
   uint_fast64_t packet_count;
   uint_fast32_t loss_count;
   std::list<LogItem> local_time_logs;

public:
   MftpServer(std::string &file_path, std::string &logfile, int port, bool verbose, float loss_probability);
   ~MftpServer() override;
   void rdt_receive();
   void system_report();
};


#endif //INCLUDE_MFTPSERVER_H
