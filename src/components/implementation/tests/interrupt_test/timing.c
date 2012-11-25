#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sched.h>
#include <malloc.h>
#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>

#define rdtscll(val) \
  __asm__ __volatile__("rdtsc" : "=A" (val))
#define likely(x)     __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)
#define ITR 100000
#define THRESHOLD 500
unsigned long irq_arr[ITR];
unsigned long time_arr[ITR];
static volatile int event_thd = 0;

void
get_irqs(void)
{
  //  int sucv = 0;
  unsigned long i = 0, j = ITR - 1; // For ITR
  unsigned long tsc, endtsc, lastirq = 0, interirq;
  /*
  unsigned long long *irq_arr = malloc((ITR + 1) * sizeof(unsigned long long));
  if(irq_arr == NULL) {
    perror("irq_arr: ");
    exit(-1);
  }
  unsigned long long *time_arr = malloc((ITR + 1) * sizeof(unsigned long long));
  if(time_arr == NULL) {
    perror("time_arr: ");
    exit(-1);
  }
  */
  rdtscll(tsc); 
  for(i = 0; i < ITR; ) {
    rdtscll(endtsc);
    if((endtsc - tsc) > THRESHOLD) {
      if(likely(lastirq != 0)) {
	interirq = tsc - lastirq;
	irq_arr[i] = interirq;
      }
      lastirq = tsc;
      time_arr[i] = endtsc - tsc;
      // add here b/c we'd waste a lot of iterations otherwise
      i++;
    }
 
    if(unlikely(i == j)) {
      //printf("newrep\n");
      for(i = 0; i < ITR; i++) {
       	if(likely(time_arr[i] == 0 || irq_arr[i] == 0))
	  continue;
	printc("%lu %lu\n", time_arr[i], irq_arr[i]);
      }
      i = 0;
      rdtscll(endtsc); // for making up for the print overhead
    }
    tsc = endtsc;
  }
  
  return;
  //pthread_exit(NULL);
}

void 
cos_init(void *arg)
{
  printc("***BEGIN INTERRUPT TEST***\n");

  union sched_param sp;

  sp.c.type = SCHEDP_PRIO;
  sp.c.value = 3;

  if(0 > (event_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0))) BUG();
  
  get_irqs();
  //event_thd = sched    

  printc("***DONE***\n");
  return;
}
