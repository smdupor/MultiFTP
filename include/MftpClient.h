/**
 * MftpClient.h class inherits all member functions from the UDP_Communicator superclass, and implements the rdt_send()
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

#ifndef INCLUDE_MFTPCLIENT_H_
#define INCLUDE_MFTPCLIENT_H_

#include "UDP_Communicator.h"

class MftpClient : public UDP_Communicator {

private:
   //Communication Variables and methods
   std::vector<RemoteHost> remote_hosts;
   uint16_t MSS, byte_index;
   int system_port;
   bool all_acked();
   void estimate_timeout();

   // Timing variables
   std::chrono::time_point<std::chrono::steady_clock> timeout_start;
   uint_fast64_t timeout_us;
   long double EstRTT, DevRTT;

   // Utility variables and methods
   std::vector<LogItem> local_time_logs;
   uint_fast32_t loss_count;
   uint_fast64_t packet_count;
   void system_report();
   void write_time_log();

public:
   MftpClient(std::list<std::string> &server_list, std::string &logfile, int port, bool verbose,
              uint16_t max_seg_size);
   ~MftpClient() override;
   void rdt_send(char data);
   void SaW_process_acks_retransmissions();

   void shutdown();

};

#endif /* INCLUDE_MFTPCLIENT_H_ */
