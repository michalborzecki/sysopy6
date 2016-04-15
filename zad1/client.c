#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include "messages.h"

int read_args(int argc, char *argv[], char **pathname, int *proj_id);
void remove_queue();
void send_ready_msg();
int is_prime(int num);
void sigint_handler(int signum);

int queue_id = -1;
int server_queue_id = -1;
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

    char *args_help = "Enter pathname and id number.\n";
    char *pathname;
    int proj_id;
    if (read_args(argc, argv, &pathname, &proj_id) != 0) {
        printf(args_help);
        return 1;
    }

    key_t server_queue_key = ftok(pathname, proj_id);
    if (server_queue_key == -1) {
        printf("Error while connecting to server queue occurred.\n");
        return 1;
    }
    server_queue_id = msgget(server_queue_key, S_IRUSR | S_IWUSR);
    if (server_queue_id == -1) {
        printf("Error while connecting to server queue occurred.\n");
        return 1;
    }

    queue_id = msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);
    if (queue_id == -1) {
        printf("Error while creating client queue occurred.\n");
        return 1;
    }

    struct int_msg client_intro;
    client_intro.mtype = 1;
    client_intro.mtext.number = queue_id;
    if(msgsnd(server_queue_id, (void*)&client_intro, sizeof(struct int_msg_mtext), 0) != 0) {
        printf("Error while sending registration data to server.\n");
        return 1;
    }

    void * message = malloc(MAX_MSG_SIZE + sizeof(long));
    if (message == NULL) {
        printf("Error while allocating memory occurred.\n");
        return 1;
    }

    struct client_result_msg cr;
    cr.mtype = 3;
    while (1) {
        msgrcv(queue_id, message, MAX_MSG_SIZE, 0, 0);
        switch (((struct default_msg *)message)->mtype) {
            case 1: // server respond with client_id
                client_id = ((struct int_msg *)message)->mtext.number;
                if (client_id == -1) {
                    printf("Server refused client.\n");
                    server_queue_id = -1;
                    return 1;
                }
                cr.mtext.client_id = client_id;
                send_ready_msg();
                break;
            case 2: // new server task
                cr.mtext.number = ((struct int_msg *)message)->mtext.number;
                cr.mtext.is_prime = is_prime(cr.mtext.number);
                sleep(2);
                if(msgsnd(server_queue_id, (void*)&cr, sizeof(struct client_result), 0) != 0) {
                    printf("Error while sending client result to server.\n");
                }
                send_ready_msg();
                break;
            case 3: // server closed
                server_queue_id = -1;
                printf("Server closed.\n");
                return 1;
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

void remove_queue() {
    if (server_queue_id != -1) {
        struct int_msg new_int_msg;
        new_int_msg.mtype = 4;
        new_int_msg.mtext.number = client_id;
        msgsnd(server_queue_id, (void *) &new_int_msg, sizeof(struct int_msg_mtext), 0);
    }
    if (queue_id != -1)
        msgctl(queue_id, IPC_RMID, NULL);
}

void send_ready_msg() {
    struct int_msg new_int_msg;
    new_int_msg.mtype = 2;
    new_int_msg.mtext.number = client_id;
    if(msgsnd(server_queue_id, (void*)&new_int_msg, sizeof(struct int_msg_mtext), 0) != 0) {
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
