/**
 *
 * Created on: June 23th, 2021
 * Author: smdupor
 */

#include <iostream>

#include "MftpServer.h"

int main(int argc, char *argv[]) {
   uint8_t repetitions = 1;
   // Handle commandline arguments
   // format: ./Server portnum filename loss_probability r5
   --argc;
   // Optional argument: Repeat experiment r(this) many times
   if(argv[argc][0] == 'r') {
      repetitions = atoi( argv[argc]+1);
      --argc;
   }
   std::string logfile = "Mftp_time_log.csv";
   float loss_probability = atof(argv[argc]);
   --argc;
   std::string file_name(argv[argc]);
   --argc;
   int port = atoi(argv[argc]);
   --argc;
   if(argc == -1) {
      MftpServer::error("Invalid number of arguments passed.");
      return EXIT_FAILURE;
   }

   for(uint8_t i = 0; i < repetitions; ++i) {
      MftpServer server = MftpServer(file_name, logfile, port, false, loss_probability);

      server.start();
      server.rdt_receive();
   }

   //Say goodbye and exit.
   UDP_Communicator::info("***************System is exiting successfully***********\n");
		return EXIT_SUCCESS;
}