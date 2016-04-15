#ifndef ZAD1_MESSAGES_H
#define ZAD1_MESSAGES_H

struct default_msg {
    long mtype;
    char mtext[1];
};

struct int_msg_mtext {
    int number;
};

struct int_msg {
    long mtype;
    struct int_msg_mtext mtext;
};

struct client_result {
    int client_id;
    int number;
    int is_prime;
};

struct client_result_msg {
    long mtype;
    struct client_result mtext;
};

#define MAX_MSG_SIZE sizeof(struct client_result)

#endif //ZAD1_MESSAGES_H
