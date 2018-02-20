#include <sys/socket.h> 
#include <netpacket/packet.h> 
#include <net/ethernet.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
//#include <if_ether.h>
#include <netinet/in.h>
#include <string.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
//#include <netinet/in.h>
#include <arpa/inet.h>
//Allison Bolen, Cade Baker, Andy Hung



int main(){

  fd_set sockets;
  FD_ZERO(&sockets);

  int packet_socket;
  //get list of interface addresses. This is a linked list. Next
  //pointer is in ifa_next, interface name is in ifa_name, address is
  //in ifa_addr. You will have multiple entries in the list with the
  //same name, if the same interface has multiple addresses. This is
  //common since most interfaces will have a MAC, IPv4, and IPv6
  //address. You can use the names to match up which IPv4 address goes
  //with which MAC address.
  struct ifaddrs *ifaddr, *tmp;
  if(getifaddrs(&ifaddr)==-1){
    perror("getifaddrs");
    return 1;
  }
  struct interface {
        char* name;
        uint8_t MAC[6];
        uint8_t IP[4];
  };
  struct interface interfaces[20];

  //  fd_set tmp_set = sockets;
  //have the list, loop over the list
  int count =0;
  for(tmp = ifaddr; tmp!=NULL; tmp=tmp->ifa_next){
    //Check if this is a packet address, there will be one per
    //interface.  There are IPv4 and IPv6 as well, but we don't care
    //about those for the purpose of enumerating interfaces. We can
    //use the AF_INET addresses in this list for example to get a list
    //of our own IP addresses
    if(tmp->ifa_addr->sa_family==AF_PACKET){
      printf("Interface: %s\n",tmp->ifa_name);
      //create a packet socket on interface r?-eth1
      if(!strncmp(&(tmp->ifa_name[3]),"eth",3)){
	printf("Creating Socket on interface %s\n",tmp->ifa_name);
	//create a packet socket
	//AF_PACKET makes it a packet socket
	//SOCK_RAW makes it so we get the entire packet
	//could also use SOCK_DGRAM to cut off link layer header
	//ETH_P_ALL indicates we want all (upper layer) protocols
	//we could specify just a specific one
	packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(packet_socket<0){
	  perror("socket");
	  return 2;
	}
        FD_SET(packet_socket, &sockets);
        struct sockaddr_ll *addrLL;
        addrLL = (struct sockaddr_ll *)(tmp->ifa_addr);
        printf("\t InterFace MAC: %s\n", ether_ntoa((struct ether_addr*)(addrLL->sll_addr)));
        memcpy(&interfaces[count].MAC,&((struct sockaddr_ll*)tmp->ifa_addr)->sll_addr,6);
        interfaces[count].name = tmp->ifa_name;
        memcpy(&interfaces[count].IP, &((struct sockaddr_in*)tmp->ifa_addr)->sin_addr.s_addr,4);
        count++;
        printf("\nMAC in Interface STRUCT: %s\n", ether_ntoa( (struct ether_addr*) &interfaces[count].MAC ));
     	//Bind the socket to the address, so we only get packets
	//recieved on this specific interface. For packet sockets, the
	//address structure is a struct sockaddr_ll (see the man page
	//for "packet"), but of course bind takes a struct sockaddr.
	//Here, we can use the sockaddr we got from getifaddrs (which
	//we could convert to sockaddr_ll if we needed to)
	if(bind(packet_socket,tmp->ifa_addr,sizeof(struct sockaddr_ll))==-1){
	  perror("bind");
	}
      }
    }
  }
  //loop and recieve packets. We are only looking at one interface,
  //for the project you will probably want to look at more (to do so,
  //a good way is to have one socket per interface and use select to
  //see which ones have data)
  printf("Ready to recieve now\n");
  while(1){
    char buf[1500];
    struct sockaddr_ll recvaddr;
    int recvaddrlen=sizeof(struct sockaddr_ll);
    fd_set tmp_set = sockets;
    select(FD_SETSIZE,&tmp_set, NULL, NULL, NULL);
    //we can use recv, since the addresses are in the packet, but we
    //use recvfrom because it gives us an easy way to determine if
    //this packet is incoming or outgoing (when using ETH_P_ALL, we
    //see packets in both directions. Only outgoing can be seen when
    //using a packet socket with some specific protocol)
    //for an intertor see if we have stuff to read that if from the lab and recive the packets theu are on 
    //teh same sockect jsut parse tha packets
    int i; 

    for(i=0; i<FD_SETSIZE;i++){
      if(FD_ISSET(i,&tmp_set)){
      int n = recvfrom(i, buf, 1500,0,(struct sockaddr*)&recvaddr, &recvaddrlen);
      //ignore outgoing packets (we can't disable some from being sent
      //by the OS automatically, for example ICMP port unreachable
      //messages, so we will just ignore them here)
        if(recvaddr.sll_pkttype==PACKET_OUTGOING){
          continue;
        }
        struct ether_header eh;
        memcpy(&eh,&buf[0],14);
        int type = ntohs(eh.ether_type);

        if(type == 0x0806){ // got an arp packet
          printf("got a packet in arp\n");
          //build the response for arp
          struct ether_header ethHdrResp;
          struct ether_arp arpReq, arpResp;

          memcpy(&arpReq, &buf[sizeof(struct ether_header)], sizeof(struct ether_arp));
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
          memcpy(&ethHdrResp.ether_dhost, &eh.ether_shost, 6);
          ethHdrResp.ether_type = eh.ether_type;

          memcpy(&buf[0], &ethHdrResp, sizeof(struct ether_header));
          memcpy(&buf[sizeof(struct ether_header)], &arpResp, sizeof(struct ether_arp));

          send(i, buf, 42, 0);// send the arp
        }

        if(type == ETHERTYPE_IP){ // got an icmp packet


          //start processing all others
          //need
//           struct iphdr ipHdr;
//           struct iphdr temIp;
//           struct ether_header ethHdr;
//           struct ether_header temEth;
//           struct icmphdr *icmpHdr = malloc(sizeof(struct icmpHdr));
        }
      //what else to do is up to you, you can send packets with send,
      //just like we used for TCP sockets (or you can use sendto, but it
      //is not necessary, since the headers, including all addresses,
      //need to be in the buffer you are sending)
    }
  }
  //free the interface list when we don't need it anymore

}
freeifaddrs(ifaddr);
  //exit
  return 0;
}
