//
// Created by smdupor on 6/25/21.
//


#include "MftpServer.h"

MftpServer::MftpServer(std::string &file_path, std::string &logfile, int port, bool verbose, float loss_probability) {
   log = logfile;
   debug = verbose;
   start_time = (const time_t) std::time(nullptr);
   system_on = true;
   system_port = port;
   seq_num = 0;
   ack_num = 0;

   inbound_socket = create_inbound_UDP_socket(port);

   remote_sock_addr = new sockaddr_in;
   bzero((char *) remote_sock_addr, sizeof(*remote_sock_addr));
}

/**
 * System destructor.
 */
MftpServer::~MftpServer() {
   free(remote_sock_addr);
}

/** Server0-sde
 */
void MftpServer::start() {
   local_time_logs.push_back(LogItem(0));

}

void MftpServer::rdt_receive() {

   int sockfd = inbound_socket;

   struct sockaddr_in cli_addr;
   bzero(&cli_addr, sizeof(cli_addr));
   socklen_t length = sizeof(*remote_sock_addr);

   while(true) {
      bzero(in_buffer, MSG_LEN);
      int n = recvfrom(sockfd, (char *) in_buffer, MSG_LEN, 0, (struct sockaddr *) &*remote_sock_addr, &length);
      in_buffer[n] = '\0';

      error(std::string(in_buffer+8));

      if(valid_seq_num(in_buffer) && valid_checksum(in_buffer) && valid_pkt_type(in_buffer) && probability_not_dropped())
      {
         // write and ack
         //encode_packet_type(ACK, buffer);
        // encode_seq_num(current_ack_num, buffer);
      }

      //sendto(sockfd, out_msg.c_str(), strlen(out_msg.c_str()), 0, (const struct sockaddr *) &*remote_sock_addr, length);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      error(std::to_string(decode_seq_num()));
      if(decode_seq_num() >= (uint32_t ) 49)
         break;
   }

   close(sockfd);
}

bool MftpServer::valid_seq_num(char *buffer) {
 return  true;
}

bool MftpServer::valid_checksum(char *buffer) {
   return true;
}

bool MftpServer::valid_pkt_type(char *buffer) {
   return true;
}

bool MftpServer::probability_not_dropped() {
   return true;
}