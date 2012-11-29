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

#define ITR 10000
#define THRESHOLD 500
#define REP 0
unsigned long long irq_arr[ITR];
unsigned long long time_arr[ITR];
static volatile int event_thd = 0;

void
get_irqs(void)
{
  //  int sucv = 0;
  unsigned long long i = 0, j = ITR - 1; // For ITR
  unsigned long long tsc, endtsc, lastirq = 0, interirq;
  int rep = 0;

  rdtscll(tsc); 
  for(i = 0; i < ITR; ) {
    rdtscll(endtsc);
    //      printc("tsc(%lu) lastirq(%lu) endtsc(%lu)\n", tsc, lastirq, endtsc);      
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
      for(i = 0; i < ITR; i++) {
       	if(likely(time_arr[i] == 0 || irq_arr[i] == 0))
	  continue;
	//printc("t(%u) i(%u)\n", time_arr[i], irq_arr[i]);
	printc("(%llu) [%llu]\n", time_arr[i], irq_arr[i]); 
      }
      if(rep < REP)
	rep++;
      else
	return;
      i = 0;
      rdtscll(endtsc); // for making up for the print overhead
    }
    tsc = endtsc;
  }
  
  return;
}

static volatile int first = 1;

void 
cos_init(void *arg)
{
  
  printc("***BEGIN INTERRUPT TEST***\n");

  union sched_param sp;

  if(cos_get_thd_id() == event_thd) get_irqs();

  sp.c.type = SCHEDP_PRIO;
  sp.c.value = 3;
  if(first) {
    first = 0;
    if(0 > (event_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0))) BUG();;
  }

  printc("***DONE***\n");
  return;
}
