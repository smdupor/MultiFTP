/**
 */

#include "UDP_Communicator.h"
#include "MftpClient.h"

/**
 * Destructor -- override in subclasses.
 */
UDP_Communicator::~UDP_Communicator() {
   //Empty Destructor -- Override in Subclasses

}

/**
 * Establish a listening TCP socket on this host.
 *
 * @param listen_port Port to listen on
 * @return a socket file descriptor for the listening socket
 */
int UDP_Communicator::create_inbound_UDP_socket(int port) {
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
//   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(port);

   // Bind the socket
   if (bind(sockfd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      error("Error on socket bind");
      return -1;
   }

   // Listen for new connections
   verbose("Incoming Socket bound to port: " + std::to_string(port));
   return sockfd;
}

/**
 * Establish a listening TCP socket on this host.
 *
 * @param listen_port Port to listen on
 * @return a socket file descriptor for the listening socket
 */
int UDP_Communicator::create_outbound_UDP_socket(int port) {
   int sockfd; // socket descriptor


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

   // Listen for new connections
   verbose("Outgoing Socket established on port: " + std::to_string(port));
   return sockfd;
}

uint32_t UDP_Communicator::decode_seq_num() {
   return (in_buffer[3] << 24) | (in_buffer[2] << 16) | (in_buffer[1] << 8) | (in_buffer[0]);
}
uint16_t UDP_Communicator::decode_checksum() {
   uint32_t sum = 0;
   uint16_t ret_val;


   size_t len = MSG_LEN;
   for(size_t i = 8; i <len;++i){
      sum += (unsigned char) in_buffer[i];
   }

   sum = (sum & 0xFFFF) + (sum >> 16);
   sum = (sum & 0xFFFF) + (sum >> 16);
   ret_val = ~(sum & 0xFFFF);
      return ret_val;
}
uint16_t UDP_Communicator::decode_packet_type() {
   if(strlen(in_buffer + 8) > 0 && in_buffer[6] == '\x55' && in_buffer[7] == '\x55')
      return DATA_PACKET;
   else if(strlen(in_buffer + 8) > 0 && in_buffer[6] == '\xAA' && in_buffer[7] == '\xAA')
     return ACK;
    else
      return 0;
}

void UDP_Communicator::encode_seq_num(uint32_t seqnum) {

   out_buffer[3] = seqnum >> 24;
   out_buffer[2] = seqnum >> 16;
   out_buffer[1] = seqnum >> 8;
   out_buffer[0] = seqnum;

}
void UDP_Communicator::encode_checksum() {
   uint32_t sum = 0;
   size_t len = MSG_LEN;
   for(size_t i = 8; i <len;++i){
      sum += (unsigned char) out_buffer[i];
   }

   sum = (sum & 0xFFFF) + (sum >> 16);
   sum = (sum & 0xFFFF) + (sum >> 16);

   uint16_t clipped_sum = ~(sum & 0xFFFF);

   out_buffer[4] = clipped_sum >> 8;
   out_buffer[5] = clipped_sum;
}

// 0-4 Seqnum, 5-6 Checksum, 7-8 Type
void UDP_Communicator::encode_packet_type(int type) {
   if(type == DATA_PACKET) {
      out_buffer[6] = '\x55';
      out_buffer[7] = '\x55';
   } else
   {
      out_buffer[6] = '\xAA';
     out_buffer[7] = '\xAA';
   }
}

/** (ent data)
 * Print data (that has been sent by this host) in dark yellow.
 * @param input the string to print
 */
void UDP_Communicator::print_sent(std::string input) { // Print sent data in dark yellow
   std::cout << "\033[33m" << input << "\033[0m";
   std::cout.flush();
}

/**(received data)
 * Print data (that has been received by this host) in green.
 * @param input the string to print
 */
void UDP_Communicator::print_recv(std::string input) { // Print receved data in green
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
void UDP_Communicator::error(std::string input) { // bright red
   std::cout << "\033[91m" << input << "\033[0m" << std::endl;
}

/** (system warnings)
 * Print the string to the console in Bright Yellow
 * @param input the string to print
 */
void UDP_Communicator::warning(std::string input) { // bright yellow
   std::cout << "\033[93m" << input << "\033[0m"<< std::endl;
}

/** (information messages)
 * Print the string to the console in Cyan
 * @param input the string to print
 */
void UDP_Communicator::info(std::string input) {
   std::cout << "\033[36m" << input << "\033[0m"<< std::endl;
}
