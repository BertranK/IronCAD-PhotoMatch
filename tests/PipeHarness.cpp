#include "../src/PipeServer.h"
#include "../third_party/nlohmann/json.hpp"
#include <iostream>

int main() {
    try {
        PipeServer pipe;pipe.Open();std::cout<<GetCurrentProcessId()<<std::endl;
        for(;;){pipe.Poll([](const std::string& data){
            auto request=nlohmann::json::parse(data);
            if(request.at("args").contains("delay"))Sleep(request["args"]["delay"].get<DWORD>());
            nlohmann::json response={{"ok",true},{"id",request.at("id")},{"state",request.at("args")}};
            if(request.at("args").contains("large"))response["state"]={{"payload",std::string(200000,'x')}};
            return response.dump();
        });Sleep(5);}
    }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
