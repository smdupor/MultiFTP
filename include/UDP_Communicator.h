/**
 * NetworkCommunicator
 *
 * The NetworkCommunicator superclass encapsulates all functionality that is shared by both major components of the system,
 * the Registration Server and the P2P Client. This includes establishing listening sockets, establishing outgoing
 * connections, transmitting strings of data on the TCP stream, and receiving strings of data back. Any universally-shared
 * instance variables (such as port, a list of peers, verbosity flags) are also encapsulated in this superclass.
 *
 * Also, all of the messaging constants, and system-wide numeric constants (like timeout settings) are encapsulated here.
 *
 * Finally, universally shared utility methods, such as a string splitter and multiple pretty-print methods, are
 * encapsulated here.
 *
 *  Created on: May 31, 2021
 *      Author: smdupor
 */

#ifndef INCLUDE_UDP_COMMUNICATOR_H_
#define INCLUDE_UDP_COMMUNICATOR_H_

#include <iostream>
#include <list>
#include <vector>

#include <string.h>
#include <unistd.h>
#include <ctime>
#include <netinet/in.h>
#include <chrono>

// Hold information about a socket
struct sockinfo {
		int socket;
		char * cli_addr;
	};

struct RemoteHost {
   explicit RemoteHost(sockaddr_in *addr, int sockfd) {
      this->address = addr;
      this->sockfd = sockfd;
      segment_num = 0;
      ack_num = 0;
   }
   sockaddr_in *address;
   int sockfd;
   uint16_t segment_num, ack_num;
};

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

class UDP_Communicator{

protected:

   static const int MSG_LEN = 1500;
   char in_buffer[MSG_LEN], out_buffer[MSG_LEN];
   uint32_t seq_num, ack_num;
	std::string log;
	int port;
	bool debug, system_on;
	std::time_t start_time;

   enum {DATA_PACKET = 1, ACK = 2};

   uint32_t decode_seq_num();
   uint16_t decode_checksum();
   uint16_t decode_packet_type();

   void encode_seq_num(uint32_t seqnum);
   void encode_checksum();
   void encode_packet_type(int type);


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
