//
// Created by ming on 10/26/23.
//

#include "lst_timer.h"
#include "../http/http_conn.h"


sort_timer_lst::~sort_timer_lst() {
    util_timer *tmp = head;
    while (head) {
        head = head->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;

    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = timer;
    }
    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }
}

void sort_timer_lst::add_timer(util_timer *timer) {
    if (!timer) return;
    if (!head) {
        head = tail = timer;
        return;
    }
    //insert in front
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer) {
    if (!timer) return;
    util_timer *temp = timer->next;
    if(!temp || (timer->expire<temp->expire)) return;
    if(timer==head){
        head=head->next;
        head->prev= nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}
void sort_timer_lst::del_timer(util_timer *timer) {
    if(!timer){

    }
}
