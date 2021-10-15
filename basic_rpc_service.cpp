
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <stdio.h>
#include "string.h"
#include "meta.pb.h"
#include <map>
#include <google/protobuf/service.h>

#define EPOLL_MAX_NUM       (2048)
#define BUFFER_MAX_LEN      (4096)

class MyController : public ::google::protobuf::RpcController {
public:
  virtual void Reset() { };

  virtual bool Failed() const { return false; };
  virtual std::string ErrorText() const { return ""; };
  virtual void StartCancel() { };
  virtual void SetFailed(const std::string& /* reason */) { };
  virtual bool IsCanceled() const { return false; };
  virtual void NotifyOnCancel(::google::protobuf::Closure* /* callback */) { };
};

class Server{
    struct ServiceInfo{
        ::google::protobuf::Service* service;
        const ::google::protobuf::ServiceDescriptor* sd;
        std::map<std::string, const ::google::protobuf::MethodDescriptor*> mds;
    };
    int _listen_fd = 0;
    int _epfd;
    struct epoll_event * _my_events =(struct epoll_event *) malloc(sizeof(struct epoll_event) * EPOLL_MAX_NUM);
    char _buffer[BUFFER_MAX_LEN];
    std::map<std::string, ServiceInfo> _services;

    public:
    void register_service(::google::protobuf::Service *service){

        ServiceInfo service_info;
        service_info.service = service;
        service_info.sd = service->GetDescriptor();
        for (int i = 0; i < service_info.sd->method_count(); ++i) {
            service_info.mds[service_info.sd->method(i)->name()] = service_info.sd->method(i);
        }

        _services[service_info.sd->name()] = service_info;

    }
    Server(int port){
        printf("init server\n");
        _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_listen_fd < 0) {
            fprintf(stderr, "socket() failed: %s\n", strerror(errno));
            exit(1);
        }
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(port);
        

        if(bind(_listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
            fprintf(stderr, "socket() failed: %s\n", strerror(errno));
            exit(1);
        }

        listen(_listen_fd, 10);
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = _listen_fd;
        _epfd = epoll_create(EPOLL_MAX_NUM);
        
        if (epoll_ctl(_epfd, EPOLL_CTL_ADD, _listen_fd, &event) < 0)
        {
            perror("epoll ctl add listen_fd ");
        }
    }
    void ServerCallback(::google::protobuf::Message* response,
                    int client_fd){
        printf("Server Callback \n");
        std::string result = response->SerializeAsString();
        write(client_fd, result.c_str(), result.size());

    }
    
    void startServer()
    {
        printf("start server\n");
        while(1){
             int active_fds_cnt = epoll_wait(_epfd, _my_events, EPOLL_MAX_NUM, -1);
         for (int i = 0; i < active_fds_cnt; i++) {
            if (_my_events[i].data.fd == _listen_fd) {
                 struct sockaddr_in client_addr;
                  socklen_t client_len = 20;
                 //accept
                 int client_fd = accept(_listen_fd, (struct sockaddr *)&client_addr, &client_len);
                 if (client_fd < 0)
                 {
                     perror("accept");
                     continue;
                }

                char ip[20];
                printf("new connection[%s:%d]\n", inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)), ntohs(client_addr.sin_port));
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client_fd;
                epoll_ctl(_epfd, EPOLL_CTL_ADD, client_fd, &event);
            }else if (_my_events[i].events & EPOLLIN) {
                printf("EPOLLIN\n");
                int client_fd = _my_events[i].data.fd;
                int n = read(client_fd, _buffer, sizeof(int));
                if(n<=0){
                    printf("get n [%d]\n", n);
                    struct epoll_event event;
                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = client_fd;
                    epoll_ctl(_epfd, EPOLL_CTL_DEL, client_fd, &event);
                    close(client_fd);
                    continue;
                }
                for (int i = 0; i < n;i++){
                    printf("char id is [%d]\n", _buffer[i] - '0');
                }
                int meta_len = *(int *)_buffer;
                printf("meta_size is [%d]\n",meta_len);
                n = read(client_fd, _buffer, meta_len);
                test::Meta meta;
                if(n<meta_len){
                    printf("error");
                }else{
                    meta.ParseFromString(std::string(_buffer, meta_len));
                    printf("meta_method is [%s]\n",meta.service_name().c_str());
                    printf("meta_name is [%s]\n",meta.method_name().c_str());
                    printf("data_size is [%d]\n",meta.data_size());
                }
                std::string input;
                while (input.size()<meta.data_size())
                {
                    n = read(client_fd, _buffer, BUFFER_MAX_LEN);
                    printf(" n is [%d]\n", n);
                    input = input.append(_buffer,n);
                    printf("current input size is[%d]\n", input.size());
                }

                
               
                ServiceInfo service_info=_services[meta.service_name()];
                const ::google::protobuf::MethodDescriptor* method = service_info.mds[meta.method_name()];
                auto request = service_info.service->GetRequestPrototype(method).New();
                request->ParseFromString(input);
                auto response = service_info.service->GetResponsePrototype(method).New();
                MyController controller;
                auto done = ::google::protobuf::NewCallback(
                                            this,
                                            &Server::ServerCallback,
                                            response,
                                            client_fd);
                service_info.service->CallMethod(method,
                                                 &controller,
                                                 request,
                                                 response,
                                                done);
                printf("event_process end\n");
            }else{
                printf("event is [%d]\n", _my_events[i].events);
                int client_fd = _my_events[i].data.fd;
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client_fd;
                epoll_ctl(_epfd, EPOLL_CTL_DEL, client_fd, &event);
                close(client_fd);

            }

        }

        }
    }
};




 