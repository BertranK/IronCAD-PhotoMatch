#pragma once
#include "StdAfx.h"

// Polled only by the host's message thread. No blocking client I/O or COM worker.
class PipeServer {
    HANDLE pipe_=INVALID_HANDLE_VALUE;
    OVERLAPPED io_{};
    enum Stage { Connecting, Reading, Writing } stage_=Connecting;
    std::array<char,65536> input_{};
    std::string output_;
    ULONGLONG started_=0;
    bool polling_=false;
    bool pending_=false;
    DWORD completed_=0;
    void Connect();
    void Read();
    void Reset();
public:
    ~PipeServer(){Close();}
    void Open();
    void Close();
    void Poll(const std::function<std::string(const std::string&)>& handle);
};
