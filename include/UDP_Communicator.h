/**
 * UDP_Communicator.h superclass encapsulates all shared UDP communication code for MultiFTP Clients and Servers. Includes
 * required buffers, functionality to read/write packet headers and perform checksums. Also includes shared utility
 * functionality like terminal printing.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#ifndef INCLUDE_UDP_COMMUNICATOR_H_
#define INCLUDE_UDP_COMMUNICATOR_H_

#include <iostream>
#include <fstream>
#include <list>
#include <vector>
#include <cstring>
#include <ctime>
#include <chrono>
#include <cmath>

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

class UDP_Communicator {
protected:
   // Communication Buffers
   static const int MSG_LEN = 1500;
   char in_buffer[MSG_LEN], out_buffer[MSG_LEN];
   uint32_t seq_num, ack_num;

   // Utility Variables
   std::string log;
   bool debug;

   // Define user-friendly packet types
   enum {
      DATA_PACKET = 1, ACK = 2, FIN = 3, RESET = 4
   };

   // Read Packet headers
   uint32_t decode_seq_num();
   uint16_t decode_checksum();
   uint16_t decode_packet_type();

   // Write packet headers
   void encode_seq_num(uint32_t seqnum);
   void encode_checksum();
   void encode_packet_type(int type);

/**
 * Encapsulate contact information, Current Sequence/segment number, and latest ack number, for a remote MultiFTP Host.
 */
   struct RemoteHost {
      explicit RemoteHost(sockaddr_in *addr, int sockfd) {
         this->address = addr;
         this->sockfd = sockfd;
         segment_num = 0;
         ack_num = 0;
      }

      sockaddr_in *address;
      int sockfd;
      uint32_t segment_num, ack_num;
   };

/**
 * Encapsulate a datapoint for an accurate time value.
 */
   struct LogItem {
      // On creation, log the immediate (steady) time
      explicit LogItem() {
         this->time = std::chrono::steady_clock::now();
      }

      std::chrono::steady_clock::time_point time;
   };

public:
   virtual ~UDP_Communicator();
   int create_inbound_UDP_socket(int port);
   int create_outbound_UDP_socket(int port);

   //Externally-accessible print methods (used in int main()s)
   static void error(std::string input);
   static void warning(std::string input);
   static void info(std::string input);
   static void print_sent(std::string input);
   static void print_recv(std::string input);
   void verbose(std::string input);

};

#endif /* INCLUDE_UDP_COMMUNICATOR_H_ */
