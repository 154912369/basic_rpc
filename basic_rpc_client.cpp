#include "meta.pb.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <google/protobuf/service.h>

#define MAX_LINE (1024)


void setnoblocking(int fd)
{
    int opts = 0;
    opts = fcntl(fd, F_GETFL);
    opts = opts | O_NONBLOCK;
    fcntl(fd, F_SETFL);
}
class MyChannel : public ::google::protobuf::RpcChannel {
    int  _sockfd;
    char _recvline[MAX_LINE + 1] = {0};
    public:
    void init(const char* ip_address, int port){
        printf("channel is [%s:%d]", ip_address, port);
        struct sockaddr_in server_addr;
        // 创建socket
        if ( (_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "socket error");
            exit(0);
        }

        // server addr 赋值
        bzero(&server_addr, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = ntohs(port);

        if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "inet_pton error for %s", ip_address);
            exit(0);
        }
        // 链接服务端
        if (connect(_sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
            perror("connect");
            fprintf(stderr, "connect error\n"); 
            exit(0);
        }
        setnoblocking(_sockfd);
    }


    std::string get(const std::string& input){
         // 把输入的字符串发送 到 服务器中去
         printf("input size is [%d]", input.size());
         int n = send(_sockfd, input.c_str(), input.size(), 0);
         if (n < 0)
         {
             perror("send");
        }

        // 读取 服务器返回的数据
        std::string output;
        while (1)
        {
            n = read(_sockfd, _recvline, MAX_LINE);
            if (n == MAX_LINE){
                output = output + _recvline;
                continue;
            }else if (n < 0){
                perror("recv");
                break;
            }else {
                output = output + _recvline;
                break;
            }
        }
        printf("[recv] %s\n", output.c_str());
        return output;
    }
    virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
            ::google::protobuf::RpcController* controller,
            const ::google::protobuf::Message* request,
            ::google::protobuf::Message* response,
            ::google::protobuf::Closure*) {
                std::string serialzied_data = request->SerializeAsString();
                test::Meta meta;
                meta.set_service_name(method->service()->name());
                meta.set_method_name(method->name());
                meta.set_data_size(serialzied_data.size());
                std::string serialzied_str = meta.SerializeAsString();
                int serialzied_size = serialzied_str.size();
                printf("meta size is [%d]\n", serialzied_size);
                printf("data size is [%d]\n", serialzied_data.size());
                std::string top_string((const char *)&serialzied_size, sizeof(int));
                for (int i = 0; i < 4;i++){
                    printf("char id is [%d]\n", top_string[i] - '0');
                }

                serialzied_str.insert(0, top_string);
                 printf("se char id is [%d]\n", serialzied_str[0] - '0');
                serialzied_str += serialzied_data;
                response->ParseFromString(get(serialzied_str));
            }
};

class MyController : public ::google::protobuf::RpcController {
public:
  virtual void Reset() { };

  virtual bool Failed() const { return false; };
  virtual std::string ErrorText() const { return ""; };
  virtual void StartCancel() { };
  virtual void SetFailed(const std::string& /* reason */) { };
  virtual bool IsCanceled() const { return false; };
  virtual void NotifyOnCancel(::google::protobuf::Closure* /* callback */) { };
};//MyController
