#ifndef __THREADPOLL_HPP__
#define __THREADPOLL_HPP__

#include <iostream>
#include <queue>
#include <pthread.h>

typedef void (*handler_t)(int);//定义一个函数指针类型
class Task
{
    private:
        int sock;
        handler_t handler;
    public:
        Task(int _sock, handler_t _handler):sock(_sock),handler(_handler)
        {}

        void Run()
        {
            handler(sock);
        }

        ~Task()
        {}
};

class ThreadPool
{
    private:
        int max_thread;
        std::queue<Task> task_queue;
        int idle_thread;
        pthread_mutex_t lock;
        pthread_cond_t cond;
    public:
        ThreadPool(int count):max_thread(count),idle_thread(0)
        {
            pthread_mutex_init(&lock,NULL);
            pthread_cond_init(&cond,NULL);
        }
        void InitThreadPool()
        {
            pthread_t tid;
            for(auto i=0;i<max_thread;i++)
            {
                pthread_create(&tid,NULL,ThreadRoutine,(void*)this);
            }
        }
        void LockTaskQueue()
        {
            pthread_mutex_lock(&lock);
        }
        void UnlockTaskQueue()
        {
            pthread_mutex_unlock(&lock);
        }
        void Idle()
        {
            idle_thread++;
            pthread_cond_wait(&cond,&lock);//wait的时候会自动解锁，被唤醒的时候又会自动拿上锁
            idle_thread--;
        }
        void WakeUp()
        {
            pthread_cond_signal(&cond);
        }
        Task PopTask()
        {
            Task t=task_queue.front();
            task_queue.pop();
            return t;
        }
        bool IsTaskQueueEmpty()
        {
            return task_queue.size()==0?true:false;
        }
        void PushTask(Task &t)
        {
            LockTaskQueue();
            task_queue.push(t);
            UnlockTaskQueue();
            WakeUp();
        }
        static void *ThreadRoutine(void *arg)//类成员函数默认第一个参数为this指针，但是线程函数又只能有一个参数，所以要声明成static，那么问题又来了，线程函数中肯定是处理任务的，而任务队列又是在对象当中，所以将当前对象作为参数传递进来
        {
            pthread_detach(pthread_self());
            ThreadPool *tp=(ThreadPool*)arg;
            for(;;)
            {
                tp->LockTaskQueue();
                while(tp->IsTaskQueueEmpty())
                {
                    tp->Idle();//这里有可能出现线程Idle失败或者说任务队列中还没有任务线程就被假唤醒，这时候去Pop任务队列就会出错，所以这里将判断队列是否为空改为循环判断
                }
                Task t=tp->PopTask();
                tp->UnlockTaskQueue();
                std::cout<<"Task is handler by:"<<pthread_self()<<std::endl;
                t.Run();
            } 
            return NULL;
        }
        ~ThreadPool()
        {
            pthread_mutex_destroy(&lock);
            pthread_cond_destroy(&cond);
        }
};

class Singleton
{
    private:
        static ThreadPool *p;
        static pthread_mutex_t lock;
    public:
        static ThreadPool *GetInstance()
        {
            if(p==NULL)
            {
                pthread_mutex_lock(&lock);
                if(p==NULL)//在外面再加一层判断的原因是为了提高效率，降低锁的开销。当一个线程进入到这里面new上一个对象以后赋值给p，即使它的时间片到了，这时候p已经不为空了，别的线程在外面那层判断就会直接返回，而不会进入到内部去加锁
                {
                    p=new ThreadPool(5);
                    p->InitThreadPool();
                }
                pthread_mutex_unlock(&lock);
            }
            return p;
        }
};
ThreadPool *Singleton::p=nullptr;
pthread_mutex_t Singleton::lock=PTHREAD_MUTEX_INITIALIZER;//用来初始化锁的宏，这样不用调函数初始化也不用销毁

#endif
