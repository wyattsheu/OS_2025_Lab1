#include "receiver.h"
#include <errno.h>

// 全域變數用於累計總時間
long global_recv_time_ns = 0;

void receive(message_t *message_ptr, mailbox_t *mailbox_ptr)
{
	/*  TODO:
		1. Use flag to determine the communication method
		2. According to the communication method, receive the message
	*/
	struct timespec start, end;

	// (1) 判斷模式
	if (mailbox_ptr->flag == MSG_PASSING) {
		// (2) Message Passing
		// 透過 POSIX message queue 接收訊息
		// 重試直到成功接收，只計時成功的接收
		while (1) {
			clock_gettime(CLOCK_MONOTONIC, &start);
			ssize_t result = mq_receive(mailbox_ptr->storage.posix_mq.mqd,
										message_ptr->msgText,
										mailbox_ptr->storage.posix_mq.attr.mq_msgsize,
										NULL);
			clock_gettime(CLOCK_MONOTONIC, &end);

			if (result >= 0) {
				// 成功接收，計入時間
				long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
				global_recv_time_ns += elapsed_ns;
				break;
			}
			else if (errno == EAGAIN) {
				// 沒有訊息，重試但不計時
				usleep(100); // 短暫等待
				continue;
			}
			else {
				perror("mq_receive");
				exit(1);
			}
		}
	}
	else if (mailbox_ptr->flag == SHARED_MEM) {
		// (3) Shared Memory with Semaphore
		// 等待 sender 寫完
		sem_t *sem_empty = sem_open("/oslab1_sem_empty", 0);
		sem_t *sem_full = sem_open("/oslab1_sem_full", 0);

		sem_wait(sem_full); // 等待 sender 寫入資料

		// 只計時實際的記憶體讀取操作
		clock_gettime(CLOCK_MONOTONIC, &start);
		strcpy(message_ptr->msgText, mailbox_ptr->storage.shm_addr);
		clock_gettime(CLOCK_MONOTONIC, &end);
		long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
		global_recv_time_ns += elapsed_ns;

		sem_post(sem_empty); // 通知 sender 可以再寫

		sem_close(sem_empty);
		sem_close(sem_full);
	}
}

int main(int argc, char *argv[])
{
	/*  TODO:
		1) Call receive(&message, &mailbox) according to the flow in slide 4
		2) Measure the total receiving time
		3) Get the mechanism from command line arguments
			• e.g. ./receiver 1
		4) Print information on the console according to the output format
		5) If the exit message is received, print the total receiving time and terminate the receiver.c
	*/
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <mechanism>\n", argv[0]);
		exit(1);
	}
	int mechanism = atoi(argv[1]);
	mailbox_t mailbox;
	mailbox.flag = mechanism;

	// 輸出機制標題
	if (mechanism == MSG_PASSING) {
		printf("\033[1;36mMessage Passing\033[0m\n");
	}
	else if (mechanism == SHARED_MEM) {
		printf("\033[1;36mShared Memory\033[0m\n");
	}

	// --- 初始化 mailbox ---
	if (mechanism == MSG_PASSING) {
		// 開啟 POSIX message queue
		strcpy(mailbox.storage.posix_mq.name, "/oslab1_mq");
		memset(&mailbox.storage.posix_mq.attr, 0, sizeof(struct mq_attr));
		mailbox.storage.posix_mq.attr.mq_flags = 0;
		mailbox.storage.posix_mq.attr.mq_maxmsg = 128;
		mailbox.storage.posix_mq.attr.mq_msgsize = 1024;
		// receiver 要用 O_CREAT | O_RDONLY
		mailbox.storage.posix_mq.mqd = mq_open(
			mailbox.storage.posix_mq.name,
			O_CREAT | O_RDONLY | O_NONBLOCK,
			0666,
			&mailbox.storage.posix_mq.attr);
		if (mailbox.storage.posix_mq.mqd == (mqd_t)-1) {
			perror("mq_open");
			exit(1);
		}
	}
	else if (mechanism == SHARED_MEM) {
		// 開啟或創建共享記憶體
		int shm_fd = shm_open("/oslab1_shm", O_CREAT | O_RDWR, 0666);
		if (shm_fd == -1) {
			perror("shm_open");
			exit(1);
		}
		if (ftruncate(shm_fd, MSG_SIZE) == -1) {
			perror("ftruncate");
			exit(1);
		}

		// 映射進記憶體空間
		mailbox.storage.shm_addr = mmap(NULL, MSG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		if (mailbox.storage.shm_addr == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}

		// 開啟或創建 semaphore
		sem_t *sem_empty = sem_open("/oslab1_sem_empty", O_CREAT, 0666, 1); // 1 表示「一開始可寫」
		sem_t *sem_full = sem_open("/oslab1_sem_full", O_CREAT, 0666, 0);	// 0 表示「目前沒有資料可讀」
		if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
			perror("sem_open");
			exit(1);
		}
		sem_close(sem_empty);
		sem_close(sem_full);
		close(shm_fd);
	}
	else {
		fprintf(stderr, "Invalid mechanism (1=msg, 2=shm)\n");
		exit(1);
	}
	message_t msg;
	while (1) {
		receive(&msg, &mailbox);

		if (strcmp(msg.msgText, EXIT_MSG) == 0) {
			printf("\033[1;31mSender exit!\033[0m\n");
			break;
		}

		printf("\033[1;36mReceiving message:\033[0m \033[0;37m%s\033[0m\n", msg.msgText);
	}

	printf("\033[1;37mTotal time taken in receiving msg: %.6f s\033[0m\n", global_recv_time_ns / 1e9);

	// --- 清理 ---
	if (mechanism == MSG_PASSING) {
		mq_close(mailbox.storage.posix_mq.mqd);
		mq_unlink(mailbox.storage.posix_mq.name); // receiver 負責刪除 queue
	}
	else if (mechanism == SHARED_MEM) {
		// 清理共享記憶體和 semaphore
		munmap(mailbox.storage.shm_addr, MSG_SIZE);
		shm_unlink("/oslab1_shm");
		sem_unlink("/oslab1_sem_empty");
		sem_unlink("/oslab1_sem_full");
	}

	return 0;
}
