/**
 * MftpClient.cpp class inherits all member functions from the UDP_Communicator superclass, and implements the rdt_send()
 * (Reliable Data Transfer Send) API which takes a byte stream from a caller, and handles creation, checksumming, and
 * transmission of packets. MftpClient also handles incoming SAW acks, timeouts, and retransmissions as appropriate.
 * This class also handles timepoint measurement for experimental data gathering related to efficiency experiments
 * on the SAW rdt_send/rdt_receive protocol.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */
#include "UDP_Communicator.h"
#include "MftpClient.h"

/**
 * System constructor to initialize the client.
 *
 * @param remote_server_list a list of string hostnames of remote servers
 * @param logfile the CSV file that distribution time points are appended to
 * @param port the port to contact remote servers on
 * @param verbose a flag permitting more terminal output
 * @param max_seg_size the maximum packet payload size in bytes
 */
MftpClient::MftpClient(std::list<std::string> &remote_server_list, std::string &logfile, int port, bool verbose,
                       uint16_t max_seg_size) {
   log = logfile;
   debug = verbose;

   // Communication protocol initialization
   system_port = port;
   seq_num = 0;
   ack_num = 0;
   MSS = max_seg_size;
   byte_index = 0;

   // Timing initialization
   timeout_us = 0;
   EstRTT = 1000000.0; // 1 Second in us
   DevRTT = 1000000.0; // 1 Second in us

   // Reporting counters intitialization
   packet_count = 0;
   loss_count = 0;

   // Zero the input/output buffers
   bzero(out_buffer, MSG_LEN);
   bzero(in_buffer, MSG_LEN);

   // Setup each remote server structure and emplace into remote server list
   for (std::string &serv : remote_server_list) {
      // UDP Setup
      struct sockaddr_in *remote_addr = new sockaddr_in;
      bzero((char *) remote_addr, sizeof(*remote_addr));
      struct hostent *server = gethostbyname(serv.c_str());
      remote_addr->sin_family = AF_INET;
      bcopy((char *) server->h_addr, (char *) &remote_addr->sin_addr.s_addr, server->h_length);
      remote_addr->sin_port = htons(port);

      // Socket setup and create/emplace RemoteHost into remote_hosts list
      int sockfd = create_unbound_UDP_socket(system_port);
      remote_hosts.emplace_back(RemoteHost((sockaddr_in *) remote_addr, sockfd));
   }

   // Log the start time of the transmission
   local_time_logs.emplace_back(LogItem());
}

/**
 * System destructor.
 */
MftpClient::~MftpClient() {
   for (RemoteHost &r : remote_hosts) {
      free(r.address);
   }
}

/**
 * Shut down the client. Signal to the servers that we are done sending our file, and are closing the connections.
 * Log the distrubtion time, and call write_time_log() to output the datapoint to CSV.
 */
void MftpClient::shutdown() {
   // Shorten the MSS to the index that the byte_index pointer is currently set to, so that rdt_send() will transmit
   // the packet even if it's not full. Store the configured MSS to be restored before running system reports.
   uint16_t temp_mss = MSS;
   MSS = byte_index;

   // Send any remaining data in the buffer (the data character ' ' will be discarded)
   rdt_send(' ');

   // Create the FIN close-connection packet
   bzero(out_buffer, MSG_LEN);
   encode_seq_num(seq_num);
   encode_packet_type(FIN);

   // Send the close-connection packet to all servers and close sockets when done.
   for (RemoteHost &r : remote_hosts) {
      sendto(r.sockfd, out_buffer, MSS, 0, (const struct sockaddr *) &*r.address,
             (socklen_t) sizeof(*r.address));
      close(r.sockfd);
   }

   // Log the distribution time, restore the MSS, and write to the CSV log.
   local_time_logs.emplace_back(LogItem());
   MSS = temp_mss;
   write_time_log();
}

/**
 * Implements the client-side portion of the Reliable Data Transfer protocol. Accepts characters from the caller and
 * encapsulates all transmission of data, buffering, ACKs, and timeouts.
 * @param data a character from the caller's byte stream
 */
void MftpClient::rdt_send(char data) {
   // If buffer is not full yet, add character and return
   if (byte_index < MSS) {
      out_buffer[byte_index + 8] = data;
      ++byte_index;
   }
   // Buffer is full, begin formation of packet and transmit
   else {
      // If this is a new transmission, update the expected ACK number.
      if (ack_num == seq_num)
         ++ack_num;

      // Encode the sequence number, compute checksum, and set packet type into packet header in buffer.
      encode_seq_num(seq_num);
      encode_packet_type(DATA_PACKET);
      encode_checksum();

      // Set a timer
      timeout_start = std::chrono::steady_clock::now();

      // Send the packet
      for (RemoteHost &r : remote_hosts) {
         if (r.ack_num == seq_num) {
            sendto(r.sockfd, out_buffer, MSS + 8, 0, (const struct sockaddr *) &*r.address,
                   (socklen_t) sizeof(*r.address));
         }
      }

      // Stop-and-Wait for acks, and handle timer expiry & retransmissions
      SaW_process_acks_retransmissions();

      // Once all acks received, reset buffer, increment sequence number
      byte_index = 0;
      bzero(out_buffer, MSG_LEN);
      ++seq_num;
      ++packet_count;

      // Report to console if we have reached a milestone in MiB transmitted
      if((seq_num * MSS) % 1048576 < MSS && seq_num > 2)
         info(std::to_string((seq_num * MSS) / 1048576) + " MiB transmitted.");

      // Call self ONCE to write the byte we received from the rdt_send() API caller to the buffer
      rdt_send(data);
   }
}

/**
 * Implement the "and Wait" of the stop-and-wait protocol: Check for ACKs from remote hosts and monitor the timeout
 * timer. In case of a timeout before all ACKs received, retransmit to any hosts that have not acked.
 */
void MftpClient::SaW_process_acks_retransmissions() {
   while (!all_acked()) {
      // Check to see if new acks have come in
      for (RemoteHost &r : remote_hosts) {
         if (r.ack_num == seq_num) {
            bzero(in_buffer, MSG_LEN);
            socklen_t length = sizeof(&*r.address);
            int n = recvfrom(r.sockfd, (char *) in_buffer, MSG_LEN, 0, (struct sockaddr *) &*r.address,
                             &length);

            // A packet was received, process the ACK and update the timeout
            if (n > 0) {
               uint32_t temp_seq_num = decode_seq_num();
               if (temp_seq_num == seq_num + 1) {
                  r.ack_num = temp_seq_num;
                  ++r.segment_num;
                  estimate_timeout();
               }
            }
         }
      }

      // If we've hit a timeout condition, report to terminal and retransmit.
      if (((uint_fast64_t) std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                 timeout_start).count()) >= timeout_us) {
         error("Timeout, sequence number = " + std::to_string(seq_num));

         //Reset the timer and increment the loss counter (for reports)
         timeout_start = std::chrono::steady_clock::now();
         ++loss_count;

         // Retransmit the packet to any host that hasn't ACKed
         for (RemoteHost &r : remote_hosts) {
            if (r.ack_num == seq_num) {
               sendto(r.sockfd, out_buffer, MSS + 8, 0, (const struct sockaddr *) &*r.address,
                      (socklen_t) sizeof(*r.address));
            }
         }
      }
   }
}

/**
 * Update the timer expiration values based on the TCP-Timeout estimation algorithm using the Round-Trip Time
 * moving averages EstimatedRTT and DeviationRTT
 */
void MftpClient::estimate_timeout() {
   // Sample the current RTT
   long double SampRTT = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                               timeout_start).count();
   // Compute the estimatedRTT, DevRTT, and timeout
   EstRTT = (0.875 * EstRTT) + (0.125 * SampRTT);
   DevRTT = (0.75 * DevRTT) + (0.25 * std::abs(EstRTT - SampRTT));
   timeout_us = (uint_fast64_t) (EstRTT + (4 * DevRTT));
   verbose("The estimated timeout is (in microsec): " + std::to_string(timeout_us));
}

/**
 * Test whether we have received acks from every remote server for the packet currently in the output buffer
 * @return true if all acks have been received, false if unacked servers
 */
bool MftpClient::all_acked() {
   for (RemoteHost &r : remote_hosts) {
      // Found an un-acked host
      if (r.ack_num == seq_num)
         return false;
   }
   // all acks are received.
   return true;
}

/**
 *  Append a datapoint to the time log CSV file for this transfer, including the number of remote servers,
 *  the maximum segment size, the estimated configured loss percentage, and the distribution time for this transfer
 */
void MftpClient::write_time_log() {
   std::string outgoing_message;

   // Grab the "zero" time and open the log file
   LogItem t = local_time_logs[0];
   std::ofstream csv_file(log, std::ios_base::app);

   //Estimate the configured loss rate (PER SERVER) for recording to the CSV, and round to tenths of a percent for clarity
   double percentage =
           (round(((double) loss_count / (double) packet_count) * 1000) / 1000) / (double) remote_hosts.size();

   // Create the CSV line
   outgoing_message = std::to_string(remote_hosts.size()) + ", " +
                      std::to_string(MSS) + ", " +
                      std::to_string(percentage) + ", " +
                      std::to_string(((float) std::chrono::duration_cast<std::chrono::milliseconds>(
                              local_time_logs[1].time - t.time).count()) / 1000) +
                      "\n";

   // Append the line to the log file
   csv_file.write(outgoing_message.c_str(), outgoing_message.length());
   verbose(outgoing_message);

   // Close the log file and run a system report
   csv_file.close();
   system_report();
}

/**
 * Utility method that prints a transfer report to the terminal upon exit with statistics related to the transfer.
 */
void MftpClient::system_report() {
   double percentage = (double) loss_count / (double) packet_count;
   warning(" * * * * * * * * * * * * * * * * * * SYSTEM REPORT  * * * * * * * * * * * * * * * * * * ");
   warning("                 Successful Packets Transmitted   : " + std::to_string(packet_count));
   warning("                 Number of Timeout Events         : " + std::to_string(loss_count));
   warning("                 Estimated Effective Loss Rate    : " + std::to_string(percentage));
   warning("                 Current Timeout Setting (s)      : " + std::to_string((double)timeout_us / 1000000));
   warning("                 ExpMovingAvg EstimatedRTT (s)    : " + std::to_string(EstRTT / 1000000));
   warning(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  ");
}