#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

/**
 * 个人注释
 * 构造函数
 * 初始化部分参数
 */
connection_pool::connection_pool()
{
    this->CurConn = 0;
    this->FreeConn = 0;
}

/**
 * 个人注释
 * 单例模式调用
 * static 修饰 connPool 只有该函数能访问这个变量
 * 把引用返回
 * @return
 */
connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

/**
 * 个人注释
 * 初始化连接池 可能是因为单例模式不用构造函数初始化
 * 根据传进来的最大连接数构建数据库连接池
 */
//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn)
{
    this->url = url;
    this->Port = Port;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DBName;

    /**
     * 个人注释
     * 线程锁
     * 待研究
     */
    lock.lock();
    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        /**
         * 个人注释
         * 数据库的初始化方式
         * 不知道为啥不用new
         * 并且像是没用的初始化
         */
        con = mysql_init(con);

        /**
         * 个人注释
         * 如果初始化失败，就报错并退出，返回值为1
         * 可能有日志输出 待研究
         */
        if (con == NULL)
        {
            cout << "Error:" << mysql_error(con);
            exit(1);
        }

        /**
         * 个人注释
         * 真正开始连接
         * 传入参数为C++string类转c string类型
         */
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

        /**
         * 个人注释
         * 如果连接失败，就报错输出，返回值为1
         * 同样可能有日志输出
         */
        if (con == NULL)
        {
            cout << "Error: " << mysql_error(con);
            exit(1);
        }

        /**
         * 个人注释
         * 如果能到这里，说明连接成功
         * 将该连接放入清单中
         * C++的list类得花时间研究一下
         */
        connList.push_back(con);
        /**
         * 个人注释
         * 空闲连接数自增
         */
        ++FreeConn;
    }

    /**
     * 个人注释
     * 线程同步信号量
     * 不知道啥东西
     */
    reserve = sem(FreeConn);

    /**
     * 个人注释
     * 此时刚初始化
     * 最大连接数等于空闲连接数
     */
    this->MaxConn = FreeConn;

    lock.unlock();
}

/**
 * 个人注释
 * 从数据库连接池中获取一个数据库连接
 * @return
 */
//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;
    /**
     * 个人注释
     * 如果list里面没有连接，则获取失败,返回空值
     */
    if (0 == connList.size())
        return NULL;

    /**
     * 个人注释
     * 线程同步的函数
     * 不知道是啥
     */
    reserve.wait();

    /**
     * 个人注释
     * 线程锁
     * 待研究
     */
    lock.lock();
    /**
     * 个人注释
     * 获取最前面的数据库连接
     * 并且把最前面的连接弹出list
     */
    con = connList.front();
    connList.pop_front();

    /**
     * 个人注释
     * 空闲自减
     * 已用自增
     */
    --FreeConn;
    ++CurConn;
    /**
     * 个人注释
     * 解锁
     */
    lock.unlock();
    return con;
}

/**
 * 个人注释
 * 连接用完之后就释放
 * 即放回连接池
 * @param con
 * @return
 */
//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    /**
     * 个人注释
     * 如果传进来一个空指针，就返回错误
     */
    if (NULL == con)
        return false;

    /**
     * 个人注释
     * 线程锁
     */
    lock.lock();
    /**
     * 个人注释
     * 将传入的数据库连接放到list
     * 自增自减
     */
    connList.push_back(con);
    ++FreeConn;
    --CurConn;

    /**
     * 个人注释
     * 解锁
     */
    lock.unlock();
    /**
     * 个人注释
     * 不知道为啥在这post
     * 待研究
     */
    reserve.post();
    return true;
}

/**
 * 个人注释
 * 析构时调用
 */
//销毁数据库连接池
void connection_pool::DestroyPool()
{

    lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        /**
         * 个人注释
         * 不知道这个条件啥意思
         */
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            /**
             * 个人注释
             * 用库创建，就用库关闭
             */
            MYSQL *con = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;
        connList.clear();

        lock.unlock();
    }

    /**
     * 个人注释
     * 不知道为什么要解两次锁
     */
    lock.unlock();
}

/**
 * 个人注释
 * 普通set get
 * @return
 */
//当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->FreeConn;
}

/**
 * 个人注释
 * 析构
 */
connection_pool::~connection_pool()
{
    DestroyPool();
}

/**
 * 个人注释
 * 给线程池队列分配数据库连接用的
 * @param SQL
 * @param connPool
 */
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
    *SQL = connPool->GetConnection();

    /**
     * 当前分配的连接
     */
    conRAII = *SQL;
    /**
     * 当前用的连接池
     */
    poolRAII = connPool;
}

/**
 * 个人注释
 * 析构
 */
connectionRAII::~connectionRAII()
{
    /**
     * 个人注释
     * 使用连接池将当前连接放回连接池
     */
    poolRAII->ReleaseConnection(conRAII);
}