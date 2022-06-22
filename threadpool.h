#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
//一旦声明了类模板，就可以将类型参数用于类的成员函数和成员变凉了。换句话说，原来用int、float、插入、等内置类型的地方，都可以用类型参数来代替。
template<typename T>
class threadpool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg); //静态成员函数
    void run();
    //回调函数必须是静态成员函数或者全局函数来实现回调函数，大概原因是普通的C++成员函数都隐含了一个函数参数，即this指针，
    //C++通过传递this指针给成员函数从而实现函数可以访问类的特定对象的数据成员。由于this指针的原因，使
    //得一个普通成员函数作为回调函数时就会因为隐含的this指针问题使得函数参数个数不匹配，从而导致回调函数编译失败。

private:
    // 线程的数量
    int m_thread_number;  
    
    // 描述线程池的数组，大小为m_thread_number    
    pthread_t * m_threads;

    // 请求队列中最多允许的、等待处理的请求的数量  
    int m_max_requests; 
    
    // 请求队列
    std::list< T* > m_workqueue;  

    // 保护请求队列的互斥锁
    locker m_queuelocker;   //互斥锁对象

    // 是否有任务需要处理
    sem m_queuestat; //信号量对象  不带参数构造对象则执行不带参数的构造函数；带参数构造对象则执行带参数的构造函数。
    //所以信号量初始值为0
    // 是否结束线程          
    bool m_stop;                    
};

template< typename T >//模板头
//类外定义成员函数需带上模板头   返回值类型 类名<类型参数1，类型参数2> ：：函数名（形参列表）
threadpool< T >::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests),  //函数表达研究一下
        m_stop(false), m_threads(NULL) {

    if((thread_number <= 0) || (max_requests <= 0) ) {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];//线程池数组
    if(!m_threads) {
        throw std::exception();
    }

    // 创建thread_number 个线程，并将他们设置为脱离线程。
    for ( int i = 0; i < thread_number; ++i ) {
        printf( "create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this ) != 0) {//worker必须是静态函数
            delete [] m_threads;
            throw std::exception();
        }
        
        if( pthread_detach( m_threads[i] ) ) {
            delete [] m_threads;//出错就删除
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool() {
    delete [] m_threads;
    m_stop = true; //停止线程
}

template< typename T >
bool threadpool< T >::append( T* request ) //这个request有什么属性与权限
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    m_queuelocker.lock();
    if ( m_workqueue.size() > m_max_requests ) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); //信号量+1
    return true;
}

template< typename T >
//创建线程通过回调函数执行run进行process处理
void* threadpool< T >::worker( void* arg ) //回调函数，创建线程时触发
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run() {

    while (!m_stop) {
        m_queuestat.wait();//信号量有值 就不堵塞；没有值就堵塞  这里初始值为0，所以只有执行append 信号量+1之后才可以继续执行下去，保证了多线程多任务同步的。
        m_queuelocker.lock();
        if ( m_workqueue.empty() ) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( !request ) {
            continue;
        }
        request->process();
    }

}

#endif
