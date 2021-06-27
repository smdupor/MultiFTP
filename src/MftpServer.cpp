//
// Created by smdupor on 6/25/21.
//


#include "MftpServer.h"

MftpServer::MftpServer(std::string &file_path, std::string &logfile, int port, bool verbose, float loss_probability) {
   log = logfile;
   debug = verbose;
   system_port = port;
   seq_num = 0;
   ack_num = 1;
   srand(getpid() * getpid() * std::time(nullptr));
   inbound_socket = create_inbound_UDP_socket(port);
   loss_count = 0;
   packet_count = 0;
   this->loss_probability = std::roundf(loss_probability * 10000);
   remote_sock_addr = new sockaddr_in;
   bzero((char *) remote_sock_addr, sizeof(*remote_sock_addr));
   filename = file_path;
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
   local_time_logs.emplace_back(LogItem());
}

void MftpServer::rdt_receive() {
   int sockfd = inbound_socket;
   std::ofstream fd(filename, std::ios_base::binary);

   struct sockaddr_in cli_addr;
   bzero(&cli_addr, sizeof(cli_addr));
   socklen_t length = sizeof(*remote_sock_addr);

   while (true) {
      bzero(in_buffer, MSG_LEN);
      int n = recvfrom(sockfd, (char *) in_buffer, MSG_LEN, 0, (struct sockaddr *) &*remote_sock_addr, &length);
      if (n > 0) {
         if (decode_packet_type() == FIN) {
            system_report();
            break;
         }

         if (valid_seq_num() && valid_checksum() && valid_pkt_type() && probability_not_dropped()) {
            // write and ack
            fd.write(in_buffer + 8, n - 8);
            bzero(out_buffer, MSG_LEN);
            encode_packet_type(ACK);
            encode_seq_num(ack_num);
            sendto(sockfd, out_buffer, 8, 0, (const struct sockaddr *) &*remote_sock_addr, length);
            ++ack_num;
            ++seq_num;
            ++packet_count;
         }
      }
   }
   fd.close();
   close(sockfd);
}

bool MftpServer::valid_seq_num() {
   if (decode_seq_num() == seq_num)
      return true;
   return false;
}

bool MftpServer::valid_checksum() {
   uint16_t checksum = decode_checksum();
   char a = checksum >> 8;
   char b = checksum;

   if (in_buffer[4] == a && in_buffer[5] == b)
      return true;
   error("invalid checksum");
   return false;
}

bool MftpServer::valid_pkt_type() {
   if (decode_packet_type() == DATA_PACKET)
      return true;
   error("Invalid packet type");
   return false;
}

bool MftpServer::probability_not_dropped() {
   if (rand() % 10000 < loss_probability) {
      error("Packet loss, sequence number = " + std::to_string(decode_seq_num()));
      ++loss_count;
      return false;
   }
   return true;
}

void MftpServer::system_report() {
   double percentage = (double) loss_count / (double) packet_count;
   warning(" * * * * * * * * * * * * * * * * * * SYSTEM REPORT  * * * * * * * * * * * * * * * * * * ");
   warning("              Local Packets Received Successfully  : " + std::to_string(packet_count));
   warning("              Local Packets Lost                   : " + std::to_string(loss_count));
   warning("              Local Configured Loss Rate           : " + std::to_string((float) loss_probability / 10000));
   warning("              Local Effective Loss Rate            : " + std::to_string(percentage));
   warning(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * ");
}