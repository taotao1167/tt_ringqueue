#ifndef __TT_RINGQUEUE_H__
#define __TT_RINGQUEUE_H__

#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TTRingQueue {
	int inited;
	int entry_size;
	int buf_len;
	int put_pos; /* (get_pos == put_pos): empty; (get_pos == (put_pos + 1) % capacity): empty */
	int get_pos;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	void *buffer;
} TTRingQueue;

int ringqueue_init(TTRingQueue *ringq, int capacity, int entry_size);
int ringqueue_destroy(TTRingQueue *ringq);
int ringqueue_put(TTRingQueue *ringq, void *entry);
int ringqueue_tryput(TTRingQueue *ringq, void *entry);
int ringqueue_timedput(TTRingQueue *ringq, void *entry, const struct timespec *tmout);
int ringqueue_get(TTRingQueue *ringq, void *entry);
int ringqueue_tryget(TTRingQueue *ringq, void *entry);
int ringqueue_timedget(TTRingQueue *ringq, void *entry, const struct timespec *tmout);

#ifdef __cplusplus
}
#endif

#endif /* __TT_RINGQUEUE_H__ */
