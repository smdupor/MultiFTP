/**
 *
 * Created on: June 23th, 2021
 * Author: smdupor
 */

#include <iostream>
#include <thread>

#include "MftpClient.h"

int main(int argc, char *argv[]) {
   std::list<std::string> remotes;
   uint8_t repetitions = 1;
   // Handle commandline arguments
   // format: ./Client server-1 server-2 portnum filename MSS r5

   --argc;
   // Optional argument: Repeat experiment r(this) many times
   if (*argv[argc] == 'r') {
      repetitions = atoi(argv[argc] + 1);
      --argc;
   }
   std::string logfile = "Mftp_time_log.csv";
   uint16_t max_seg = atoi((const char *) argv[argc]);
   --argc;
   std::string file_name(argv[argc]);
   --argc;
   int port = atoi((const char *) argv[argc]);
   --argc;
   if (argc == 0) {
      MftpClient::error("Invalid number of arguments passed.");
      return EXIT_FAILURE;
   }
   while (argc > 0) {
      remotes.push_front(std::string(argv[argc]));
      --argc;
   }

   for (uint8_t i = 0; i < repetitions; ++i) {
      char f_in;
      std::ifstream fd(file_name, std::ios_base::binary);
      MftpClient client = MftpClient(remotes, logfile, port, false, max_seg);

      while (fd >> std::noskipws >> f_in) {
         client.rdt_send(f_in);
      }
      fd.close();
      client.shutdown();
      //Sleep while server resets so we get an accurate startup synchronization
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   }

   //Say goodbye and exit.
   UDP_Communicator::info("***************System is exiting successfully***********\n");

   system("(speaker-test -t sine -f 2200)& pid=$!; sleep 5.0s; kill -9 $pid");
   return EXIT_SUCCESS;
}