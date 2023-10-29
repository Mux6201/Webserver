//
// Created by ming on 10/28/23.
//


#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";


locker m_lock;
map<string, string> users;

void http_conn::init_mysql_result(connection_pool *connPool) {
    MYSQL *mysql = nullptr;
    connectionRAII mysqlCon(&mysql, connPool);

    if (mysql_query(mysql, "SELECT username , passwd FROM user")) {
        LOG_ERROR("SELECT error: %s\n", mysql_errno(mysql));
    }
    MYSQL_RES *result = mysql_store_result(mysql);
    int num_fields = mysql_num_fields(result);
    MYSQL_FIELD *fields = mysql_fetch_fields(result);
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string tmp1(row[0]), tmp2(row[1]);
        users[tmp1] = tmp2;
    }

}


int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//reset to EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;


void http_conn::close_conn(bool real_close) {
    if (real_close && (m_sockfd != -1)) {
        ::printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, int close_log, std::string user,
                     std::string passwd,
                     std::string sqlname) {
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;
    ::strcpy(sql_user, user.c_str());
    ::strcpy(sql_passwd, passwd.c_str());
    ::strcpy(sql_name, sqlname.c_str());
    init();
}


void http_conn::init() {
    m_mysql = nullptr;
    bytes_to_send = 0;
    bytes_have_read = 0;
    m_check_state = CHECK_STATUS_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITER_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') {
            if ((m_checked_idx + 1) == m_read_idx) {
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }

    }
    return LINE_OPEN;
}

bool http_conn::read_once() {
    if(m_read_idx>=READ_BUFFER_SIZE) return false;
    int bytes_read = 0;
    //LT mode
    if (0 == m_TRIGMode) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;
        if (bytes_read <= 0) {
            return false;
        }
        return true;
    }else{
        while (true) {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }

}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
    //strpbrk return the location of accept in s, or return NULL.
    m_url = strpbrk(text, " \t");
    if (!m_url) {
        return BAD_REQUEST;
    }
    *m_url += '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = GET;
    }else if(strcasecmp(method,"POST")==0){
        m_method = POST;
        cgi=1;
    } else return BAD_REQUEST;
    // return trim location
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    }
    *m_version += '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version,"HTTP/1.1")!=0) return BAD_REQUEST;

    if(strncasecmp(m_url,"http://",7)==0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if(strncasecmp(m_url,"https://",8)==0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    if (strlen(m_url) == 1) {
        strcat(m_url, "judge.html");

    }
    m_check_state = CHECK_STATUS_HEADER;
    return NO_REQUEST;

}

http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
    if (text[0] == '\0') {
        if(m_content_length!=0){
            m_check_state = CHECK_STATUS_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        //skip Connection: and "\t"
        text+=11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }

    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }else if (strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }else{
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text) {
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char  *text = 0;
    while ((m_check_state == CHECK_STATUS_CONTENT && line_status == LINE_OK) ||
           (line_status = parse_line()) == LINE_OK) {
        
    }
}
