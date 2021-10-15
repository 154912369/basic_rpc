#include "basic_rpc_client.cpp"
#include "message.pb.h"
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage ./client <SERVER_IP> <SERVER_PORT>\n");
        exit(0);
    }

    MyChannel channel;
    channel.init(argv[1], atoi(argv[2]));

    tutorial::Request request;
    char input[100];
    while (fgets(input, 100, stdin) != NULL){
        request.set_name(input);
        request.set_email(input);
        request.set_id(sizeof(input));
        tutorial::ResponseOther response;

        tutorial::TestController_Stub stub(&channel);
        MyController cntl;
        stub.echo(&cntl, &request, &response, NULL);
        std::cout << "resp:" << response.other(0).c_str()<< std::endl;

    }
    
     return 0;
}