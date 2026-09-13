#include "PipeServer.h"
#include <sddl.h>

void PipeServer::Open() {
    HANDLE token=nullptr;checked(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)?S_OK:HRESULT_FROM_WIN32(GetLastError()));
    DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<BYTE> buffer(size);
    BOOL ok=GetTokenInformation(token,TokenUser,buffer.data(),size,&size);CloseHandle(token);
    if(!ok)throw std::runtime_error("Cannot identify pipe owner");
    LPWSTR sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid))throw std::runtime_error("Cannot encode pipe owner");
    std::wstring acl=L"D:P(A;;GA;;;";acl+=sid;acl+=L")";LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))throw std::runtime_error("Cannot set pipe access");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
    std::wstring name=L"\\\\.\\pipe\\PhotoMatchProto-"+std::to_wstring(GetCurrentProcessId());
    pipe_=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&security);
    LocalFree(descriptor);
    if(pipe_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create PhotoMatch pipe");
    io_.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!io_.hEvent){Close();throw std::runtime_error("Cannot create pipe event");}Connect();
}
void PipeServer::Close() {
    if(pipe_!=INVALID_HANDLE_VALUE){if(pending_){CancelIoEx(pipe_,nullptr);DWORD ignored;GetOverlappedResult(pipe_,&io_,&ignored,TRUE);}CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;pending_=false;}
    if(io_.hEvent){CloseHandle(io_.hEvent);io_.hEvent=nullptr;}
}
void PipeServer::Connect() {
    ResetEvent(io_.hEvent);stage_=Connecting;started_=GetTickCount64();pending_=false;completed_=0;
    if(ConnectNamedPipe(pipe_,&io_))SetEvent(io_.hEvent);
    else {DWORD error=GetLastError();if(error==ERROR_PIPE_CONNECTED)SetEvent(io_.hEvent);else if(error==ERROR_IO_PENDING)pending_=true;else throw std::runtime_error("Pipe connection failed");}
}
void PipeServer::Reset() {
    if(pending_){CancelIoEx(pipe_,&io_);DWORD ignored;GetOverlappedResult(pipe_,&io_,&ignored,TRUE);pending_=false;}
    DisconnectNamedPipe(pipe_);output_.clear();Connect();
}
void PipeServer::Read() {
    stage_=Reading;started_=GetTickCount64();ResetEvent(io_.hEvent);pending_=false;
    if(ReadFile(pipe_,input_.data(),DWORD(input_.size()),&completed_,&io_))SetEvent(io_.hEvent);
    else if(GetLastError()==ERROR_IO_PENDING)pending_=true;else Reset();
}
void PipeServer::Poll(const std::function<std::string(const std::string&)>& handle) {
    if(pipe_==INVALID_HANDLE_VALUE||polling_)return;
    polling_=true;
    try {
        if(WaitForSingleObject(io_.hEvent,0)!=WAIT_OBJECT_0){
            if(stage_!=Connecting&&GetTickCount64()-started_>10000)Reset();
        } else {
            DWORD count=completed_;
            BOOL ok=pending_?GetOverlappedResult(pipe_,&io_,&count,FALSE):TRUE;pending_=false;
            if(!ok&&!(stage_==Connecting&&GetLastError()==ERROR_PIPE_CONNECTED))Reset();
            else if(stage_==Connecting||stage_==Writing)Read();
            else if(!count)Reset();
            else {
                output_=handle(std::string(input_.data(),count));
                if(output_.size()>4*1024*1024)output_="{\"ok\":false,\"error\":\"Result too large; start a new capture\"}";
                stage_=Writing;started_=GetTickCount64();ResetEvent(io_.hEvent);
                if(WriteFile(pipe_,output_.data(),DWORD(output_.size()),&completed_,&io_))SetEvent(io_.hEvent);
                else if(GetLastError()==ERROR_IO_PENDING)pending_=true;else Reset();
            }
        }
    }catch(...){polling_=false;throw;}polling_=false;
}
