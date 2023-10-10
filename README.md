This program implements a udp based reliable FTP between client and server.


From the client we can do the following to the server:
      ls
      del <filename>
      get <filename>
      put <filename>
      exit

All the above operations are instructed at the client side and executed at the server side.
The get transfers file from server to client and put transfers file from client to server, verified on both sides after reception that file size remains same, which implies no loss incurred in transmission.

Steps to run:
Run make command from the project directory.
./<server> <port>
./<client> <ip> <port>

or 

Separately compile files using gcc.
 - Go to server directory and run ./<server> <port>
 - Go to client directory and run ./<client> <ip> <port>

<ip> is the IP of the server and port number should be >5000.

Reliability:
 - File is divided into chunks/packets.
 - First we advertise number of packets to be sent across the medium.
 - After that we send each packet and get an acknowledgement, if the sent packet doesnt get its corresponding ACK, we resend 50 times until it matches.
 - After all packets are sent, file transfer is complete for PUT functionality. The same logic is implemented from server during GET functionality.
 - At the rx side, if out-of-sequence packet was received the code will wait to receive that packet again.
