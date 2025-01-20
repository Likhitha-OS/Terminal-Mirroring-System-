#include <stdlib.h>
#include <stdio.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <signal.h>
#include <pthread.h>

struct Msg {
    long mtype;
    char command[1000];
    char msg[600];
    int pid;
    int terminal;
};

int curr = 1;
int ids[10000];

int send_message(int qid, struct Msg *qbuf) {
    int result, length;

    /* The length is essentially the size of the structure minus sizeof(mtype) */
    length = sizeof(struct Msg) - sizeof(long);

    if ((result = msgsnd(qid, qbuf, length, 0)) == -1) {
        return -1;
    }

    return result;
}

int read_message(int qid, long type, struct Msg *qbuf) {
    int result, length;

    /* The length is essentially the size of the structure minus sizeof(mtype) */
    length = sizeof(struct Msg) - sizeof(long);

    if ((result = msgrcv(qid, qbuf, length, type, 0)) == -1) {
        return -1;
    }

    return result;
}

void *uncoupl(void *arg) {
    int qid = *(int *)arg;
    struct Msg msg;

    while (1) {
        if (read_message(qid, 2, &msg) == -1) {
            continue;
        } else {
            ids[msg.terminal] = -1;
            printf("Updated List Of Clients:\n");
            for (int i = 1; i < curr; i++) {
                if (ids[i] != -1) {
                    printf("Terminal: %d   Pid: %d\n", i, ids[i]);
                    msg.mtype = ids[i];
                    if (send_message(qid, &msg) == -1) {
                        perror("Error while sending message");
                        msgctl(qid, IPC_RMID, NULL);
                        exit(1);
                    }
                }
            }
        }
    }

    pthread_exit(NULL);
}

void *coupl(void *arg) {
    int qid = *(int *)arg;
    struct Msg msg;

    while (1) {
        if (read_message(qid, 1, &msg) == -1) {
            continue;
        } else {
            msg.terminal = curr;
            ids[curr] = msg.pid;
            msg.pid = -1;
            curr++;
            if(send_message(qid,&msg)==-1)
            {
             perror("error while sending the message ");
            }
            printf("Updated List Of Clients:\n");
            for (int i = 1; i < curr; i++) {
                if (ids[i] != -1) {
                    printf("Terminal: %d   Pid: %d\n", i, ids[i]);
                }
            }
        }
        continue;
    }

    pthread_exit(NULL);
}

void *rec_brod(void *arg) {
    int qid = *(int *)arg;
    struct Msg msg;

    while (1) {
        if (read_message(qid, 3, &msg) == -1) {
            continue;
        } else {
            int cur_pid = ids[msg.terminal];
            for (int i = 1; i < curr; i++) {
                if (ids[i] != -1 && ids[i] != cur_pid) {
                    msg.mtype = ids[i];
                    if (send_message(qid, &msg) == -1) {
                        perror("Error while sending message");
                        exit(1);
                    }
                }
            }
        }
    }

    pthread_exit(NULL);
}

int main() {
    key_t key = 1234;
    int qid;

    qid = msgget(key, IPC_CREAT | 0666);
    if (qid == -1) {
        perror("msgget failed");
        exit(1);
    }

    for (int i = 0; i < 10000; i++) {
        ids[i] = -1;
    }

    printf("Message queue ID: %d\n", qid);

    pthread_t thread1, thread2, thread3;
    int rc;

    rc = pthread_create(&thread1, NULL, coupl, &qid);
    if (rc) {
        printf("Error creating thread 1: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    rc = pthread_create(&thread2, NULL, uncoupl, &qid);
    if (rc) {
        printf("Error creating thread 2: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    rc = pthread_create(&thread3, NULL, rec_brod, &qid);
    if (rc) {
        printf("Error creating thread 3: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    return 0;
}
