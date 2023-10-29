#ifndef HTTPCONN_H
#define HTTPCONN_H



#include "../lock/locker.h"
#include "../sql/sql_conn_pool.h"
#include "../log/log.h"
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <map>

class http_conn {

public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITER_BUFFER_SIZE = 1024;
    enum METHOD{
        GET=0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE{
        CHECK_STATUS_REQUESTLINE=0,
        CHECK_STATUS_HEADER,
        CHECK_STATUS_CONTENT
    };
    enum HTTP_CODE{
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSE_CONNECTION
    };
    enum LINE_STATUS{
        LINE_OK=0,
        LINE_BAD,
        LINE_OPEN
    };
public:
    http_conn()= default;
    ~http_conn()= default;
public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);

    void close_conn(bool real_close = true);

    void process();

    bool read_once();

    bool write();
    sockaddr_in *get_address(){
        return  &m_address;
    }

    void init_mysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;
private:
    void init();
    HTTP_CODE process_read();

    bool process_write(HTTP_CODE ret);

    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line(){ return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();
    void unmap();

    bool add_response(const char *format, ...);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *m_mysql;
    int m_state;
private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx;
    long m_checked_idx;

    char m_write_buf[WRITER_BUFFER_SIZE];
    int m_start_line;
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char *m_string;
    int bytes_to_send;
    int bytes_have_read;
    char *doc_root;
    map<string ,string > m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];


};
#endif HTTPCONN_H