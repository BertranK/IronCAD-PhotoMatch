#include "ProtoDialog.h"

extern const CLSID CLSID_PhotoMatchProto={0xa44d3379,0xfc03,0x4cbf,{0x9b,0x10,0xa8,0xcc,0x56,0xb3,0xa7,0xe1}};
CComModule _Module;
// Persist initialization stages so a failed host load is diagnosable without UI automation.
static void loadLog(const CString& message) noexcept {
    try {
        wchar_t modulePath[MAX_PATH]={};GetModuleFileNameW(_Module.GetModuleInstance(),modulePath,MAX_PATH);
        CString path(modulePath);int slash=path.ReverseFind(L'\\');if(slash<0)return;
        path=path.Left(slash+1)+L"PhotoMatchProto-load.log";
        CStdioFile file;if(!file.Open(path,CFile::modeCreate|CFile::modeNoTruncate|CFile::modeWrite|CFile::shareDenyNone))return;
        SYSTEMTIME now{};GetLocalTime(&now);CString prefix;prefix.Format(L"%04u-%02u-%02u %02u:%02u:%02u pid=%lu ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,GetCurrentProcessId());
        file.SeekToEnd();file.WriteString(prefix+message+L"\n");file.Close();
    }catch(CException* e){e->Delete();}catch(...){}
}
class ShowCommand : public CZCommandEventsSink {
public: ProtoDialog* dialog=nullptr;
    HRESULT STDMETHODCALLTYPE OnClick() override {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        if(dialog){dialog->ShowWindow(SW_SHOW);dialog->SetForegroundWindow();}return S_OK;
    }
};
class ATL_NO_VTABLE PhotoMatchAddin : public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<PhotoMatchAddin,&CLSID_PhotoMatchProto>,public IZAddinServer {
    IZAddinSitePtr site_;
    IZCommandHandlerPtr command_;
    std::unique_ptr<ProtoDialog> dialog_;
    CComObject<ShowCommand>* sink_=nullptr;
public:
    DECLARE_REGISTRY_RESOURCEID(IDR_ADDIN)
    BEGIN_COM_MAP(PhotoMatchAddin)
        COM_INTERFACE_ENTRY(IZAddinServer)
    END_COM_MAP()
    HRESULT STDMETHODCALLTYPE InitSelf(IZAddinSite* site) override {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        loadLog(L"InitSelf entered");
        try {
            if(site_||!site)return E_UNEXPECTED;site_=site;IZBaseAppPtr app;checked(site_->get_Application(&app));
            loadLog(L"Host application acquired");
            dialog_.reset(new ProtoDialog(app));if(!dialog_->Create(IDD_PROTO))throw std::runtime_error("Cannot create Proto dialog");
            loadLog(L"Modeless dialog created");
            checked(site_->CreateCommandHandler(CComBSTR(L"PhotoMatchProto.Open"),CComBSTR(L"PhotoMatch Proto"),
                CComBSTR(L"Open PhotoMatch connection diagnostics"),CComBSTR(L"PhotoMatch Proto 0"),nullptr,nullptr,&command_));
            checked(CComObject<ShowCommand>::CreateInstance(&sink_));sink_->AddRef();sink_->dialog=dialog_.get();checked(sink_->Advise(command_));
            IZEnvironmentMgrPtr environments;checked(app->get_EnvironmentMgr(&environments));IZEnvironmentPtr scene;
            checked(environments->get_Environment(Z_ENV_SCENE,&scene));IZControlBarPtr bar;checked(scene->AddControlBar(site_,CComBSTR(L"PhotoMatch Proto"),&bar));
            IZControlsPtr controls;checked(bar->get_Controls(&controls));IZControlDescriptorPtr descriptor;checked(command_->get_ControlDescriptor(&descriptor));
            IZControlPtr button;checked(controls->Add(Z_CONTROL_BUTTON,descriptor,nullptr,&button));dialog_->ShowWindow(SW_SHOW);loadLog(L"InitSelf ready; window shown");return S_OK;
        }catch(const _com_error& e){CString msg;msg.Format(L"InitSelf failed: HRESULT 0x%08X",unsigned(e.Error()));loadLog(msg);DeInitSelf();return e.Error();}
        catch(const std::exception& e){loadLog(CString(L"InitSelf failed: ")+CString(CA2W(e.what(),CP_UTF8)));DeInitSelf();return E_FAIL;}
        catch(CException* e){wchar_t msg[512]={};e->GetErrorMessage(msg,512);e->Delete();loadLog(msg);DeInitSelf();return E_FAIL;}
        catch(...){loadLog(L"InitSelf failed: unknown exception");DeInitSelf();return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE DeInitSelf() override {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        if(sink_){sink_->dialog=nullptr;sink_->Unadvise();sink_->Release();sink_=nullptr;}
        if(dialog_){dialog_->Shutdown();if(dialog_->GetSafeHwnd())dialog_->DestroyWindow();dialog_.reset();}
        command_=nullptr;site_=nullptr;return S_OK;
    }
    void FinalRelease(){DeInitSelf();}
};
BEGIN_OBJECT_MAP(PhotoMatchObjects)
    OBJECT_ENTRY(CLSID_PhotoMatchProto,PhotoMatchAddin)
END_OBJECT_MAP()
class ProtoApp : public CWinApp {
public:
    BOOL InitInstance() override {_Module.Init(PhotoMatchObjects,m_hInstance);return CWinApp::InitInstance();}
    int ExitInstance() override {_Module.Term();return CWinApp::ExitInstance();}
} theApp;
STDAPI DllCanUnloadNow(){AFX_MANAGE_STATE(AfxGetStaticModuleState());return AfxDllCanUnloadNow()==S_OK&&_Module.GetLockCount()==0?S_OK:S_FALSE;}
STDAPI DllGetClassObject(REFCLSID c,REFIID i,void** result){AFX_MANAGE_STATE(AfxGetStaticModuleState());loadLog(L"DllGetClassObject entered");HRESULT hr=_Module.GetClassObject(c,i,result);CString message;message.Format(L"DllGetClassObject returned 0x%08X",unsigned(hr));loadLog(message);return hr;}
STDAPI DllRegisterServer(){AFX_MANAGE_STATE(AfxGetStaticModuleState());return _Module.RegisterServer(FALSE);}
STDAPI DllUnregisterServer(){AFX_MANAGE_STATE(AfxGetStaticModuleState());return _Module.UnregisterServer(FALSE);}
