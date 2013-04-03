#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define LINUX_TEST
#include "../../../translator/translator_ioctl.h"
#include "../../../../../kernel/include/shared/cos_types.h"
#include "../../../../../components/include/cringbuf.h"
#include "../../../../../kernel/include/shared/cos_config.h"

#define PROC_FILE "/proc/translator"
#define MAP_SIZE  COS_PRINT_MEM_SZ 
#define PRINT_CHUNK_SZ (4096*16)
#define HIPRIOPORT 8888
#define LOPRIOPORT 9999
struct cringbuf sharedbuf;

#define IPADDR "192.168.0.104"
//#define IPADDR "10.0.2.8"
#define IFNAME "tun0"
#define P2PPEER "192.168.0.110"
#define MAXIPI 50000

int ratectl = 0;
unsigned long long ipis = 0, total_read = 0, lo = 0, hi = 0;

void
signal_handler(int signo)
{
  printf("IPIs generated: %lld", hi+lo);
  if(ratectl){
      printf(", highprio IPIs: %lld, loprio IPIs: %lld\n", hi, lo);
      lo = hi = ipis = 0;
  } 
  //else 
  //    printf("\n");
  ipis = 0;
}

void 
start_timers()
{
	struct itimerval itv;
	struct sigaction sa;

	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;
	//sigemptyset(&sa.sa_mask);
	sigfillset(&sa.sa_mask);

	if(sigaction(SIGALRM, &sa, NULL)) {
	    perror("Setting up alarm handler");
	    exit(-1);
	}
	memset(&itv, 0, sizeof(itv));
	itv.it_value.tv_sec = 1;
	itv.it_interval.tv_sec = 1;
	if(setitimer(ITIMER_REAL, &itv, NULL)) {
	    perror("setitimer; setting up sigalarm");
	    return;
	}
	return;
}


void *
recv_pkt(void *data)
{
    char **arguments = (char**) data;

    int msg_size = atoi(arguments[1]), sleep_var = atoi(arguments[2]);
    int fdr, ret;
    char buf[] = "ifconfig "IFNAME" inet "IPADDR" netmask 255.255.255.255 pointopoint "P2PPEER;
    struct ifreq ifr;

    int fd, _read = 0;
    void *a;
    char c, buf1[PRINT_CHUNK_SZ];

    printf("Message size is (%d)\n", msg_size);
    memset(buf1, 'b', PRINT_CHUNK_SZ);
   
    // open the tun device
    if((fdr = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("open /dev/net/tun: ");
        exit(-1);
    }
    printf("tun FD (%d) created\n", fdr);
    
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if(ioctl(fdr, TUNSETIFF, (void *) &ifr) < 0) {
        perror("ioctl TUNSETIFF: ");
        close(fdr);
        exit(-1);
    }
    
    printf("Running: %s\n", buf);
    if(system(buf) < 0) {
        perror("setting tun ipaddress: ");
        exit(-1);
    }
    printf("Done setting TUN. \n");

    fd = open(PROC_FILE, O_RDWR);
    if (fd < 0) {
      perror("open");
      exit(-1);
    }

    trans_ioctl_set_channel(fd, COS_TRANS_SERVICE_TERM);
    trans_ioctl_set_direction(fd, COS_TRANS_DIR_LTOC);
    a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == a) {
      perror("mmap");
      exit(-1);
    }
    cringbuf_init(&sharedbuf, a, MAP_SIZE);

    char portstr[7];
    int portnum;
    portstr[6] = '\0';
    printf("going to sleep for a few seconds; hurry and activate composite!\n");
    sleep(sleep_var); 
         
    do {
      int amread = 0;
    
      ret = read(fdr, buf1, msg_size);
      amread += snprintf(portstr, 7, "%#x", buf1[22] & 0xff);
      if((buf1[23] & 0xff) < 16) 
	portstr[amread++] = '0';
        amread += snprintf(portstr+amread, 7, "%x", buf1[23] & 0xff);
        portnum = strtol(portstr, NULL, 16);

      switch(portnum) { 
      case HIPRIOPORT:
	write(fd, &c, 1);
	hi++;
	break;
      case LOPRIOPORT:	
	//if(ipis < MAXIPI) {
	if(lo+hi < MAXIPI) {
	  write(fd, &c, 1);
	  lo++;
	  //ipis++;
	}
	break;
      default:
	break;
      }
    } while(1);

    close(fdr);
    if (munmap(a, MAP_SIZE) < 0) {
      perror("munmap");
      exit(-1);
    }
    pthread_exit(NULL);
}

int 
main(int argc, char *argv[])
{
    // thread stuff
    pthread_t tid_recv;
    struct sched_param sp_recv;
    struct rlimit rl;
    cpu_set_t mask;

    void *thd_ret;

    if (argc != 3) {
        printf("Usage: %s <msg size> <sleep_var>\n", argv[0]);
        return -1;
    }

    // For signal handlers
    ratectl = 1;
    start_timers();

    // Set up rlimits to allow more CPU usage
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    if(setrlimit(RLIMIT_CPU, &rl)) {
        perror("set rlimit: ");
        return -1;
    }
    printf("CPU limit removed\n");

    // Set up the receiver and create a thread for it
    sp_recv.sched_priority = sched_get_priority_max(SCHED_RR);
    if(pthread_create(&tid_recv, NULL, recv_pkt, (void *) argv) != 0) {
        perror("pthread create receiver: ");
        return -1;
    }
    CPU_ZERO(&mask);
    CPU_SET(1, &mask);
    if(pthread_setaffinity_np(tid_recv, sizeof(mask), &mask ) == -1 ) {
        perror("setaffinity recv error: ");
        return -1;
    }
    printf("Set affinity for recv thread\n");

    // We're done here; we'll wait for the threads to do their thing
    pthread_join(tid_recv, &thd_ret);
 
    return 0;
}
