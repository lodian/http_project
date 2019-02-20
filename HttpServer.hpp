#ifndef __HTTPSERVER_HPP__
#define __HTTPSERVER_HPP__

#include <iostream>
#include <pthread.h>
#include "ProtocolUtil.hpp"
#include "ThreadPool.hpp"

class HttpServer{
    private:
        int listen_sock;
        int port;
        //ThreadPool thread_pool;
    public:
        HttpServer(int _port):port(_port),listen_sock(-1)//,thread_pool(5)
        {}
        void InitServer()
        {
            listen_sock=SocketApi::Socket();
            SocketApi::Bind(listen_sock,port);
            SocketApi::Listen(listen_sock);
            //thread_pool.InitThreadPool();
        }
        void Start()
        {
           for(;;)
           {
               std::string peer_ip;
               int peer_port;
               //int *sockp=new int;
               //*sockp=SocketApi::Accept(listen_sock,peer_ip,peer_port);
               int sock=SocketApi::Accept(listen_sock,peer_ip,peer_port);
               //if(*sockp>=0)
               if(sock>=0)
               {
                   std::cout<<peer_ip<<" : "<<peer_port<<std::endl;
                   Task t(sock, Entry::HandlerRequest);
                   //thread_pool.PushTask(t);
                   Singleton::GetInstance()->PushTask(t);
                   //pthread_t tid;
                   //pthread_create(&tid,NULL,Entry::HandlerRequest,(void*)sockp);

               }
           } 
        }
        ~HttpServer()
        {
            if(listen_sock>=0)
            {
                close(listen_sock);
            }
        }
};

#endif
