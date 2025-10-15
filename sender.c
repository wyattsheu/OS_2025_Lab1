#include "sender.h"
#include <errno.h>

// 全域變數用於累計總時間
long global_send_time_ns = 0;

void send(message_t message, mailbox_t *mailbox_ptr)
{
	/*  TODO:
		1. Use flag to determine the communication method
		2. According to the communication method, sed the message
	*/
	struct timespec start, end;

	if (mailbox_ptr->flag == MSG_PASSING) {
		// (2) Message Passing
		// 透過 POSIX message queue 傳送訊息
		// 重試直到成功發送，只計時成功的發送
		while (1) {
			clock_gettime(CLOCK_MONOTONIC, &start);
			int result = mq_send(mailbox_ptr->storage.posix_mq.mqd, message.msgText, strlen(message.msgText) + 1, 0);
			clock_gettime(CLOCK_MONOTONIC, &end);

			if (result == 0) {
				// 成功發送，計入時間
				long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
				global_send_time_ns += elapsed_ns;
				break;
			}
			else if (errno == EAGAIN) {
				// 佇列滿，重試但不計時
				usleep(100); // 短暫等待
				continue;
			}
			else {
				perror("mq_send");
				exit(1);
			}
		}
	}
	else if (mailbox_ptr->flag == SHARED_MEM) {
		// (3) Shared Memory with Semaphore
		// 等待「可寫」狀態
		sem_t *sem_empty = sem_open("/oslab1_sem_empty", 0);
		sem_t *sem_full = sem_open("/oslab1_sem_full", 0);

		sem_wait(sem_empty); // 等待 receiver 讀完

		// 只計時實際的記憶體寫入操作
		clock_gettime(CLOCK_MONOTONIC, &start);
		strcpy(mailbox_ptr->storage.shm_addr, message.msgText);
		clock_gettime(CLOCK_MONOTONIC, &end);
		long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
		global_send_time_ns += elapsed_ns;

		sem_post(sem_full); // 通知 receiver 可以讀取

		sem_close(sem_empty);
		sem_close(sem_full);
	}
}

int main(int argc, char *argv[])
{
	/*  TODO:
		1) Call send(message, &mailbox) according to the flow in slide 4
		2) Measure the total sending time
		3) Get the mechanism and the input file from command line arguments
			• e.g. ./sender 1 input.txt
					(1 for Message Passing, 2 for Shared Memory)
		4) Get the messages to be sent from the input file
		5) Print information on the console according to the output format
		6) If the message form the input file is EOF, send an exit message to the receiver.c
		7) Print the total sending time and terminate the sender.c
	*/

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <mechanism> <input_file>\n", argv[0]);
		exit(1);
	}
	int mechanism = atoi(argv[1]);
	char *input_file = argv[2];
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
		// 建立 POSIX message queue
		strcpy(mailbox.storage.posix_mq.name, "/oslab1_mq");

		// ✅ 完整初始化 mq_attr 結構體
		memset(&mailbox.storage.posix_mq.attr, 0, sizeof(struct mq_attr));
		mailbox.storage.posix_mq.attr.mq_flags = 0;		 // 阻塞模式
		mailbox.storage.posix_mq.attr.mq_maxmsg = 128;	 // 提高到 128（足夠處理 100+ 訊息）
		mailbox.storage.posix_mq.attr.mq_msgsize = 1024; // 每則大小 1024 bytes

		mailbox.storage.posix_mq.mqd = mq_open(
			mailbox.storage.posix_mq.name,
			O_CREAT | O_WRONLY | O_NONBLOCK,
			0666,
			&mailbox.storage.posix_mq.attr);
		if (mailbox.storage.posix_mq.mqd == (mqd_t)-1) {
			perror("mq_open");
			exit(1);
		}
	}
	else if (mechanism == SHARED_MEM) {
		// 建立共享記憶體區段
		int shm_fd = shm_open("/oslab1_shm", O_CREAT | O_RDWR, 0666);
		if (shm_fd == -1) {
			perror("shm_open");
			exit(1);
		}
		if (ftruncate(shm_fd, MSG_SIZE) == -1) {
			perror("ftruncate");
			exit(1);
		}

		mailbox.storage.shm_addr = mmap(NULL, MSG_SIZE,
										PROT_READ | PROT_WRITE,
										MAP_SHARED, shm_fd, 0);
		if (mailbox.storage.shm_addr == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}

		// 建立 semaphore
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
	// --- 開啟輸入檔 ---
	FILE *fp = fopen(input_file, "r");
	if (!fp) {
		perror("fopen");
		exit(1);
	}

	// --- 計時 ---
	message_t msg;
	char line[1024];

	while (fgets(line, sizeof(line), fp)) {
		// 移除換行符號
		line[strcspn(line, "\n")] = 0;
		// 設定訊息內容
		// msg.mType = 1; // 可以根據需要設定不同的類型
		strncpy(msg.msgText, line, sizeof(msg.msgText));

		// 傳送訊息（計時在 send 函數內部進行）
		send(msg, &mailbox);

		// 印出傳送訊息（彩色格式）
		printf("\033[1;36mSending message:\033[0m \033[0;37m%s\033[0m\n", msg.msgText);
	}
	fclose(fp);

	// --- 傳送結束訊息 ---
	strcpy(msg.msgText, EXIT_MSG);
	send(msg, &mailbox);
	printf("\033[1;31mEnd of input file! exit!\033[0m\n");

	// --- 印出總時間（秒為單位）---
	printf("\033[1;37mTotal time taken in sending msg: %.6f s\033[0m\n", global_send_time_ns / 1e9);

	// --- 清理 ---
	if (mechanism == MSG_PASSING) {
		mq_close(mailbox.storage.posix_mq.mqd);
		// sender 不 unlink，留給 receiver 處理
	}
	else if (mechanism == SHARED_MEM) {
		munmap(mailbox.storage.shm_addr, MSG_SIZE);
		// sender 不 unlink，共享區留給 receiver 用
	}

	return 0;
}
