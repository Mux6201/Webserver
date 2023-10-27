#include<string.h>
#include<time.h>
#include<sys/time.h>
#include<pthread.h>
#include"log.h"
using namespace std;


Log::Log(){
    m_count=0;
    m_is_async=false;
}
Log::~Log(){
    if(m_fp!=nullptr) fclose(m_fp);
}

void Log::flush()
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    //如果有队列，就设置为异步
    if(max_queue_size>0){
        m_is_async=true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //创建新线程用于写日志，回调里面是一个循环，不停的从队列中拿出日志写入文件
        pthread_create(&tid,nullptr,flush_log_thread,nullptr);
    }
    m_close_log=close_log;
    m_log_buf_size=log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines=split_lines;
    time_t t = time(nullptr);
    struct tm *sys_tm= localtime(&t);
    struct tm my_tm=*sys_tm;
    //搜索最后一次出现的位置,去掉路径，只要文件名
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};
    if (!p) {
        //直接在当前文件夹下创建log
        ::snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                   file_name);
    }else{
        //如果有路径和文件名，将路径和文件分离
        strcpy(log_name, p + 1);    //文件名
        strncpy(dir_name,file_name,p-file_name+1);  //路径名称
        //安全格式化输出，需要填入最大长度，format后是可变参数，依据format而定
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    m_today = my_tm.tm_mday;
    //打开日志文件
    m_fp = fopen(log_full_name, "a");
    if(!m_fp) return false;
    return true;

}

void Log::write_log(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    switch (level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[error]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;

    }
    //写入一个log
    m_mutex.lock();
    m_count++;
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        //m_today在init里面确定，如果是新的一天，开新文件，如果达到最大分割条数，也要开新文件
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();
    va_list vaList;
    va_start(vaList,format);
    string log_str;
    m_mutex.lock();
    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, vaList);
    //结束这条日志
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';

    m_mutex.unlock();

    //队列没满放队列，队列满了直接写如文件
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(vaList);

}