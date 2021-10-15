#include "basic_rpc_service.cpp"
#include "message.pb.h"
class  TestServiceImpl: public tutorial::TestController{
    void send(::google::protobuf::RpcController* controller,
                       const ::tutorial::Request* request,
                       ::tutorial::Response* response,
                       ::google::protobuf::Closure* done){
        printf("process ControllerImpl, send\n");
        response->add_result("TestController,send" + request->name());
        done->Run();
    }

    void echo(::google::protobuf::RpcController* controller,
                    const ::tutorial::Request* request,
                    ::tutorial::ResponseOther* response,
                    ::google::protobuf::Closure* done){
    printf("process ControllerImpl, echo\n");
    response->add_other("TestController,echo" + request->name());
    done->Run();
                    }
};

int main(){
    Server ser(7778);
    TestServiceImpl impl;
    ser.register_service(&impl);
    ser.startServer();
}