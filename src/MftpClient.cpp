/**
 * MftpClient.cpp
 *
 *	Contains the P2P client/server code for the file sharing nodes. Maintains the distributed index, serves files,
 *	and requests files to download from other peers.
 *
 * Subclass of NetworkCommunicator.
 *
 *  Created on: May 31, 2021
 *      Author: smdupor
 */

#include "UDP_Communicator.h"
#include "MftpClient.h"

// Mutex locks to control access to the packet list to prevent data race.
//std::mutex packet_lock;

/** System constructor.
 *
 * @param addr_reg_serv The address of the registration server
 * @param logfile The path to the CSV log file used to store timing data for running latency experiments
 * @param verbose Turn on or off verbose mode, where more CLI interaction is printed when on.
 */
MftpClient::MftpClient(std::list<std::string> &remote_server_list, std::string &logfile, int port, bool verbose,
                       uint16_t max_seg_size) {
   log = logfile;
   debug = verbose;
   start_time = (const time_t) std::time(nullptr);
   system_on = true;
   system_port = port;
   seq_num = 0;
   ack_num = 0;
   MSS = max_seg_size;
   byte_index = 0;
   timeout_us=0;
   EstRTT = 1000000.0; // 1 Second in us
   DevRTT = 1000000.0; // 1 Second in us
   packet_count = 0;
   loss_count = 0;

   bzero(out_buffer, MSG_LEN);
   bzero(in_buffer, MSG_LEN);

   //inbound_socket = create_inbound_UDP_socket(port);

   for(std::string &serv : remote_server_list) {
      struct sockaddr_in *remote_addr = new sockaddr_in;
      bzero((char *) remote_addr, sizeof(*remote_addr));

      struct hostent *server = gethostbyname(serv.c_str());

      remote_addr->sin_family = AF_INET;
      bcopy((char *) server->h_addr, (char *) &remote_addr->sin_addr.s_addr, server->h_length);
      remote_addr->sin_port = htons(port);
      int sockfd = create_outbound_UDP_socket(system_port);
      remote_hosts.push_back(RemoteHost((sockaddr_in *) remote_addr, sockfd));

   }
  // sockfd = create_outbound_UDP_socket(system_port);
}

/**
 * System destructor.
 */
MftpClient::~MftpClient() {
   for(RemoteHost &r : remote_hosts){
      free(r.address);
   }
   //close(sockfd);
}

void MftpClient::shutdown() {
   // Store MSS for restoration after signaling "done".
   uint16_t temp_mss = MSS;
   MSS=byte_index;
   rdt_send(' ');

   bzero(out_buffer, MSG_LEN);
   encode_seq_num(seq_num);
   encode_packet_type(FIN);

   for (RemoteHost &r : remote_hosts) {
      sendto(r.sockfd, out_buffer, MSS, 0, (const struct sockaddr *) &*r.address,
             (socklen_t) sizeof(*r.address));
      close(r.sockfd);
   }
   local_time_logs.push_back(LogItem(1));
   MSS = temp_mss;
   write_time_log();
}


void MftpClient::rdt_send(char data) {
   if(byte_index < MSS){
      out_buffer[byte_index+8] = data;

      // compute checksum so-far
      ++byte_index;
   }
   else { // packet is full of data, transmit
      if(ack_num == seq_num)
         ++ack_num; // We are expecting acks with number, NEXT packet.

      // Compute the values
      encode_seq_num(seq_num);
      encode_packet_type(DATA_PACKET);
      encode_checksum();

      //TODO Set a timer
      timeout_start = std::chrono::steady_clock::now();
      // Send the packet
      for (RemoteHost &r : remote_hosts) {
         // This packet is not yet acked, send
         if(r.ack_num == seq_num) {
            sendto(r.sockfd, out_buffer, MSS + 8, 0, (const struct sockaddr *) &*r.address,
                   (socklen_t) sizeof(*r.address));

         }

      }
      //temp prevent flooding for test
      //std::this_thread::sleep_for(std::chrono::milliseconds(10));
      //warning(out_buffer);
      // Wait for acks while timer unexpired

      while(!all_acked()) {
         // Check to see if new acks have come in
         for(RemoteHost &r : remote_hosts){
            if(r.ack_num == seq_num) {
               bzero(in_buffer, MSG_LEN);
               socklen_t length = sizeof(&*r.address);
               int n = recvfrom(r.sockfd, (char *) in_buffer, MSG_LEN, 0, (struct sockaddr *) &*r.address,
                                &length);
               if(n>0){
                  uint16_t temp = decode_seq_num();
                  if(temp == seq_num+1) {
                     r.ack_num = temp;
                     ++r.segment_num;
                     estimate_timeout();
                  }
               }
            }
         }
         // If we've hit a timeout condition, resend to any hosts that haven't acked
         if(((uint_fast64_t )std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                             timeout_start).count()) >= timeout_us) {
            error("Timeout, sequence number = " + std::to_string(seq_num));
            ++loss_count;
            for (RemoteHost &r : remote_hosts) {
               // This packet is not yet acked from this host, re-send it
               if(r.ack_num == seq_num) {
                  sendto(r.sockfd, out_buffer, MSS + 8, 0, (const struct sockaddr *) &*r.address,
                         (socklen_t) sizeof(*r.address));
               }
            }
         }
      }

      // If all acks, reset buffer, increment seqnum, and call self again to fill first byte
      byte_index = 0;
      bzero(out_buffer, MSG_LEN);
      ++seq_num;
      ++packet_count;
      rdt_send(data);
   }



}

void MftpClient::estimate_timeout() {
   long double SampRTT = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                         timeout_start).count();
   EstRTT = (0.875*EstRTT) + (0.125*SampRTT);
   DevRTT = (0.75*DevRTT) + (0.25* abs(EstRTT - SampRTT));
   timeout_us = (uint_fast64_t) (EstRTT + (4*DevRTT));
   verbose("The estimated timeout is (in microsecs): " + std::to_string(timeout_us));
}


bool MftpClient::all_acked() {
   for(RemoteHost &r : remote_hosts){
      // If this host's most recent ack is equal to the segment we're working on
      if(r.ack_num == seq_num)
         return false;
   }
   // all acks are received.
   return true;
}

// RUNS ON "client"
void MftpClient::start() {
   local_time_logs.push_back(LogItem(0));


}

/**
 * Output the CSV file of timestamps for each file that this client downloaded for timing experiments.
 */
void MftpClient::write_time_log() {
   std::string outgoing_message;

   // Grab the "zero" time and open the file
   LogItem t = local_time_logs[0];
   std::ofstream csv_file(log, std::ios_base::app);

      //Estimate the configured loss rate for recording to the CSV, and round to tenths of a percent for clarity
      double percentage = (round(((double) loss_count / (double) packet_count)*1000)/1000) / (double) remote_hosts.size();

      outgoing_message = std::to_string(remote_hosts.size()) + ", " +
                         std::to_string(MSS) + ", " +
                         std::to_string(percentage) + ", " +
                         std::to_string(((float) std::chrono::duration_cast<std::chrono::milliseconds>(
                                 local_time_logs[1].time - t.time).count()) / 1000) +
                         "\n";
      csv_file.write(outgoing_message.c_str(), outgoing_message.length());
      verbose(outgoing_message);

   csv_file.close();
   system_report();
}


/** Getter for system state
 *
 * @return true when system is still running, false when client has left the system. Used to break create_inbound_UDP_socket loop in main()
 */
bool MftpClient::get_system_on() {
   return system_on;
}

void MftpClient::system_report() {
   double percentage = (double) loss_count / (double) packet_count;
   warning(" * * * * * * * * * * * * * * * * * * SYSTEM REPORT  * * * * * * * * * * * * * * * * * * ");
   warning( "                 Successful Packets Transmitted   : " + std::to_string(packet_count));
   warning( "                 System-Wide Packets Lost         : " + std::to_string(loss_count));
   warning( "                 System-Wide Effective Loss Rate  : " + std::to_string(percentage));
   warning(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * ");
}