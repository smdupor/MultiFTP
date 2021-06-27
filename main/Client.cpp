/**
 * Client.cpp encapsulates the int main() for the MultiFTP Client executable, to handle incoming parameter arguments,
 * reading of a local (binary or text) file, and sending a stream of bytes to rdt_send(). This class also includes an
 * optional argument to repeat the transfer (n) number of times for experimental data gathering.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include <iostream>
#include <thread>

#include "MftpClient.h"

int main(int argc, char *argv[]) {
   std::list<std::string> remotes;
   // Default to one transfer unless we receive instructions to repeat (n) times
   uint8_t repetitions = 1;

   // Handle commandline arguments format: ./Client server-1 server-2 portnum filename MSS r5

   // Pop the 'empty' commandline argument index
   --argc;

   // Optional argument: Repeat experiment r(this) many times. Read and pop argument off the array.
   if (*argv[argc] == 'r') {
      repetitions = atoi(argv[argc] + 1);
      --argc;
   }

   // Configure the time log path (Always appends)
   std::string logfile = "Mftp_time_log.csv";

   // Read the Maximum Segment Size argument and pop it
   uint16_t max_seg = atoi((const char *) argv[argc]);
   --argc;

   // Read the input file name (to be transferred) and pop it
   std::string file_name(argv[argc]);
   --argc;

   // Read the remote port number and pop it
   int port = atoi((const char *) argv[argc]);
   --argc;

   // Confirm we've received the correct quantity of arguments, if not, exit failure
   if (argc == 0) {
      MftpClient::error("Invalid number of arguments passed.");
      return EXIT_FAILURE;
   }

   // The rest of the arguments are an unknown number of remote server hostnames. Read them all and pop each.
   while (argc > 0) {
      remotes.push_front(std::string(argv[argc]));
      --argc;
   }

   // Run the transfer (repetitions) times
   for (uint8_t i = 0; i < repetitions; ++i) {
      char f_in;
      std::ifstream fd(file_name, std::ios_base::binary);
      MftpClient client = MftpClient(remotes, logfile, port, false, max_seg);

      // Stream bytes from the input file to rdt_send()
      while (fd >> std::noskipws >> f_in) {
         client.rdt_send(f_in);
      }

      // Close file and shutdown
      fd.close();
      client.shutdown();

      //Sleep while server resets so we get an accurate startup synchronization
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   }

   //Say goodbye and exit.
   UDP_Communicator::info("***************System is exiting successfully***********\n");
   return EXIT_SUCCESS;
}