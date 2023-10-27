#ifndef SQL_CONN_POOL
#define SQL_CONN_POOL

#include<cstdio>
#include<list>
#include<error.h>
#include<string>
#include<iostream>
#include<cstring>
#include<mysql/mysql.h>

#include"../log/log.h"
#include"../lock/locker.h"



using namespace std;
class connection_pool{
    connection_pool();
    ~connection_pool();

    int m_MaxConn;  //最大连接数
    int m_CurConn;  //当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数
    locker lock;
    list<MYSQL *> connList; //连接池
    sem reserve;
public:
    string m_url;			 //主机地址
    string m_Port;		 //数据库端口号
    string m_User;		 //登陆数据库用户名
    string m_PassWord;	 //登陆数据库密码
    string m_DatabaseName; //使用数据库名
    int m_close_log;	//日志开关

public:
    MYSQL *GetConnection();

    bool ReleaseConnection(MYSQL *conn);

    int GetFreeConn();

    void DestroyPool();

    //单例
    static connection_pool *GetInstance();
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);
};

class connectionRAII{

public:
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};

#endif //SQL_CONN_POOL