/**
 * Server.cpp encapsulates the int main() for the MultiFTP Server executable, to handle incoming parameter arguments,
 * and to instantiate the receiver-component of the SAW protocol,  rdt_receive(). This class also includes an
 * optional argument to repeat the transfer (n) number of times for experimental data gathering.
 *
 * Created on: June 23th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include <iostream>

#include "MftpServer.h"

int main(int argc, char *argv[]) {
   // Default  to one transfer unless we receive an argument configuring repeats
   uint8_t repetitions = 1;

   // Handle commandline arguments format: ./Server portnum filename loss_probability r5
   // Pop the "blank" argument index
   --argc;

   // Optional argument: Repeat experiment r(this) many times: Interpret and pop the argument off the array
   if (argv[argc][0] == 'r') {
      repetitions = atoi(argv[argc] + 1);
      --argc;
   }

   // Instantiate the server time logfile
   std::string logfile = "Mftp_time_log.csv";

   // Read and pop the loss probability argument
   float loss_probability = atof(argv[argc]);
   --argc;

   // Read and pop the file name argument
   std::string file_name(argv[argc]);
   --argc;

   // Read and pop the Bind port argument
   int port = atoi(argv[argc]);
   --argc;

   // Confirm number of arguments was correct and exit failure if not.
   if (argc == -1) {
      MftpServer::error("Invalid number of arguments passed.");
      return EXIT_FAILURE;
   }

   // Start the server and repeat the experiment (repetitions) number of times
   for (uint8_t i = 0; i < repetitions; ++i) {
      MftpServer server = MftpServer(file_name, logfile, port, false, loss_probability);
      server.rdt_receive();
   }

   //Say goodbye and exit.
   UDP_Communicator::info("***************System is exiting successfully***********\n");
   return EXIT_SUCCESS;
}