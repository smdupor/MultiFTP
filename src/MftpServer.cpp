//
// Created by smdupor on 6/25/21.
//


#include "MftpServer.h"

MftpServer::MftpServer(std::list<std::string> &remote_server_list, std::string &logfile, int port, bool verbose) {
   log = logfile;
   debug = verbose;
   start_time = (const time_t) std::time(nullptr);
   system_on = true;
   system_port = port;

   //inbound_socket = create_inbound_UDP_socket(port);

   for(std::string &serv : remote_server_list) {
      struct sockaddr_in *remote_addr = new sockaddr_in;
      bzero((char *) remote_addr, sizeof(*remote_addr));

      struct hostent *server = gethostbyname(serv.c_str());

      remote_addr->sin_family = AF_INET;
      bcopy((char *) server->h_addr, (char *) &remote_addr->sin_addr.s_addr, server->h_length);
      remote_addr->sin_port = htons(port);

      remote_hosts.push_back(RemoteHost((sockaddr_in *) remote_addr));
   }
}

/**
 * System destructor.
 */
MftpServer::~MftpServer() {
   for(RemoteHost &r : remote_hosts){
      free(r.address);
   }
}

/** Server0-sde
 */
void MftpServer::start() {
   local_time_logs.push_back(LogItem(0));

   int sockfd = create_inbound_UDP_socket(system_port);

   char buffer[1024];
   bzero(buffer, 1024);

   std::string out_msg = "Testing a remote msg";

   struct sockaddr_in cli_addr;
   bzero(&cli_addr, sizeof(cli_addr));

   error("About to receive\n");
   //for(RemoteHost &r : remote_hosts) {
   socklen_t length = sizeof(cli_addr);

   int n = recvfrom(sockfd, (char *) buffer, 1024, 0, (struct sockaddr *) &cli_addr, &length);
   error("received\n");

   buffer[n] = '\0';
   std::cout << buffer<<std::endl;

   out_msg = "OvernetSending a reply msg";
   sendto(sockfd, out_msg.c_str(), strlen(out_msg.c_str()), 0, (const struct sockaddr *) &cli_addr, length);
   std::this_thread::sleep_for(std::chrono::milliseconds(50));

   //}
   close(sockfd);
}