#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>
#include "messages.h"

int read_args(int argc, char *argv[], char **queue_name);
void remove_queue();
void send_ready_msg();
int is_prime(int num);
void sigint_handler(int signum);

mqd_t queue_id = -1;
char queue_name[MAX_QUEUE_NAME_SIZE + 1] = "/client";
mqd_t server_queue_id = -1;
int client_id = -1;

/*
 * Types of messages:
 * 1 - sending client queue id
 * 2 - sending "client ready"
 * 3 - sending task results
 * 4 - sending "client closed"
 */
int main(int argc, char *argv[]) {
    atexit(remove_queue);
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    char *server_queue_name;
    char *args_help = "Enter queue name (with preceding /).\n";
    if (read_args(argc, argv, &server_queue_name) != 0) {
        printf(args_help);
        return 1;
    }

    sprintf(queue_name + strlen(queue_name), "%d", getpid());

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MSG_NUM;
    attr.mq_msgsize = MAX_MSG_SIZE;

    queue_id = mq_open(queue_name, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attr);
    if (queue_id == -1) {
        printf("Error while creating client queue occurred.\n");
        return 1;
    }
    server_queue_id = mq_open(server_queue_name, O_WRONLY, 0, &attr);
    if (server_queue_id == -1) {
        printf("Error while opening server queue occurred.\n");
        return 1;
    }

    queue_id = mq_open(queue_name, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attr);
    if (queue_id == -1) {
        printf("Error while creating client queue occurred.\n");
        return 1;
    }

    char message[MAX_MSG_SIZE];
    message[0] = 1;
    strcpy(message + 1, queue_name);
    if(mq_send(server_queue_id, message, MAX_MSG_SIZE, 0) != 0) {
        printf("Error while sending registration data to server.\n");
        return 1;
    }

    int task_number;
    while (1) {
        if (mq_receive(queue_id, message, MAX_MSG_SIZE, NULL) == -1) {
            printf("Error while receiving message occurred.\n");
            sleep(1);
            continue;
        }
        switch ((int)message[0]) {
            case 1: // server respond with client_id
                sscanf(message + 1, "%d", &client_id);
                if (client_id == -1) {
                    printf("Server refused client.\n");
                    mq_close(server_queue_id);
                    server_queue_id = -1;
                    return 1;
                }
                printf("Client accepted.\n");
                send_ready_msg();
                break;
            case 2: // new server task
                sscanf(message + 1, "%d", &task_number);
                message[0] = 3;
                sprintf(message + 1, "%d %d %d", client_id, task_number, is_prime(task_number));
                sleep(2);
                if(mq_send(server_queue_id, message, MAX_MSG_SIZE, 0) != 0) {
                    printf("Error while sending client result to server.\n");
                }
                send_ready_msg();
                break;
            case 3: // server closed
                mq_close(server_queue_id);
                server_queue_id = -1;
                printf("Server closed.\n");
                return 1;
        }
    }
}

int read_args(int argc, char *argv[], char **queue_name) {
    if (argc != 2) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    if (argv[1][0] != '/') {
        printf("Queue name must start with / character.\n");
        return 1;
    }
    if (strlen(argv[1]) == 1 || strlen(argv[1]) > MAX_QUEUE_NAME_SIZE) {
        printf("Queue name must be longer than 1 and shorter than %d.\n", MAX_QUEUE_NAME_SIZE);
        return 1;
    }
    for (int i = 1; argv[1][i] != '\0'; i++) {
        if (argv[1][i] == '/') {
            printf("Queue name must not contain / character (except / as a first char).\n");
            return 1;
        }
    }
    *queue_name = argv[1];

    return 0;
}

void remove_queue() {
    if (server_queue_id != -1) {
        char message[MAX_MSG_SIZE];
        message[0] = 4;
        sprintf(message + 1, "%d", client_id);
        mq_send(server_queue_id, message, MAX_MSG_SIZE, 0);
    }
    if (queue_id != -1)
        mq_close(queue_id);
    mq_unlink(queue_name);
}

void send_ready_msg() {
    char message[MAX_MSG_SIZE];
    message[0] = 2;
    sprintf(message + 1, "%d", client_id);
    if(mq_send(server_queue_id, message, MAX_MSG_SIZE, 0) != 0) {
        printf("Error while sending 'client ready' state to server.\n");
    }
}

int is_prime(int num) {
    if (num <= 1) return 0;
    if (num % 2 == 0) return 0;
    for(int i = 3; i < num / 2; i+= 2)
        if (num % i == 0)
            return 0;
    return 1;
}

void sigint_handler(int signum) {
    printf("Client closed.\n");
    exit(0);
}
