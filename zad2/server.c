#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <mqueue.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include "messages.h"

#define CLIENT_MAX 3

int read_args(int argc, char *argv[], char **queue_name);
int get_next_client_id();
int get_new_task();
void remove_queue();
void sigint_handler(int signum);

mqd_t clients[CLIENT_MAX];
int available_client_slots = CLIENT_MAX;
char * queue_name = NULL;
mqd_t queue_id = -1;

/*
 * Types of messages:
 * 1 - sending new client_id
 * 2 - sending new task
 * 3 - sending "server closed"
 */
int main(int argc, char *argv[]) {
    atexit(remove_queue);
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    char *args_help = "Enter queue name (with preceding /).\n";
    if (read_args(argc, argv, &queue_name) != 0) {
        printf(args_help);
        return 1;
    }
    srand(time(NULL));

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MSG_NUM;
    attr.mq_msgsize = MAX_MSG_SIZE;

    queue_id = mq_open(queue_name, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attr);

    if (queue_id == -1) {
        printf("Error while creating server queue occurred.\n");
        return 1;
    }
    for (int i = 0; i < CLIENT_MAX; i++)
        clients[i] = -1;

    char message[MAX_MSG_SIZE];
    int client_id;
    mqd_t client_queue_id;
    int client_task_number;
    int client_task_is_prime;
    while (1) {
        if (mq_receive(queue_id, message, MAX_MSG_SIZE, NULL) == -1) {
            printf("Error while receiving message occurred.\n");
            sleep(1);
            continue;
        }
        switch ((int)message[0]) {
            case 1: // client intro - queue name in message
                client_id = get_next_client_id();
                client_queue_id = mq_open(message + 1, O_WRONLY, 0, &attr);
                if (client_queue_id == -1) {
                    printf("Cannot open clients' queue.\n");
                    break;
                }
                message[0] = 1;
                if (client_id == -1) {
                    printf("Cannot accept next client.\n");
                    sprintf(message + 1, "%d", -1);
                    mq_send(client_queue_id, message, MAX_MSG_SIZE, 0);
                    mq_close(client_queue_id);
                    break;
                }

                sprintf(message + 1, "%d", client_id);
                if(mq_send(client_queue_id, message, MAX_MSG_SIZE, 0) != 0) {
                    printf("Error while accepting new client occurred.\n");
                    mq_close(client_queue_id);
                    break;
                }
                clients[client_id] = client_queue_id;
                available_client_slots--;
                printf("Client %d connected.\n", client_id);
                break;
            case 2: // client is ready
                sscanf(message + 1, "%d", &client_id);
                if (client_id < 0 || client_id >= CLIENT_MAX || clients[client_id] == -1) {
                    printf("Incorrect client_id in message. Ignoring.\n");
                    break;
                }
                message[0] = 2;
                sprintf(message + 1, "%d", get_new_task());
                if(mq_send(clients[client_id], message, MAX_MSG_SIZE, 0) != 0) {
                    printf("Error while sending a new task to the client.\n");
                    break;
                }
                break;
            case 3: // client task results
                sscanf(message + 1, "%d %d %d", &client_id, &client_task_number, &client_task_is_prime);
                char * result_msg = "Composite number";
                if (client_task_is_prime)
                    result_msg = "Prime number";
                printf("%s: %d (client: %d)\n", result_msg, client_task_number, client_id);
                break;
            case 4: // client closed
                sscanf(message + 1, "%d", &client_id);
                if (client_id < 0 || client_id >= CLIENT_MAX || clients[client_id] == -1) {
                    printf("Incorrect client_id in message. Ignoring.\n");
                    break;
                }
                mq_close(clients[client_id]);
                clients[client_id] = -1;
                available_client_slots++;
                printf("Client %d exited.\n", client_id);
                break;
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

int get_next_client_id() {
    if (available_client_slots == 0)
        return -1;
    else
        for (int i = 0; i < CLIENT_MAX; i++)
            if (clients[i] == -1)
                return i;
    return -1;
}

int get_new_task() {
    return rand() % 1000;
}

void remove_queue() {
    if (queue_id != -1) { // send "server closed" to all clients
        mq_close(queue_id);
        char message[MAX_MSG_SIZE];
        message[0] = 3;
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i] != -1) {
                mq_send(clients[i], message, MAX_MSG_SIZE, 0);
                mq_close(clients[i]);
            }
        }
    }
    if (queue_name != NULL) {
        mq_unlink(queue_name);
    }
}

void sigint_handler(int signum) {
    printf("Server closed.\n");
    exit(0);
}
