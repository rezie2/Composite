#include <cos_component.h>
#include <torrent.h>
#include <torlib.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <valloc.h>
#include <sched.h>

cos_lock_t l;
#define LOCK() if (lock_take(&l)) BUG();
#define UNLOCK() if (lock_release(&l)) BUG();

#include <cringbuf.h>
struct channel_info {
	int exists, direction;
	/* if channel is read-only (direction = COS_TRANS_DIR_LTOC),
	   there is only one torrent, otherwise, NULL */
	struct torrent *t; 	
	struct cringbuf rb;
} channels[10];

td_t 
tsplit(spdid_t spdid, td_t td, char *param, 
       int len, tor_flags_t tflags, long evtid) 
{
	td_t ret = -1;
	struct torrent *t, *nt;
	int channel, direction;

	LOCK();
	if (tor_isnull(td)) ERR_THROW(-EINVAL, done);
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	if (len > 1) ERR_THROW(-EINVAL, done);

	channel = (int)(*param - '0');
 
	if (channel > 9 || channel < 0) ERR_THROW(-EINVAL, done);
	if (!channels[channel].exists)  ERR_THROW(-ENOENT, done);

	nt = tor_alloc(&channels[channel], tflags);
	if (!nt) ERR_THROW(-ENOMEM, done);
	ret = nt->td;
	
	direction = channels[channel].direction;
	if (direction == COS_TRANS_DIR_LTOC) {
		if (tflags != TOR_READ)  ERR_THROW(-EINVAL, free);
		if (channels[channel].t) ERR_THROW(-EBUSY, free);
	}
	if (direction == COS_TRANS_DIR_CTOL && tflags != TOR_WRITE) ERR_THROW(-EINVAL, free);
	if (direction == COS_TRANS_DIR_LTOC) {
		nt->evtid = evtid;
		channels[channel].t = nt;
	} else {
		nt->evtid = 0;
	}
done:
	UNLOCK();
	return ret;
free:
	tor_free(nt);
	goto done;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	struct torrent *t;
	int ret = 0;

	LOCK();
	if (!tor_is_usrdef(td)) ERR_THROW(-EINVAL, done);
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	/* currently only allow deletion */
	if (td_into != td_null) ERR_THROW(-EINVAL, done);
	assert(t->data);
	((struct channel_info *)t->data)->t = NULL;
	tor_free(t);
done:
	UNLOCK();
	return ret;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;

	if (!tor_is_usrdef(td)) return;
	LOCK();
	t = tor_lookup(td);
	if (!t) goto done;
	assert(t->data);
	((struct channel_info *)t->data)->t = NULL;
	tor_free(t);
done:
	UNLOCK();
	return;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1;
	struct channel_info *channel;
	struct torrent *t;
	char *buf;

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(!tor_is_usrdef(td) || t->data);
	if (!(t->flags & TOR_READ)) ERR_THROW(-EACCES, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) goto done;

	channel = (struct channel_info*)t->data;
	ret = cringbuf_consume(&channel->rb, buf, sz);
done:	
	UNLOCK();
	return ret;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1;
	struct channel_info *channel;
	struct torrent *t;
	char *buf;

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	channel = (struct channel_info*)t->data;
	ret = cringbuf_produce(&channels->rb, buf, sz);
	cos_trans_cntl(COS_TRANS_TRIGGER, 0, 0, 0);

	t->offset += ret;
done:	
	UNLOCK();
	return ret;
}

void channel_thd(int channel) 
{
  unsigned short int bid; 
  
  bid = cos_brand_cntl(COS_BRAND_CREATE, 0, 0, cos_spd_id());
  assert(bid > 0);
  assert(!cos_trans_cntl(COS_TRANS_BRAND, channel, bid, 0));
  if (sched_add_thd_to_brand(cos_spd_id(), bid, cos_get_thd_id())) BUG();
  while (1) {
    int ret;
    //printc("IN TTRANS BEFORE IF\n");
    if (-1 == (ret = cos_brand_wait(bid))) BUG();
    //printc("IN TTRANS AFTER IF\n");
    assert(channels[channel].t);
    //printc("IN TTRANS. EVTID (%lu)\n", channels[channel].t->evtid);
    evt_trigger(cos_spd_id(), channels[channel].t->evtid);
  }
}

static int channel_init(int channel)
{
	char *addr, *start;
	unsigned long i, sz;
	unsigned short int bid;
	int direction;

	direction = cos_trans_cntl(COS_TRANS_DIRECTION, channel, 0, 0);
	if (direction < 0) {
	  printc("no torrent found: direction (%d)\n", direction);
		channels[channel].exists = 0;
		return 0;
	}  
	channels[channel].exists = 1;
	channels[channel].direction = direction;

	sz = cos_trans_cntl(COS_TRANS_MAP_SZ, channel, 0, 0);
	assert(sz <= (4*1024*1024)); /* current 8MB max */
	start = valloc_alloc(cos_spd_id(), cos_spd_id(), sz/PAGE_SIZE);
	assert(start);
	for (i = 0, addr = start ; i < sz ; i += PAGE_SIZE, addr += PAGE_SIZE) {
		assert(!cos_trans_cntl(COS_TRANS_MAP, channel, (unsigned long)addr, i));
	}
	cringbuf_init(&channels[channel].rb, start, sz);

	if (direction == COS_TRANS_DIR_LTOC)
	  // call the bottom block in its own separate function
	  channel_thd(channel);
	return 0;
}

//curr_thd can be set at some COS_TRANS_MIN to make this work with all channels
//To have that work, make sure to add some min to cos_types.h
static volatile int curr_thd = COS_TRANS_SERVICE_PRINT, first = 1; 

int cos_init(void)
{
	if(first) {
	  int i = curr_thd;
	  union sched_param sp;
	
	  first = 0;
	  lock_static_init(&l);
	  torlib_init();
	  
	  while(i < COS_TRANS_SERVICE_MAX) {
	    sp.c.type = SCHEDP_PRIO;
	    sp.c.value = i++;
	    if(sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
	  }
	  return 0;
	}
	printc("CHANNEL WITH PRIO (%d) STARTED\n", curr_thd);
	channel_init(curr_thd++);
	return 0;
}
