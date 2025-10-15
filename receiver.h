#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MSG_PASSING 1
#define SHARED_MEM 2

#include <mqueue.h>	  // 🔹 定義 mqd_t, mq_attr, mq_*()
#include <sys/mman.h> // 🔹 定義 mmap, munmap
// 🔹 自訂常數（取代錯誤的 MQ_NAME_MAX）
#define MQ_NAME_LEN 64
#define MSG_SIZE 1024
#define EXIT_MSG "__EXIT__"
// 統一資源名稱
#define MQ_NAME "/oslab1_mq"
#define SHM_NAME "/oslab1_shm"
#define SEM_EMPTY_NAME "/oslab1_sem_empty"
#define SEM_FULL_NAME "/oslab1_sem_full"

typedef struct {
	int flag; // 1 for message passing, 2 for shared memory
	union {
		// int msqid; //for system V api. You can replace it with structure for POSIX api

		// POSIX mqueue
		struct {
			mqd_t mqd;				// 佇列描述子
			char name[MQ_NAME_LEN]; // 佇列名稱，如 "/my_mailbox"
			struct mq_attr attr;	// 屬性：mq_maxmsg, mq_msgsize, ...
		} posix_mq;

		char *shm_addr;
	} storage;
} mailbox_t;

typedef struct {
	/*  TODO:
		Message structure for wrapper
	*/
	long mType;
	char msgText[1024];
} message_t;

void receive(message_t *message_ptr, mailbox_t *mailbox_ptr);
