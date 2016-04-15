#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include "messages.h"

#define CLIENT_MAX 3

int read_args(int argc, char *argv[], char **pathname, int *proj_id);
int get_next_client_id();
int get_new_task();
void remove_queue();
void sigint_handler(int signum);

int clients[CLIENT_MAX];
int available_client_slots = CLIENT_MAX;
int queue_id = -1;

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

    char *args_help = "Enter pathname and id number.\n";
    char *pathname;
    int proj_id;
    if (read_args(argc, argv, &pathname, &proj_id) != 0) {
        printf(args_help);
        return 1;
    }
    srand(time(NULL));

    key_t queue_key = ftok(pathname, proj_id);
    if (queue_key == -1) {
        printf("Error while creating server queue occurred.\n");
        return 1;
    }
    queue_id = msgget(queue_key, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (queue_id == -1) {
        printf("Error while creating server queue occurred.\n");
        return 1;
    }
    for (int i = 0; i < CLIENT_MAX; i++)
        clients[i] = -1;

    void * message = malloc(MAX_MSG_SIZE + sizeof(long));
    if (message == NULL) {
        printf("Error while allocating memory occurred.\n");
        return 1;
    }
    struct int_msg new_int_msg;
    struct client_result *cres;
    int client_id;
    while (1) {
        msgrcv(queue_id, message, MAX_MSG_SIZE, 0, 0);
        switch (((struct default_msg *)message)->mtype) {
            case 1: // client intro - queue id in message
                client_id = get_next_client_id();
                if (client_id == -1) {
                    printf("Cannot accept next client.\n");
                    new_int_msg.mtype = 1;
                    new_int_msg.mtext.number = -1;
                    msgsnd(((struct int_msg *)message)->mtext.number,
                           (void*)&new_int_msg, sizeof(struct int_msg_mtext), 0);
                    break;
                }
                clients[client_id] = ((struct int_msg *)message)->mtext.number;
                new_int_msg.mtype = 1;
                new_int_msg.mtext.number = client_id;
                if(msgsnd(clients[client_id], (void*)&new_int_msg, sizeof(struct int_msg_mtext), 0) != 0) {
                    printf("Error while accepting new client occurred.\n");
                    clients[client_id] = -1;
                    break;
                }
                available_client_slots--;
                printf("Client %d connected.\n", client_id);
                break;
            case 2: // client is ready
                client_id = ((struct int_msg *)message)->mtext.number;
                if (client_id < 0 || client_id >= CLIENT_MAX || clients[client_id] == -1) {
                    printf("Incorrect client_id in message. Ignoring.\n");
                    break;
                }
                new_int_msg.mtype = 2;
                new_int_msg.mtext.number = get_new_task();
                if(msgsnd(clients[client_id], (void*)&new_int_msg, sizeof(struct int_msg_mtext), 0) != 0) {
                    printf("Error while sending a new task to the client.\n");
                    break;
                }
                break;
            case 3: // client task results
                cres = &((struct client_result_msg *)message)->mtext;
                char * result_msg = "Composite number";
                if (cres->is_prime)
                    result_msg = "Prime number";
                printf("%s: %d (client: %d)\n", result_msg, cres->number, cres->client_id);
                break;
            case 4: // client closed
                client_id = ((struct int_msg *)message)->mtext.number;
                if (client_id < 0 || client_id >= CLIENT_MAX || clients[client_id] == -1) {
                    printf("Incorrect client_id in message. Ignoring.\n");
                    break;
                }
                clients[client_id] = -1;
                available_client_slots++;
                printf("Client %d exited.\n", client_id);
                break;
        }
    }
}

int read_args(int argc, char *argv[], char **pathname, int *proj_id) {
    if (argc != 3) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    *pathname = argv[1];
    int n = atoi(argv[2]);
    if (n <= 0) {
        printf("Incorrect id number. It should be > 0.\n");
        return 1;
    }
    *proj_id = n;

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
        msgctl(queue_id, IPC_RMID, NULL);
        struct default_msg end_msg;
        end_msg.mtype = 3;
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i] != -1) {
                msgsnd(clients[i], (void *) &end_msg, sizeof(char), 0);
            }
        }
    }
}

void sigint_handler(int signum) {
    printf("Server closed.\n");
    exit(0);
}
