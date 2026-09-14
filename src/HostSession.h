#pragma once
#include "StdAfx.h"
#include "Resource.h"
#include "PipeServer.h"
#include "../third_party/nlohmann/json.hpp"
#pragma warning(push)
#pragma warning(disable:4100)
#include "AppEventsSink.h"
#include "SelectEventsSink.h"
#include "DrawEventsSink.h"
#include "CommandEventsSink.h"
#pragma warning(pop)

class HostSession;
class ProtoAppEvents : public CZAppEventsSink {
public: HostSession* owner=nullptr;
    HRESULT STDMETHODCALLTYPE OnActiveDocChanged(IZDoc*) override;
    HRESULT STDMETHODCALLTYPE OnDocumentPreClosed(IZDoc*) override;
    HRESULT STDMETHODCALLTYPE OnAppDestroyNotify() override;
    HRESULT STDMETHODCALLTYPE OnActiveViewChanged(IZWindow*) override;
};
class ProtoSelectEvents : public CZSelectEventsSink {
public: HostSession* owner=nullptr;
    HRESULT STDMETHODCALLTYPE OnSelected(IZElement*,IZMathPoint*,long,long,long,eZEntityType,VARIANT) override;
};
class ProtoDrawEvents : public CZDrawEventsSink {
public: HostSession* owner=nullptr;
    HRESULT STDMETHODCALLTYPE OnPostDraw(IZRender*) override;
};
class PhotoOverlay : public CWnd {
public:
    CImage image;
    photomatch::Rect imageRect{};
    photomatch::Rect drawnRect{};
    void Open(const CString& path,HWND host);
    void Align(HWND host);
    void Clear();
    afx_msg LRESULT OnHitTest(CPoint) { return HTTRANSPARENT; }
    afx_msg int OnMouseActivate(CWnd*,UINT,UINT) {return MA_NOACTIVATE;}
    DECLARE_MESSAGE_MAP()
private:
    CImage frame_;
    CPoint position_{};
};
struct CameraState {
    photomatch::Camera values;
    CComVariant position,direction,up,interest;
    VARIANT_BOOL perspective=VARIANT_FALSE;
    double nearClip=0,farClip=0,scale=1;
};
struct PickedPoint {
    long vertexId=0;
    CString name;
    CComVariant objectId;
    IZElementPtr element;
    photomatch::Vec3 apiPoint,transformedPoint;
    std::array<double,16> matrix;
};
class HostSession : public CWnd {
public:
    HostSession(IZBaseApp* app):app_(app),thread_(GetCurrentThreadId()){}
    void Start();
    void LaunchGui();
    ~HostSession();
    void Shutdown();
    void ContextChanging(IZDoc* closing=nullptr);
    void ActiveDocumentChanged(IZDoc* next);
    void ActiveViewChanged();
    void Select(IZElement*,IZMathPoint*,eZEntityType,const VARIANT&);
    void Draw(IZRender*);
    void Guard(const std::function<void()>&);
    void Log(const CString&);
protected:

    afx_msg void OnTimer(UINT_PTR);
    DECLARE_MESSAGE_MAP()
private:
    PipeServer pipe_;
    HANDLE guiProcess_=nullptr;
    IZDocPtr observedDoc_;
    std::string instanceId_;
    unsigned long generation_=0,captureId_=0;
    bool servicing_=false;
    std::vector<std::string> logs_;
    void SyncDocument();
    std::string SessionId() const;
    std::string Request(const std::string& request);
    nlohmann::json Snapshot();
    nlohmann::json CoordinateFixture();
    std::string Report();
    IZBaseAppPtr app_;
    IZDocPtr doc_;
    IZSceneDocPtr scene_;
    IZCameraMgrPtr cameras_;
    IZCameraPtr original_,test_;
    IZInteractorPtr interactor_;
    CComObject<ProtoAppEvents>* appEvents_=nullptr;
    CComObject<ProtoSelectEvents>* selectEvents_=nullptr;
    CComObject<ProtoDrawEvents>* drawEvents_=nullptr;
    CameraState saved_;
    bool captured_=false,measuring_=false,restoring_=false,restored_=false;
    bool modelUnchanged_=false;
    DWORD thread_;
    HWND capturedWindow_=nullptr;
    std::string modelBefore_;
    std::vector<PickedPoint> points_;
    std::vector<photomatch::Observation> observations_;
    std::vector<std::string> sampleRecords_;
    std::vector<std::string> errors_;
    std::vector<std::string> cameraRecords_;
    PhotoOverlay overlay_;
    CString imagePath_,backgroundStatus_=L"not_tested";
    CComPtr<IPersistStream> backgroundPersistence_;
    CComPtr<IStream> backgroundBackup_;
    bool backgroundChanged_=false;
    double imageFocal_=0,recordedImageFocal_=0,lastW_=0,lastH_=0,lastRenderW_=0,lastRenderH_=0;
    std::string savedCameraRecord_="null",restoredCameraRecord_="null";
    unsigned int dpi_=0;
    void CheckContext();
    void Capture();
    void Resume();
    photomatch::Convention OverlayConvention();
    void Reconnect(const nlohmann::json& saved);
    void Pick();
    void StopPicking();
    void Apply(const photomatch::Camera& input);
    void Restore();
    void Photo(const CString& path,double focal);
    void Background(const CString& path);
    void UpdateOverlay();
    HWND GraphicsWindow();
    std::string ModelFingerprint();
    CameraState ReadCamera(IZCamera*);
    void WriteCamera(IZCamera*,const CameraState&);
};
