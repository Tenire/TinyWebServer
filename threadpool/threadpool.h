#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

/**
 * 个人注释
 * 模板 不是很清楚这个怎么用
 * @tparam T
 */
template <typename T>
/**
 * 个人注释
 * 线程池类
 * @tparam T
 */
class threadpool
{
public:
    /**
     * 个人注释
     * 带默认参数的构造函数
     * @param connPool
     * @param thread_number
     * @param max_request
     */
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    /**
     * 个人注释
     * 析构
     */
    ~threadpool();
    /**
     * 个人注释
     * 追加
     * @param request
     * @return
     */
    bool append(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
    connection_pool *m_connPool;  //数据库
};
/**
 * 个人注释
 * 模板
 * 话说上面已经有声明了
 * @tparam T
 * @param connPool
 * @param thread_number
 * @param max_requests
 */
template <typename T>
/**
 * 个人注释
 * 构造函数
 * 这个参数不太懂
 * @tparam T
 * @param connPool
 * @param thread_number
 * @param max_requests
 */
threadpool<T>::threadpool( connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL),m_connPool(connPool)
{
    /**
     * 个人注释
     * 都为0创建了个寂寞
     */
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    /**
     * 个人注释
     * 初始化一个线程数组，即线程池
     */
    m_threads = new pthread_t[m_thread_number];
    /**
     * 个人注释
     * 这都能失败那还是别初始化了
     */
    if (!m_threads)
        throw std::exception();
    /**
     * 个人注释
     * 线程一个个初始化创建
     */
    for (int i = 0; i < thread_number; ++i)
    {
        //printf("create the %dth thread\n",i);
        /**
         * 个人注释
         * 失败了释放内存别干了
         */
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        /**
         * 个人注释
         * 不知道detach是干嘛的
         */
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
/**
 * 个人注释
 * 为什么到处声明模板
 * @tparam T
 */
template <typename T>
/**
 * 个人注释
 * 析构
 * @tparam T
 */
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}
/**
 * 个人注释
 * 又又又声明一次
 * @tparam T
 * @param request
 * @return
 */
template <typename T>

/**
 * 个人注释
 * 追加线程供使用
 * @tparam T
 * @param request
 * @return
 */
bool threadpool<T>::append(T *request)
{
    /**
     * 个人注释
     * 线程锁
     */
    m_queuelocker.lock();
    /**
     * 个人注释
     * 如果还有线程能用就给你
     * 否则直接解锁退出
     */
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    /**
     * 个人注释
     * 放到队列里面
     */
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
/**
 * 个人注释
 * 不管你了
 * @tparam T
 * @param arg
 * @return
 */
template <typename T>
/**
 * 个人注释
 * 线程池跑起来？
 * @tparam T
 * @param arg
 * @return
 */
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
template <typename T>
/**
 * 个人注释
 * 线程池跑
 * @tparam T
 */
void threadpool<T>::run()
{
    /**
     * 个人注释
     * 只要线程池没死，就一直运行
     */
    while (!m_stop)
    {
        /**
         * 个人注释
         * 这两不知道干嘛用的
         */
        m_queuestat.wait();
        m_queuelocker.lock();
        /**
         * 个人注释
         * 队列空的
         * 那就不管了
         */
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        /**
         * 个人注释
         * 到这明显队列不空
         * 那就获取队列第一个请求
         */
        T *request = m_workqueue.front();
        /**
         * 个人注释
         * 获取后当然就弹出了
         */
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        /**
         * 个人注释
         * 如果获取的是空的，那就不搞了
         */
        if (!request)
            continue;

        /**
         * 个人注释
         * 传入队列中元素的数据库指针的指针，然后从数据库连接池中获取一个连接(指针)
         */
        connectionRAII mysqlcon(&request->mysql, m_connPool);

        /**
         * 个人注释
         * 处理(不知道处理个啥)
         */
        request->process();
    }
}
#endif
