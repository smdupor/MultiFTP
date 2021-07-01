/**
 * MftpServer.cpp class inherits all member functions from the UDP_Communicator superclass, and implements the rdt_receive()
 * (Reliable Data Transfer Receive) API which receives packets from a remote client, checksums the packets for
 * validity and in-orderness, and returns ACKs to the client when appropriate. The class also implements a
 * probabilistic loss service to simulate lossy connections for performance experiments.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include "MftpServer.h"

/**
 * Server constructor. Create and initialize communication constants, ports, and start time logging
 *
 * @param file_path Path to output file that will be written
 * @param logfile Path to the time log CSV file
 * @param port that this server shall bind to for incoming connections
 * @param verbose switch to enable verbose terminal output
 * @param loss_probability float value 0 < loss_probability < 1 indicating the probability that any given packet shall
 *         be artificially "lost" by this server.
 */
MftpServer::MftpServer(std::string &file_path, std::string &logfile, int port, bool verbose, float loss_probability) {
   // Counters initialization
   seq_num = 0;
   ack_num = 1;
   loss_count = 0;
   packet_count = 0;
   bytes_written = 0;

   // Probabilistic initialization
   srand(getpid() * getpid() * std::time(nullptr));
   this->loss_probability = std::roundf(loss_probability * 10000);

   // Socket init
   remote_sock_addr = new sockaddr_in;
   bzero((char *) remote_sock_addr, sizeof(*remote_sock_addr));
   inbound_socket = create_bound_UDP_socket(port);

   // Files and debug init
   filename = file_path;
   log = logfile;
   debug = verbose;
}

/**
 * System destructor.
 */
MftpServer::~MftpServer() {
   free(remote_sock_addr);
}

/**
 * Reliable data transfer Protocol receive component implementation. Receives packets from a remote host, processes
 * them for validity based on sequence number, checksum, and probabilistic loss. If valid, ACKs packet and writes data
 * to disk, otherwise, drops packet.
 */
void MftpServer::rdt_receive() {
   // Initialize socket and output file
   int sockfd = inbound_socket;
   std::ofstream fd(filename, std::ios_base::binary);

   // Initialize remote client address struct
   struct sockaddr_in cli_addr;
   bzero(&cli_addr, sizeof(cli_addr));
   socklen_t length = sizeof(*remote_sock_addr);

   // Write a timepoint to note experiment start time
   local_time_logs.emplace_back(LogItem());

   // Read packets until we get a FIN packet indicating the client is closing the connection
   while (true) {
      bzero(in_buffer, MSG_LEN);
      int n = recvfrom(sockfd, (char *) in_buffer, MSG_LEN, 0, (struct sockaddr *) &*remote_sock_addr, &length);

      if (n > 0) {
         // We have received a Close-Connection packet; Run a system report to console and exit
         if (decode_packet_type() == FIN) {
            system_report();
            break;
         }

         // We have received another type of packet, examine for validity
         if (valid_seq_num() && valid_checksum() && valid_data_pkt_type() && probability_not_dropped()) {
            // Write the data
            fd.write(in_buffer + 8, n - 8);
            bytes_written += n - 8;

            // ACK the packet
            bzero(out_buffer, MSG_LEN);
            encode_packet_type(ACK);
            encode_seq_num(ack_num);
            sendto(sockfd, out_buffer, 8, 0, (const struct sockaddr *) &*remote_sock_addr, length);

            // Report to the terminal if we've received a multiple of 1 MiB of data (progress report)
            if(bytes_written % 1048576 < n && seq_num > 2)
               info(std::to_string(bytes_written / 1000000) + " MiB received.");

            // Update ack, sequence number for communication, and packet count for system reports
            ++ack_num;
            ++seq_num;
            ++packet_count;
         }
      }
   }

   // Close the file and socket and exit
   fd.close();
   close(sockfd);
}

/**
 * Determine if the sequence number of the packet in the input buffer is the next one we want to receive
 * @return true if this packet is the one we are waiting for, false otherwise (eg duplicate or out-of-order packet)
 */
bool MftpServer::valid_seq_num() {
   if (decode_seq_num() == seq_num)
      return true;
   return false;
}

/**
 * Call to superclass to compute the checksum of the packet in the input buffer; Compare checksum to the checksum
 * we received from the transmitter.
 * @return true if checksum indicates no errors, false if checksum indicates errors
 */
bool MftpServer::valid_checksum() {
   // Checksum the input buffer
   uint16_t checksum = decode_checksum();

   // Copy the checksums we received from the sender
   char a = checksum >> 8;
   char b = checksum;

   // Confirm match
   if (in_buffer[4] == a && in_buffer[5] == b)
      return true;
   error("invalid checksum");
   return false;
}

/**
 * Confirm that the packet type field of the packet in the input buffer is a DATA_PACKET
 * @return true if this is a DATA_PACKET, false otherwise
 */
bool MftpServer::valid_data_pkt_type() {
   if (decode_packet_type() == DATA_PACKET)
      return true;
   error("Invalid packet type");
   return false;
}

/**
 * Probabilistic loss generator. Compute a random number in the range of 0 - 10000 (resolution: hundreths of one percent)
 * and, if the random number is less than the configured probability percentage, indicate to the caller that this
 * packet should be artifically dropped.
 * @return true if packet should be kept, false if packet should be dropped
 */
bool MftpServer::probability_not_dropped() {
   if (rand() % 10000 < loss_probability) {
      error("Packet loss, sequence number = " + std::to_string(decode_seq_num()));
      ++loss_count;
      return false;
   }
   return true;
}

/**
 * Utility method that prints a transfer report to the terminal upon exit with statistics related to the transfer.
 */
void MftpServer::system_report() {
   double percentage = (double) loss_count / (double) packet_count;
   warning(" * * * * * * * * * * * * * * * * * * SYSTEM REPORT  * * * * * * * * * * * * * * * * * * ");
   warning("              Local Packets Received Successfully  : " + std::to_string(packet_count));
   warning("              Packets Probabilistically Dropped    : " + std::to_string(loss_count));
   warning("              Local Configured Loss Rate           : " + std::to_string((float) loss_probability / 10000));
   warning("              Local Effective Loss Rate            : " + std::to_string(percentage));
   warning(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * ");
}