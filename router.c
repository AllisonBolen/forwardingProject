#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>
///  Allison Bolen, Cade Baker, Andy Hung  ///

/// table structure for storing file data ///
struct table {
      char* ip;
      char* prefix;
      char* name;
};
/// interface structure for storing interface data ///
struct interface {
      char* name;
      uint8_t MAC[6];
      uint8_t IP[4];
      int socket;
};
/// message structure for storing messages that need to be sent ///
struct message{
  char buff[1500];
  int valid;
  in_addr_t waitingfor;
  int timeMS;
  int socketTO; /// socket to send on for timeout ///
  /// evertime you store a packet sßtore a time stamp, every so often check the tiem statmp and if their old invalidate tehm and send an error mesage about it.
  /// how long before we go and check, sleact has a time out se t it to 100ms if selesct has a time out check you list because you got
};

/// do the checksum stuff sends in the ipheader or icmp header that needs checking or filling/generating and calculates the checksum
u_short cksum(u_short *buf, int count);
///  send teh arp packet request when we are forwarding, takes in a name and a packet and the intefaces list  ///
void arpPacketReq(char *buf, in_addr_t tableIP, char* name, struct interface interfaces[]);
///  sends the arp response when the router gets an arp request  ///
void arpPacketResp(struct interface interfaces[], struct ether_header eh, char *buf);
///  reads the files and populates the table structures in the list correctly depending on where its reading from  ///
void readFiles(char* filename, struct table tableInfo[4]);
///  sends the ICMP reply if the router is being pinged  ///
void icmpPacket(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf);
///  sends the ICMP ERROR packet if the router is being pinged  ///
void icmpPacketERROR(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf, int error);
///  used for timeout checking stored in the message sturcture  ///
void genTime(long timeMS);
///  the number of interfaces we are connected to at the moment   ///
int numInterfaces = 0;
///  the numebr of prefix inforamtion we need to hold  ///
int numTable = 0;
///  sets up and runs the socket connections  ///
int main(){
    // ask for user input
    char filename[12];
    printf("Please enter a table name to read from:\n");
    fgets(filename, 13, stdin);
    if(strcmp(filename, "r1-table.txt")==0){ ///  if its r1 read from r1-table and set up values for array length  ///
      numTable = 4;
      numInterfaces = 3;
    }
    else if(strcmp(filename, "r2-table.txt")==0){ ///  if its r2 read from r2-table and set up values for array length  ///
      numTable = 5;
      numInterfaces = 4;
    }
    else{ ///  file doesnt exist  ///
      printf("File not found: %s\n",filename);
      return 0;
    }

    ///  setup arrays  ///
    struct interface interfaces[numInterfaces];
    struct table tableInfo[numTable];
    struct message storedMessage[100];

    ///  set up sockets  ///
    fd_set sockets;
    FD_ZERO(&sockets);
    int packet_socket;
    struct ifaddrs *ifaddr, *tmp;
    if(getifaddrs(&ifaddr)==-1){ /// given code  ///
      perror("getifaddrs");
      return 1;
    }
    int count = 0;
    int count2 = 0;
    for(tmp = ifaddr; tmp!=NULL; tmp=tmp->ifa_next){ /// given code  ///
      if(tmp->ifa_addr->sa_family==AF_PACKET){ /// given code ///
        printf("Interface: %s\n",tmp->ifa_name);
        if(!strncmp(&(tmp->ifa_name[3]),"eth",3)){ ///  creates a packet socket on the interface r*-eth* ///
  	      printf("Creating Socket on interface %s\n",tmp->ifa_name);
        	packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        	if(packet_socket<0){
        	  perror("socket");
        	  return 2;
        	}
          FD_SET(packet_socket, &sockets);
          struct sockaddr_ll *addrLL;
          ///  add data to the interfaces sturctures in the array  ///
          addrLL = (struct sockaddr_ll *)(tmp->ifa_addr);
          printf("\t InterFace MAC: %s\n", ether_ntoa((struct ether_addr*)(addrLL->sll_addr)));
          memcpy(interfaces[count].MAC,addrLL->sll_addr,6);
          interfaces[count].name = tmp->ifa_name;
          memcpy(&interfaces[count].IP, &((struct sockaddr_in*)tmp->ifa_addr)->sin_addr.s_addr,4);
        	if(bind(packet_socket,tmp->ifa_addr,sizeof(struct sockaddr_ll))==-1){ ///  given code  ///
        	  perror("bind");
        	}
          interfaces[count].socket = packet_socket;
        	count++;
        }
      }
      if(tmp->ifa_addr->sa_family==AF_INET){ /// sets up the right ip ///
         if(!strncmp(&(tmp->ifa_name[3]),"eth",3)){
           memcpy(&interfaces[count2].IP, &((struct sockaddr_in*)tmp->ifa_addr)->sin_addr.s_addr,4);
          count2++;
        }
      }
    }
    ///  reads the file  ///
    readFiles(filename, tableInfo);
    ///  reciving code packets from the structures  ///
    printf("Ready to recieve now\n");
    while(1){ ///  will run forever becacuse 1 is always 1  ///
      char buf[1500];
      struct sockaddr_ll recvaddr;
      int recvaddrlen=sizeof(struct sockaddr_ll);
      fd_set tmp_set = sockets;
      struct timeval tv = {100, 0};
      int timeout = select(FD_SETSIZE,&tmp_set, NULL, NULL, &tv);
      if(timeout == 0){
        printf("TIMEOUT");
        // send the thing i need it to send // didnt get to the host we wanted
        // check the message array for the time in each one and then delecte the ones that have been sitting for a while
        // for every packet we delete then send an error for each one send it on the path it came on

        int now = (int)time(NULL)*1000; ///current time in miliseconds
        int k;
        struct iphdr ipReq;
        struct iphdr ipResp;
        struct ether_header ethResp, eth;
        printf("this is a thing");
        printf("\nNow: %d",now);
        for(k = 0; k < sizeof(storedMessage); k++){
          if((now - storedMessage[k].timeMS) > 200){
            char* pck = storedMessage[k].buff;
            memcpy(&eth, &pck[0], 14);
            memcpy(&ipReq, &pck[14], sizeof(struct iphdr));
            icmpPacketERROR(interfaces, eth, ipReq, eth, ipReq, pck, 2);
            send(storedMessage[k].socketTO, pck, 98, 0);
            storedMessage[k].valid = 0;time_t seconds;
          }
        }
      }
      int i;
      for(i=0; i<FD_SETSIZE;i++){ ///  given code  ///
        if(FD_ISSET(i,&tmp_set)){ ///  given code  ///
        int n = recvfrom(i, buf, 1500,0,(struct sockaddr*)&recvaddr, &recvaddrlen);
        if(recvaddr.sll_pkttype==PACKET_OUTGOING){
          continue;
        }
        struct ether_header eh;
        ///  ether header for any packet recived ///
        memcpy(&eh,&buf[0],14);
        int type = ntohs(eh.ether_type);

        if(type == 0x0806){ ///  got and ARP packet  ///
          struct ether_arp arpReq;
          memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));
          if(ntohs(arpReq.ea_hdr.ar_op)== 1){ /// is it a request that we recived - thsi means the router needs to respond to it directly ///
            ///  respond to said request because you are the only one who can see it  ///
            arpPacketResp(interfaces, eh, buf);
            send(i, buf, 42, 0);
          }
          if(ntohs(arpReq.ea_hdr.ar_op)==2){ /// we got a response from someone we have amessage stored for ///
            /// send data to the thing that we got a response from  ///
            struct iphdr ipReq;
            struct ether_header sendEh;
            ///  switch the data in the stored message packet to whats being sent  ///
            int y;
            for(y = 0; y < 100; y++){ ///  loop tthrough all of our messages stored  ///
              if(storedMessage[y].valid == 1){ ///  if the message is waiting to be sent - the value of one in valid means it needs to be sent  ///
                memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));
                memcpy(&ipReq, &storedMessage[y].buff[sizeof(struct ether_header)], sizeof(struct iphdr));
                if(memcmp(&arpReq.arp_spa, &storedMessage[y].waitingfor, 4)==0){  ///  is the storedMessage destined for the destination we jsut recived for, if so then send it  ///
                  /// switch the source to r1 and add the mac of the arp resp to teh message packet  ///
                  printf("SENT THE STORED PACKET TO THE DESTINATION\n");
                  /// decrement ttl before sending ///
                  __u8 dec = 1;
                  dec = ipReq.ttl - dec;
                  memcpy(&ipReq.ttl, &dec, sizeof(__u8)); // probably fine ????
                  int lol = 0;
                  memcpy(&ipReq.check, &lol, 1); // make chekcsum zero
                  memcpy(&storedMessage[y].buff[14], &ipReq, sizeof(struct iphdr));
                  memcpy(&sendEh,&storedMessage[y].buff[0],14);
                  // change the checksum
                  u_short hold[10];
                  memcpy(&hold, &ipReq, sizeof(ipReq));
                  int wordnum = sizeof(ipReq)/2; /// how many 2 bytes are there in this thing because one 16bitword for every 2 bytes ///
                  __u16 sumcheck = cksum(hold, wordnum);

                  memcpy(&ipReq.check, &sumcheck, 16);
                  memcpy(&storedMessage[y].buff[14], &ipReq, sizeof(struct iphdr));
                  memcpy(&sendEh.ether_shost, &eh.ether_dhost, 6); //switch ehter source to r1
                  memcpy(&sendEh.ether_dhost, &eh.ether_shost, 6); // technically wrong but whatever should be r=from teh arp packet
                  memcpy(&storedMessage[y].buff[0], &sendEh,14); //reset the biffer to be sents header.
                  send(i, storedMessage[y].buff, 98, 0);
                  storedMessage[y].valid = 0;
                }
              }
            }
          }
        }

        if(type == ETHERTYPE_IP){ /// got an IP packet ///
          printf("%s\n", "Received IP Packet");
          struct iphdr ipReq;
          struct iphdr ipResp;
          struct ether_header ethResp;
          memcpy(&ipReq, &buf[sizeof(struct ether_header)], sizeof(struct iphdr));
          if(ntohs(ipReq.ttl) > 1 ){ /// check the ttl for next hop if it equals one do sending for it ///
            /// check the checksum on the ip header ///
            u_short hold[10];
            memcpy(&hold, &ipReq, sizeof(ipReq));
            int wordnum = sizeof(ipReq)/2; /// how many 2 bytes are there in this thing because one 16bitword for every 2 bytes ///
            __u16 sumcheck = cksum(hold, wordnum);
            //if(memcmp(&sumcheck, &ipReq.check, sizeof(ipReq.check)) == 0){
             /// if the cecksum and sumcheck match continue as normal ///
             int n;
             int forus = 0; /// boolean for telling us if teh packet was meant for the current router or not. ///
             for(n =0;n < numInterfaces; n++){ /// check if its for me or not if its not for me we forward if ///
               if(memcmp(&ipReq.daddr, &interfaces[n].IP, 4) == 0){ /// if the ip the prequest is destined for is the current router respond to it ///
                 forus=1;
                 printf("\nPACKET FOR THE ROUTER\n");
                 if((ipReq.protocol) == 1){ /// if the ip packet we just got is imcp
                   struct icmphdr icmpReq;
                   memcpy(&icmpReq, &buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], sizeof(struct icmphdr));// get the icmp header
                   // __u8 sumcheck = cksum(icmpReq, sizeof(ipReq));
                   // if(memcmp(&sumcheck, &icmpReq) == 0){ ///  icmp checksum sumcheck matches  ///
                     if(icmpReq.type == 8){ /// got an ICMP packet request for the current router send an ICMP reply packet back
                       // got an ICMP request packet
                       printf("%s\n", "Received ICMP Request Packet");
                       icmpPacket(interfaces, eh, ipReq, ethResp, ipResp, buf);
                       send(i,buf, 98, 0);
                       printf("%s\n", "Sending ICMP Response packet");
                     }
                   }
                 }
               }

               int here = 1;
               if(forus==0){ /// the packet is not destined for the router so we need to find weher it needs to go by comparing it to table info  ///
                 printf("Packet for somethign other than the router");
                 ///  table look up  ///
                 in_addr_t tableIP;
                 char name[20];
                 in_addr_t fromPacket;
                 memcpy(&fromPacket, &ipReq.daddr, 4);
                 int k;
                 for(k = 0 ; k < numTable; k++){ /// for each table structure in our table list compare its prefix to the first bytenum bytes ///
                   /// sepreate on slash to extract and convert the data in the prefix char 10.0.0.0/16 total lenght 11 ///
                   char byteCmp[3];
                   memcpy(&byteCmp, &tableInfo[k].prefix[9], 2);
                   char tablePrefIP[9];
                   memcpy(&tablePrefIP, &tableInfo[k].prefix[0],8);
                   tablePrefIP[8] = '\0';
                   in_addr_t IPNum = inet_addr(tablePrefIP);//
                   char * y = inet_ntoa(*(struct in_addr *)&IPNum);
                   int compare = atoi(byteCmp);
                   int bytenum = compare/8;
                   char * t = inet_ntoa(*(struct in_addr *)&IPNum);
                   ///  compare the prefix bytes of the ips  ///
                   int matches = memcmp(&fromPacket, &IPNum, bytenum);
                   if(matches==0){ ///  if they match then the target of the packet is something we can reach somehow  ///
                     here = 0;
                     if(strcmp(tableInfo[k].ip, "-") != 0){  /// if the ip is NOT a dash then it is somthing we have to go trough router two to get to. ///
                       /// sets the tableip to that ip for s new dest in ou rarp packet ///
                       tableIP = inet_addr(tableInfo[k].ip);
                       strcpy(name, tableInfo[k].name);
                     }
                     else{ ///  the ip didnt match the other routers IP so that means it on the current routers side of the system so the next dst is the actual dstination of the packet///
                       tableIP = fromPacket;
                       strcpy(name, tableInfo[k].name);
                     }
                   }
                 }
                 if(here == 1){
                     /// the network didnt mathc anything we hold in teh table so send and error packet back out the same socket
                     printf("didnt match the table: network not found");
                     icmpPacketERROR(interfaces, eh, ipReq, ethResp, ipResp, buf, 3);
                     send(i, buf, 98, 0);
                 }

                 /// we have to store the message, cant just send it becasue the router doesnt know the mac of the next hop of the packet so we need to store it and then send and arp ///
                 int m;
                 for(m = 0 ; m < 100; m++){ ///  check the whole array of message structures for an empty or overwritable structure   ///
                   if(storedMessage[m].valid == 0){ ///  if its zero then the structure at this spot is valid to be overwritten  ///
                       memcpy(storedMessage[m].buff, buf, 1500);
                     storedMessage[m].valid = 1;
                     storedMessage[m].waitingfor = tableIP; /// address arp is being sent to that the message needs to wait for a response from  ///
                     storedMessage[m].timeMS= (int)time(NULL); /// timeout trash limit
                     int p;
                     for(p =0; p < numInterfaces; p++){ ///  loop through sockets to find the one to store for the icmp error thing on timeout  ///
                       if(strcmp(name, interfaces[p].name)==0){ ///  if the name of the table ip we found mathches the name of the interface we are at then thats the socket we want  ///
                         storedMessage[m].socketTO = interfaces[p].socket;
                       }
                     break;
                   }
                 }
                 char buffer[1500];
                 int x;
                 int foundSocket;
                 for(x =0; x < numInterfaces; x++){ ///  loop through sockets to find the one to send the arp request on  ///
                   if(strcmp(name, interfaces[x].name)==0){ ///  if the name of the table ip we found mathches the name of the interface we are at then thats the socket we want  ///
                     foundSocket = interfaces[x].socket;
                     arpPacketReq(buffer, tableIP, name, interfaces);
                     send(foundSocket, buffer, 42, 0);
                     printf("Sent the ARP request for the packet destined for something other than the current router\n" );
                   }
                 }
               }
             }
           //}
          }
          else{// make it out of the ip digits less then one on the ttl then you should send an error packet back
            printf("TTL too low");
            icmpPacketERROR(interfaces, eh, ipReq, ethResp, ipResp, buf, 1);
            send(i, buf, 98, 0);
          }
        }
      }
    }
  }
  freeifaddrs(ifaddr);
  return 0;
}

/// get time so you can track the timeout stuff, gotten from https://stackoverflow.com/questions/3756323/how-to-get-the-current-time-in-milliseconds-from-c-in-linux ///
void genTime(long timeMS){
  //long timeMS;
  time_t seconds;
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);
  seconds = spec.tv_sec;
  timeMS = round(spec.tv_nsec/1.0e6);
  if(timeMS > 999){
    seconds++;
    timeMS = 0;
  }
}

/// populate table array of table structs ///
void readFiles( char* filename,struct table tableInfo[numTable]){
    printf("\nfilename: %s\n", filename);
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL){ /// file doesnt exist ///
        printf("Cannot open file \n");
        exit(0);
    }
    else{ /// exists ///
      int count = 0;
      char pref[10], ipaddr[10], name[10];
      while(fscanf(fptr, "%s %s %s", pref, ipaddr, name) != EOF){ /// while there is still data to read keep reading ///
        tableInfo[count].name = strdup(name);
        tableInfo[count].prefix = strdup(pref);
        tableInfo[count].ip = strdup(ipaddr);
        printf("\nthis is that table name at count: %s\n ", tableInfo[count].name);
        count ++;
      }
    }
    fclose(fptr);
  }

///  send the arp for the next hop  ///
void arpPacketReq(char *buf, in_addr_t tableIP, char* name, struct interface interfaces[]){
    printf("Setting up an ARP Request\n");
    char * z = inet_ntoa(*(struct in_addr *)&tableIP);

    /// build the request for arp ///
    int u;
    uint8_t broadcast[6];
    char f = 0xFF;
    for(u = 0; u < 6; u++){ /// populate the broadcast array ///
      broadcast[u]=f;
    }
    struct ether_header ethHdrResp;
    struct ether_arp arpReq;
    int j;
    for(j = 0; j< numInterfaces; j++){ /// for the list of interfaces if the naem matches the name we got from our table calculationt then we also want the IP associated with that name ///
      int check = memcmp(&interfaces[j].IP, &tableIP, 4);
      if(strcmp(interfaces[j].name, name) == 0){ /// the name found while searching for a table mathces the one at hte interface ///
          memcpy(&arpReq.arp_sha, &interfaces[j].MAC, 6); /// get my mac and make the new source ///
	        memcpy(&arpReq.arp_spa, &interfaces[j].IP, 4);
      }
    }

    memcpy(&arpReq.arp_tpa, &tableIP, 4);  /// switch ips ///
    memcpy(&arpReq.arp_tha, &broadcast , 6);
    arpReq.ea_hdr.ar_op = htons(1); /// change op code for r///
    arpReq.ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    arpReq.ea_hdr.ar_hln = 6;
    arpReq.ea_hdr.ar_pln = 4;
    arpReq.ea_hdr.ar_hrd = htons(1);
    /// change ehternet header ///
    memcpy(&ethHdrResp.ether_shost, &arpReq.arp_sha, 6);/// get mac of me to them  ///
    memcpy(&ethHdrResp.ether_dhost, &broadcast, 6);/// set this to the broadcast ///
    ethHdrResp.ether_type = htons(0x0806);
    /// fill the buffer ///
    memcpy(&buf[0], &ethHdrResp, sizeof(struct ether_header));
    memcpy(&buf[sizeof(struct ether_header)], &arpReq, sizeof(struct ether_arp));
  }

/// make arp response if router is the one being requested ///
void arpPacketResp(struct interface interfaces[], struct ether_header eh, char *buf){
    printf("Got an ARP PACKET FOR REGULAR\n");
    /// build the response for arp ///
    struct ether_header ethHdrResp;
    struct ether_arp arpReq, arpResp;

    memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));/// get the arp info into a header struct ///
    memcpy(&arpResp.arp_tha, &arpReq.arp_sha, 6); /// put the source into teh new packets dst ///
    int j;
    u_char wantedIp[4];
    memcpy(&wantedIp[0],arpReq.arp_tpa,4); /// wanted IP is the target of the arp ///
    for(j = 0; j< 7; j++){ /// search through the list of interfaces for the data we want ///
      if(memcmp(interfaces[j].IP, wantedIp, 4) == 0){ /// if the wnatedip mathces teh interface IP then set the mac ///
          memcpy(&arpResp.arp_sha, &interfaces[j].MAC, 6); /// get mmy mac and make the new source ///
      }
    }
    memcpy(&arpResp.arp_tpa, &arpReq.arp_spa, 4);  /// switch ips ///
    memcpy(&arpResp.arp_spa, &arpReq.arp_tpa, 4); /// switch ips ///
    arpResp.ea_hdr.ar_op = htons(2); /// change op code for reply ///
    arpResp.ea_hdr.ar_pro = arpReq.ea_hdr.ar_pro;
    arpResp.ea_hdr.ar_hln = arpReq.ea_hdr.ar_hln;
    arpResp.ea_hdr.ar_pln = arpReq.ea_hdr.ar_pln;
    arpResp.ea_hdr.ar_hrd = arpReq.ea_hdr.ar_hrd;
    /// change ehternet header ///

    memcpy(&ethHdrResp.ether_shost, &arpResp.arp_sha, 6);// get mac of me to them
    memcpy(&ethHdrResp.ether_dhost, &eh.ether_shost, 6);//
    ethHdrResp.ether_type = eh.ether_type;
    /// fill the buffer ///
    memcpy(&buf[0], &ethHdrResp, sizeof(struct ether_header));
    memcpy(&buf[sizeof(struct ether_header)], &arpResp, sizeof(struct ether_arp));
  }

/// the icmp response when we are requested ///
void icmpPacket(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf){
  struct icmphdr icmpReq;
  struct icmphdr icmpResp;
  memcpy(&icmpReq, &buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], sizeof(struct icmphdr));/// get the icmp header ///
  ///  swap ethernet header info ///
  memcpy(&ethResp.ether_shost,&eh.ether_dhost,6);
  memcpy(&ethResp.ether_dhost, &eh.ether_shost,6);
  ethResp.ether_type = eh.ether_type;
  /// swap the ip header info ///
  memcpy(&ipResp.saddr, &ipReq.daddr, 4);
  memcpy(&ipResp.daddr, &ipReq.saddr, 4);
  ipResp.ihl=ipReq.ihl;
  ipResp.version = ipReq.version;
  ipResp.tos = ipReq.tos; /// copy inof over ///
  ipResp.tot_len = ipReq.tot_len;
  ipResp.id = ipReq.id;
  ipResp.frag_off = ipReq.frag_off;
  ipResp.ttl = ipReq.ttl;
  ipResp.protocol = ipReq.protocol;
  ipResp.check = ipReq.check;
  /// swap icmp stuff now ///
  icmpResp.code = icmpReq.code;/// set it to echo reply ///
  icmpResp.type = 0;

  u_short hold[10];
  memcpy(&hold, &ipReq, sizeof(ipReq));
  int wordnum = sizeof(ipReq)/2; /// how many 2 bytes are there in this thing because one 16bitword for every 2 bytes ///
  __u16 sumcheck = cksum(hold, wordnum);

  icmpResp.checksum = sumcheck;
  icmpResp.un.echo.id = icmpReq.un.echo.id;
  icmpResp.un.echo.sequence = icmpReq.un.echo.sequence;

  /// fill the buffer again ///
  memcpy(&buf[0], &ethResp, sizeof(struct ether_header));
  memcpy(&buf[sizeof(struct ether_header)],&ipResp, sizeof(struct iphdr));
  memcpy(&buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], &icmpResp, sizeof(icmpResp));
}

void icmpPacketERROR(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf, int error){
  int typenew, codeCH;
  if( error = 1){
    /// we got a ttl of one so we drop the packet and need to send a new one with the right opcode ///
    codeCH = 0;
    typenew = 11; // time exceeded
  }
  if( error = 2){
    /// we got a timeout on the arp for the next hop need to send another icmp error packet missing host///
    // code change
    codeCH = 1;
    typenew = 3; // missing host
  }
  if( error == 3){
    /// cant find network int eh table drop the packet send an error for network unreachable ///
    // code chnage
    codeCH = 0;
    typenew  = 3 ;
  }

  struct icmphdr icmpReq;
  struct icmphdr icmpResp;
  memcpy(&icmpReq, &buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], sizeof(struct icmphdr));/// get the icmp header ///
  ///  swap ethernet header info ///
  memcpy(&ethResp.ether_shost,&eh.ether_dhost,6);
  memcpy(&ethResp.ether_dhost, &eh.ether_shost,6);
  ethResp.ether_type = eh.ether_type;

  /// swap the ip header info ///
  memcpy(&ipResp.saddr, &ipReq.daddr, 4);
  memcpy(&ipResp.daddr, &ipReq.saddr, 4);
  ipResp.ihl=ipReq.ihl;
  ipResp.version = ipReq.version;
  ipResp.tos = ipReq.tos; /// copy inof over ///
  ipResp.tot_len = ipReq.tot_len;
  ipResp.id = ipReq.id;
  ipResp.frag_off = ipReq.frag_off;
  ipResp.ttl = ipReq.ttl;
  ipResp.protocol = ipReq.protocol;
  ipResp.check = 0;
  /// this is the ip check sum build ip set the ipchsum to zero and send it in then get out the thing
  u_short hold[10];
  memcpy(&hold, &ipReq, sizeof(struct iphdr));
  int wordnum = sizeof(struct iphdr)/2; /// how many 2 bytes are there in this thing because one 16bitword for every 2 bytes ///
  __u16 sumcheck = cksum(hold, wordnum);
  ipResp.check = sumcheck;

  /// swap icmp stuff now ///
  icmpResp.code = codeCH;/// set it to echo reply ///
  icmpResp.type = typenew;

  icmpResp.checksum = 0;
  icmpResp.un.echo.id = icmpReq.un.echo.id;
  icmpResp.un.echo.sequence = icmpReq.un.echo.sequence;

  /// Icmp change
  u_short holdIcmp[18]; // size of icmp and old ip and 8 byts
  memcpy(&holdIcmp, &buf[14], 32);
  int wordnumIcmp = sizeof(ipReq)/2; /// how many 2 bytes are there in this thing because one 16bitword for every 2 bytes ///
  __u16 sumcheckIcmp = cksum(holdIcmp, wordnumIcmp);

  icmpResp.checksum = sumcheckIcmp;
  icmpResp.un.echo.id = icmpReq.un.echo.id;
  icmpResp.un.echo.sequence = icmpReq.un.echo.sequence;

  /// fill the buffer again ///
  memcpy(&buf[0], &ethResp, sizeof(struct ether_header));
  memcpy(&buf[sizeof(struct ether_header)],&ipResp, sizeof(struct iphdr));
  memcpy(&buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], &icmpResp, sizeof(struct icmphdr));
}

/// check sum for the packets gotten from page 95 of the class textbook  ///
u_short cksum(u_short *buf, int count){
  register u_long sum = 0;
  while(count--){
    sum += *buf++;
    if(sum & 0xFFFF0000){
      //carry occured. so wrap around
      sum &= 0xFFFF;
      sum++;
    }
  }
  return ~(sum & 0xFFFF);
}
