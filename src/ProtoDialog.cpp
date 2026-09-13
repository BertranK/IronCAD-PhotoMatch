#include "ProtoDialog.h"
using namespace photomatch;

// Limit DPI context changes to Win32 window measurements; never call COM in this scope.
class PhysicalPixels {
    typedef HANDLE(WINAPI* Change)(HANDLE);
    Change change_=reinterpret_cast<Change>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetThreadDpiAwarenessContext"));
    HANDLE previous_=nullptr;
public:
    PhysicalPixels(){if(change_)previous_=change_(reinterpret_cast<HANDLE>(-4));}
    ~PhysicalPixels(){if(change_&&previous_)change_(previous_);}
};
static CRect clientRect(HWND w) {PhysicalPixels scope;CRect r;::GetClientRect(w,&r);return r;}
static UINT windowDpi(HWND w) {
    typedef UINT(WINAPI* Fn)(HWND);auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
    return fn?fn(w):96;
}
static std::string jsonCamera(const Camera& c) {
    std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<"{\"position\":"<<jsonVec(c.position)
        <<",\"direction\":"<<jsonVec(c.direction)<<",\"up\":"<<jsonVec(c.up)<<",\"field_sdk\":"<<c.field<<'}';return o.str();
}
static std::string jsonCameraState(const CameraState& s) {
    std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)
        <<"{\"pose_and_field\":"<<jsonCamera(s.values)<<",\"perspective\":"<<(s.perspective?"true":"false")
        <<",\"center_of_interest\":"<<jsonVec(vector3(s.interest))<<",\"near\":"<<s.nearClip
        <<",\"far\":"<<s.farClip<<",\"scale\":"<<s.scale<<'}';return o.str();
}
static CString show(Vec3 p) {CString s;s.Format(L"%.12g %.12g %.12g",p.x,p.y,p.z);return s;}
static Vec3 parseVector(const CString& s) {
    std::wistringstream in(s.GetString());in.imbue(std::locale::classic());Vec3 p;std::wstring extra;
    if(!(in>>p.x>>p.y>>p.z)||(in>>extra)||!finite(p))throw std::runtime_error("Enter exactly three finite numbers");return p;
}
static double parseNumber(const CString& s) {
    std::wistringstream in(s.GetString());in.imbue(std::locale::classic());double d;std::wstring extra;
    if(!(in>>d)||(in>>extra)||!std::isfinite(d))throw std::runtime_error("Enter one finite number");return d;
}
template<class T> static void createSink(CComObject<T>*& sink,ProtoDialog* owner) {
    checked(CComObject<T>::CreateInstance(&sink));sink->AddRef();sink->owner=owner;
}
template<class T> static void freeSink(CComObject<T>*& sink) {
    if(sink){sink->owner=nullptr;sink->Unadvise();sink->Release();sink=nullptr;}
}

BEGIN_MESSAGE_MAP(PhotoOverlay,CWnd)
 ON_WM_PAINT()
 ON_WM_NCHITTEST()
 ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()
void PhotoOverlay::Open(const CString& path,HWND host) {
    CImage candidate;checked(candidate.Load(path));
    if(candidate.GetWidth()<=0||candidate.GetHeight()<=0)throw std::runtime_error("Empty image");
    Clear();image.Attach(candidate.Detach());PhysicalPixels scope;
    CString cls=AfxRegisterWndClass(0,LoadCursor(nullptr,IDC_ARROW));
    if(!CreateEx(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls,L"PhotoMatch reference image",
        WS_POPUP,CRect(0,0,10,10),CWnd::FromHandle(host),0))throw std::runtime_error("Cannot create overlay");
    SetLayeredWindowAttributes(RGB(1,2,3),150,LWA_COLORKEY|LWA_ALPHA);Align(host);
}
void PhotoOverlay::Clear(){if(GetSafeHwnd())DestroyWindow();if(!image.IsNull())image.Destroy();}
void PhotoOverlay::Align(HWND host) {
    if(!GetSafeHwnd())return;PhysicalPixels scope;
    if(!::IsWindowVisible(host)||::IsIconic(::GetAncestor(host,GA_ROOT))){ShowWindow(SW_HIDE);return;}
    RECT r;::GetClientRect(host,&r);POINT p{0,0};::ClientToScreen(host,&p);
    SetWindowPos(&wndTop,p.x,p.y,r.right,r.bottom,SWP_NOACTIVATE|SWP_SHOWWINDOW);Invalidate(FALSE);
}
void PhotoOverlay::OnPaint() {
    CPaintDC dc(this);CRect r;GetClientRect(&r);dc.FillSolidRect(r,RGB(1,2,3));if(image.IsNull()||r.IsRectEmpty())return;
    Rect b=contain(image.GetWidth(),image.GetHeight(),r.Width(),r.Height());
    dc.SetStretchBltMode(HALFTONE);SetBrushOrgEx(dc.GetSafeHdc(),0,0,nullptr);
    image.Draw(dc.GetSafeHdc(),int(std::round(b.x)),int(std::round(b.y)),int(std::round(b.w)),int(std::round(b.h)));
    // The frame identifies the effective image region, including letterboxing.
    dc.Draw3dRect(int(std::round(b.x)),int(std::round(b.y)),int(std::round(b.w)),int(std::round(b.h)),RGB(0,255,255),RGB(0,255,255));
}

BEGIN_MESSAGE_MAP(ProtoDialog,CDialog)
 ON_CONTROL_RANGE(BN_CLICKED,IDC_CAPTURE,IDC_BACKGROUND,OnAction)
 ON_WM_TIMER()
END_MESSAGE_MAP()
BOOL ProtoDialog::OnInitDialog() {
    CDialog::OnInitDialog();SetDlgItemText(IDC_FOCAL,L"1000");
    Guard([&]{createSink(appEvents_,this);checked(appEvents_->Advise(app_));});
    SetTimer(1,250,nullptr);Log(L"Proto 0: use a separate test scene. No runtime test has passed yet.");return TRUE;
}
ProtoDialog::~ProtoDialog(){Shutdown();}
void ProtoDialog::Log(const CString& text) {
    if(!GetSafeHwnd())return;CEdit* edit=static_cast<CEdit*>(GetDlgItem(IDC_LOG));
    int length=edit->GetWindowTextLength();edit->SetSel(length,length);edit->ReplaceSel(text+L"\r\n");
}
void ProtoDialog::Guard(const std::function<void()>& action) {
    try {if(GetCurrentThreadId()!=thread_)throw std::runtime_error("CAD call attempted outside host thread");action();}
    catch(const _com_error& e){CString message;message.Format(L"COM error 0x%08X: %s",unsigned(e.Error()),e.ErrorMessage());Log(message);errors_.push_back(utf8(message));}
    catch(const std::exception& e){CString message(CA2W(e.what(),CP_UTF8));Log(message);errors_.push_back(e.what());}
    catch(CException* e){wchar_t text[512]={};e->GetErrorMessage(text,512);e->Delete();Log(text);errors_.push_back(utf8(text));}
    catch(...){Log(L"Unexpected failure; operation stopped.");errors_.push_back("Unexpected failure");}
}
void ProtoDialog::CheckContext() {
    if(!captured_||!doc_)throw std::runtime_error("Capture a scene first");
    IZDocPtr active;checked(app_->get_ActiveDoc(&active));if(!sameObject(active,doc_))throw std::runtime_error("Document changed; recapture required");
    if(GraphicsWindow()!=capturedWindow_)throw std::runtime_error("Active viewport changed; restore and recapture");
    if(ModelFingerprint()!=modelBefore_)throw std::runtime_error("Model changed; restore and capture again");
}
HWND ProtoDialog::GraphicsWindow() {
    IZWindowMgrPtr manager;checked(scene_->get_WindowMgr(&manager));IZWindowPtr window;checked(manager->get_ActiveWindow(&window));
    if(!window)throw std::runtime_error("No active graphics window");LONG_PTR value=0;checked(window->get_GraphicsHWND(&value));
    HWND hwnd=reinterpret_cast<HWND>(value);if(!::IsWindow(hwnd))throw std::runtime_error("Invalid graphics window");return hwnd;
}
CameraState ProtoDialog::ReadCamera(IZCamera* camera) {
    if(!camera)throw std::runtime_error("No camera");CameraState s;
    checked(camera->get_Position(&s.position));checked(camera->get_Direction(&s.direction));checked(camera->get_Up(&s.up));
    checked(camera->get_CenterOfInterest(&s.interest));checked(camera->get_FieldOfView(&s.values.field));
    checked(camera->get_Perspective(&s.perspective));checked(camera->get_Near(&s.nearClip));checked(camera->get_Far(&s.farClip));checked(camera->get_Scale(&s.scale));
    s.values.position=vector3(s.position);s.values.direction=vector3(s.direction);s.values.up=vector3(s.up);return s;
}
void ProtoDialog::WriteCamera(IZCamera* camera,const CameraState& s) {
    checked(camera->put_Perspective(s.perspective));checked(camera->put_Position(s.position));
    checked(camera->put_CenterOfInterest(s.interest));checked(camera->put_Direction(s.direction));checked(camera->put_Up(s.up));
    checked(camera->put_Near(s.nearClip));checked(camera->put_Far(s.farClip));checked(camera->put_Scale(s.scale));checked(camera->put_FieldOfView(s.values.field));
}
void ProtoDialog::SetCameraInput(const CameraState& s) {
    SetDlgItemText(IDC_POSITION,show(s.values.position));SetDlgItemText(IDC_DIRECTION,show(s.values.direction));SetDlgItemText(IDC_UP,show(s.values.up));
    CString field;field.Format(L"%.12g",s.values.field);SetDlgItemText(IDC_FOV,field);
}
Camera ProtoDialog::CameraInput() {
    CString p,d,u,f;GetDlgItemText(IDC_POSITION,p);GetDlgItemText(IDC_DIRECTION,d);GetDlgItemText(IDC_UP,u);GetDlgItemText(IDC_FOV,f);
    Camera c{parseVector(p),parseVector(d),parseVector(u),parseNumber(f)};
    unit(cross(c.direction,c.up));if(c.field<=0)throw std::runtime_error("Field of view must be positive");return c;
}
std::string ProtoDialog::ModelFingerprint() {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    std::function<void(IZElement*)> visit=[&](IZElement* element){
        LONG id=0;checked(element->get_Id(&id));out<<id;
        IZSceneElementPtr se=element;if(se){IZMathMatrixPtr m;checked(se->GetTransformToGlobal(&m));double data[16];checked(m->GetDataCOM(data));for(double d:data)out<<','<<d;}
        IZPartPtr part=element;if(part){CComVariant bounds;checked(part->GetBoundingBox(VARIANT_TRUE,&bounds));for(auto v:items(bounds)){checked(v.ChangeType(VT_R8));out<<','<<v.dblVal;}}
        CComVariant children;checked(element->GetChildren(&children));if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)
            for(auto& child:items(children)){IZElementPtr next=unknown(child);if(next)visit(next);}
        out<<';';
    };
    CComVariant children;checked(scene_->GetChildElements(&children));
    if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)for(auto& child:items(children)){IZElementPtr element=unknown(child);if(element)visit(element);}
    return out.str();
}
void ProtoDialog::Capture() {
    if(captured_)Restore();StopPicking();freeSink(drawEvents_);
    checked(app_->get_ActiveDoc(&doc_));scene_=doc_;if(!scene_)throw std::runtime_error("Open a 3D test scene");
    checked(scene_->get_CameraMgr(&cameras_));checked(cameras_->get_ActiveCamera(&original_));saved_=ReadCamera(original_);
    modelBefore_=ModelFingerprint();points_.clear();observations_.clear();sampleRecords_.clear();errors_.clear();cameraRecords_.clear();
    savedCameraRecord_=jsonCameraState(saved_);restoredCameraRecord_="null";
    imagePath_.Empty();recordedImageFocal_=0;backgroundStatus_=L"not_tested";
    restored_=false;modelUnchanged_=false;captured_=true;capturedWindow_=GraphicsWindow();
    IZDrawEventsPtr events;checked(doc_->get_DrawEvents(&events));createSink(drawEvents_,this);checked(drawEvents_->Advise(events));
    SetCameraInput(saved_);Log(L"State captured. Original camera retained; model transforms and part bounds recorded.");
}
void ProtoDialog::StopPicking() {
    freeSink(selectEvents_);if(interactor_){interactor_->Stop();interactor_=nullptr;}
}
void ProtoDialog::Pick() {
    CheckContext();StopPicking();IZSelectionMgrPtr selection;checked(scene_->get_SelectionMgr(&selection));
    checked(selection->CreateInteractor(&interactor_));IZSelectEventsPtr events;checked(interactor_->get_SelectEvents(&events));
    createSink(selectEvents_,this);checked(selectEvents_->Advise(events));checked(events->SetSelectionFilterChoices(Z_SEL_FEV,Z_SEL_FEV));
    checked(interactor_->Start());Log(L"Select a solid vertex. Face/edge clicks are ignored. Click again to restart after Escape.");
}
void ProtoDialog::Select(IZElement* element,IZMathPoint*,eZEntityType type,const VARIANT& ids) {
    CheckContext();if(type!=Z_ENTITY_VERTEX||!element)return;
    IZElementPtr owner=element;IZPartPtr part=owner;
    if(!part){checked(element->GetOwnerPart(&owner));part=owner;}if(!part)throw std::runtime_error("Selected vertex has no owning part");
    CComVariant bodies;checked(part->GetBodies(VARIANT_TRUE,&bodies));auto bodyList=items(bodies);
    // Entity IDs alone cannot disambiguate per-body IDs. Reject instead of picking the first body.
    if(bodyList.size()!=1)throw std::runtime_error("Multi-body part is ambiguous in this probe; use a single-body test part");
    IZBodyVertexPtr vertices=unknown(bodyList[0]);if(!vertices)throw std::runtime_error("Body does not expose vertex coordinates");
    IZSceneElementPtr se=owner;IZMathMatrixPtr matrix;checked(se->GetTransformToGlobal(&matrix));
    for(auto id:items(ids)) {
        checked(id.ChangeType(VT_I4));PickedPoint point;point.vertexId=id.lVal;point.element=owner;
        LONG objectId=0;checked(owner->get_Id(&objectId));point.objectId=objectId;CComBSTR n;checked(owner->get_Name(&n));point.name=n;
        CComVariant position;checked(vertices->GetPosition(id.lVal,&position));point.apiPoint=vector3(position);
        checked(matrix->GetDataCOM(point.matrix.data()));point.transformedPoint=transform(point.apiPoint,matrix);
        bool duplicate=false;for(const auto& old:points_)if(sameObject(old.element,owner)&&old.vertexId==point.vertexId)duplicate=true;
        if(duplicate)continue;points_.push_back(point);
        CString message;message.Format(L"P%03u %s vertex %ld | API %s | transformed %s (coordinate convention pending)",
            unsigned(points_.size()),point.name.GetString(),point.vertexId,show(point.apiPoint).GetString(),show(point.transformedPoint).GetString());Log(message);
    }
}
void ProtoDialog::Apply() {
    CheckContext();StopPicking();Camera input=CameraInput();overlay_.Clear();imageFocal_=0;
    if(!test_)checked(cameras_->Add(&test_));CameraState state=saved_;state.perspective=VARIANT_TRUE;state.values=input;
    state.position=xyz(input.position);state.direction=xyz(input.direction);state.up=xyz(input.up);
    Vec3 f=unit(input.direction);state.interest=xyz({input.position.x+f.x,input.position.y+f.y,input.position.z+f.z});
    WriteCamera(test_,state);checked(cameras_->put_ActiveCamera(test_));auto readback=ReadCamera(test_);
    cameraRecords_.push_back("{\"input\":"+jsonCameraState(state)+",\"readback\":"+jsonCameraState(readback)+"}");
    checked(scene_->Redraw());Log(L"Test camera applied. Measure at multiple aspect ratios and camera settings.");
}
void ProtoDialog::Draw(IZRender* render) {
    if(!measuring_||!captured_)return;measuring_=false;CheckContext();
    IZCameraPtr current;checked(cameras_->get_ActiveCamera(&current));CameraState state=ReadCamera(current);
    if(!state.perspective)throw std::runtime_error("Projection measurement requires a perspective camera");
    LONG rw=0,rh=0;checked(render->GetViewExtents(&rw,&rh));HWND hwnd=GraphicsWindow();CRect physical=clientRect(hwnd);
    if(rw<=0||rh<=0||physical.IsRectEmpty())throw std::runtime_error("Empty viewport");
    double sx=double(physical.Width())/rw,sy=double(physical.Height())/rh;
    if(std::abs(sx-sy)>0.02)throw std::runtime_error("Viewport/render extent mismatch; use one unsplit view");
    dpi_=windowDpi(hwnd);Camera c=state.values;Vec3 f=unit(c.direction),r=unit(cross(f,c.up)),u=unit(cross(r,f));
    for(int i=0;i<8;++i){double depth=10+5*(i%3),x=(i&1)?2:-2,y=(i&2)?1.5:-1.5;
        Vec3 world{c.position.x+f.x*depth+r.x*x+u.x*y,c.position.y+f.y*depth+r.y*x+u.y*y,c.position.z+f.z*depth+r.z*x+u.z*y};
        LONG px=0,py=0,pz=0;checked(render->XformWorldToView2(world.x,world.y,world.z,&px,&py,&pz));
        observations_.push_back({c,world,{px*sx,py*sy},double(physical.Width()),double(physical.Height())});
    }
    std::ostringstream record;record.imbue(std::locale::classic());record<<std::setprecision(17)<<"{\"dpi\":"<<dpi_<<",\"render_size\":["<<rw<<','<<rh
        <<"],\"physical_size\":["<<physical.Width()<<','<<physical.Height()<<"],\"picked_point_projections\":[";
    for(size_t i=0;i<points_.size();++i){const auto& p=points_[i];LONG ax,ay,az,gx,gy,gz;
        checked(render->XformWorldToView2(p.apiPoint.x,p.apiPoint.y,p.apiPoint.z,&ax,&ay,&az));
        checked(render->XformWorldToView2(p.transformedPoint.x,p.transformedPoint.y,p.transformedPoint.z,&gx,&gy,&gz));
        if(i)record<<',';record<<"{\"id\":"<<quote("P"+std::to_string(i+1))<<",\"api_as_world_px\":["<<ax*sx<<','<<ay*sy<<"],\"transformed_as_world_px\":["<<gx*sx<<','<<gy*sy<<"]}";
    }record<<"]}";sampleRecords_.push_back(record.str());auto result=fit(observations_);
    CString message;message.Format(L"%zu projections; best %s, max %.4f physical px; %s",observations_.size(),
        CString(CA2W(name(result[0].convention).c_str())).GetString(),result[0].maxError,
        resolved(result)?L"FOV identified":L"UNRESOLVED: change aspect ratio / inspect axes");Log(message);
}
void ProtoDialog::Restore() {
    if(!captured_||restoring_)return;restoring_=true;
    try {
        StopPicking();measuring_=false;overlay_.Clear();imageFocal_=0;
        if(backgroundChanged_){LARGE_INTEGER zero{};checked(backgroundBackup_->Seek(zero,STREAM_SEEK_SET,nullptr));checked(backgroundPersistence_->Load(backgroundBackup_));backgroundChanged_=false;}
        WriteCamera(original_,saved_);checked(cameras_->put_ActiveCamera(original_));
        if(test_){LONG count=0;checked(cameras_->get_Count(&count));
            // Find by COM identity; never remove an unrelated camera by an assumed index.
            for(LONG i=0;i<count;++i){IZCameraPtr camera;if(SUCCEEDED(cameras_->get_Camera(i,&camera))&&sameObject(camera,test_)){checked(cameras_->Remove(i));break;}}
            test_=nullptr;
        }
        auto now=ReadCamera(original_);restoredCameraRecord_=jsonCameraState(now);restored_=jsonCamera(now.values)==jsonCamera(saved_.values)&&now.perspective==saved_.perspective&&now.nearClip==saved_.nearClip&&now.farClip==saved_.farClip&&now.scale==saved_.scale
            &&jsonVec(vector3(now.interest))==jsonVec(vector3(saved_.interest));
        modelUnchanged_=ModelFingerprint()==modelBefore_;checked(scene_->Redraw());
        Log(restored_&&modelUnchanged_?L"Camera restored; model transforms / part bounds unchanged.":L"RESTORE CHECK FAILED; save diagnostics.");
        backgroundPersistence_.Release();backgroundBackup_.Release();captured_=false;
    }catch(...){restoring_=false;throw;}restoring_=false;
}
void ProtoDialog::ContextChanging(IZDoc* closing) {
    if(closing&&!sameObject(closing,doc_))return;
    if(captured_)Guard([&]{Restore();});overlay_.Clear();StopPicking();freeSink(drawEvents_);captured_=false;
    doc_=nullptr;scene_=nullptr;cameras_=nullptr;original_=nullptr;test_=nullptr;
    Log(L"Document/view context changed. Operations stopped; capture again.");
}
void ProtoDialog::ActiveDocumentChanged(IZDoc* next) {
    if(doc_&&!sameObject(doc_,next))ContextChanging();
}
void ProtoDialog::ActiveViewChanged() {
    if(restoring_||!captured_)return;
    // Camera changes can also emit this event. Defer validation to the next action.
    if(overlay_.GetSafeHwnd())overlay_.ShowWindow(SW_HIDE);
}
void ProtoDialog::Shutdown() {
    if(GetSafeHwnd())KillTimer(1);if(captured_)Guard([&]{Restore();});StopPicking();freeSink(drawEvents_);freeSink(appEvents_);overlay_.Clear();
}
void ProtoDialog::OnCancel(){Guard([&]{Restore();});ShowWindow(SW_HIDE);}
void ProtoDialog::Photo() {
    CheckContext();if(!test_)throw std::runtime_error("Apply a test camera first");auto result=fit(observations_);
    if(!resolved(result))throw std::runtime_error("Measure landscape and portrait views to identify FOV before photo alignment");
    CString value;GetDlgItemText(IDC_FOCAL,value);double focal=parseNumber(value);if(focal<=0)throw std::runtime_error("Focal length must be positive");
    CFileDialog file(TRUE,nullptr,nullptr,OFN_FILEMUSTEXIST|OFN_HIDEREADONLY,L"Images|*.png;*.jpg;*.jpeg;*.bmp|All files|*.*||",this);
    if(file.DoModal()!=IDOK)return;CheckContext();imagePath_=file.GetPathName();overlay_.Open(imagePath_,GraphicsWindow());imageFocal_=focal;recordedImageFocal_=focal;lastW_=lastH_=0;UpdateOverlay();
    Log(L"Reference image overlay enabled; 150/255 opacity. Image principal point is centered.");
}
void ProtoDialog::UpdateOverlay() {
    if(!overlay_.GetSafeHwnd())return;CheckContext();IZCameraPtr active;checked(cameras_->get_ActiveCamera(&active));
    if(!sameObject(active,test_)){overlay_.Clear();throw std::runtime_error("Active camera changed; photo hidden");}
    HWND host=GraphicsWindow();CRect size=clientRect(host);if(size.IsRectEmpty()){overlay_.ShowWindow(SW_HIDE);return;}
    if(lastW_!=size.Width()||lastH_!=size.Height()){
        auto result=fit(observations_);if(!resolved(result))throw std::runtime_error("FOV no longer resolved");
        double field=imageField(imageFocal_,overlay_.image.GetWidth(),overlay_.image.GetHeight(),size.Width(),size.Height(),result[0].convention);
        checked(test_->put_FieldOfView(field));lastW_=size.Width();lastH_=size.Height();checked(scene_->Redraw());
    }overlay_.Align(host);
}
void ProtoDialog::Background() {
    CheckContext();if(backgroundChanged_)throw std::runtime_error("Restore the first background probe before applying another");IZSurfaceFinishPtr finish;checked(scene_->get_Background(&finish));
    CComPtr<IPersistStream> persist;HRESULT hr=finish->QueryInterface(IID_PPV_ARGS(&persist));
    if(FAILED(hr)){backgroundStatus_=L"unavailable: background has no independent persistence snapshot";Log(backgroundStatus_+L". Use overlay.");return;}
    CComPtr<IStream> backup;checked(CreateStreamOnHGlobal(nullptr,TRUE,&backup));checked(persist->Save(backup,FALSE));
    LARGE_INTEGER zero{};checked(backup->Seek(zero,STREAM_SEEK_SET,nullptr));checked(persist->Load(backup));
    CFileDialog file(TRUE,nullptr,nullptr,OFN_FILEMUSTEXIST|OFN_HIDEREADONLY,L"Images|*.png;*.jpg;*.bmp||",this);
    if(file.DoModal()!=IDOK)return;CheckContext();CImage probe;checked(probe.Load(file.GetPathName()));
    backgroundBackup_=backup;backgroundPersistence_=persist;backgroundChanged_=true;
    IZTexturePtr texture;checked(finish->CreateTexture(CComBSTR(file.GetPathName()),&texture));checked(scene_->Redraw());
    backgroundStatus_=L"applied_pending_visual_verification";Log(L"Native background probe applied. Alignment unverified. Restore returns the captured background.");
}
void ProtoDialog::Save() {
    CFileDialog file(FALSE,L"json",L"photomatch-proto0.json",OFN_OVERWRITEPROMPT,L"JSON|*.json||",this);if(file.DoModal()!=IDOK)return;
    CComBSTR version;checked(app_->get_ApiVersion(&version));std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
    o<<"{\n\"schema_version\":1,\"sdk_version\":"<<quote(utf8(version))<<",\"overall_status\":\"runtime_matrix_not_completed\","
      <<"\"restored\":"<<(restored_?"true":"false")<<",\"model_transforms_bounds_unchanged\":"<<(modelUnchanged_?"true":"false")
      <<",\"background\":"<<quote(utf8(backgroundStatus_))<<",\"image\":"<<quote(utf8(imagePath_))<<",\"image_focal_px\":"<<recordedImageFocal_<<",\"original_camera\":"<<savedCameraRecord_
      <<",\"restored_camera\":"<<restoredCameraRecord_<<",\"points\":[";
    for(size_t i=0;i<points_.size();++i){const auto& p=points_[i];CComVariant id=p.objectId;checked(id.ChangeType(VT_BSTR));if(i)o<<',';
        o<<"{\"id\":"<<quote("P"+std::to_string(i+1))<<",\"object_id\":"<<quote(utf8(id.bstrVal))<<",\"object_name\":"<<quote(utf8(p.name))
         <<",\"vertex_id\":"<<p.vertexId<<",\"api_coordinates\":"<<jsonVec(p.apiPoint)<<",\"transformed_coordinates\":"<<jsonVec(p.transformedPoint)
         <<",\"coordinate_convention\":\"pending_fixture_verification\",\"transform\":[";
        for(int j=0;j<16;++j){if(j)o<<',';o<<p.matrix[j];}o<<"]}";
    }o<<"],\"observations\":[";
    for(size_t i=0;i<observations_.size();++i){auto& v=observations_[i];if(i)o<<',';o<<"{\"camera\":"<<jsonCamera(v.camera)<<",\"world\":"<<jsonVec(v.world)
        <<",\"actual_physical_px\":["<<v.actual.x<<','<<v.actual.y<<"],\"viewport\":["<<v.width<<','<<v.height<<"]}";}
    auto fits=fit(observations_);o<<"],\"fov_resolved\":"<<(resolved(fits)?"true":"false")<<",\"fov_candidates\":[";
    for(size_t i=0;i<fits.size();++i){if(i)o<<',';o<<"{\"name\":"<<quote(name(fits[i].convention))<<",\"max_error_px\":";
        if(std::isfinite(fits[i].maxError))o<<fits[i].maxError;else o<<"null";o<<'}';}
    auto array=[&](const char* key,const std::vector<std::string>& values,bool quoted){o<<"],\""<<key<<"\":[";for(size_t i=0;i<values.size();++i){if(i)o<<',';o<<(quoted?quote(values[i]):values[i]);}};
    array("measurements",sampleRecords_,false);array("camera_apply",cameraRecords_,false);array("errors",errors_,true);o<<"]\n}\n";
    CFile output(file.GetPathName(),CFile::modeCreate|CFile::modeWrite);auto data=o.str();output.Write(data.data(),UINT(data.size()));output.Close();Log(L"JSON saved. Unverified fixture, image and DPI tests remain explicitly pending.");
}
void ProtoDialog::OnAction(UINT id) {Guard([&]{switch(id){case IDC_CAPTURE:Capture();break;case IDC_PICK:Pick();break;case IDC_APPLY:Apply();break;
    case IDC_PHOTO:Photo();break;case IDC_RESTORE:Restore();break;case IDC_SAVE:Save();break;case IDC_BACKGROUND:Background();break;
    case IDC_MEASURE:CheckContext();measuring_=true;checked(scene_->Redraw());break;}});}
void ProtoDialog::OnTimer(UINT_PTR id) {if(id==1&&overlay_.GetSafeHwnd())Guard([&]{try{UpdateOverlay();}catch(...){overlay_.Clear();imageFocal_=0;throw;}});CDialog::OnTimer(id);}
HRESULT ProtoAppEvents::OnActiveDocChanged(IZDoc* next) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ActiveDocumentChanged(next);});return S_OK;}
HRESULT ProtoAppEvents::OnDocumentPreClosed(IZDoc* d) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ContextChanging(d);});return S_OK;}
HRESULT ProtoAppEvents::OnAppDestroyNotify() {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ContextChanging();});return S_OK;}
HRESULT ProtoAppEvents::OnActiveViewChanged(IZWindow*) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ActiveViewChanged();});return S_OK;}
HRESULT ProtoSelectEvents::OnSelected(IZElement* e,IZMathPoint* p,long,long,long,eZEntityType t,VARIANT v) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->Select(e,p,t,v);});return S_OK;}
HRESULT ProtoDrawEvents::OnPostDraw(IZRender* r) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->Draw(r);});return S_OK;}
