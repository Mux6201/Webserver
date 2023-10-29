//
// Created by ming on 10/26/23.
//

#ifndef WEBSERVER_LST_TIMER_H
#define WEBSERVER_LST_TIMER_H


#include <time.h>
#include "../log/log.h"
#include <netinet/in.h>

class util_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer {
public:
    util_timer *prev, *next;

    util_timer() : prev(nullptr), next(nullptr) {}

    time_t expire;
    client_data *user_data;
    //callback function
    void (*cb_func)(client_data *);
};

class sort_timer_lst {
    util_timer *head, *tail;

    void add_timer(util_timer *timer, util_timer *lst_head);

public:
    sort_timer_lst():head(nullptr),tail(nullptr){}

    ~sort_timer_lst();

    void add_timer(util_timer *timer);

    void adjust_timer(util_timer *timer);

    void del_timer(util_timer *timer);

    void tick();
};
class Utils{

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_list;
    static int u_epollfd;
    int m_TIMESLOT;

    void init(int timeslot);

    int setnonblocking(int fd);

    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    static void sig_handler(int sig);

    void addsig(int sig, void(handler)(int), bool restart = true);

    void timer_handler();

    void show_error(int connfd, const char *info);
};

void cb_func(client_data *user_data);
#endif //WEBSERVER_LST_TIMER_H
