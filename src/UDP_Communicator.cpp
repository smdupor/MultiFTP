/**
 * UDP_Communicator.cpp superclass encapsulates all shared UDP communication code for MultiFTP Clients and Servers. Includes
 * required buffers, functionality to read/write packet headers and perform checksums. Also includes shared utility
 * functionality like terminal printing.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include "UDP_Communicator.h"

/**
 * Destructor -- override in subclasses.
 */
UDP_Communicator::~UDP_Communicator() {
   //Empty Destructor -- Override in Subclasses
}

/**
 * Establish a bound with bind() UDP Socket on this port (incoming communication)
 *
 * @param port Port to bind socket to
 * @return a socket file descriptor for the bound socket
 */
int UDP_Communicator::create_bound_UDP_socket(int port) {
   int sockfd; // socket descriptor
   struct sockaddr_in serv_addr; //socket addresses

   // Create the socket
   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0) {
      error("ERROR opening socket");
      return -1;
   }

   // Initialize address and port values
   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(port);

   // Bind the socket
   if (bind(sockfd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      error("Error on socket bind");
      return -1;
   }

   // Report socket creation and return the sockfd
   verbose("Incoming Socket bound to port: " + std::to_string(port));
   return sockfd;
}

/**
 * Establish a UDP Socket on this port that is NOT bound (outgoing communication, multiple remote hosts) and NOT blocking
 * such that data read calls will return 0 bytes and continue to enable polling of this socket.
 *
 * @param port Port to bind socket to
 * @return a socket file descriptor for the bound socket
 */
int UDP_Communicator::create_unbound_UDP_socket(int port) {
   int sockfd; // socket descriptor

   // Establish the timeout of nonblocking socket in microseconds
   struct timeval socket_timeout;
   socket_timeout.tv_sec = 0;
   socket_timeout.tv_usec = 10;

   // Create the socket
   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof socket_timeout);
   if (sockfd < 0) {
      error("ERROR opening socket");
      return -1;
   }

   // Report that the socket was established and return the sockfd
   verbose("Outgoing Socket established on port: " + std::to_string(port));
   return sockfd;
}

/**
 * Read the sequence number of the packet currently in the input buffer and convert from 4 characters to a 32-bit
 * unsigned int
 * @return the sequence number as a uint32_t
 */
uint32_t UDP_Communicator::decode_seq_num() {
   return ((unsigned char) in_buffer[3] << 24) | ((unsigned char) in_buffer[2] << 16) |
          ((unsigned char) in_buffer[1] << 8) | ((unsigned char) in_buffer[0]);
}

/**
 * Traverse the packet in the input buffer and compute the 1's complement UDP-checksum of this packet.
 * @return 16-bit UDP checksum
 */
uint16_t UDP_Communicator::decode_checksum() {
   uint32_t sum = 0;
   uint16_t ret_val;

   // Traverse entire buffer and sum, allowing overflow to build up in the 16 higher-significance bits
   size_t len = MSG_LEN;
   for (size_t i = 8; i < len; ++i) {
      sum += (unsigned char) in_buffer[i];
   }

   // Bit shift and add back the overflow (twice, to capture new overflow from the add-back)
   sum = (sum & 0xFFFF) + (sum >> 16);
   sum = (sum & 0xFFFF) + (sum >> 16);

   // Capture the 16 bit checksum and invert it
   ret_val = ~(sum & 0xFFFF);
   return ret_val;
}

/**
 * Read the packet type of the packet in the input buffer and return the user-friendly enum value of this packet.
 * @return enum user-friendly type of this input packet
 */
uint16_t UDP_Communicator::decode_packet_type() {
   if (in_buffer[6] == '\x55' && in_buffer[7] == '\x55')
      return DATA_PACKET;
   else if (in_buffer[6] == '\xAA' && in_buffer[7] == '\xAA')
      return ACK;
   else if (in_buffer[6] == '\xA5' && in_buffer[7] == '\xA5')
      return FIN;
   else if (in_buffer[6] == '\x5A' && in_buffer[7] == '\x5A')
      return RESET;
   else
      return 0;
}

/**
 * Convert the 32-bit sequence number into four 8-bit characters and emplace them into the packet header of
 * the packet in the output buffer
 * @param sequence_number 32-bit sequence number
 */
void UDP_Communicator::encode_seq_num(uint32_t sequence_number) {

   out_buffer[3] = sequence_number >> 24;
   out_buffer[2] = sequence_number >> 16;
   out_buffer[1] = sequence_number >> 8;
   out_buffer[0] = sequence_number;

}

/**
 * Traverse the packet in the output buffer and compute the 16-bit 1's complement UDP checksum; Convert this to two
 * 8-bit characters and emplace them in the packet header in the output buffer
 */
void UDP_Communicator::encode_checksum() {
   uint32_t sum = 0;
   size_t len = MSG_LEN;

   // Traverse entire output buffer and sum, allowing overflow to build up in the 16 higher-significance bits
   // Note that unused bits are zeroed by the protocol when each packet is created.
   for (size_t i = 8; i < len; ++i) {
      sum += (unsigned char) out_buffer[i];
   }

   // Bit shift and add back the overflow (twice, to capture new overflow from the add-back)
   sum = (sum & 0xFFFF) + (sum >> 16);
   sum = (sum & 0xFFFF) + (sum >> 16);

   // Discard empty upper 16 bits and invert the result
   uint16_t truncated_sum = ~(sum & 0xFFFF);

   // Convert into two 8-bit characters and emplace into the packet in the output buffer
   out_buffer[4] = truncated_sum >> 8;
   out_buffer[5] = truncated_sum;
}

/**
 * Encode a user-friendly enum packet type into the 16-bit packet type field (two 8-bit characters) and emplace
 * these characters into the packet header in the output buffer
 * @param type
 */
void UDP_Communicator::encode_packet_type(int type) {
   switch (type) {
      case DATA_PACKET:
         out_buffer[6] = '\x55';
         out_buffer[7] = '\x55';
         break;
      case ACK:
         out_buffer[6] = '\xAA';
         out_buffer[7] = '\xAA';
         break;
      case FIN:
         out_buffer[6] = '\xA5';
         out_buffer[7] = '\xA5';
         break;
      case RESET:
         out_buffer[6] = '\x5A';
         out_buffer[7] = '\x5A';
         break;
   }
}

/** (Sent data)
 * Print data (that has been sent by this host) in dark yellow.
 * @param input the string to print
 */
void UDP_Communicator::print_sent(std::string input) {
   std::cout << "\033[33m" << input << "\033[0m";
   std::cout.flush();
}

/**(received data)
 * Print data (that has been received by this host) in green.
 * @param input the string to print
 */
void UDP_Communicator::print_recv(std::string input) {
   std::cout << "\033[32m" << input << "\033[0m";
   std::cout.flush();

}

/** (verbose mode support)
 * When system is running in "verbose" mode, print the string to the console in purple.
 * @param input the string to print
 */
void UDP_Communicator::verbose(std::string input) {
   if (debug) {
      std::cout << "\033[35m" << input << "\033[0m" << std::endl;
   }
}

/** (system errors)
 * Print the string to the console in Bright Red.
 * @param input the string to print
 */
void UDP_Communicator::error(std::string input) {
   std::cout << "\033[91m" << input << "\033[0m" << std::endl;
}

/** (system warnings)
 * Print the string to the console in Bright Yellow
 * @param input the string to print
 */
void UDP_Communicator::warning(std::string input) {
   std::cout << "\033[93m" << input << "\033[0m" << std::endl;
}

/** (information messages)
 * Print the string to the console in Cyan
 * @param input the string to print
 */
void UDP_Communicator::info(std::string input) {
   std::cout << "\033[36m" << input << "\033[0m" << std::endl;
}
