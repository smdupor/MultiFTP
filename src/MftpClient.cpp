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
std::mutex packet_lock;

/** System constructor.
 *
 * @param addr_reg_serv The address of the registration server
 * @param logfile The path to the CSV log file used to store timing data for running latency experiments
 * @param verbose Turn on or off verbose mode, where more CLI interaction is printed when on.
 */
MftpClient::MftpClient(std::list<std::string> &remote_server_list, std::string &logfile, int port, bool verbose) {
   log = logfile;
   debug = verbose;
   start_time = (const time_t) std::time(nullptr);
   system_on = true;
   system_port = port;

   //inbound_socket = create_inbound_UDP_socket(port);

   for(std::string &serv : remote_server_list) {
      struct sockaddr_in remote_addr;
      bzero((char *) &remote_addr, sizeof(remote_addr));

      remote_addr.sin_family = AF_INET;
      remote_addr.sin_addr.s_addr = inet_addr(serv.c_str());
      remote_addr.sin_port = port;

      remote_hosts.push_back(RemoteClient((sockaddr *) &remote_addr));
   }
}

/**
 * System destructor.
 */
MftpClient::~MftpClient() {

}

/** Initialization method that registers with the Reg. Server to determine public-facing IP and finishes setting up
 * the Client.
 * @param config_file Path to the configuration file
 */
void MftpClient::start() {
   local_time_logs.push_back(LogItem(0));

   int sockfd = create_inbound_UDP_socket(system_port);

   char buffer[1024];
   bzero(buffer, 1024);

   std::string out_msg = "Testing a remote msg";

   int n = recvfrom(sockfd, (char *) buffer, 1024, MSG_WAITALL, (struct sockaddr *) remote_hosts.begin()->address,
           (socklen_t *)sizeof(remote_hosts.begin()->address) );

   buffer[n] = '\0';
   std::cout << buffer <<std::endl;
   out_msg = "Sending a reply msg";
   sendto(sockfd, out_msg.c_str(), 1024, MSG_CONFIRM, (const struct sockaddr *) remote_hosts.begin()->address,
           (socklen_t) sizeof(remote_hosts.begin()->address) );
   close(sockfd);
}

void MftpClient::start_reversed() {
   local_time_logs.push_back(LogItem(0));

   int sockfd = create_inbound_UDP_socket(system_port);

   char buffer[1024];
   bzero(buffer, 1024);

   std::string out_msg = "Testing a backward remote msg";

   out_msg = "Sending a mesage";
   sendto(sockfd, out_msg.c_str(), 1024, MSG_CONFIRM, (const struct sockaddr *) remote_hosts.begin()->address,
          (socklen_t) sizeof(remote_hosts.begin()->address) );

   int n = recvfrom(sockfd, (char *) buffer, 1024, MSG_WAITALL, (struct sockaddr *) remote_hosts.begin()->address,
                    (socklen_t *)sizeof(remote_hosts.begin()->address) );

   buffer[n] = '\0';
   std::cout << buffer<<std::endl;

   close(sockfd);
}


/**
 * Load and parse each file to ensure it's valid, and to record the length in bytes.
 */
inline void MftpClient::check_files() {

      std::ifstream infile("asdf");
      char buffer[MSG_LEN];
      int bytecount = 0;
      // Read continuously and count the bytes
      bzero(buffer, MSG_LEN);
      while (infile.getline(buffer, MSG_LEN)) {
         buffer[strlen(buffer)] = '\n';
         bytecount += strlen(buffer);
         bzero(buffer, MSG_LEN);
      }
      infile.close();

}



/**
 * Continuous timing loop to handle all regularly-scheduled timing activities including:
 * My own Host TTL, my file list item's TTLs, and whether a remote host that's marked inactive is scheduled to be dropped
 * from my peer database.
 *
 * Contacts registration server with a KeepAlive when timeout is met.
 *
 * THREADING: Runs in a single detached thread that starts with initial boot-up and runs until shutdown.
 *//*
void MftpClient::keep_alive() {
   int client_ttl_counter = 0, file_ttl_counter = 0;

   while (system_on) {
      // Every second, increment the counters
      std::this_thread::sleep_for(std::chrono::seconds(1));
      ++client_ttl_counter;
      ++file_ttl_counter;

      // If I need to send a keepAlive, contact the registration server and do so.
      if (client_ttl_counter >= kKeepAliveTimeout) {
         regserv_lock.lock();
         int sockfd = outgoing_connection(reg_serv, kControlPort);
         if (sockfd >= 0) {
            std::string outgoing_message = kKeepAlive + " Cookie: " + std::to_string(cookie) + " \n\n";
            transmit(sockfd, outgoing_message);
            std::string incoming_message = receive(sockfd); // swallow the ack
            close(sockfd);
            client_ttl_counter = 0;
         }
         regserv_lock.unlock();
      }

      // If it's time to update the file KeepAlives, do so.
      if (file_ttl_counter >= kFileKeepAliveTimeout) {
         for (FileEntry &f : files) {
            if (f.get_hostname() != this->hostname)
               f.decrement_ttl();
         }
         file_ttl_counter = 0;
      }
      // For any peers that have been marked inactive, increment their downtime counter.
      for (PeerNode &p : peers) {
         if (!p.active()) {
            p.increment_drop_counter();
         }
      }

      // For any peers that need to be dropped from the local list, do so.
      client_peerlist_lock.lock();
      peers.remove_if([&](PeerNode &p) { return p.has_drop_counter_expired(); });
      client_peerlist_lock.unlock();
   }
}*/


/**
 * Peer randomization algorithm. In order to align with the project requirements (using a linked list to store Peers)
 * We copy the list thrice, and randomly shuffle the entries as we put them into the new list, then swap in the new list
 * to replace the old.
 *//*
void MftpClient::shuffle_peer_list() {
   for (int i = 0; i < 3; i++) {
      std::list<PeerNode> shuffled_peers;
      for (PeerNode &p : peers) {
         if (rand() % 2 == 1) {
            shuffled_peers.push_front(p);
         } else {
            shuffled_peers.push_back(p);
         }
      }
      std::swap(shuffled_peers, peers);
   }
}*/

/** Randomized search algorithm (replaces std::find_if()) which randomly selects a remote file to be downloaded.
 *
 * @param want_file Iterator that points to the address in the list where the randomly selected remote file is.
 *//*
void MftpClient::find_wanted_file(std::_List_iterator<FileEntry> &want_file) {
   // Run a simple search to count the number of non-locally-available files listed in index entries
   int count_non_local = 0;
   for (FileEntry &f : files) {
      if (!f.is_local()) {
         ++count_non_local;
      }
   }

   // If none found, let the caller know that none were found in the same format as C++11's library std::find_if()
   if (count_non_local == 0) {
      want_file = files.end();
   } else {
      // Otherwise, perform a randomization operation to randomly select a remote file to download, by first picking
      // a random number to iterate to within the range of available items:
      static constexpr double fraction{1.0 / (RAND_MAX + 1.0)};
      int rand_selector = 1 + static_cast<int>((count_non_local) * (std::rand() * fraction));

      // Then iterate through the remote file items in the list rand_selector number of times
      std::_List_iterator<FileEntry> random_iterator = files.begin();
      for (int i = 0; i < rand_selector; i++) {
         random_iterator = std::find_if(random_iterator, files.end(), [&](FileEntry &f) {
            return !f.is_local() && !f.is_locked();
         });
      }
      want_file = random_iterator;
   }
}
*/



/** Timing method to back off the rate at which we poll the registration server when we've been unsuccessful at finding
 * any new files download.
 *
 * @param past_local_qty Set at the top of downloader(), indicates whether we've gotten anything new on this iteration.
 * @param backoff_time: The amount of time to wait when a backoff is necessary in milliseconds.
 *//*
void MftpClient::downloader_backoff(size_t past_local_qty, int &backoff_time) {
   // Nothing new downloaded this iteration
   if (past_local_qty == local_qty) {
      if (backoff_time < 1000) {
         backoff_time *= 2;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_time));
      milliseconds_slept += backoff_time;
      verbose("Nothing new available to download. Waiting for: " + std::to_string(((float) backoff_time) * 0.001) +
              " Seconds.");

      // ADDED FOR ASSIGNMENT SUBMISSION: If 10 seconds have elapsed, ask the user if they are done and want to exit
      if (milliseconds_slept > 12000) {
         ece_573_TA_interaction();
      }
   } else {
      // We just downloaded a new file, reset the backoff window to the shortest setting.
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_time));
      backoff_time = 10;
      milliseconds_slept = 0;

   }
}*/

/**
 * Output the CSV file of timestamps for each file that this client downloaded for timing experiments.
 */
void MftpClient::write_time_log() {
   std::string outgoing_message;

   // Grab the "zero" time and open the file
   LogItem t = *local_time_logs.begin();
   std::ofstream csv_file(log);

   //Output CSV header
   outgoing_message = "Qty, time\n";
   verbose(outgoing_message);
   csv_file.write(outgoing_message.c_str(), outgoing_message.length());

   // Write each entry as a row to the CSV File, subtracting the zero time from (each) time to get elapsed (seconds)
   for (LogItem &l : local_time_logs) {
      outgoing_message = std::to_string(l.qty) + ", " +
                         std::to_string(((float) std::chrono::duration_cast<std::chrono::milliseconds>(
                                 l.time - t.time).count()) / 1000) +
                         "\n";
      csv_file.write(outgoing_message.c_str(), outgoing_message.length());
      verbose(outgoing_message);
   }
   csv_file.close();
}

/**
 * Called when client has decided to leave the system. Mark system_on as false, Contact the registration server to leave,
 * and use callback from the registration server to unblock the create_inbound_UDP_socket in the main() thread.
 */
void MftpClient::shutdown_system() {
   system_on = false;
   warning("****************CLIENT LEAVING SYSTEM****************\n");

   // Sleep for 3 seconds to allow any running downloads to finish gracefully.
   std::this_thread::sleep_for(std::chrono::seconds(3));

   // Write the CSV timestamp file.
   write_time_log();
}

/** Getter for system state
 *
 * @return true when system is still running, false when client has left the system. Used to break create_inbound_UDP_socket loop in main()
 */
bool MftpClient::get_system_on() {
   return system_on;
}
