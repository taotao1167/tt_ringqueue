#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "tt_ringqueue.h"

int ringqueue_destroy(TTRingQueue *ringq) {
	int ret = -1;

	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}
	if (ringq->buffer != NULL) {
		free(ringq->buffer);
	}
	pthread_mutex_destroy(&(ringq->lock));
	pthread_cond_destroy(&(ringq->cond));
	ret = 0;
func_end:
	return ret;
}

int ringqueue_init(TTRingQueue *ringq, int capacity, int entry_size) {
	int ret = -1;
	pthread_condattr_t cond_attr;

	memset(ringq, 0x00, sizeof(TTRingQueue));
	ringq->buf_len = capacity + 1; /* add 1 for full judge */
	ringq->entry_size = entry_size;
	ringq->buffer = malloc(entry_size * ringq->buf_len);
	if (ringq->buffer == NULL) {
		goto func_end;
	}
	memset(ringq->buffer, 0x00, entry_size * ringq->buf_len);
	ret = pthread_mutex_init(&(ringq->lock), NULL);
	if (ret != 0) {
		goto func_end;
	}
	ret = pthread_condattr_init(&cond_attr);
	if (ret != 0) {
		goto func_end;
	}
	ret = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
	if (ret != 0) {
		goto func_end;
	}
	ret = pthread_cond_init(&(ringq->cond), &cond_attr);
	if (ret != 0) {
		goto func_end;
	}
	ringq->inited = 1;
	ret = 0;
func_end:
	return ret;
}

int ringqueue_put(TTRingQueue *ringq, void *entry) {
	int ret = -1;

	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}
	pthread_mutex_lock(&(ringq->lock));
	while ((ringq->put_pos + 1) % ringq->buf_len == ringq->get_pos) { /* full */
		pthread_cond_wait(&(ringq->cond), &(ringq->lock));
	}
	memcpy(ringq->buffer + (ringq->entry_size * ringq->put_pos), entry, ringq->entry_size);
	ringq->put_pos = (ringq->put_pos + 1) % ringq->buf_len;
	pthread_cond_signal(&(ringq->cond));
	pthread_mutex_unlock(&(ringq->lock));
	ret = 0;
func_end:
	return ret;
}

int ringqueue_tryput(TTRingQueue *ringq, void *entry) {
	int ret = -1;

	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}
	pthread_mutex_lock(&(ringq->lock));
	if ((ringq->put_pos + 1) % ringq->buf_len == ringq->get_pos) { /* full */
		goto func_end;
	}
	memcpy(ringq->buffer + (ringq->entry_size * ringq->put_pos), entry, ringq->entry_size);
	ringq->put_pos = (ringq->put_pos + 1) % ringq->buf_len;
	pthread_cond_signal(&(ringq->cond));
	ret = 0;
func_end:
	pthread_mutex_unlock(&(ringq->lock));
	return ret;
}

int ringqueue_timedput(TTRingQueue *ringq, void *entry, const struct timespec *tmout) {
	int ret = -1;
	struct timespec tmutil, now;

	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}
	pthread_mutex_lock(&(ringq->lock));
	if ((ringq->put_pos + 1) % ringq->buf_len == ringq->get_pos) { /* full */
		clock_gettime(CLOCK_MONOTONIC, &tmutil);
		tmutil.tv_sec += tmout->tv_sec;
		tmutil.tv_nsec += tmout->tv_nsec;
		if (tmutil.tv_nsec >= 1000000000) {
			tmutil.tv_nsec -= 1000000000;
			tmutil.tv_sec += 1;
		}
		while (1) {
			pthread_cond_timedwait(&(ringq->cond), &(ringq->lock), &tmutil);
			if ((ringq->put_pos + 1) % ringq->buf_len != ringq->get_pos) { /* not full */
				break;
			}
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (tmutil.tv_sec < now.tv_sec || (tmutil.tv_sec == now.tv_sec && tmutil.tv_nsec < now.tv_nsec)) {
				break;
			}
		}
	}
	if ((ringq->put_pos + 1) % ringq->buf_len == ringq->get_pos) { /* timeout, still full */
		goto func_end;
	}
	memcpy(ringq->buffer + (ringq->entry_size * ringq->put_pos), entry, ringq->entry_size);
	ringq->put_pos = (ringq->put_pos + 1) % ringq->buf_len;
	pthread_cond_signal(&(ringq->cond));
	ret = 0;
func_end:
	pthread_mutex_unlock(&(ringq->lock));
	return ret;
}

int ringqueue_get(TTRingQueue *ringq, void *entry) {
	int ret = -1;
	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}

	pthread_mutex_lock(&(ringq->lock));
	while (ringq->put_pos == ringq->get_pos) { /* empty */
		pthread_cond_wait(&(ringq->cond), &(ringq->lock));
	}
	memcpy(entry, ringq->buffer + (ringq->entry_size * ringq->get_pos), ringq->entry_size);
	memset(ringq->buffer + (ringq->entry_size * ringq->get_pos), 0x00, ringq->entry_size);
	ringq->get_pos = (ringq->get_pos + 1) % ringq->buf_len;
	pthread_cond_signal(&(ringq->cond));
	pthread_mutex_unlock(&(ringq->lock));
	ret = 0;
func_end:
	return ret;
}

int ringqueue_tryget(TTRingQueue *ringq, void *entry) {
	int ret = -1;

	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}
	pthread_mutex_lock(&(ringq->lock));
	memset(entry, 0x00, ringq->entry_size);
	if (ringq->put_pos == ringq->get_pos) { /* empty */
		errno = EAGAIN;
		goto func_end;
	}
	memcpy(entry, ringq->buffer + (ringq->entry_size * ringq->get_pos), ringq->entry_size);
	memset(ringq->buffer + (ringq->entry_size * ringq->get_pos), 0x00, ringq->entry_size);
	ringq->get_pos = (ringq->get_pos + 1) % ringq->buf_len;
	pthread_cond_signal(&(ringq->cond));
	ret = 0;
func_end:
	pthread_mutex_unlock(&(ringq->lock));
	return ret;
}

int ringqueue_timedget(TTRingQueue *ringq, void *entry, const struct timespec *tmout) {
	int ret = -1;
	struct timespec tmutil, now;

	if (!ringq->inited) {
		errno = EINVAL;
		goto func_end;
	}
	pthread_mutex_lock(&(ringq->lock));
	memset(entry, 0x00, ringq->entry_size);
	if (ringq->put_pos == ringq->get_pos) { /* empty */
		clock_gettime(CLOCK_MONOTONIC, &tmutil);
		tmutil.tv_sec += tmout->tv_sec;
		tmutil.tv_nsec += tmout->tv_nsec;
		if (tmutil.tv_nsec >= 1000000000) {
			tmutil.tv_nsec -= 1000000000;
			tmutil.tv_sec += 1;
		}
		while (1) {
			pthread_cond_timedwait(&(ringq->cond), &(ringq->lock), &tmutil);
			if (ringq->put_pos != ringq->get_pos) { /* has entry */
				break;
			}
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (tmutil.tv_sec < now.tv_sec || (tmutil.tv_sec == now.tv_sec && tmutil.tv_nsec < now.tv_nsec)) {
				break;
			}
		}
	}
	if (ringq->put_pos == ringq->get_pos) { /* timeout, still empty */
		errno = EAGAIN;
		goto func_end;
	}
	memcpy(entry, ringq->buffer + (ringq->entry_size * ringq->get_pos), ringq->entry_size);
	ringq->get_pos = (ringq->get_pos + 1) % ringq->buf_len;
	pthread_cond_signal(&(ringq->cond));
	ret = 0;
func_end:
	pthread_mutex_unlock(&(ringq->lock));
	return ret;
}

int main() { /* rename to main for run standalone */
	int ret = -1, i = 0, data = 0;
	TTRingQueue ringq;
	struct timespec tmout;

	tmout.tv_sec = 1;
	tmout.tv_nsec = 0;

	ringqueue_init(&ringq, 5, sizeof(int));
#if 0
	for (i = 0; i < 5; i++) {
		data = i + 1;
		printf("before ringqueue_put[%d], %d\n", i, data);
		ret = ringqueue_put(&ringq, &data);
		printf("after ringqueue_put[%d] ret %d\n", i, ret);
	}
#endif
#if 0
	for (i = 0; i < 6; i++) {
		data = i + 1;
		printf("before ringqueue_tryput[%d], %d\n", i, data);
		ret = ringqueue_tryput(&ringq, &data);
		printf("after ringqueue_tryput[%d] ret %d\n", i, ret);
	}
#endif
#if 1
	for (i = 0; i < 6; i++) {
		data = i + 1;
		printf("before ringqueue_timedput[%d], %d\n", i, data);
		ret = ringqueue_timedput(&ringq, &data, &tmout);
		printf("after ringqueue_timedput[%d] ret %d\n", i, ret);
	}
#endif
#if 0
	for (i = 0; i < 6; i++) {
		printf("before ringqueue_get[%d]\n", i);
		ret = ringqueue_get(&ringq, &data);
		if (ret == 0) {
			printf("after ringqueue_get[%d], get %d\n", i, data);
		} else {
			printf("after ringqueue_get[%d] error, ret %d\n", i, ret);
		}
	}
#endif
#if 0
	for (i = 0; i < 6; i++) {
		printf("before ringqueue_tryget[%d]\n", i);
		ret = ringqueue_tryget(&ringq, &data);
		if (ret == 0) {
			printf("after ringqueue_tryget[%d], get %d\n", i, data);
		} else {
			printf("after ringqueue_tryget[%d] error, ret %d\n", i, ret);
		}
	}
#endif
#if 1
	for (i = 0; i < 6; i++) {
		printf("before ringqueue_timedget[%d]\n", i);
		ret = ringqueue_timedget(&ringq, &data, &tmout);
		if (ret == 0) {
			printf("after ringqueue_timedget[%d], get %d\n", i, data);
		} else {
			printf("after ringqueue_timedget[%d] error, ret %d\n", i, ret);
		}
	}
#endif
	ringqueue_destroy(&ringq);
	return 0;
}

