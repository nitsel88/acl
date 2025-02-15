#include "StdAfx.h"
#ifndef ACL_PREPARE_COMPILE

#include "stdlib/acl_define.h"
#ifdef	ACL_UNIX
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "stdlib/acl_sys_patch.h"
#include "stdlib/acl_mymalloc.h"
#include "stdlib/acl_msg.h"
#include "stdlib/acl_dbuf_pool.h"

#endif

typedef struct ACL_DBUF {
        struct ACL_DBUF *next;
        char *ptr;
        char  buf_addr[1];
} ACL_DBUF;

struct ACL_DBUF_POOL {
        size_t block_size;
        ACL_DBUF *head;
	char  buf_addr[1];
};

ACL_DBUF_POOL *acl_dbuf_pool_create(size_t block_size)
{
	ACL_DBUF_POOL *pool;
	size_t size;
	int    page_size;

#ifdef ACL_UNIX
	page_size = getpagesize();
#elif defined(ACL_WINDOWS)
	SYSTEM_INFO info;

	memset(&info, 0, sizeof(SYSTEM_INFO));
	GetSystemInfo(&info);
	page_size = info.dwPageSize;
	if (page_size <= 0)
		page_size = 4096;
#else
	page_size = 4096;
#endif

	size = (block_size / (size_t) page_size) * (size_t) page_size;
	if (size == 0)
		size = page_size;

#ifdef	USE_VALLOC
	pool = (ACL_DBUF_POOL*) valloc(sizeof(struct ACL_DBUF_POOL)
			+ sizeof(ACL_DBUF) + size);
#else
	pool = (ACL_DBUF_POOL*) acl_mymalloc(sizeof(struct ACL_DBUF_POOL)
			+ sizeof(ACL_DBUF) + size);
#endif

	pool->block_size = size;
	pool->head = (ACL_DBUF*) pool->buf_addr;
	pool->head->next = NULL;
	pool->head->ptr = pool->head->buf_addr;
	return pool;
}

void acl_dbuf_pool_destroy(ACL_DBUF_POOL *pool)
{
	ACL_DBUF *iter = pool->head, *tmp;

	while (iter) {
		tmp = iter;
		iter = iter->next;
		if ((char*) tmp == pool->buf_addr)
			break;
#ifdef	USE_VALLOC
		free(tmp);
#else
		acl_myfree(tmp);
#endif
	}

#ifdef	USE_VALLOC
	free(pool);
#else
	acl_myfree(pool);
#endif
}

void acl_dbuf_pool_reset(ACL_DBUF_POOL *pool, size_t off)
{
	ACL_DBUF *iter = pool->head, *tmp;

	while (iter) {
		tmp = iter;
		iter = iter->next;
		if ((char*) tmp == pool->buf_addr)
			break;
#ifdef	USE_VALLOC
		free(tmp);
#else
		acl_myfree(tmp);
#endif
	}
	pool->head = (ACL_DBUF*) pool->buf_addr;
	pool->head->next = NULL;

	if (pool->head->buf_addr + off >= (char*) pool + pool->block_size)
		acl_msg_fatal("%s(%d) off(%ld) too big, should < %ld",
			__FUNCTION__, __LINE__, (long) off, (long)
			((char*) pool + pool->block_size
			 - pool->head->buf_addr));

	pool->head->ptr = pool->head->buf_addr + off;
}

static ACL_DBUF *acl_dbuf_alloc(ACL_DBUF_POOL *pool, size_t length)
{
#ifdef	USE_VALLOC
	ACL_DBUF *dbuf = (ACL_DBUF*) valloc(sizeof(ACL_DBUF) + length);
#else
	ACL_DBUF *dbuf = (ACL_DBUF*) acl_mymalloc(sizeof(ACL_DBUF) + length);
#endif
	dbuf->next = NULL;
	dbuf->ptr = (void*) dbuf->buf_addr;

	if (pool->head == NULL)
		pool->head = dbuf;
	else {
		dbuf->next = pool->head;
		pool->head = dbuf;
	}
	return dbuf;
}

void *acl_dbuf_pool_alloc(ACL_DBUF_POOL *pool, size_t length)
{
	void *ptr;
	ACL_DBUF *dbuf;

	length += length % 4;

	if (length > pool->block_size)
		dbuf = acl_dbuf_alloc(pool, length);
	else if (pool->head == NULL)
		dbuf = acl_dbuf_alloc(pool, pool->block_size);
	else if (pool->block_size < ((char*) pool->head->ptr
		- (char*) pool->head->buf_addr) + length)
	{
		dbuf = acl_dbuf_alloc(pool, pool->block_size);
	}
	else
		dbuf = pool->head;

	ptr = dbuf->ptr;
	dbuf->ptr = (char*) dbuf->ptr + length;
	return ptr;
}

void *acl_dbuf_pool_calloc(ACL_DBUF_POOL *pool, size_t length)
{
	void *ptr;

	ptr = acl_dbuf_pool_alloc(pool, length);
	if (ptr)
		memset(ptr, 0, length);
	return ptr;
}

char *acl_dbuf_pool_strdup(ACL_DBUF_POOL *pool, const char *s)
{
	size_t  len = strlen(s);
	char *ptr = (char*) acl_dbuf_pool_alloc(pool, len + 1);

	memcpy(ptr, s, len);
	ptr[len] = 0;
	return ptr;
}

void *acl_dbuf_pool_memdup(ACL_DBUF_POOL *pool, const void *s, size_t len)
{
	void *ptr = acl_dbuf_pool_alloc(pool, len);

	memcpy(ptr, s, len);
	return ptr;
}

void acl_dbuf_pool_test(size_t max)
{
	ACL_DBUF_POOL *pool;
	size_t   i, n = 1000000, len, j, k;

	for (j = 0; j < max; j++) {
		printf("begin alloc, max: %d\n", (int) n);
		pool = acl_dbuf_pool_create(0);
		for (i = 0; i < n; i++) {
			k = i % 10;
			switch (k) {
			case 0:
				len = 1024;
				break;
			case 1:
				len = 1999;
				break;
			case 2:
				len = 999;
				break;
			case 3:
				len = 230;
				break;
			case 4:
				len = 199;
				break;
			case 5:
				len = 99;
				break;
			case 6:
				len = 19;
				break;
			case 7:
				len = 29;
				break;
			case 8:
				len = 9;
				break;
			case 9:
				len = 399;
				break;
			default:
				len = 88;
				break;
			}
			(void) acl_dbuf_pool_alloc(pool, len);
		}
		printf("alloc over now, sleep(10)\n");
		sleep(10);
		acl_dbuf_pool_destroy(pool);
	}
}
