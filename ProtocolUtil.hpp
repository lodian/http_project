#ifndef __PROTOCOLUTIL_HPP__
#define __PROTOCOLUTIL_HPP__

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <algorithm>
#include <strings.h>
#include <unordered_map>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 5
#define BUFF_NUM 1024

#define NORMAL 0
#define WARNING 1
#define ERROR 2

#define WEBROOT "wwwroot"
#define HOMEPAGE "index.html"
#define HTTPVERSION "HTTP/1.0"

const char *ErrLevel[]={
    "Normal",
    "Warning",
    "Error"
};

void log(std::string msg, int level, std::string file, int line)
{
    std::cout<<"["<<file<<":"<<line<<"] "<<msg<<"["<<ErrLevel[level]<<"]"<<std::endl;
}

#define LOG(msg,level) log(msg,level,__FILE__,__LINE__);

class Util
{
    public:
        static void MakeKV(std::string s, std::string &k, std::string &v)
        {
            std::size_t pos=s.find(": ");
            k=s.substr(0,pos);
            v=s.substr(pos+2);
        }
        static std::string IntToString(int &x)
        {
            std::stringstream ss;
            ss<<x; 
            return ss.str();
        }
        static std::string CodeToDesc(int code)
        {
            switch(code)
            {
                case 200:
                    return "OK";
                case 404:
                    return "Page Not Found";
                default:
                    break;
            }
            return "Unknow";
        }
        static std::string SuffixToContent(std::string &suffix)
        {
            if(suffix==".css")
            {
                return "text/css";
            }
            if(suffix==".js")
            {
                return "application/x-javascript";
            }
            if(suffix==".html" || suffix==".htm")
            {
                return "text/html";
            }
            if(suffix==".css")
            {
                return "text/css";
            }
            if(suffix==".svg")
            {
                return "text/xml";
            }
            if(suffix==".jpg")
            {
                return "application/x-jpg";
            }
            if(suffix==".png")
            {
                return "application/x-png";
            }
            if(suffix==".ico")
            {
                return "image/x-icon";
            }
            return "text/html";
        }
};
class SocketApi{
    public:
        static int Socket()
        {
            int sock=socket(AF_INET,SOCK_STREAM,0);
            if(sock<0)
            {
                LOG("socket error!",ERROR);
                exit(2);
            }
            int opt=1;
            setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
            return sock;
        }
        static void Bind(int sock, int port)
        {
            struct sockaddr_in local;
            bzero(&local,sizeof(local));
            local.sin_family=AF_INET;
            local.sin_port=htons(port);
            local.sin_addr.s_addr=htonl(INADDR_ANY);
            if(bind(sock,(struct sockaddr*)&local,sizeof(local))<0)
            {
                LOG("bind error!",ERROR);
                exit(3);
            }
        }
        static void Listen(int sock)
        {
            if(listen(sock,BACKLOG)<0)
            {
                LOG("listen error!",ERROR);
                exit(4);
            }
        }
        static int Accept(int listen_sock, std::string &ip, int &port)
        {
            struct sockaddr_in peer;
            socklen_t len=sizeof(peer);
            int sock=accept(listen_sock,(struct sockaddr*)&peer,&len);
            if(sock<0)
            {
                LOG("accept error!",WARNING);
                return -1;
            }
            port=ntohs(peer.sin_port);
            ip=inet_ntoa(peer.sin_addr);
            return sock;
        }
};


class Http_Response
{
    public:
        //协议字段
        std::string status_line;
        std::vector<std::string> response_header;
        std::string blank;
        std::string response_text;
    private:
        int code;
        std::string path;
        int resource_size;
    public:
        Http_Response():blank("\r\n"),code(200)
    {}
        int& GetCode()
        {
            return code;
        }
        void SetPath(std::string &_path)
        {
            path=_path;
        }
        std::string &GetPath()
        {
            return path;
        }
        void SetResourceSize(int rs)
        {
            resource_size=rs;
        }
        int ResourceSize()
        {
            return resource_size;
        } 
        void MakeStatusLine()
        {
            status_line = HTTPVERSION;
            status_line += " ";
            status_line += Util::IntToString(code);
            status_line += " ";
            status_line += Util::CodeToDesc(code);
            status_line += "\r\n";
            LOG("Make StatusLine Done!", NORMAL);
        }
        void MakeResponseHeader()
        {
            std::string line;
            std::string suffix;
            //构建Content-Type
            line="Content-Type: ";
            std::size_t pos = path.rfind('.');
            if(std::string::npos!=pos)//没后缀的不考虑
            {
                suffix=path.substr(pos);
                transform(suffix.begin(),suffix.end(),suffix.begin(),::tolower);//将后缀全部转为小写
            }
            line+=Util::SuffixToContent(suffix);
            line+="\r\n";
            response_header.push_back(line);

            //构建Content-length
            line = "Content-Length: ";
            line += Util::IntToString(resource_size);
            line += "\r\n";
            response_header.push_back(line);

            //空行
            line = "\r\n";
            response_header.push_back(line);
            LOG("Make ResponseHeader Done!", NORMAL);
        }
        ~Http_Response()
        {}
};

class Http_Request{
    public:
        //协议字段
        std::string request_line;
        std::vector<std::string> request_header;
        std::string blank;
        std::string request_text;
    private:
        //解析字段
        std::string method;
        std::string uri;
        std::string version;
        std::string path;
        std::string query_string;
        std::unordered_map<std::string,std::string> header_kv;
        bool cgi;
    public:
        Http_Request():path(WEBROOT),cgi(false),blank("\r\n")
    {}
        void RequestLineParse()//将一个字符串以空格分割成三个字符串
        {
            std::stringstream ss(request_line);
            ss>>method>>uri>>version;
            transform(method.begin(),method.end(),method.begin(),::toupper);
        }
        void UriParse()
        {
            if(method=="GET")
            {
                std::size_t pos=uri.find('?');
                if(pos!=std::string::npos)
                {
                    cgi=true;
                    path+=uri.substr(0,pos);
                    query_string=uri.substr(pos+1);
                } 
                else
                {
                    path+=uri;
                }
            }
            else
            {
                cgi=true;
                path+=uri;
            }
            if(path[path.size() - 1] == '/')
            {
                path+=HOMEPAGE;
            }
        }
        void HeaderParse()
        {
            std::string k, v;
            for(auto it=request_header.begin();it!=request_header.end();it++)
            {
                Util::MakeKV(*it, k, v);
                header_kv.insert({k,v});
            }
        }
        bool IsMethodLegal()
        {
            //要考虑到大小写的不同，稳妥起见先将method转成大写
            if(method!="GET" && method!="POST")
            {
                return false;
            }
            return true;
        }
        int IsPathLegal(Http_Response *rsp)
        {
            int rs=0;
            struct stat st;
            if(stat(path.c_str(),&st)<0)
            {
                return 404;
            }
            else
            {  
                rs=st.st_size;
                if(S_ISDIR(st.st_mode))
                {
                    path += "/";
                    path += HOMEPAGE;
                    stat(path.c_str(), &st);
                    rs=st.st_size;
                }
                else if((st.st_mode & S_IXUSR) || 
                        (st.st_mode & S_IXGRP) ||
                        (st.st_mode & S_IXOTH) )
                {
                    cgi=true;
                }
                else
                {
                    //TODO
                }
            }
            rsp->SetPath(path);
            rsp->SetResourceSize(rs);
            LOG("Path is OK!", NORMAL);
            return 0;
        }
        bool IsNeedContinueRecv()
        {
            return method == "POST" ? true : false;
        }
        bool IsCgi()
        {
            return cgi;
        }
        int ContentLength()
        {
            int content_length=-1;
            std::string cl = header_kv["Content-Length"];
            std::stringstream ss(cl);
            ss>>content_length;
            return content_length;
        }
        std::string GetParam()
        {
            if(method=="GET")
            {
                return query_string;
            }
            else
            {
                return request_text;
            }

        }
        ~Http_Request()
        {}
};

class Connect
{
    private:
        int sock;
    public:
        Connect(int _sock):sock(_sock)
    {}
        int RecvOneLine(std::string &_line)
        {
            char buff[BUFF_NUM];
            int i=0;
            char c='X';
            while(c != '\n' && i < BUFF_NUM - 1)
            {
                ssize_t s=recv(sock,&c,1,0);
                if(s>0)
                {
                    if(c=='\r')
                    {
                        recv(sock,&c,1,MSG_PEEK);
                        if(c=='\n')
                        {
                            recv(sock,&c,1,0);
                        }
                        else
                        {
                            c='\n';
                        }
                    }
                    //\r \r\n \n -> \n
                    buff[i++]=c;
                }
                else
                {
                    break;
                }
            }
            buff[i]=0;
            _line=buff;
            return i;
        }
        void RecvRequestHeader(std::vector<std::string> &v)
        {
            std::string line="X";
            while(line!="\n")
            {
                RecvOneLine(line);
                if(line!="\n")
                {
                    v.push_back(line);
                }
            }
            LOG("Header Recv OK!", NORMAL);
        }
        void RecvText(std::string &text , int content_length)
        {
            char c;
            for(auto i=0;i<content_length;i++)
            {
                recv(sock,&c,1,0);
                text.push_back(c);
            }
        }
        void SendStatusLine(Http_Response *rsp)
        {
            std::string &sl=rsp->status_line;
            send(sock, sl.c_str(), sl.size(), 0);
        }
        void SendResponseHeader(Http_Response *rsp)
        {
            std::vector<std::string> &v=rsp->response_header;
            for(auto it=v.begin();it!=v.end();it++)
            {
                send(sock,it->c_str(),it->size(),0);
            }
        }
        void SendResponseText(Http_Response *rsp, bool _cgi)
        {
            if(!_cgi)
            {
                std::string &path=rsp->GetPath();
                int fd=open(path.c_str(), O_RDONLY);
                if(fd<0)
                {
                    LOG("open file error!", WARNING);
                    return;
                }
                sendfile(sock,fd,NULL,rsp->ResourceSize());
                close(fd);
            }
            else
            {
                std::string &rsp_text=rsp->response_text;
                send(sock, rsp_text.c_str(), rsp_text.size(), 0);
            }
        }
        ~Connect()
        {
            close(sock);
        }
};

class Entry
{
    public:
        static void ProcessNonCgi(Connect *con, Http_Request *rq, Http_Response *rsp)
        {
            rsp->MakeStatusLine();
            rsp->MakeResponseHeader();
            //rsp->MakeResponseText(rq);

            con->SendStatusLine(rsp);
            con->SendResponseHeader(rsp);//将空行一起构建好发送
            con->SendResponseText(rsp, false);
            LOG("Send Response Done!", NORMAL);
        }
        static int ProcessCgi(Connect *con, Http_Request *rq, Http_Response *rsp)
        {
            int input[2];
            int output[2];
            pipe(input);
            pipe(output);
            std::string bin=rsp->GetPath();//path中放的就是客户端请求的可执行程序的路径
            std::string param=rq->GetParam();
            int size=param.size();
            std::string param_size="CONTENT-LENGTH=";
            param_size+=Util::IntToString(size);

            std::string &response_text=rsp->response_text;

            pid_t pid=fork();
            if(pid<0)
            {
                return 503;
                LOG("Fork Error!", WARNING);
            }
            else if(pid==0)
            {
                //子进程从input中读数据，把运行结果写入output中
                close(input[1]);
                close(output[0]);
                //子进程要拿到参数，必须知道参数有多长,所以这里设置一个环境变量，而子进程即使进行了程序替换还是会继承环境变量
                putenv((char *)param_size.c_str());
                //管道本质是文件，程序替换之后，文件描述符可能被删除或者被替换掉，所以bin程序没有办法拿到这两个管道
                //所以这里做一个规定，bin程序从标准输入读数据，向标准输出写数据，因此这里要进行重定向
                dup2(input[0],0);
                dup2(output[1],1);
                execl(bin.c_str(), bin.c_str(), NULL);
                //不用判断是否调用失败，如果替换失败，代码就没有被替换掉，程序继续运行就会exit
                exit(1);
            }
            else
            {   
                close(input[0]);
                close(output[1]);
                char c;
                for(auto i=0;i<size;i++)
                {
                    c=param[i];
                    write(input[1], &c, 1);
                }
                waitpid(pid, NULL, 0);
                while(read(output[0], &c, 1)>0)//管道本质是文件，而文件描述符的生命周期随进程，所以程序走到这里，waitpid肯定成功，子进程退出了，所以文件描述符会被关闭，而写端被关闭，read肯定不会阻塞
                {
                    response_text.push_back(c);
                }

                rsp->MakeStatusLine();
                rsp->SetResourceSize(response_text.size());
                rsp->MakeResponseHeader();

                con->SendStatusLine(rsp);
                con->SendResponseHeader(rsp);
                con->SendResponseText(rsp, true);
            }
            return 200;
        }
        static void ProcessResponse(Connect *con, Http_Request *rq, Http_Response *rsp)
        {
            if(rq->IsCgi())
            {
                //能走到这里，说明请求中肯定传参了，而且这个参数已经拿到了
                LOG("MakeResponse Use Cgi!", NORMAL);
                ProcessCgi(con, rq, rsp);
            }
            else
            {
                LOG("MakeResponse Use NonCgi!", NORMAL);
                ProcessNonCgi(con, rq, rsp);
            }
        }
        static void HandlerRequest(int sock)
        {
            pthread_detach(pthread_self());
            //int *sock=(int*)arg;

#ifdef _DEBUG_
            //for test
            char buff[10240];
            read(sock,buff,sizeof(buff));
            std::cout<<buff<<std::endl;

#else
            Connect *con=new Connect(sock);
            Http_Request *rq=new Http_Request;
            Http_Response *rsp=new Http_Response;
            int& code=rsp->GetCode();
            con->RecvOneLine(rq->request_line);
            rq->RequestLineParse();
            if(!rq->IsMethodLegal())
            {
                code=404;
                LOG("Request Method Is Not Legal!",WARNING);
                goto end;
            }
            rq->UriParse();
            if(rq->IsPathLegal(rsp)!=0)
            {
                code=404; 
                LOG("file is not exist!",WARNING);
                goto end;
            }
            con->RecvRequestHeader(rq->request_header);
            rq->HeaderParse();
            if(rq->IsNeedContinueRecv())
            {
                LOG("POST Method, Need Recv Continue!", NORMAL);
                con->RecvText(rq->request_text, rq->ContentLength());
            }
            LOG("Http Request Recv Done!", NORMAL);
            ProcessResponse(con, rq,rsp);


end:
            delete con;
            delete rq;
            delete rsp;
            //delete sock;
#endif
            //return NULL;
        }
};

#endif
