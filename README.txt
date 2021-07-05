Compatibility: This program is fully compatible and tested with Ubuntu 14 through 21. Requires linux networking headers.

QUICK-START GUIDE:
1. scp the tarball to the systems you want to test on.

2. On each computer, Untar the file, cd into the directory, and run make.

   > tar -xf MFTP-smdupor.tar.gz
   > cd MFTP-smdupor
   > make

3. (For NCSU VCL): If you have not added IPTABLES rules to permit traffic used by this app, run the iptables
     configuration (requires sudo):

   > ./configure_iptables.sh

4. SERVERS: The example file-to-be-transmitted is included in the tarball. It will always be overwritten, so removal is
    not *strictly* required, but to confirm functionality, you should remove it on all server hosts:

    > rm ./linux-2.2.1.tar.bz2

Now, start each server:

    > ./Server <port> <filename> <loss probability> <optional repeat>
Ex: > ./Server 7735 linux-2.2.1.tar.bz2 0.05

5. CLIENT: Once all servers are running and waiting, start Client using server hostnames as arguments:

    > ./Client <host a> <host b> <...> <port> <max_segment_size> <optional repeat>
Ex: > ./Client 192.168.1.32 192.168.1.33 192.168.1.34 7735 500

Optional Repeat arguments are in the form, "r2", "r3", "r4" ... for 2, 3, 4, ... repetitions of the experiment.


6. EXITING: Upon experiment conclusion, the client will terminate all connections and exit. The Servers, upon
    acknowledgement of the terminated connection, will also exit. A statistics report is printed to the terminal at
    both the Client and all Servers before exiting.

7. CONFIRMING SUCCESS:
    Uploaded files are stored in the same working directory as the ./Server symlink. To confirm a successful transfer,
    use the linux MD5 Checksumming utility on the client and the server, and compare the MD5 sums. These will be
    exactly identical in the case of a successful transfer (regardless of filename), and totally different in the case
    of any failure.

   > md5sum ./linux-2.2.1.tar.bz2
        9028ee0a6c29908b0cc1758e446dd7d2 linux-2.2.1.tar.bz2

APPENDIX: Directory Structure:
./bin       -- holds the compiled binaries. Please run the program using the included symlinks in the working directory.
./include   -- .h header files for all c++ classes
./main      -- int main() files for the Client and the Server executables
./src       -- .cpp source files for all c++ classes

Appendix: Program Structure:
UDP_Communicator -- Superclass holding shared functionality between both Servers and Clients
MftpServer       -- Subclass holding Server-specific code
MftpClient       -- Subclass holding Client-Specific code
** See PDF report for in-depth discussion of structure.

Appendix: Color code definitions for console printed information:
Cyan: Information message, such as MiB transferred or system status
Yellow: A warning, interaction with the user, or system report
Red:    An error is being reported (dropped packet)
Purple: Verbose mode output (Give statistics on items downloaded so far, etc).