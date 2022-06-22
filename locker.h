#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 线程同步机制封装类

// 互斥锁类  ------ 数据安全
class locker {
public:
    locker() {
        if(pthread_mutex_init(&m_mutex, NULL) != 0) {//互斥锁初始化
            throw std::exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get()//获取互斥量
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};


// 条件变量类,满足某个条件解除阻塞或者阻塞，不是锁 --- 队列中数据
//信号量与条件变量的区别：
//1 、使用条件变量可以一次唤醒所有等待者，而这个信号量没有的功能
//2 、信号量是有一个值（状态的），而条件变量是没有的，没有地方记录唤醒（发送信号）过多少次，也没有地方唤醒线程（wait返回）过多少次
class cond {
public:
    cond(){
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex); //阻塞函数，调用该函数，线程会阻塞，等待唤醒，进入之后，首先进行解锁 
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);//等待多长时间，调用了这个函数，线程会阻塞，直到指定的时间结束。、
        //当这个函数调用阻塞的时候，会对互斥锁进行解锁，当不阻塞时，继续向下执行，会重新加锁。
        return ret == 0;
    }
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};


// 信号量类  注意与锁的功用区分开，锁是为了保证操作安全，信号量则是为了增强线程流畅性，有资源了再去上锁操作，相当于多了一次判断，而不是所有线程不管有没有资源就去抢这个锁，有资源才能抢，不然一直堵塞，如果没资源抢锁，极易导致线程死锁，降低线程效率
class sem {
public:
    sem() {
        if( sem_init( &m_sem, 0, 0 ) != 0 ) { //信号量初始值为0（第三个值） 第二个值不为0时信号量在进程间共享，否则只能为当前进程的所有线程共享
            throw std::exception();
        }
    }
    sem(int num) {
        if( sem_init( &m_sem, 0, num ) != 0 ) {
            throw std::exception();
        }
    }
    ~sem() {
        sem_destroy( &m_sem );
    }
    // 等待信号量
    bool wait() {
        return sem_wait( &m_sem ) == 0;//被用来阻塞当前线程直到信号量sem值大于0，解除阻塞后将sem的值减一，表明公共资源经使用后减少
    }
    // 增加信号量
    bool post() {
        return sem_post( &m_sem ) == 0;//用来增加信号量的值,当有线程阻塞在这个信号量时，调用这个函数会使其中的一个线程不再阻塞
    }
private:
    sem_t m_sem;
};

#endif