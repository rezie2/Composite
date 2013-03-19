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

#include <sys/mman.h>

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
#define MAP_SIZE  COS_PRINT_MEM_SZ //(4096 * 256)
#define PRINT_CHUNK_SZ (4096*16)
#define HIPRIOPORT 8888
#define LOPRIOPORT 9999
struct cringbuf sharedbuf;

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITR 1000000 // # of iterations we want
volatile unsigned long long stsc; // timer thread timestamp 
volatile int done = 0; 
unsigned long long timer_arr[ITR];

#define IPADDR "192.168.0.104"
//#define IPADDR "10.0.2.8"
#define IFNAME "tun0"
#define P2PPEER "192.168.0.110"

void *
timer_thd(void *data)
{
    while(1) {
        rdtscll(stsc);
        if(done) {
            break;
        }
    }
    pthread_exit(NULL);
}

void *
recv_pkt(void *data)
{
    char **arguments = (char**) data;

    int msg_size = atoi(arguments[2]);
    int fdr, i = ITR, j = 0, ret;
    unsigned long long tsc;
    char *msg;
    char buf[] = "ifconfig "IFNAME" inet "IPADDR" netmask 255.255.255.255 pointopoint "P2PPEER;
    struct ifreq ifr;

    int fd, _read = 0;
    void *a;
    char c, buf1[PRINT_CHUNK_SZ];

    msg = malloc(msg_size);
    printf("Message size is (%d)\n", msg_size);
    memset(msg, 'a', msg_size);
    memset(buf1, 'b', PRINT_CHUNK_SZ);
    printf("buf1 strlen is (%d)\n", strlen(buf1));
   
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

    trans_ioctl_set_channel(fd, COS_TRANS_SERVICE_TERM);//PONG);
    trans_ioctl_set_direction(fd, COS_TRANS_DIR_LTOC);
    a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == a) {
      perror("mmap");
      exit(-1);
    }
    cringbuf_init(&sharedbuf, a, MAP_SIZE);
    /* wait for user to start */
    //	_read = read(0, buf1, PRINT_CHUNK_SZ);

    int k;
    //char *str="HELLOHELLOHELLOHELLO";
    char buff[6], portstr[10], *pend;
    int portnum;
    buff[0] = 'H';  buff[1] = 'E'; buff[2] = 'L'; buff[3] = 'L'; buff[4] = 'O';  buff[5] = '\0';
    printf("bufflen is (%d)\n", strlen(buff));
    printf("going to sleep for 5 seconds. hurry and activate composite!\n");
    //sleep(5); 
    int hi = 0, lo = 0, unrecog = 0;
    while (i) {
      int off = 0, portnum_index = 0, amread = 0;
      unsigned long long *p;

      ret = read(fdr, buf1, msg_size);
      //for(k = 22; k < 24; k++) {
      amread += snprintf(portstr+amread, 10, "%#x", buf1[22] & 0xff);
      if((buf1[23] & 0xff) < 16) 
	portstr[amread++] = '0';
      amread += snprintf(portstr+amread, 10, "%x", buf1[23] & 0xff);
      //  printf("port number is int(%d) hex(%#x)\n", buf1[22], buf1[22] & 0xff);
      
      //printf("port number is int(%d) hex(%#x)\n", buf1[23] & 0xff, buf1[23] & 0xff);
	// }
      //printf("amount read: (%d) portstr(%s)\n", amread, portstr);
      portstr[amread] = '\0';
      portnum = strtol(portstr, NULL, 16);
      //printf("portstr is (%s) ; converted is (%d)\n", portstr, portnum);
      //sleep(5);
      //      printf("buf1[%d] = (%c)\n", k, buf1[k]);
      //return;
      //_read = 8 + 1;
      //_read = ret;
      //p = buf1;
      //*p = stsc;
      //*(buf1+8) = *msg;
      // rdtscll(*p);
            
      switch(portnum) {
        case HIPRIOPORT:
	  // printf("high priority port\n");
	  hi++;
	  break;
        case LOPRIOPORT:
	  //printf("low priority port (%d)\n", portnum);
	  lo++;
	  break;
        default:
	  //printf("uncategorized port (%d)\n", portnum);
	  unrecog++;
	  break;
      }
      //sleep(5);
      /*
      do {
	int p, u;
	p = cringbuf_produce(&sharedbuf, buff, strlen(buff));//_read);
	printf("Produced string p(%d)\n",  p);
	_read -= p;
	off += p;
	if (p) {
	  printf("waking composite\n");
	  write(fd, &c, 1);
	}
	if(u = usleep(2500)) {
	  printf("sleep return error: u(%d)\n", u);
	  return;
	}
      } while (_read);
      j += 1;
      */
      i--;
      if(!(i % 100000)) printf("100k done i(%d)\n", i);
    }
    printf("ITERATIONS DONE. HI(%d) LO(%d)\n", hi, lo);
    done = 1;
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
    pthread_t tid_timer, tid_recv;
    struct sched_param sp_timer, sp_recv;
    struct rlimit rl;
    cpu_set_t mask;

    void *thd_ret;

    if (argc != 3) {
        printf("Usage: %s <in_port> <msg size>\n", argv[0]);
        return -1;
    }

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
    /*    
    if(pthread_setschedparam(tid_recv, SCHED_RR, &sp_recv) != 0) {
        perror("pthread setsched recv: ");
        return -1;
    }
    printf("receiver priority: (%d)\n", sp_recv.sched_priority);
    */
    /*
    // Set up the timer and create a thread for it
    sp_timer.sched_priority = (sched_get_priority_max(SCHED_RR) - 1);
    if(pthread_create(&tid_timer, NULL, timer_thd, NULL) != 0) {
        perror("pthread create timer: ");
        return -1;
    }
    */
    /*
    if(pthread_setschedparam(tid_timer, SCHED_RR, &sp_timer) != 0) {
        perror("pthread setsched timer: ");
        return -1;
    }
    printf("timer priority: (%d)\n", sp_timer.sched_priority);
    */
    /*
    // Set up processor affinity for both threads
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if(pthread_setaffinity_np(tid_timer, sizeof(mask), &mask ) == -1 ) {
        perror("setaffinity timer error: ");
        return -1;
    }
    printf("Set affinity for timer thread\n");
    */
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if(pthread_setaffinity_np(tid_recv, sizeof(mask), &mask ) == -1 ) {
        perror("setaffinity recv error: ");
        return -1;
    }
    printf("Set affinity for recv thread\n");

    // We're done here; we'll wait for the threads to do their thing
    pthread_join(tid_recv, &thd_ret);
    //pthread_join(tid_timer, &thd_ret);

    return 0;
}
