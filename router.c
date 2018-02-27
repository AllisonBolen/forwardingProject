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
//noice
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

void arpPacket(struct interface interfaces[], struct ether_header eh, char *buf);
void readFiles(struct table tableInfo[7]);
void icmpPacket(struct interface interfaces[], struct ether_header eh, struct iphdr ipReq, struct ether_header ethResp, struct iphdr ipResp, char *buf);
char saveICMPBuffer[1500];
void saveICMP(char* buf);

int main(){
    struct interface interfaces[7];
    struct table tableInfo[7];
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

      readFiles(tableInfo);

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
            arpPacket(interfaces, eh, buf);
            send(i, buf, 42, 0);
          }
          if(ntohs(arpReq.ea_hdr.ar_op)==2){
            // we got a Response
            // send data to the ting that we got a response from
          }
          //is this arp for the router?

          // checkdestination(buf);

          // arpPacket(interfaces, eh, buf);
          // send(i, buf, 42, 0);// send the arp
        }

        if(type == ETHERTYPE_IP){ // got an icmp packet
  	      printf("%s\n", "Received IP Packet");
          struct iphdr ipReq;
          struct iphdr ipResp;
          //struct ether_header ethHdr;
          struct ether_header ethResp;
          memcpy(&ipReq, &buf[sizeof(struct ether_header)], sizeof(struct iphdr)); // get the ip header
          // is it a request or a reply
          if((ipReq.protocol) == 1){
            struct icmphdr icmpReq;
            memcpy(&icmpReq, &buf[(sizeof(struct ether_header) + sizeof(struct iphdr))], sizeof(struct icmphdr));// get the icmp header

            if(icmpReq.type == 8){
              // got an ICMP request packet
              // standin Response
              printf("%s\n", "Received ICMP Request Packet");
              icmpPacket(interfaces, eh, ipReq, ethResp, ipResp, buf);
              send(i,buf, 98, 0);
    		      printf("%s\n", "Sending ICMP Response");
              // this is where we need to find what the destination is and send it off to that one
              // store the icmp stuff in a buffer array
              //
              // request an arp on that socket if it matches whats wanted in the ip header target host
            }
            if(icmpReq.type == 0){
              // got a ICMP reply
              printf("hit this so we got an icmp packet reply.\n");
            }
          }
          else{
            // we just got an ip packet make sure to forward it or ignore it? not sure????
            printf("hit the else so we didnt get an icmp packet but it was an IP packet\n.");
          }
        }
      }
    }
  }
  freeifaddrs(ifaddr);
  return 0;
}
// return one if its the destiontion of the router
// int checkdestination(buf, struct interface intefaces[]){
//   struct ether_header ethReq;
//   memcpy(&ethReq, &buf[0], 14);
//   int i;
//   for(i = 0 ; i < sizeof(interfaces); i++){
//     if(memcmp(ethReq.ether_dhost==interfaces[i].MAC)){// may have an issue
//
//     }
//   }
// }

// check the prefix for the next thing if its
// int checkRouteNext(buf){
//
// }

// populate table struct
void readFiles(struct table tableInfo[7]){
    printf("here2\n");
    char filename[12];
    fgets(filename, 13, stdin);
    printf("\nfilename: %s\n", filename);
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL)
    {
        printf("Cannot open file \n");
        exit(0);
    }
    else{
      printf("here3");
      int count = 0;
      char pref[10], ipaddr[10], name[10];
      while(fscanf(fptr, "%s %s %s", pref, ipaddr, name) != EOF){
        tableInfo[count].name = strdup(name);
        tableInfo[count].prefix = strdup(pref);
        //if(strcmp(ipaddr, "-") != 0){
          // in_addr_t actualIPaddr = inet_addr(ipaddr);
          // memcpy(tableInfo[count].ip,&actualIPaddr,4);
        tableInfo[count].ip = strdup(ipaddr);
        //}
        printf("\nthis is that table name at count: %s\n ", tableInfo[count].name);
        count ++;
      }
    }
    fclose(fptr);
  }

  void arpPacket(struct interface interfaces[], struct ether_header eh, char *buf){
    printf("Got an ARP PACKET\n");
    //build the response for arp
    struct ether_header ethHdrResp;
    struct ether_arp arpReq, arpResp;

    memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));// get the arp info into a header struct
    memcpy(&arpResp.arp_tha, &arpReq.arp_sha, 6); // put the source into teh new packets dst
    int j;
    u_char wantedIp[4];
    memcpy(&wantedIp[0],arpReq.arp_tpa,4);
    for(j = 0; j< 20; j++){
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

void saveICMP(char *buf){
  memcpy(saveICMPBuffer, buf, sizeof(saveICMPBuffer));
  //printf("%s\n", saveICMPBuffer);
}
