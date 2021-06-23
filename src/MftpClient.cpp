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

#include "NetworkCommunicator.h"
#include "MftpClient.h"

// Mutex locks to control access to the entire downloader flow, the files index, outgoing registration server contact,
// and the local peer list.
std::mutex downloader_lock, files_lock, regserv_lock, client_peerlist_lock;

/** System constructor.
 *
 * @param addr_reg_serv The address of the registration server
 * @param logfile The path to the CSV log file used to store timing data for running latency experiments
 * @param verbose Turn on or off verbose mode, where more CLI interaction is printed when on.
 */
MftpClient::MftpClient(std::string &addr_reg_serv, std::string &logfile, bool verbose) {
   reg_serv = (const char *) addr_reg_serv.c_str();
   log = logfile;
   lock = false;
   debug = verbose;
   start_time = (const time_t) std::time(nullptr);
   cookie = -1;
   system_on = true;
   milliseconds_slept = 0;
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
void MftpClient::start(std::string config_file) {
   // Connect to the Registration Server and Register (Establish external IP)
   int sockfd = outgoing_connection(reg_serv, kControlPort);
   contact_registration_server(sockfd, true);
   close(sockfd);

   // Take both file and downloader mutexes while we write in the incoming data.
   downloader_lock.lock();
   files_lock.lock();
   // Load our configuration file, check validity and size of files
   parse_config(config_file);
   check_files();
   // Release the locks
   downloader_lock.unlock();
   files_lock.unlock();

   // Record (now) as the local start-up time, and first entry in the logs list.
   local_time_logs.push_back(LogItem(local_qty));
}

/** Read the configuration file, and set up the client accordingly.
 *
 * @param config_file Path to configuration file
 */
inline void MftpClient::parse_config(std::string config_file) {
   char buffer[512];
   std::vector<std::string> tokens;
   std::string local_file;
   std::ifstream file(config_file.c_str());

   if (file) {
      // How many files we want to download
      bzero(buffer, 512);
      file.getline(buffer, 512);
      tokens = split(std::string(buffer), '=');
      this->expected_qty = stoi(tokens[1]);

      // How many files are stored locally
      bzero(buffer, 512);
      file.getline(buffer, 512);
      tokens = split(std::string(buffer), '=');
      this->local_qty = stoi(tokens[1]);

      // How many files will be available across the system (which will trigger a clean shutdown)
      bzero(buffer, 512);
      file.getline(buffer, 512);
      tokens = split(std::string(buffer), '=');
      this->system_wide_qty = stoi(tokens[1]);

      // Get the prefix for the download directory
      bzero(buffer, 512);
      file.getline(buffer, 512);
      tokens = split(std::string(buffer), '=');
      path_prefix = std::string(tokens[1]);

      // Get each filename that we want to share and have available locally
      for (size_t i = 0; i < local_qty; ++i) {
         bzero(buffer, 512);
         file.getline(buffer, 512);
         local_file = std::string(path_prefix + std::string(buffer));
         files.push_back(FileEntry(stoi(std::string(buffer).substr(3, 4)), hostname, cookie,
                                   local_file, true, kInitialTTL));
      }
   } else {
      error("*****************************PROBLEM WITH CONFIGURATION FILE.***************************");
      error("The system has likely been started from an incorrect working directory. ");
      system_on = false;
   }
   file.close();
}
/**
 * Load and parse each file to ensure it's valid, and to record the length in bytes.
 */
inline void MftpClient::check_files() {
   for (FileEntry &file : files) {
      std::ifstream infile(file.get_path());
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
      // Update this entry with the count
      file.set_length(bytecount);
      // Allow the file to be transmitted to other clients
      file.clear_lock();
   }
}


/** Handle all Setup and query communication with the registration server, including registering and getting peer list.
 *
 * @param sockfd Socket descriptor
 * @param registration Whether we are registering or requesting a peer list. True = registering, false = request peers.
 */
inline void MftpClient::contact_registration_server(int sockfd, bool registration) {
   std::string in_message, out_message;
   // Take control of mutex lock for peer list and downloading (contacting of peers)
   client_peerlist_lock.lock();
   downloader_lock.lock();

   if (registration && cookie == -1) { // This is a new registration (joining the system)
      out_message = kCliRegister + " NEW \n\n";
      transmit(sockfd, out_message);
   } else if (registration) { // This is a returning registration (joining the system)
      out_message = kCliRegister + " Cookie: " + std::to_string(cookie) + " \n\n";
      transmit(sockfd, out_message);
   } else { //This client is already registered and we are requesting the peer list
      out_message = kGetPeerList + " " + "Cookie: " + std::to_string(cookie) + " \n\n";
      transmit(sockfd, out_message);
   }

   // Receive response, split into messages, and parse
   in_message = receive(sockfd);
   std::vector<std::string> messages = split((const std::string &) in_message, '\n');
   for (std::string &message : messages) {
      // Split into tokens
      std::vector<std::string> tokens = split((const std::string &) message, ' ');

      if (tokens[CONTROL] == kCliRegAck) { // Welcome to system, here is your ID info (and your external IP)
         hostname = tokens[HOSTNAME];
         cookie = stoi(tokens[COOKIE]);
         this->port = stoi(tokens[PORT]);
         this->ttl = stoi(tokens[TTL]);
      } else if (tokens[CONTROL] == kPeerListItem) { // We have been sent a peer list node item
         // IF this peer item is not me based on my cookie,
         if (this->cookie != stoi(tokens[COOKIE])) {
            // Search to see if we have this peer in the local list
            auto p = std::find_if(peers.begin(), peers.end(), [&](PeerNode node) {
               return node.equals(stoi(tokens[COOKIE]));
            });

            // We don't have the peer in our local list, and it's an active host, so add it
            if (p == peers.end() && tokens[ACTIVE] == "TRUE") {
               peers.push_back(PeerNode(tokens[HOSTNAME], stoi(tokens[COOKIE]), stoi(tokens[PORT]), stoi(tokens[TTL])));
               lock = false;
            } else if (tokens[ACTIVE] == "TRUE") { // We have the peer, and it's active now.
               p->set_active(stoi(tokens[TTL]));
            } else if (tokens[ACTIVE] ==
                       "FALSE") { // We have the peer, and it's inactive now, so needs to be marked for dropping.
               p->set_inactive();
               // Go ahead and drop the down peer's file entries from the database
               files.remove_if([&](FileEntry &f) { return f.equals(*p); });
            }
         }
      } else if (tokens[CONTROL] == kDone) { // List is finished downloading. Return mutex and control flow to caller.
         downloader_lock.unlock();
         client_peerlist_lock.unlock();
         return;
      }
   }
}

/**
 * Continuous timing loop to handle all regularly-scheduled timing activities including:
 * My own Host TTL, my file list item's TTLs, and whether a remote host that's marked inactive is scheduled to be dropped
 * from my peer database.
 *
 * Contacts registration server with a KeepAlive when timeout is met.
 *
 * THREADING: Runs in a single detached thread that starts with initial boot-up and runs until shutdown.
 */
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
}

/** Accept an incoming connection from another peer to download either the distributed database or a file.
 *
 * THREADING: Runs in a detached thread that is spawned with every accepted request. There may be many of these threads
 * at any given time, and they are short-lived -- only as long as a single download action.
 *
 * @param sockfd Socket descriptor
 */
void MftpClient::accept_download_request(int sockfd) {
   std::string remote_addr, out_message, in_message;
   std::vector<std::string> messages, tokens;

   in_message = receive(sockfd);

   messages = split((const std::string &) in_message, '\n');

   for (std::string &message : messages) {
      tokens = split(message, ' ');

      // Client is requesting the index, so take the files lock and send it to them.
      if (tokens[CONTROL] == kGetIndex) {
         files_lock.lock();
         for (FileEntry file : files) {
            out_message = kIndexItem + file.to_msg();
            transmit(sockfd, out_message);
         }
         out_message = kDone + " \n\n";
         transmit(sockfd, out_message);
         // Release the files lock and sleep this thread while the client processes and closes.
         files_lock.unlock();
         std::this_thread::sleep_for(std::chrono::seconds(2));
         return;
      }
         // Client is requesting a file download; Find it and call transmit_file
      else if (tokens[CONTROL] == kGetFile) {
         auto want_file = *std::find_if(files.begin(), files.end(), [&](FileEntry &f) {
            return f.equals(stoi(tokens[FILEID]), this->cookie);
         });
         transmit_file(sockfd, want_file);
      }
   }
}
/** Handle the transmission of a requested file to a client.
 * THREADING: Part of the detached connection acceptance thread, called by accept_download_request()
 *
 * @param sockfd Socket descriptor
 * @param want_file Reference to the entry in our local database for the file to be sent
 */
inline void MftpClient::transmit_file(int sockfd, FileEntry &want_file) {
   std::string out_message;
   // Open the file on the hard drive.
   std::ifstream infile(want_file.get_path());

   // Send the client the control character and number of bytes of payload to expect.
   out_message = kFileLine + " Length: " + std::to_string(want_file.get_length()) + " " + "\n";
   transmit(sockfd, out_message);

   // Read the file and transmit it
   char buffer[MSG_LEN * 2];
   bzero(buffer, MSG_LEN * 2);
   while (infile.getline(buffer, MSG_LEN * 2)) {
      // Add back the newline stripped by getline()
      buffer[strlen(buffer)] = '\n';
      out_message = std::string(buffer);
      transmit(sockfd, out_message);
      bzero(buffer, MSG_LEN * 2);
   }
   infile.close();
   want_file.clear_lock(); // Clear database lock
   /******************************************************************************************************************/
   /************ ADDED FOR SUBMISSION REQUIREMENTS (Leave after xmit one file). REMOVE FOR NORMAL OPERATION***********/
   shutdown_system(); // REMOVE THIS COMMAND FOR NORMAL OPERATION
   /******************************** ADDED FOR SUBMISSION REQUIREMENTS. REMOVE FOR NORMAL OPERATION*******************/
   /******************************************************************************************************************/
}

/**
 * The "Client" component of the P2P Host. Contact the registration server to get the latest peers, contact the peers
 * to get their database, initiate download requests, and handle leaving the system. Runs in main thread for the life
 * of the execution.
 */
void MftpClient::downloader() {
   std::string remote_addr, outgoing_message, incoming_message;
   std::vector<std::string> messages, tokens;
   std::list<FileEntry>::iterator want_file;
   size_t past_local_qty;
   int backoff_time = 10;

   // Run Downloads as long as we don't have all the files we want
   while (system_on) {
      past_local_qty = local_qty; // Used later to determine if we have downloaded a new file

      // Contact the registration server and get the list of peers
      regserv_lock.lock();
      int sockfd = outgoing_connection(reg_serv, kControlPort);
      if (sockfd >= 0) {
         contact_registration_server(sockfd, false);
      }
      close(sockfd);
      regserv_lock.unlock();

      // Take control of the downloader lock and search to see if there are files to download.
      downloader_lock.lock();
      if (system_on) {
         auto want_file = files.begin();
         find_wanted_file(want_file);

         // If our list is entirely local files. Update the database with info from other peers
         if (want_file == files.end())
            update_database(want_file);

         // We've found something to download (or not), proceed to attempt to download.
         download_file(want_file);
         downloader_lock.unlock();
      }
      // Call the backoff which determines if we should backoff our request speed to prevent network flooding.
      downloader_backoff(past_local_qty, backoff_time);

      // If all the peers across the entire ecosystem have finished propagating the files, shutdown (leave) the system.
      if (system_wide_qty == files.size()) {
         shutdown_system();
      }

   }
}

/** Query other system peers to determine if they have files that we want to download.
 *
 * @param want_file Iterator reference used to indicate whether we've found anything to download
 *                  (same format as std::find_if()
 */
void MftpClient::update_database(std::_List_iterator<FileEntry> &want_file) {
   std::string outgoing_message, incoming_message;
   std::vector<std::string> messages, tokens;

   // Run the peer randomization algorithm so we aren't always contacting the same peer first.
   shuffle_peer_list();

   for (PeerNode &p : peers) {
      if (p.active() && !p.locked()) {
         // Contact each peer until we have new items to download
         int sockfd = outgoing_connection(p.get_address(), p.get_port());
         std::this_thread::sleep_for(std::chrono::milliseconds(5));
         if (sockfd >= 0) {
            //Ask for the remote host's copy of the index
            outgoing_message = kGetIndex + " Cookie: " + std::to_string(cookie) + " \n\n";
            transmit(sockfd, outgoing_message);
            incoming_message = receive(sockfd);


            //Split and process the messages
            messages = split(incoming_message, '\n');
            files_lock.lock();
            for (std::string &message : messages) {
               tokens = split(message, ' ');
               if (tokens[CONTROL] == kIndexItem) {
                  // This is an index item message, check to see if we have it
                  auto file = std::find_if(files.begin(), files.end(), [&](FileEntry &f) {
                     return f.equals(stoi(tokens[FILEID]), stoi(tokens[COOKIE]));
                  });

                  // We don't have it, need to add it
                  if (file == files.end()) {
                     add_file_entry(tokens);
                  }
               }
            }
            files_lock.unlock();
         }
         close(sockfd);
      }
      // Search again to see if there are new files available for download, if so, break and return to downloader().
      find_wanted_file(want_file);
      if (want_file != files.end())
         break;
   }
}

/**
 * Peer randomization algorithm. In order to align with the project requirements (using a linked list to store Peers)
 * We copy the list thrice, and randomly shuffle the entries as we put them into the new list, then swap in the new list
 * to replace the old.
 */
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
}

/** Randomized search algorithm (replaces std::find_if()) which randomly selects a remote file to be downloaded.
 *
 * @param want_file Iterator that points to the address in the list where the randomly selected remote file is.
 */
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

/** Create a new distributed index entry and add it to the local database.
 *  THREADING/LOCKING: The files database modification lock is set by the caller.
 *
 * @param tokens Tokenized version of the message from the remote peer
 */
void MftpClient::add_file_entry(const std::vector<std::string> &tokens) {
   std::string temp_path = path_prefix + "rfc" + tokens[FILEID] + ".txt";
   int temp_id = stoi(tokens[FILEID]);
   int temp_cookie = stoi(tokens[COOKIE]);

   bool temp_local = false;

   // if we can find a copy of this file that is stored locally, then we will note in the DB
   // that this file is also available locally on this machine
   for (FileEntry &temp : files) {
      if (temp.equals(temp_id) && temp.is_local()) {
         temp_local = true;
      }
   }

   files.push_back(FileEntry(temp_id, tokens[HOSTNAME],
                             temp_cookie, temp_path, temp_local, stoi(tokens[TTL])));
}

/** Request a specific file to download from a remote peer, and download it.
 *
 * @param want_file Iterator pointing to the FileEntry for the file we want to download.
 */
inline void MftpClient::download_file(std::list<FileEntry>::iterator &want_file) {
   std::string incoming_message, outgoing_message;
   std::vector<std::string> tokens, messages;

   // Check to ensure we aren't trying to request a "null" file entry and look up the Peer contact info
   if (want_file != files.end()) {
      PeerNode &peer = *std::find_if(peers.begin(), peers.end(), [&](PeerNode &node) {
         return node.equals(want_file->get_cookie());
      });
      peer.lock();

      // ask for the file from the peer
      int sockfd = outgoing_connection(peer.get_address(), peer.get_port());
      outgoing_message = kGetFile + want_file->to_msg() + "\n";
      transmit(sockfd, outgoing_message);

      if(sockfd >= 0){
         // Get back the control packet that tells us the amount of bytes of file payload.
         incoming_message = receive_no_delim(sockfd);
         messages = split(incoming_message, '\n');
         if (messages[0].length() > 0) {
            tokens = split(messages[0], ' ');

            // Set up the file IO stream to write to disk and the bytes counter.
            std::ofstream output_file(want_file->get_path());
            int end_length = stoi(tokens[2]);
            int bytes_written = 0;

            // If we have gotten both the header and some data in the first message (very likely) split, and use the data
            if (messages.size() > 1) {
               print_recv(messages[0]);
               warning("\n\n************ RECEIVING FILE: OUTPUT SUPPRESSED *************\n\n");
               int initial_offset = messages[0].length() + 1;
               incoming_message = incoming_message.substr(initial_offset);
               output_file.write(incoming_message.c_str(), incoming_message.length());
               bytes_written += incoming_message.length();
            }

            // Continue to get file data and write to file until we have the entire file
            while (bytes_written < end_length) {
               incoming_message = receive_no_delim(sockfd);
               output_file.write(incoming_message.c_str(), incoming_message.length());
               bytes_written += incoming_message.length();
            }
            // Close the file and the connection.
            output_file.close();
            want_file->clear_lock();

            close(sockfd);
            peer.unlock();
            // Add an entry for the file to the database
            files.push_back(FileEntry(want_file->get_id(), hostname, cookie, want_file->get_path(), end_length));
         }
         // Update our quantity of files on hard disk.
         ++local_qty;

         // Log the elapsed time for this download
         local_time_logs.push_back(LogItem(local_qty));

         // Check through the database and mark any remote entries of this file as also-locally-available.
         for (FileEntry &f : files) {
            if (f.get_id() == want_file->get_id()) {
               f.set_local();
            }
         }
      } else {
         error("An error occurred during the download step; Likely, a malformed packet was received.");
      }
   }
}

/** Timing method to back off the rate at which we poll the registration server when we've been unsuccessful at finding
 * any new files download.
 *
 * @param past_local_qty Set at the top of downloader(), indicates whether we've gotten anything new on this iteration.
 * @param backoff_time: The amount of time to wait when a backoff is necessary in milliseconds.
 */
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
}

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
 * and use callback from the registration server to unblock the listener in the main() thread.
 */
void MftpClient::shutdown_system() {
   system_on = false;
   warning("****************CLIENT LEAVING SYSTEM****************\n");

   // Tell registration server we're leaving
   regserv_lock.lock();
   int sockfd = outgoing_connection(reg_serv, kControlPort);
   std::string outgoing_message = kLeave + " Cookie: " + std::to_string(cookie) + " \n\n";
   transmit(sockfd, outgoing_message);

   // Sleep until registration server receives our leave
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
   close(sockfd);
   regserv_lock.unlock();

   // Sleep for 3 seconds to allow any running downloads to finish gracefully.
   std::this_thread::sleep_for(std::chrono::seconds(3));

   // Write the CSV timestamp file.
   write_time_log();
}

/** Getter for system state
 *
 * @return true when system is still running, false when client has left the system. Used to break listener loop in main()
 */
bool MftpClient::get_system_on() {
   return system_on;
}

/**
 * Special method for this submission to interact with the user after client B has left, so they can demo the system
 * leaving and shutting down cleanly without ctrl-c.
 */
void MftpClient::ece_573_TA_interaction() {
   warning("**************************  PAUSING *****************************\n\n");
   std::string warn_message = " I noticed that nothing new has become available for download \n"
                              "in 10 seconds. For the purpose of this assignment submission \n"
                              "and your convenience, I have paused the loop that requests peer\n"
                              "and attempts to download files. The threads that accept connections\n"
                              "and handle TTL Keep-alives are still running, so if you wish to \n"
                              "see a KeepAlive message exchange, simply leave me paused for 10 more seconds.\n\n"
                              "To continue downloading, enter 'y', otherwise, to exit the system (and \n"
                              "remotely clean-shutdown the Registration Server), enter 'n'.\n\n"
                              "**************************  PAUSING *****************************\n"
                              " RESUME DOWNLOAD LOOP? (y/n): ";
   warning(warn_message);
   char choice;
   do {
      std::cin >> choice;
      if (choice == 'n')
         shutdown_system();
      else if (choice == 'y')
         milliseconds_slept = 0;
      else
         error("Please enter (y/n) to resume or stop: ");
   } while (choice != 'n' && choice != 'y');
}