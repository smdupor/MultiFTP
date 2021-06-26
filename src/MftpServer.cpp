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
   ack_num = 1;

   inbound_socket = create_inbound_UDP_socket(port);

   this->loss_probability =loss_probability * 100;

   remote_sock_addr = new sockaddr_in;
   bzero((char *) remote_sock_addr, sizeof(*remote_sock_addr));

   filename = file_path;
   //*fd = std::ofstream(file_path);
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
   std::ofstream fd(filename);

   struct sockaddr_in cli_addr;
   bzero(&cli_addr, sizeof(cli_addr));
   socklen_t length = sizeof(*remote_sock_addr);

   while(true) {
      bzero(in_buffer, MSG_LEN);
      int n = recvfrom(sockfd, (char *) in_buffer, MSG_LEN, 0, (struct sockaddr *) &*remote_sock_addr, &length);
      //in_buffer[n] = '\0';
      if(decode_seq_num() >= (uint32_t ) 0xFFFFFFFE)
         break;

      if(valid_seq_num() && valid_checksum() && valid_pkt_type() && probability_not_dropped())
      {
         // write and ack
         warning(in_buffer+8);
         fd.write(in_buffer+8, strlen(in_buffer+8));
         bzero(out_buffer, MSG_LEN);
         encode_packet_type(ACK);
         encode_seq_num(ack_num);
         sendto(sockfd, out_buffer, 8, 0, (const struct sockaddr *) &*remote_sock_addr, length);
         ++ack_num;
         ++seq_num;
      }

      //sendto(sockfd, out_msg.c_str(), strlen(out_msg.c_str()), 0, (const struct sockaddr *) &*remote_sock_addr, length);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

   }
   fd.close();

   close(sockfd);
}

bool MftpServer::valid_seq_num() {
   if(decode_seq_num() == seq_num)
      return  true;
   return false;
}

bool MftpServer::valid_checksum() {
   uint16_t checksum = decode_checksum();
   char a = checksum >> 8;
   char b = checksum;

   if(in_buffer[4] == a && in_buffer[5] == b)
      return true;
   return false;
}

bool MftpServer::valid_pkt_type() {
   if(decode_packet_type() == DATA_PACKET)
      return true;
   return false;
}

bool MftpServer::probability_not_dropped() {
   if(rand() % 100 < loss_probability)
      return false;
   return true;

}