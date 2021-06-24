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

// Hold information about a socket
struct sockinfo {
		int socket;
		char * cli_addr;
	};

class UDP_Communicator{

protected:

   static const int MSG_LEN = 1024; // TODO: Get rid of this constant, needs to be come variable

	std::string log;
	int port;
	bool debug, system_on;
	std::time_t start_time;

	// Socket communication
   void transmit(int sockfd, std::string &out_message);
   std::string receive(int sockfd);
   int outgoing_connection(std::string hostname, int port);
   std::string receive_no_delim(int sockfd);

   // Utility methods
   std::vector<std::string> split(const std::string &input, char delim);


public:
	virtual ~UDP_Communicator();
   int create_inbound_UDP_socket(int port);
   int get_port();

   //Externally-accessible print methods (used in int main()s)
   static void error(std::string input);
   static void warning(std::string input);
   static void info(std::string input);
   static void print_sent(std::string input);
   static void print_recv(std::string input);
   void verbose(std::string input);

};

#endif /* INCLUDE_UDP_COMMUNICATOR_H_ */
