#pragma once
#include "StdAfx.h"
#include "Resource.h"
#pragma warning(push)
#pragma warning(disable:4100)
#include "AppEventsSink.h"
#include "SelectEventsSink.h"
#include "DrawEventsSink.h"
#include "CommandEventsSink.h"
#pragma warning(pop)

class ProtoDialog;
class ProtoAppEvents : public CZAppEventsSink {
public: ProtoDialog* owner=nullptr;
    HRESULT STDMETHODCALLTYPE OnActiveDocChanged(IZDoc*) override;
    HRESULT STDMETHODCALLTYPE OnDocumentPreClosed(IZDoc*) override;
    HRESULT STDMETHODCALLTYPE OnAppDestroyNotify() override;
    HRESULT STDMETHODCALLTYPE OnActiveViewChanged(IZWindow*) override;
};
class ProtoSelectEvents : public CZSelectEventsSink {
public: ProtoDialog* owner=nullptr;
    HRESULT STDMETHODCALLTYPE OnSelected(IZElement*,IZMathPoint*,long,long,long,eZEntityType,VARIANT) override;
};
class ProtoDrawEvents : public CZDrawEventsSink {
public: ProtoDialog* owner=nullptr;
    HRESULT STDMETHODCALLTYPE OnPostDraw(IZRender*) override;
};
class PhotoOverlay : public CWnd {
public:
    CImage image;
    void Open(const CString& path,HWND host);
    void Align(HWND host);
    void Clear();
    afx_msg void OnPaint();
    afx_msg LRESULT OnHitTest(CPoint) { return HTTRANSPARENT; }
    afx_msg int OnMouseActivate(CWnd*,UINT,UINT) {return MA_NOACTIVATE;}
    DECLARE_MESSAGE_MAP()
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
class ProtoDialog : public CDialog {
public:
    ProtoDialog(IZBaseApp* app):CDialog(IDD_PROTO),app_(app),thread_(GetCurrentThreadId()){}
    ~ProtoDialog();
    void Shutdown();
    void ContextChanging(IZDoc* closing=nullptr);
    void ActiveDocumentChanged(IZDoc* next);
    void ActiveViewChanged();
    void Select(IZElement*,IZMathPoint*,eZEntityType,const VARIANT&);
    void Draw(IZRender*);
    void Guard(const std::function<void()>&);
    void Log(const CString&);
protected:
    BOOL OnInitDialog() override;
    void OnCancel() override;
    afx_msg void OnAction(UINT);
    afx_msg void OnTimer(UINT_PTR);
    DECLARE_MESSAGE_MAP()
private:
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
    double imageFocal_=0,recordedImageFocal_=0,lastW_=0,lastH_=0;
    std::string savedCameraRecord_="null",restoredCameraRecord_="null";
    unsigned int dpi_=0;
    void CheckContext();
    void Capture();
    void Pick();
    void StopPicking();
    void Apply();
    void Restore();
    void Photo();
    void Background();
    void Save();
    void UpdateOverlay();
    HWND GraphicsWindow();
    std::string ModelFingerprint();
    CameraState ReadCamera(IZCamera*);
    void WriteCamera(IZCamera*,const CameraState&);
    photomatch::Camera CameraInput();
    void SetCameraInput(const CameraState&);
};
