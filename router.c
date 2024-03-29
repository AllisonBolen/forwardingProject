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
//noice boys 
//Allison Bolen, Cade Baker, Andy Hung
// part two
struct table {
      char* ip;
      char* prefix;
      char* name;
};

struct interface {
      char* name;
      uint8_t MAC[6];
      uint8_t IP[4];
      int socket;
};

struct message{
  char buff[1500];
  int valid;
  in_addr_t waitingfor;
};

void arpPacketReq(char *buf, in_addr_t tableIP, char* name, struct interface interfaces[]);
void arpPacketResp(struct interface interfaces[], struct ether_header eh, char *buf);
void readFiles(char* filename, struct table tableInfo[4]);
void icmpPacket(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf);
int numInterfaces = 0;
int numTable = 0;


int main(){
    //ask for user input
    char filename[12];
    printf("Please enter a table name to read from:\n");
    fgets(filename, 13, stdin);
    if(strcmp(filename, "r1-table.txt")==0){
      numTable = 4;
      numInterfaces = 3;
    }
    else if(strcmp(filename, "r2-table.txt")==0){
      numTable = 5;
      numInterfaces = 4;
    }
    else{
      printf("File not found: %s\n",filename);
      return 0;
    }

    struct interface interfaces[numInterfaces];
    struct table tableInfo[numTable];
    struct message storedMessage[100];


    fd_set sockets;
    FD_ZERO(&sockets);
    int packet_socket;
    struct ifaddrs *ifaddr, *tmp;
    if(getifaddrs(&ifaddr)==-1){
      perror("getifaddrs");
      return 1;
    }

    int count =0;
    int count2 = 0;
    for(tmp = ifaddr; tmp!=NULL; tmp=tmp->ifa_next){
      if(tmp->ifa_addr->sa_family==AF_PACKET){
        printf("Interface: %s\n",tmp->ifa_name);
        //create a packet socket on interface r?-eth1
        if(!strncmp(&(tmp->ifa_name[3]),"eth",3)){
  	      printf("Creating Socket on interface %s\n",tmp->ifa_name);
        	packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        	if(packet_socket<0){
        	  perror("socket");
        	  return 2;
        	}
          FD_SET(packet_socket, &sockets);
          struct sockaddr_ll *addrLL;
          addrLL = (struct sockaddr_ll *)(tmp->ifa_addr);
          printf("\t InterFace MAC: %s\n", ether_ntoa((struct ether_addr*)(addrLL->sll_addr)));
          memcpy(interfaces[count].MAC,addrLL->sll_addr,6);
          interfaces[count].name = tmp->ifa_name;
          memcpy(&interfaces[count].IP, &((struct sockaddr_in*)tmp->ifa_addr)->sin_addr.s_addr,4);
          printf("\nMAC in Interface STRUCT: %s\n", ether_ntoa( (struct ether_addr*) interfaces[count].MAC ));
        	if(bind(packet_socket,tmp->ifa_addr,sizeof(struct sockaddr_ll))==-1){
        	  perror("bind");
        	}
          interfaces[count].socket = packet_socket;
        	count++;
        }
      }
      if(tmp->ifa_addr->sa_family==AF_INET){
         if(!strncmp(&(tmp->ifa_name[3]),"eth",3)){
           memcpy(&interfaces[count2].IP, &((struct sockaddr_in*)tmp->ifa_addr)->sin_addr.s_addr,4);
          count2++;
        }
      }
    }

    readFiles(filename, tableInfo);

    printf("Ready to recieve now\n");
    while(1){
      char buf[1500];
      struct sockaddr_ll recvaddr;
      int recvaddrlen=sizeof(struct sockaddr_ll);
      fd_set tmp_set = sockets;
      select(FD_SETSIZE,&tmp_set, NULL, NULL, NULL);
      int i;
      for(i=0; i<FD_SETSIZE;i++){
        if(FD_ISSET(i,&tmp_set)){
        int n = recvfrom(i, buf, 1500,0,(struct sockaddr*)&recvaddr, &recvaddrlen);
        if(recvaddr.sll_pkttype==PACKET_OUTGOING){
          continue;
        }
        struct ether_header eh;
        //ether header for any packet
        memcpy(&eh,&buf[0],14);
        int type = ntohs(eh.ether_type);

        if(type == 0x0806){ // got an arp packet
          printf("THIS IS ARP\n");
          fflush(stdout);
          // is it a response or a request
          struct ether_arp arpReq;
          memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));
          if(ntohs(arpReq.ea_hdr.ar_op)== 1){
            // we got a Request
            //respond to said request because you are the only one who can see it
            arpPacketResp(interfaces, eh, buf);
            send(i, buf, 42, 0);
          }
          if(ntohs(arpReq.ea_hdr.ar_op)==2){
            // we got a Response
            // send data to the thing that we got a response from
            printf("\n\nTHIS IS Response for the furthest host\n");
            struct iphdr ipReq;
            struct ether_header sendEh;
            // switch the data in the stored message packet thats being sent
            int y;
            for(y = 0; y < 100; y++){
              if(storedMessage[y].valid == 1){
                memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));
                memcpy(&ipReq, &storedMessage[y].buff[sizeof(struct ether_header)], sizeof(struct iphdr)); //from the adta packet
                if(memcmp(&arpReq.arp_spa, &storedMessage[y].waitingfor, 4)==0){
                  // switch the source to r1 and add the mac of the arp resp to teh message packt
                  printf("SENT THE STORED PACKET TO THE DESTINATION\n");
                  memcpy(&sendEh,&storedMessage[y].buff[0],14);
                  memcpy(&sendEh.ether_shost, &eh.ether_dhost, 6); //switch ehter source to r1
                  memcpy(&sendEh.ether_dhost, &eh.ether_shost, 6); // technically wrong but whatever should be r=from teh arp packet
                  memcpy(&storedMessage[y].buff[0], &sendEh,14); //reset the biffer to be sents header.
                  send(i, storedMessage[y].buff, 98, 0); // how and when do I store a message?
                  storedMessage[y].valid = 0;
              }
            }
          }
        }
      }

        if(type == ETHERTYPE_IP){ // got an icmp packet
  	      printf("%s\n", "Received IP Packet");
          struct iphdr ipReq;
          struct iphdr ipResp;
          //struct ether_header ethHdr;
          struct ether_header ethResp;
          memcpy(&ipReq, &buf[sizeof(struct ether_header)], sizeof(struct iphdr)); // get the ip header
          // check if its for me or not if its not for me we forward if
          int n;
          int forus = 0;
          for(n =0;n < numInterfaces; n++){ // check for if its me
            if(memcmp(&ipReq.daddr, &interfaces[n].IP, 4) == 0){ // if it is do like part one
              forus=1;
              printf("PACKET FOR THE ROUTER");
              if((ipReq.protocol) == 1){
                struct icmphdr icmpReq;
                memcpy(&icmpReq, &buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], sizeof(struct icmphdr));// get the icmp header
                if(icmpReq.type == 8){
                  // got an ICMP request packet
                  printf("%s\n", "Received ICMP Request Packet");
                  icmpPacket(interfaces, eh, ipReq, ethResp, ipResp, buf);
                  send(i,buf, 98, 0);
        		      printf("%s\n", "Sending ICMP Response");
              }
            }
          }
        }

        if(forus==0){
          //printf("\n\nWe got a packet that isnt meant for us\n");
          // table look up
          in_addr_t tableIP;
          char name[20];
          int k;
          in_addr_t fromPacket;
          memcpy(&fromPacket, &ipReq.daddr, 4);
          for(k = 0 ; k < numTable; k++){
            // sepreate on slash
            //10.0.0.0/16 total lenght 11
            char byteCmp[3];
            memcpy(&byteCmp, &tableInfo[k].prefix[9], 2); //print these at some point
            //printf("this is the prefix string: %s\n", byteCmp );
            char tablePrefIP[9];
            memcpy(&tablePrefIP, &tableInfo[k].prefix[0],8);
	          tablePrefIP[8] = '\0';
            //printf("this is the stuff for the ip preix: %s\n",tablePrefIP );
            in_addr_t IPNum = inet_addr(tablePrefIP);//
	          char * y = inet_ntoa(*(struct in_addr *)&IPNum);
            //printf("IPNum from FORUS=0: %s\n", y);
            int compare = atoi(byteCmp);
            int bytenum = compare/8;
            // get addres form packet
	          char * t = inet_ntoa(*(struct in_addr *)&IPNum);
            //printf("FromPacket: %s\n", t);
            int matches = memcmp(&fromPacket, &IPNum, bytenum);
	          //printf("Matches Value: %d\n", matches);
            if(matches==0){
              if(strcmp(tableInfo[k].ip, "-") != 0){
                tableIP = inet_addr(tableInfo[k].ip);
                strcpy(name, tableInfo[k].name);
		            //printf("this is the stuff for the ip preix: %s\n",tablePrefIP );
		            //printf("this is the prefix string: %s\n", byteCmp );
		            //break;
              }
              else{
                tableIP = fromPacket;
                strcpy(name, tableInfo[k].name);
              }
            }
          }
          // store the message
          int m;
          for(m = 0 ; m < 100; m++){
            if(storedMessage[m].valid == 0){
              //storedMessage[m].buff = buf;
	            memcpy(storedMessage[m].buff, buf, 1500);
              storedMessage[m].valid = 1;
              storedMessage[m].waitingfor = tableIP;// address arp is being sent to
              break;
            }
          }
          char buffer[1500];
          // sends message
          // loop through sockets to find theone to send it on
          int x;
          int foundSocket;
          for(x =0; x < numInterfaces; x++){
            if(strcmp(name, interfaces[x].name)==0){
              foundSocket = interfaces[x].socket;
              char * z = inet_ntoa(*(struct in_addr *)&tableIP);
              printf("%s\n", z);
              arpPacketReq(buffer, tableIP, name, interfaces);
              send(foundSocket, buffer, 42, 0);
              printf("HERE WE SENT THE ARP for the further destination\n" );
            }
          }
        }
      }
    }
  }
 }
  freeifaddrs(ifaddr);
  return 0;
}

// populate table struct
void readFiles( char* filename,struct table tableInfo[numTable]){
    //printf("here2\n");
    // char filename[12];
    // fgets(filename, 13, stdin);
    printf("\nfilename: %s\n", filename);
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL)
    {
        printf("Cannot open file \n");
        exit(0);
    }
    else{
      //printf("here3");
      int count = 0;
      char pref[10], ipaddr[10], name[10];
      while(fscanf(fptr, "%s %s %s", pref, ipaddr, name) != EOF){
        tableInfo[count].name = strdup(name);
        tableInfo[count].prefix = strdup(pref);
        tableInfo[count].ip = strdup(ipaddr);
        printf("\nthis is that table name at count: %s\n ", tableInfo[count].name);
        count ++;
      }
    }
    fclose(fptr);
  }

void arpPacketReq(char *buf, in_addr_t tableIP, char* name, struct interface interfaces[]){
    printf("Setting up an ARP Request\n");
    char * z = inet_ntoa(*(struct in_addr *)&tableIP);
              //printf("Table IP Address in ARPPACKETREQ: %s\n", z);
    //build the response for arp
    int u;
    uint8_t broadcast[6];
    char f = 0xFF;
    for(u = 0; u < 6; u++){
      broadcast[u]=f;
    }
    struct ether_header ethHdrResp;
    struct ether_arp arpReq;
    int j;
    for(j = 0; j< numInterfaces; j++){
      int check = memcmp(&interfaces[j].IP, &tableIP, 4);
      //printf("Memory Compare Result: %d\n", check);
      if(strcmp(interfaces[j].name, name) == 0){
	        //printf("Entered Interface IF: %s\n", interfaces[j].name);
          memcpy(&arpReq.arp_sha, &interfaces[j].MAC, 6); // get my mac and make the new source
	        memcpy(&arpReq.arp_spa, &interfaces[j].IP, 4);
      }
    }
    memcpy(&arpReq.arp_tpa, &tableIP, 4);  // switch ips // should be what we want
    memcpy(&arpReq.arp_tha, &broadcast , 6);
    //memcpy(&arpReq.arp_spa, tableIP, 4); //switch ips // source should be the router address
    arpReq.ea_hdr.ar_op = htons(1); // change op code for r
    arpReq.ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    arpReq.ea_hdr.ar_hln = 6;
    arpReq.ea_hdr.ar_pln = 4;
    arpReq.ea_hdr.ar_hrd = htons(1);
    // change ehternet header

    memcpy(&ethHdrResp.ether_shost, &arpReq.arp_sha, 6);// get mac of me to them
    memcpy(&ethHdrResp.ether_dhost, &broadcast, 6);// set this to the broadcast
    ethHdrResp.ether_type = htons(0x0806);
      // fill the buffer
    memcpy(&buf[0], &ethHdrResp, sizeof(struct ether_header));
    memcpy(&buf[sizeof(struct ether_header)], &arpReq, sizeof(struct ether_arp));
  }

void arpPacketResp(struct interface interfaces[], struct ether_header eh, char *buf){
    printf("Got an ARP PACKET FOR REGULAR\n");
    //build the response for arp
    struct ether_header ethHdrResp;
    struct ether_arp arpReq, arpResp;

    memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));// get the arp info into a header struct
    memcpy(&arpResp.arp_tha, &arpReq.arp_sha, 6); // put the source into teh new packets dst
    int j;
    u_char wantedIp[4];
    memcpy(&wantedIp[0],arpReq.arp_tpa,4);
    for(j = 0; j< 7; j++){
      if(memcmp(interfaces[j].IP, wantedIp, 4) == 0){
          memcpy(&arpResp.arp_sha, &interfaces[j].MAC, 6); // get mmy mac and make the new source
      }
    }
    memcpy(&arpResp.arp_tpa, &arpReq.arp_spa, 4);  // switch ips
    memcpy(&arpResp.arp_spa, &arpReq.arp_tpa, 4); //switch ips
    arpResp.ea_hdr.ar_op = htons(2); // change op code for reply
    arpResp.ea_hdr.ar_pro = arpReq.ea_hdr.ar_pro;
    arpResp.ea_hdr.ar_hln = arpReq.ea_hdr.ar_hln;
    arpResp.ea_hdr.ar_pln = arpReq.ea_hdr.ar_pln;
    arpResp.ea_hdr.ar_hrd = arpReq.ea_hdr.ar_hrd;
    // change ehternet header

    memcpy(&ethHdrResp.ether_shost, &arpResp.arp_sha, 6);// get mac of me to them
    memcpy(&ethHdrResp.ether_dhost, &eh.ether_shost, 6);//
    ethHdrResp.ether_type = eh.ether_type;
      // fill the buffer
    memcpy(&buf[0], &ethHdrResp, sizeof(struct ether_header));
    memcpy(&buf[sizeof(struct ether_header)], &arpResp, sizeof(struct ether_arp));
  }

void icmpPacket(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf){
  struct icmphdr icmpReq;
  struct icmphdr icmpResp;
  memcpy(&icmpReq, &buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], sizeof(struct icmphdr));// get the icmp header
  //swap ethernet header info
  memcpy(&ethResp.ether_shost,&eh.ether_dhost,6);
  memcpy(&ethResp.ether_dhost, &eh.ether_shost,6);
  ethResp.ether_type = eh.ether_type;
  // swap the ip header info
  memcpy(&ipResp.saddr, &ipReq.daddr, 4);
  memcpy(&ipResp.daddr, &ipReq.saddr, 4);
  ipResp.ihl=ipReq.ihl;
  ipResp.version = ipReq.version;
  ipResp.tos = ipReq.tos; //copy inof over
  ipResp.tot_len = ipReq.tot_len;
  ipResp.id = ipReq.id;
  ipResp.frag_off = ipReq.frag_off;
  ipResp.ttl = ipReq.ttl;
  ipResp.protocol = ipReq.protocol;
  ipResp.check = ipReq.check;
  // swap icmp stuff now
  icmpResp.code = icmpReq.code;// set it to echo reply
  icmpResp.type = 0;
  icmpResp.checksum = icmpReq.checksum;
  icmpResp.un.echo.id = icmpReq.un.echo.id;
  icmpResp.un.echo.sequence = icmpReq.un.echo.sequence;

  //fill the buffer again
  memcpy(&buf[0], &ethResp, sizeof(struct ether_header));
  memcpy(&buf[sizeof(struct ether_header)],&ipResp, sizeof(struct iphdr));
  memcpy(&buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], &icmpResp, sizeof(icmpResp));
}
