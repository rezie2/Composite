#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <torrent.h>
#include <evt.h>
#include <interrupt.h>
#include <sched.h>
/*
//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif
*/
#define SIZE 6
//#define ITR 1000

//static volatile int cur_itr = 0;
void trans_recv_lo(void)
{  
  unsigned int amnt_lo = 1;
  long evt_lo;
  td_t td_lo;
  char *params_lo = "8";

  printc("***LOW PRIO RECV STARTING***\n");
  evt_lo = evt_split(cos_spd_id(), 0, 0);
  assert(evt_lo > 0);
  td_lo = tsplit(cos_spd_id(), td_root, params_lo, strlen(params_lo), TOR_READ, evt_lo);
  printc("EVT_LO (%ld) TD_LO (%d)\n", evt_lo, td_lo);

  do {
    evt_wait(cos_spd_id(), evt_lo);
    //   if((amnt_lo++ % 1000) == 0)
    printc("lo prio count (%u) spd(%d) tid(%d)\n", amnt_lo++, cos_spd_id(), cos_get_thd_id());
  } while (1);//cur_itr++ < ITR);
  
  return;
}

void trans_recv_hi(void)
{ 
  unsigned int amnt_hi = 1;
  long evt_hi;
  td_t td_hi;
  char *params_hi = "7";

  printc("***HIGH PRIO RECV STARTING***\n");
  evt_hi = evt_split(cos_spd_id(), 0, 0);
  assert(evt_hi > 0);
  td_hi = tsplit(cos_spd_id(), td_root, params_hi, strlen(params_hi), TOR_READ, evt_hi);
  printc("EVT_HI (%ld) TD_HI (%d)\n", evt_hi, td_hi);

  do {
    evt_wait(cos_spd_id(), evt_hi);
    // if((amnt_hi++ % 1000) == 0)
    printc("hi prio count (%u) spd(%d) tid(%d)\n", amnt_hi++, cos_spd_id(), cos_get_thd_id());
  } while (1);//cur_itr++ < ITR);
  return;
}

static volatile int first_lo = 1, first_hi = 1, prios = 0, first = 1;
//volatile union sched_param sp;

void
cos_init(void *arg)
{
  printc("  **BEGIN RECV TRANS**\n");
  
  if(first) {
    first = 0;
    union sched_param sp;
    int prio = 11;
    while(prios++ < 2) {
      sp.c.type = SCHEDP_PRIO;
      sp.c.value = prio;
      if(sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
      printc("*trans.c: thread started!\n");
    }
    return;
  }
  if(first_hi && first_lo) {
    first_hi = 0;
    trans_recv_hi();
  }
  if(!first_hi && first_lo) {
    first_lo = 0;
    trans_recv_lo();
  }
    
    
  
  /*
  //  union sched_param sp;
  if(first_hi) {
    first_hi = 0;
    sp.c.type = SCHEDP_PRIO;
    sp.c.value = 8;
    if(sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
    return;
  }
  //trans_recv_lo();
  
  if(!first_hi && first_lo) { 
    first_lo = 0;
    sp.c.value = 7;
    if(sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
    trans_recv_hi();
  }
  if(!first_hi && !first_lo) 
     trans_recv_lo();
  */
  printc("  **RECV TRANS DONE**\n");
  return;
}

