#include "HostSession.h"
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
static unsigned long pointNumber(const std::string& id) {
    if(id.size()<2||id[0]!='P')throw std::runtime_error("Invalid point ID");
    auto number=std::stoul(id.substr(1));
    if(!number||number==ULONG_MAX||id!="P"+std::to_string(number))throw std::runtime_error("Invalid point ID");
    return number;
}
template<class T> static void createSink(CComObject<T>*& sink,HostSession* owner) {
    checked(CComObject<T>::CreateInstance(&sink));sink->AddRef();sink->owner=owner;
}
template<class T> static void freeSink(CComObject<T>*& sink) {
    if(sink){sink->owner=nullptr;sink->Unadvise();sink->Release();sink=nullptr;}
}

BEGIN_MESSAGE_MAP(PhotoOverlay,CWnd)
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
    // The first Align follows calculation of the image rectangle in UpdateOverlay.
}
void PhotoOverlay::Clear(){if(GetSafeHwnd())DestroyWindow();if(!image.IsNull())image.Destroy();if(!frame_.IsNull())frame_.Destroy();}
void PhotoOverlay::Align(HWND host) {
    if(!GetSafeHwnd())return;PhysicalPixels scope;
    if(!::IsWindowVisible(host)||::IsIconic(::GetAncestor(host,GA_ROOT))){ShowWindow(SW_HIDE);return;}
    RECT r;::GetClientRect(host,&r);POINT p{0,0};::ClientToScreen(host,&p);
    if(image.IsNull()||r.right<=0||r.bottom<=0||imageRect.w<=0||imageRect.h<=0)return;
    photomatch::Rect target{std::round(imageRect.x),std::round(imageRect.y),std::round(imageRect.w),std::round(imageRect.h)};
    bool repaint=frame_.IsNull()||frame_.GetWidth()!=r.right||frame_.GetHeight()!=r.bottom
        ||drawnRect.x!=target.x||drawnRect.y!=target.y||drawnRect.w!=target.w||drawnRect.h!=target.h;
    if(!repaint&&position_==CPoint(p)&&IsWindowVisible()&&opacity==drawnOpacity_)return;
    if(repaint){
        if(!frame_.IsNull())frame_.Destroy();
        if(!frame_.Create(r.right,r.bottom,32))throw std::runtime_error("Cannot allocate photo overlay");
        HDC memory=frame_.GetDC();CDC* dc=CDC::FromHandle(memory);
        dc->FillSolidRect(&r,RGB(1,2,3));dc->SetStretchBltMode(HALFTONE);SetBrushOrgEx(memory,0,0,nullptr);
        int x=int(target.x),y=int(target.y),w=int(target.w),h=int(target.h);
        BOOL drawn=image.Draw(memory,x,y,w,h);dc->Draw3dRect(x,y,w,h,RGB(0,255,255),RGB(0,255,255));
        GdiFlush();frame_.ReleaseDC();if(!drawn)throw std::runtime_error("Cannot draw photo overlay");
        // Make the letterbox transparent; all visible pixels have straight alpha 255.
        for(int row=0;row<r.bottom;++row){auto pixels=static_cast<BYTE*>(frame_.GetBits())+row*frame_.GetPitch();
            for(int col=0;col<r.right;++col){BYTE* pixel=pixels+col*4;
                if(pixel[0]==3&&pixel[1]==2&&pixel[2]==1)memset(pixel,0,4);else pixel[3]=255;
            }
        }
    }
    // Commit the entire backing image with its new bounds, including maximize/DPI changes.
    HDC memory=frame_.GetDC();POINT source{0,0};SIZE size{r.right,r.bottom};BLENDFUNCTION blend{AC_SRC_OVER,0,opacity,AC_SRC_ALPHA};
    BOOL updated=::UpdateLayeredWindow(GetSafeHwnd(),nullptr,&p,&size,memory,&source,0,&blend,ULW_ALPHA);
    frame_.ReleaseDC();if(!updated)throw std::runtime_error("Cannot update photo overlay");
    drawnRect=target;drawnOpacity_=opacity;position_=CPoint(p);ShowWindow(SW_SHOWNOACTIVATE);
}

BEGIN_MESSAGE_MAP(HostSession,CWnd)
 ON_WM_TIMER()
END_MESSAGE_MAP()
void HostSession::Start() {
    GUID guid;checked(CoCreateGuid(&guid));wchar_t identifier[40];StringFromGUID2(guid,identifier,40);instanceId_=utf8(identifier);
    CString cls=AfxRegisterWndClass(0);
    if(!CreateEx(0,cls,L"PhotoMatch host bridge",0,0,0,0,0,HWND_MESSAGE,nullptr))throw std::runtime_error("Cannot create host dispatcher");
    createSink(appEvents_,this);checked(appEvents_->Advise(app_));SyncDocument();pipe_.Open();
    SetTimer(1,50,nullptr);Log(L"Host bridge ready. Runtime acceptance is pending.");
}
HostSession::~HostSession(){Shutdown();}
void HostSession::Log(const CString& text) {
    logs_.push_back(utf8(text));if(logs_.size()>200)logs_.erase(logs_.begin());
}
void HostSession::Guard(const std::function<void()>& action) {
    try {if(GetCurrentThreadId()!=thread_)throw std::runtime_error("CAD call attempted outside host thread");action();}
    catch(const _com_error& e){CString message;message.Format(L"COM error 0x%08X: %s",unsigned(e.Error()),e.ErrorMessage());Log(message);errors_.push_back(utf8(message));}
    catch(const std::exception& e){CString message(CA2W(e.what(),CP_UTF8));Log(message);errors_.push_back(e.what());}
    catch(CException* e){wchar_t text[512]={};e->GetErrorMessage(text,512);e->Delete();Log(text);errors_.push_back(utf8(text));}
    catch(...){Log(L"Unexpected failure; operation stopped.");errors_.push_back("Unexpected failure");}
}
void HostSession::CheckContext() {
    if(!captured_||!doc_)throw std::runtime_error("Capture a scene first");
    IZDocPtr active;checked(app_->get_ActiveDoc(&active));if(!sameObject(active,doc_))throw std::runtime_error("Document changed; recapture required");
    if(GraphicsWindow()!=capturedWindow_)throw std::runtime_error("Active viewport changed; restore and recapture");
    RefreshPoints();
}
static std::vector<IZElementPtr> sceneParts(IZSceneDoc* scene) {
    std::vector<IZElementPtr> parts;
    std::function<void(IZElement*)> visit=[&](IZElement* element){
        eZElementType type;checked(element->get_Type(&type));
        if(type==Z_ELEMENT_PART)parts.push_back(element);
        if(type==Z_ELEMENT_ASSEMBLY){CComVariant children;checked(element->GetChildren(&children));
            if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)for(auto& child:items(children)){IZElementPtr next=unknown(child);if(next)visit(next);}}
    };
    CComVariant children;checked(scene->GetChildElements(&children));
    if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)for(auto& child:items(children)){IZElementPtr next=unknown(child);if(next)visit(next);}
    return parts;
}
void HostSession::InitializeReference(PickedPoint& point) {
    // Persistent references authorize coordinate changes only when the SDK
    // validates the same part and vertex. Failure never selects a nearby vertex.
    point.reference=nullptr;
    try {
        IZProjectionPersistentRefObjPtr ref;checked(ref.CreateInstance(__uuidof(ZProjectionPersistentRefObj)));
        CComVariant id=point.objectId;checked(id.ChangeType(VT_I4));
        IZMathPointPtr position;checked(position.CreateInstance(__uuidof(ZMathPoint)));checked(position->put_Data(xyz(point.transformedPoint)));
        checked(ref->Initialize(scene_,id.lVal,Z_SubElem_Vertex,point.vertexId,Z_SubPos_VertexOnElem,Z_SubDir_None,position));
        checked(ref->Activate(scene_));point.reference=ref;
    }catch(const std::exception&){}catch(const _com_error&){}
}
void HostSession::RefreshPoints() {
    if(!doc_||points_.empty()||(!captured_&&!restored_))return;
    IZDocPtr active;checked(app_->get_ActiveDoc(&active));if(!sameObject(active,doc_))return;
    auto parts=sceneParts(scene_);bool changed=false;
    for(auto& point:points_){
        auto next=point;
        try {
            unsigned matches=0;for(auto& candidate:parts)if(sameObject(candidate,point.element))++matches;
            if(matches!=1)throw std::runtime_error("object_missing_or_ambiguous");
            IZPartPtr part=point.element;CComVariant bodies;checked(part->GetBodies(VARIANT_TRUE,&bodies));auto list=items(bodies);
            if(list.size()!=1)throw std::runtime_error("body_ambiguous");
            IZBodyPtr body=unknown(list[0]);IZBodyVertexPtr vertices=body;CComVariant ids;checked(body->GetVertexIds(&ids));
            unsigned count=0;for(auto id:items(ids)){checked(id.ChangeType(VT_I4));if(id.lVal==point.vertexId)++count;}
            if(count!=1)throw std::runtime_error("vertex_missing_or_ambiguous");
            CComVariant position;checked(vertices->GetPosition(point.vertexId,&position));next.apiPoint=vector3(position);
            if(jsonVec(next.apiPoint)!=jsonVec(point.apiPoint)){
                if(!point.reference)throw std::runtime_error("vertex_identity_unverified");
                IZCameraPtr camera;checked(cameras_->get_ActiveCamera(&camera));eZValidationResult valid=Z_ValRes_Invalid;
                checked(point.reference->Validate(scene_,camera,&valid));
                IZElementPtr owner;unsigned long vertex=0;eZSubElemType type=Z_SubElem_Unknown;
                checked(point.reference->get_Element(&owner));checked(point.reference->get_SubElementID(&vertex));checked(point.reference->get_SubElementType(&type));
                if(valid!=Z_ValRes_Valid||!sameObject(owner,point.element)||type!=Z_SubElem_Vertex||vertex!=unsigned(point.vertexId))
                    throw std::runtime_error("vertex_identity_unverified");
            }
            IZSceneElementPtr se=point.element;IZMathMatrixPtr matrix;checked(se->GetTransformToGlobal(&matrix));
            checked(matrix->GetDataCOM(next.matrix.data()));next.transformedPoint=transform(next.apiPoint,matrix);
            next.bindingStatus="connected";
        }catch(const std::exception&){next=point;next.bindingStatus="needs_reconnection";}
        catch(const _com_error&){next=point;next.bindingStatus="needs_reconnection";}
        if(next.matrix!=point.matrix||jsonVec(next.apiPoint)!=jsonVec(point.apiPoint)||next.bindingStatus!=point.bindingStatus){point=next;changed=true;}
    }
    if(changed){++modelRevision_;sampleRecords_.clear();measuring_=false;manualRecord_="null";
        Log(L"Model references refreshed; photo points retained. Unverified vertices require reconnection.");}
}
void HostSession::RequireConnectedPoints() {
    for(const auto& point:points_)if(point.bindingStatus!="connected")throw std::runtime_error("Reconnect the affected model points first");
}
HWND HostSession::GraphicsWindow() {
    IZWindowMgrPtr manager;checked(scene_->get_WindowMgr(&manager));IZWindowPtr window;checked(manager->get_ActiveWindow(&window));
    if(!window)throw std::runtime_error("No active graphics window");LONG_PTR value=0;checked(window->get_GraphicsHWND(&value));
    HWND hwnd=reinterpret_cast<HWND>(value);if(!::IsWindow(hwnd))throw std::runtime_error("Invalid graphics window");return hwnd;
}
CameraState HostSession::ReadCamera(IZCamera* camera) {
    if(!camera)throw std::runtime_error("No camera");CameraState s;
    checked(camera->get_Position(&s.position));checked(camera->get_Direction(&s.direction));checked(camera->get_Up(&s.up));
    checked(camera->get_CenterOfInterest(&s.interest));checked(camera->get_FieldOfView(&s.values.field));
    checked(camera->get_Perspective(&s.perspective));checked(camera->get_Near(&s.nearClip));checked(camera->get_Far(&s.farClip));checked(camera->get_Scale(&s.scale));
    s.values.position=vector3(s.position);s.values.direction=vector3(s.direction);s.values.up=vector3(s.up);return s;
}
IZCameraPtr HostSession::LiveCamera() {
    IZWindowMgrPtr manager;checked(scene_->get_WindowMgr(&manager));
    IZPanePtr pane;checked(manager->get_ActivePane(&pane));
    if(!pane)throw std::runtime_error("No active graphics pane");
    IZCameraPtr camera;checked(pane->get_Camera(&camera));return camera;
}
CameraState HostSession::ReadLiveCamera() { return ReadCamera(LiveCamera()); }
void HostSession::WriteCamera(IZCamera* camera,const CameraState& s) {
    checked(camera->put_Perspective(s.perspective));checked(camera->put_Position(s.position));
    checked(camera->put_Direction(s.direction));checked(camera->put_Up(s.up));
    checked(camera->put_Near(s.nearClip));checked(camera->put_Far(s.farClip));checked(camera->put_Scale(s.scale));checked(camera->put_FieldOfView(s.values.field));
    // Earlier setters can replace the orbit center; restore it last.
    checked(camera->put_CenterOfInterest(s.interest));
}
std::string HostSession::ModelFingerprint() {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    std::function<void(IZElement*)> visit=[&](IZElement* element){
        eZElementType type;checked(element->get_Type(&type));
        if(type!=Z_ELEMENT_PART&&type!=Z_ELEMENT_ASSEMBLY)return;
        LONG id=0;checked(element->get_Id(&id));out<<id;
        IZSceneElementPtr se=element;if(se){IZMathMatrixPtr m;checked(se->GetTransformToGlobal(&m));double data[16];checked(m->GetDataCOM(data));for(double d:data)out<<','<<d;}
        IZPartPtr part=element;if(part){CComVariant bounds;checked(part->GetBoundingBox(VARIANT_TRUE,&bounds));for(auto v:items(bounds)){checked(v.ChangeType(VT_R8));out<<','<<v.dblVal;}}
        // GetChildren is an assembly traversal API; a part can return E_FAIL.
        if(type==Z_ELEMENT_ASSEMBLY){
            CComVariant children;checked(element->GetChildren(&children));if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)
                for(auto& child:items(children)){IZElementPtr next=unknown(child);if(next)visit(next);}
        }
        out<<';';
    };
    CComVariant children;checked(scene_->GetChildElements(&children));
    if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)for(auto& child:items(children)){IZElementPtr element=unknown(child);if(element)visit(element);}
    return out.str();
}
void HostSession::Capture() {
    if(captured_)Restore();StopPicking();freeSink(drawEvents_);++captureId_;
    checked(app_->get_ActiveDoc(&doc_));scene_=doc_;if(!scene_)throw std::runtime_error("Open a 3D test scene");
    checked(scene_->get_CameraMgr(&cameras_));original_=LiveCamera();saved_=ReadCamera(original_);
    modelBefore_=ModelFingerprint();observations_.clear();sampleRecords_.clear();errors_.clear();cameraRecords_.clear();
    savedCameraRecord_=jsonCameraState(saved_);restoredCameraRecord_="null";
    imagePath_.Empty();recordedImageFocal_=0;backgroundStatus_=L"not_tested";
    restored_=false;modelUnchanged_=false;captured_=true;capturedWindow_=GraphicsWindow();RefreshPoints();
    IZDrawEventsPtr events;checked(doc_->get_DrawEvents(&events));createSink(drawEvents_,this);checked(drawEvents_->Advise(events));
    Log(L"State captured. Original camera retained; model transforms and part bounds recorded.");
}
void HostSession::StopPicking() {
    freeSink(selectEvents_);if(interactor_){interactor_->Stop();interactor_=nullptr;}
    replacePoint_=size_t(-1);
}
void HostSession::Reconnect(const nlohmann::json& saved) {
    IZDocPtr active;checked(app_->get_ActiveDoc(&active));IZSceneDocPtr scene=active;
    if(!scene)throw std::runtime_error("Open the saved 3D document first");
    // Save As can change the document path. Verify the actual references below.
    std::vector<IZElementPtr> parts;
    std::function<void(IZElement*)> visit=[&](IZElement* element){
        eZElementType type;checked(element->get_Type(&type));
        if(type==Z_ELEMENT_PART)parts.push_back(element);
        if(type==Z_ELEMENT_ASSEMBLY){CComVariant children;checked(element->GetChildren(&children));
            if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)for(auto& child:items(children)){IZElementPtr next=unknown(child);if(next)visit(next);}}
    };
    CComVariant children;checked(scene->GetChildElements(&children));
    if(children.vt!=VT_EMPTY&&children.vt!=VT_NULL)for(auto& child:items(children)){IZElementPtr next=unknown(child);if(next)visit(next);}
    const auto& records=saved.at("points");
    if(!records.is_array()||records.size()>1000)throw std::runtime_error("Invalid saved points");
    std::vector<PickedPoint> rebound;
    auto equal=[](double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-9;};
    for(const auto& record:records){
        auto number=pointNumber(record.at("id").get<std::string>());
        for(const auto& old:rebound)if(old.number==number)throw std::runtime_error("Duplicate point ID");
        if(record.value("binding_status",std::string())=="needs_reconnection"){
            PickedPoint point{};point.number=number;point.objectId=long(0);point.bindingStatus="needs_reconnection";
            rebound.push_back(point);continue;
        }
        IZElementPtr owner;unsigned matches=0;
        for(auto& candidate:parts){LONG id=0;checked(candidate->get_Id(&id));
            if(std::to_string(id)==record.at("object_id").get<std::string>()){owner=candidate;++matches;}}
        if(matches!=1)throw std::runtime_error("Saved model object is missing or ambiguous");
        IZPartPtr part=owner;CComVariant bodies;checked(part->GetBodies(VARIANT_TRUE,&bodies));auto list=items(bodies);
        if(list.size()!=1)throw std::runtime_error("Saved model body is ambiguous");
        IZBodyVertexPtr vertices=unknown(list[0]);if(!vertices)throw std::runtime_error("Saved vertex API is unavailable");
        PickedPoint point;point.number=number;point.element=owner;point.vertexId=record.at("vertex_id").get<long>();
        for(const auto& old:rebound)if(sameObject(old.element,owner)&&old.vertexId==point.vertexId)throw std::runtime_error("Duplicate saved vertex");
        LONG id=0;checked(owner->get_Id(&id));point.objectId=id;CComBSTR name;checked(owner->get_Name(&name));point.name=name;
        CComVariant position;checked(vertices->GetPosition(point.vertexId,&position));point.apiPoint=vector3(position);
        IZSceneElementPtr se=owner;IZMathMatrixPtr matrix;checked(se->GetTransformToGlobal(&matrix));
        checked(matrix->GetDataCOM(point.matrix.data()));point.transformedPoint=transform(point.apiPoint,matrix);
        auto local=record.at("api_coordinates").get<std::vector<double>>(),world=record.at("transformed_coordinates").get<std::vector<double>>(),savedMatrix=record.at("transform").get<std::vector<double>>();
        if(local.size()!=3||world.size()!=3||savedMatrix.size()!=16)throw std::runtime_error("Invalid saved coordinates");
        if(!equal(local[0],point.apiPoint.x)||!equal(local[1],point.apiPoint.y)||!equal(local[2],point.apiPoint.z)||
           !equal(world[0],point.transformedPoint.x)||!equal(world[1],point.transformedPoint.y)||!equal(world[2],point.transformedPoint.z))throw std::runtime_error("Saved vertex geometry changed");
        for(int i=0;i<16;++i)if(!equal(savedMatrix[i],point.matrix[i]))throw std::runtime_error("Saved model transform changed");
        rebound.push_back(point);
    }
    // Validate every reference before replacing the current capture or its points.
    Capture();points_=std::move(rebound);for(auto& point:points_){nextPointNumber_=std::max(nextPointNumber_,point.number+1);if(point.element)InitializeReference(point);}
    markersVisible_=true;checked(scene_->Redraw());Log(L"Saved vertex references verified; unconnected rows retained.");
}
void HostSession::Resume() {
    if(!captured_&&restored_&&doc_){
        IZDocPtr active;checked(app_->get_ActiveDoc(&active));
        if(!sameObject(active,doc_)||GraphicsWindow()!=capturedWindow_)
            throw std::runtime_error("Document or viewport changed; capture again");
        original_=LiveCamera();saved_=ReadCamera(original_);
        savedCameraRecord_=jsonCameraState(saved_);restoredCameraRecord_="null";
        captured_=true;restored_=false;modelUnchanged_=false;
        Log(L"Camera preview resumed; existing model and photo correspondence IDs retained.");
    }
    CheckContext();
}
void HostSession::Pick() {
    Resume();StopPicking();IZSelectionMgrPtr selection;checked(scene_->get_SelectionMgr(&selection));
    checked(selection->CreateInteractor(&interactor_));IZSelectEventsPtr events;checked(interactor_->get_SelectEvents(&events));
    createSink(selectEvents_,this);checked(selectEvents_->Advise(events));checked(events->SetSelectionFilterChoices(Z_SEL_FEV,Z_SEL_FEV));
    checked(interactor_->Start());markersVisible_=true;checked(scene_->Redraw());Log(L"Select a solid vertex. Face/edge clicks are ignored. Click again to restart after Escape.");
}
void HostSession::Select(IZElement* element,IZMathPoint*,eZEntityType type,const VARIANT& ids) {
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
        bool duplicate=false;for(size_t j=0;j<points_.size();++j)if(j!=replacePoint_&&sameObject(points_[j].element,owner)&&points_[j].vertexId==point.vertexId)duplicate=true;
        if(duplicate)continue;InitializeReference(point);
        if(replacePoint_<points_.size()){point.number=points_[replacePoint_].number;points_[replacePoint_]=point;++modelRevision_;StopPicking();checked(scene_->Redraw());Log(L"Selected correspondence reconnected; its photo location is unchanged.");return;}
        point.number=nextPointNumber_++;
        points_.push_back(point);
        CString message;message.Format(L"P%03u %s vertex %ld | API %s | transformed %s (coordinate convention pending)",
            unsigned(point.number),point.name.GetString(),point.vertexId,show(point.apiPoint).GetString(),show(point.transformedPoint).GetString());Log(message);
    }
    checked(scene_->Redraw());
}
void HostSession::Apply(const Camera& input) {
    unit(cross(input.direction,input.up));
    if(!finite(input.position)||!std::isfinite(input.field)||input.field<=0)throw std::runtime_error("Invalid camera values");
    Resume();
    CheckContext();RequireConnectedPoints();adjusting_=manualCamera_=false;manualRecord_="null";
    StopPicking();overlay_.Clear();imageFocal_=0;
    if(!test_)test_=LiveCamera();CameraState state=saved_;state.perspective=VARIANT_TRUE;state.values=input;
    state.position=xyz(input.position);state.direction=xyz(input.direction);state.up=xyz(input.up);
    Vec3 f=unit(input.direction);state.interest=xyz({input.position.x+f.x,input.position.y+f.y,input.position.z+f.z});
    WriteCamera(test_,state);auto readback=ReadLiveCamera();
    cameraRecords_.push_back("{\"input\":"+jsonCameraState(state)+",\"readback\":"+jsonCameraState(readback)+"}");
    checked(scene_->Redraw());Log(L"Test camera applied. Measure at multiple aspect ratios and camera settings.");
}
void HostSession::DrawPoints(IZRender* render) {
    if(!markersVisible_||!doc_||(!captured_&&!restored_)||points_.empty())return;
    IZDocPtr active;checked(app_->get_ActiveDoc(&active));if(!sameObject(active,doc_)||GraphicsWindow()!=capturedWindow_)return;
    RefreshPoints();LONG width=0,height=0;checked(render->GetViewExtents(&width,&height));
    const auto camera=ReadLiveCamera().values;
    checked(render->SetTextStyle(CComBSTR(L"Arial"),18.,VARIANT_FALSE,VARIANT_TRUE));
    for(const auto& point:points_){
        if(point.bindingStatus!="connected")continue;
        const auto p=point.transformedPoint;
        if((p.x-camera.position.x)*camera.direction.x+(p.y-camera.position.y)*camera.direction.y+(p.z-camera.position.z)*camera.direction.z<=0)continue;
        LONG x=0,y=0,z=0;checked(render->XformWorldToView2(p.x,p.y,p.z,&x,&y,&z));
        if(x<0||y<0||x>=width||y>=height)continue;
        checked(render->DrawEllipse2D(x,y,10,10,RGB(0,0,0),2,RGB(93,230,186)));
        CString label;label.Format(L"P%lu",point.number);
        LONG textWidth=0,textHeight=0;checked(render->GetTextStringSize2D(CComBSTR(label),&textWidth,&textHeight));
        LONG tx=std::max(0L,std::min(x+8,width-textWidth-2)),ty=std::max(0L,std::min(y+8,height-textHeight-2));
        checked(render->DrawTextString2D(CComBSTR(label),tx+1,ty+1,RGB(0,0,0),nullptr,nullptr));
        checked(render->DrawTextString2D(CComBSTR(label),tx,ty,RGB(93,230,186),nullptr,nullptr));
    }
}
void HostSession::Draw(IZRender* render) {
    DrawPoints(render);
    if(!measuring_||!captured_)return;measuring_=false;CheckContext();RequireConnectedPoints();
    CameraState state=ReadLiveCamera();
    if(!state.perspective)throw std::runtime_error("Projection measurement requires a perspective camera");
    LONG rw=0,rh=0;checked(render->GetViewExtents(&rw,&rh));HWND hwnd=GraphicsWindow();CRect physical=clientRect(hwnd);
    if(rw<=0||rh<=0||physical.IsRectEmpty())throw std::runtime_error("Empty viewport");
    double sx=double(physical.Width())/rw,sy=double(physical.Height())/rh;
    if(std::abs(sx-sy)>0.02)throw std::runtime_error("Viewport/render extent mismatch; use one unsplit view");
    dpi_=windowDpi(hwnd);Camera c=state.values;Vec3 f=unit(c.direction),r=unit(cross(f,c.up)),u=unit(cross(r,f));
    std::ostringstream diagnostics;diagnostics.imbue(std::locale::classic());diagnostics<<std::setprecision(17);
    for(int i=0;i<8;++i){double depth=10+5*(i%3),x=(i&1)?2:-2,y=(i&2)?1.5:-1.5;
        Vec3 world{c.position.x+f.x*depth+r.x*x+u.x*y,c.position.y+f.y*depth+r.y*x+u.y*y,c.position.z+f.z*depth+r.z*x+u.z*y};
        LONG px=0,py=0,pz=0;checked(render->XformWorldToView2(world.x,world.y,world.z,&px,&py,&pz));
        observations_.push_back({c,world,{px*sx,py*sy},double(physical.Width()),double(physical.Height()),double(rw),double(rh)});
        // Model-space doubles are diagnostic only: the render matrix is not assumed to be identity.
        double mx=0,my=0,mz=0;HRESULT modelHr=render->XformModelToView3(world.x,world.y,world.z,&mx,&my,&mz);
        if(i)diagnostics<<',';diagnostics<<"{\"world_integer_px\":["<<px<<','<<py<<"],\"model_double_hresult\":"<<modelHr;
        if(SUCCEEDED(modelHr)&&std::isfinite(mx)&&std::isfinite(my)&&std::isfinite(mz))diagnostics<<",\"model_double_px\":["<<mx<<','<<my<<','<<mz<<']';
        diagnostics<<'}';
    }
    auto result=fit(observations_);bool floatingVerified=!points_.empty()&&result[0].maxError<=1;
    std::ostringstream record;record.imbue(std::locale::classic());record<<std::setprecision(17)<<"{\"dpi\":"<<dpi_<<",\"render_size\":["<<rw<<','<<rh
        <<"],\"physical_size\":["<<physical.Width()<<','<<physical.Height()<<"],\"picked_point_projections\":[";
    for(size_t i=0;i<points_.size();++i){const auto& p=points_[i];LONG ax,ay,az,gx,gy,gz;
        checked(render->XformWorldToView2(p.apiPoint.x,p.apiPoint.y,p.apiPoint.z,&ax,&ay,&az));
        checked(render->XformWorldToView2(p.transformedPoint.x,p.transformedPoint.y,p.transformedPoint.z,&gx,&gy,&gz));
        if(i)record<<',';record<<"{\"id\":"<<quote("P"+std::to_string(p.number))<<",\"api_as_world_px\":["<<ax*sx<<','<<ay*sy<<"],\"transformed_as_world_px\":["<<gx*sx<<','<<gy*sy<<']';
        double dx=0,dy=0,dz=0;HRESULT hr=render->XformModelToView3(p.transformedPoint.x,p.transformedPoint.y,p.transformedPoint.z,&dx,&dy,&dz);
        record<<",\"model_double_hresult\":"<<hr;
        if(SUCCEEDED(hr)&&std::isfinite(dx)&&std::isfinite(dy))record<<",\"model_double_physical_px\":["<<dx*sx<<','<<dy*sy<<']';
        try {auto expected=projectSdk(p.transformedPoint,c,result[0].convention,rw,rh,physical.Width(),physical.Height(),false);
            floatingVerified=floatingVerified&&SUCCEEDED(hr)&&floatingProjectionAgrees({dx*sx,dy*sy},{gx*sx,gy*sy},expected,sx,sy);
        }catch(...){floatingVerified=false;}
        record<<'}';
    }record<<"],\"projection_diagnostics\":["<<diagnostics.str()<<"],\"floating_frame_anchors\":[";
    // Probe all corners of the selected points' world bounds, spanning depths
    // and nested transforms. Never mutate the SDK's rendering matrix.
    if(!points_.empty()){
        Vec3 low=points_[0].transformedPoint,high=low;
        for(const auto& p:points_){auto q=p.transformedPoint;
            low={(std::min)(low.x,q.x),(std::min)(low.y,q.y),(std::min)(low.z,q.z)};
            high={(std::max)(high.x,q.x),(std::max)(high.y,q.y),(std::max)(high.z,q.z)};
        }
        floatingVerified=floatingVerified&&high.x-low.x>1e-12&&high.y-low.y>1e-12&&high.z-low.z>1e-12;
        for(int i=0;i<8;++i){Vec3 q{(i&1)?high.x:low.x,(i&2)?high.y:low.y,(i&4)?high.z:low.z};
            double dx=0,dy=0,dz=0;LONG ix=0,iy=0,iz=0;
            HRESULT hr=render->XformModelToView3(q.x,q.y,q.z,&dx,&dy,&dz),world=render->XformWorldToView2(q.x,q.y,q.z,&ix,&iy,&iz);
            if(i)record<<',';record<<"{\"world\":"<<jsonVec(q)<<",\"model_hresult\":"<<hr<<",\"world_hresult\":"<<world;
            bool agrees=false;
            try {auto expected=projectSdk(q,c,result[0].convention,rw,rh,physical.Width(),physical.Height(),false);
                agrees=SUCCEEDED(hr)&&SUCCEEDED(world)&&floatingProjectionAgrees({dx*sx,dy*sy},{ix*sx,iy*sy},expected,sx,sy);
                if(std::isfinite(dx)&&std::isfinite(dy))record<<",\"floating_physical_px\":["<<dx*sx<<','<<dy*sy<<']';
                record<<",\"integer_physical_px\":["<<ix*sx<<','<<iy*sy<<"],\"analytical_physical_px\":["<<expected.x<<','<<expected.y<<']';
            }catch(...){agrees=false;}
            floatingVerified=floatingVerified&&agrees;record<<",\"agrees\":"<<(agrees?"true":"false")<<'}';
        }
    }
    record<<"],\"floating_world_frame_verified\":"<<(floatingVerified?"true":"false")<<'}';sampleRecords_.push_back(record.str());
    CString message;message.Format(L"%zu projections; best %s, max %.4f physical px; %s",observations_.size(),
        CString(CA2W(name(result[0].convention).c_str())).GetString(),result[0].maxError,
        resolved(result)?L"FOV identified":L"UNRESOLVED: change aspect ratio / inspect axes");Log(message);
}
void HostSession::Restore() {
    if(!captured_||restoring_)return;restoring_=true;
    try {
        StopPicking();measuring_=false;overlay_.Clear();imageFocal_=0;adjusting_=manualCamera_=false;
        if(backgroundChanged_){LARGE_INTEGER zero{};checked(backgroundBackup_->Seek(zero,STREAM_SEEK_SET,nullptr));checked(backgroundPersistence_->Load(backgroundBackup_));backgroundChanged_=false;}
        // Apply the complete saved state to the reactivated camera.
        // The pane camera is the camera that native orbit/pan/zoom edits.
        // It is borrowed, not added to the document's saved camera collection.
        WriteCamera(original_,saved_);test_=nullptr;
        auto now=ReadLiveCamera();restoredCameraRecord_=jsonCameraState(now);restored_=sameCameraValue(now.values,saved_.values)&&now.perspective==saved_.perspective&&sameCameraValue(now.nearClip,saved_.nearClip)&&sameCameraValue(now.farClip,saved_.farClip)&&sameCameraValue(now.scale,saved_.scale)
            &&sameCameraValue(vector3(now.interest),vector3(saved_.interest));
        modelUnchanged_=ModelFingerprint()==modelBefore_;checked(scene_->Redraw());
        Log(!restored_?L"CAMERA RESTORE CHECK FAILED; save diagnostics.":modelUnchanged_?L"Camera restored; model transforms / part bounds unchanged.":L"Camera restored; model edits retained.");
        backgroundPersistence_.Release();backgroundBackup_.Release();captured_=false;
    }catch(...){restoring_=false;throw;}restoring_=false;
}
void HostSession::ContextChanging(IZDoc* closing) {
    if(closing&&!sameObject(closing,doc_))return;
    markersVisible_=false;
    if(captured_)Guard([&]{Restore();});overlay_.Clear();StopPicking();freeSink(drawEvents_);captured_=false;
    doc_=nullptr;scene_=nullptr;cameras_=nullptr;original_=nullptr;test_=nullptr;
    // References and measurements belong to the old document, even after Restore.
    points_.clear();observations_.clear();sampleRecords_.clear();cameraRecords_.clear();
    nextPointNumber_=1;
    adjusting_=manualCamera_=false;manualRecord_="null";modelRevision_=0;
    measuring_=false;restored_=false;modelUnchanged_=false;capturedWindow_=nullptr;modelBefore_.clear();
    savedCameraRecord_=restoredCameraRecord_="null";imagePath_.Empty();recordedImageFocal_=0;
    backgroundStatus_=L"not_tested";
    Log(L"Document/view context changed. Operations stopped; capture again.");
}
void HostSession::ActiveDocumentChanged(IZDoc* next) {
    if(!sameObject(observedDoc_,next)){ContextChanging();observedDoc_=next;++generation_;}
}
void HostSession::ActiveViewChanged() {
    if(restoring_||!captured_||adjusting_||manualCamera_)return;
    // Camera changes can also emit this event. Defer validation to the next action.
    if(overlay_.GetSafeHwnd())overlay_.ShowWindow(SW_HIDE);
}
void HostSession::Shutdown() {
    markersVisible_=false;
    pipe_.Close();if(guiProcess_){CloseHandle(guiProcess_);guiProcess_=nullptr;}
    if(GetSafeHwnd())KillTimer(1);if(captured_)Guard([&]{Restore();});StopPicking();freeSink(drawEvents_);freeSink(appEvents_);overlay_.Clear();
}
Convention HostSession::OverlayConvention() {
    auto result=fit(observations_);if(resolved(result))return result[0].convention;
    CComBSTR version;checked(app_->get_ApiVersion(&version));
    // This SDK convention was verified in actual portrait/landscape views.
    // A fresh projection measurement must still agree in the current capture.
    if(utf8(version)=="29.0.2.20605")for(const auto& candidate:result)
        if(name(candidate.convention)=="minimum_radians_full"&&candidate.maxError<=1.0)return candidate.convention;
    throw std::runtime_error("Measure landscape and portrait views to identify FOV before photo alignment");
}
void HostSession::Photo(const CString& path,double focal,const nlohmann::json& principal) {
    CheckContext();if(!test_)throw std::runtime_error("Apply a test camera first");OverlayConvention();
    if(!std::isfinite(focal)||focal<=0)throw std::runtime_error("Focal length must be positive");
    Pixel center{};
    if(!principal.is_null()){
        auto v=principal.get<std::vector<double>>();if(v.size()!=2||!std::isfinite(v[0])||!std::isfinite(v[1]))throw std::runtime_error("Invalid principal point");center={v[0],v[1]};}
    overlay_.Open(path,GraphicsWindow());imagePrincipal_=principal.is_null()?Pixel{overlay_.image.GetWidth()/2.,overlay_.image.GetHeight()/2.}:center;
    imagePath_=path;imageFocal_=focal;recordedImageFocal_=focal;lastW_=lastH_=0;UpdateOverlay();
    Log(L"Reference image overlay enabled; camera and photo use the same original-image principal point.");
}
void HostSession::UpdateOverlay() {
    if(!overlay_.GetSafeHwnd())return;CheckContext();IZCameraPtr active=LiveCamera();
    if(!sameObject(active,test_)&&!adjusting_&&!manualCamera_){overlay_.Clear();throw std::runtime_error("Active camera changed; photo hidden");}
    HWND host=GraphicsWindow();CRect size=clientRect(host),renderSize;::GetClientRect(host,&renderSize);
    if(size.IsRectEmpty()||renderSize.Width()<=2||renderSize.Height()<=2){overlay_.ShowWindow(SW_HIDE);return;}
    if(lastW_!=size.Width()||lastH_!=size.Height()||lastRenderW_!=renderSize.Width()||lastRenderH_!=renderSize.Height()){
        double iw=overlay_.image.GetWidth(),ih=overlay_.image.GetHeight(),rw=renderSize.Width(),rh=renderSize.Height();
        overlay_.imageRect=sdkImageRect(iw,ih,rw,rh,size.Width(),size.Height(),imagePrincipal_);
        if(!adjusting_&&!manualCamera_){double field=sdkImageField(imageFocal_,iw,ih,rw,rh,size.Width(),size.Height(),OverlayConvention(),imagePrincipal_);checked(test_->put_FieldOfView(field));}
        lastW_=size.Width();lastH_=size.Height();lastRenderW_=rw;lastRenderH_=rh;checked(scene_->Redraw());
    }overlay_.Align(host);
}
void HostSession::ClosePhoto() {
    // Restore only the capture owned by this document. ContextChanging already
    // disposes the old document's capture before accepting a new session.
    markersVisible_=false;
    if(captured_){Restore();if(!restored_)throw std::runtime_error("Original camera restoration failed");}
    overlay_.Clear();imagePath_.Empty();imageFocal_=recordedImageFocal_=0;
    adjusting_=manualCamera_=false;manualRecord_="null";
    if(scene_)checked(scene_->Redraw());
}
void HostSession::AdjustCamera(const std::string& action,const nlohmann::json& args) {
    if(action=="use"){
        if(args.contains("camera_state")){
            const auto& record=args.at("camera_state");const auto& pose=record.at("pose_and_field");CameraState candidate;
            auto vec=[](const nlohmann::json& value){auto v=value.get<std::vector<double>>();
                if(v.size()!=3||!finite(Vec3{v[0],v[1],v[2]}))throw std::runtime_error("Invalid saved camera vector");return Vec3{v[0],v[1],v[2]};};
            candidate.values={vec(pose.at("position")),vec(pose.at("direction")),vec(pose.at("up")),pose.at("field_sdk").get<double>()};
            unit(cross(candidate.values.direction,candidate.values.up));
            candidate.position=xyz(candidate.values.position);candidate.direction=xyz(candidate.values.direction);candidate.up=xyz(candidate.values.up);
            candidate.interest=xyz(vec(record.at("center_of_interest")));candidate.perspective=record.at("perspective").get<bool>()?VARIANT_TRUE:VARIANT_FALSE;
            candidate.nearClip=record.at("near").get<double>();candidate.farClip=record.at("far").get<double>();candidate.scale=record.at("scale").get<double>();
            if(!candidate.perspective||!std::isfinite(candidate.values.field)||candidate.values.field<=0||candidate.values.field>=3.141592653589793
                ||!std::isfinite(candidate.nearClip)||!std::isfinite(candidate.farClip)||candidate.farClip<=candidate.nearClip||!std::isfinite(candidate.scale)||candidate.scale<=0)
                throw std::runtime_error("Invalid saved manual camera");
            auto principal=args.at("principal_px").get<std::vector<double>>();double focal=args.at("focal_px").get<double>();
            if(principal.size()!=2||!std::isfinite(principal[0])||!std::isfinite(principal[1])||!std::isfinite(focal)||focal<=0)throw std::runtime_error("Invalid saved image intrinsics");
            auto path=args.at("path").get<std::string>();CString imagePath(CA2W(path.c_str(),CP_UTF8));CImage probe;checked(probe.Load(imagePath));
            // This is a user-saved camera, never the restoration baseline.
            manualState_=candidate;manualRecord_=jsonCameraState(candidate);imagePrincipal_={principal[0],principal[1]};
            recordedImageFocal_=focal;imagePath_=imagePath;
        }
        if(manualRecord_=="null"||imagePath_.IsEmpty())throw std::runtime_error("No saved manual camera in this document");
        Resume();RequireConnectedPoints();
        if(!test_)test_=LiveCamera();WriteCamera(test_,manualState_);
        manualCamera_=true;adjusting_=false;
        Photo(imagePath_,recordedImageFocal_,nlohmann::json::array({imagePrincipal_.x,imagePrincipal_.y}));
        checked(scene_->Redraw());return;
    }
    CheckContext();
    if(!overlay_.GetSafeHwnd())throw std::runtime_error("Preview the photo before adjusting the camera");
    if(action=="begin"){
        if(adjusting_)throw std::runtime_error("Camera adjustment is already active");
        adjustmentStart_=ReadLiveCamera();adjustmentRect_=overlay_.imageRect;adjustmentWasManual_=manualCamera_;adjusting_=true;
    }else if(action=="cancel"){
        if(!adjusting_)throw std::runtime_error("Camera adjustment is not active");
        WriteCamera(test_,adjustmentStart_);
        overlay_.imageRect=adjustmentRect_;adjusting_=false;manualCamera_=adjustmentWasManual_;
        // Adjustment can span a viewport resize; rebuild placement for the
        // current viewport instead of restoring obsolete physical bounds.
        lastW_=lastH_=0;checked(scene_->Redraw());
    }else if(action=="save"){
        if(!adjusting_)throw std::runtime_error("Camera adjustment is not active");
        RequireConnectedPoints();auto actual=ReadLiveCamera();
        if(!actual.perspective)throw std::runtime_error("Use a perspective camera before saving adjustment");
        manualState_=actual;manualRecord_=jsonCameraState(actual);adjusting_=false;manualCamera_=true;
        sampleRecords_.clear();
    }else throw std::runtime_error("Unknown camera adjustment action");
    UpdateOverlay();
}
void HostSession::Background(const CString& path) {
    CheckContext();if(backgroundChanged_)throw std::runtime_error("Restore the first background probe before applying another");IZSurfaceFinishPtr finish;checked(scene_->get_Background(&finish));
    CComPtr<IPersistStream> persist;HRESULT hr=finish->QueryInterface(IID_PPV_ARGS(&persist));
    if(FAILED(hr)){backgroundStatus_=L"unavailable: background has no independent persistence snapshot";Log(backgroundStatus_+L". Use overlay.");return;}
    CComPtr<IStream> backup;checked(CreateStreamOnHGlobal(nullptr,TRUE,&backup));checked(persist->Save(backup,FALSE));
    LARGE_INTEGER zero{};checked(backup->Seek(zero,STREAM_SEEK_SET,nullptr));checked(persist->Load(backup));
    CImage probe;checked(probe.Load(path));
    backgroundBackup_=backup;backgroundPersistence_=persist;backgroundChanged_=true;
    IZTexturePtr texture;checked(finish->CreateTexture(CComBSTR(path),&texture));checked(scene_->Redraw());
    backgroundStatus_=L"applied_pending_visual_verification";Log(L"Native background probe applied. Alignment unverified. Restore returns the captured background.");
}
std::string HostSession::Report() {
    CComBSTR version;checked(app_->get_ApiVersion(&version));std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
    o<<"{\n\"schema_version\":1,\"sdk_version\":"<<quote(utf8(version))<<",\"overall_status\":\"runtime_matrix_not_completed\","
      <<"\"restored\":"<<(restored_?"true":"false")<<",\"model_transforms_bounds_unchanged\":"<<(modelUnchanged_?"true":"false")
      <<",\"camera_restore_comparison\":\"abs(a-b) <= 16 * DBL_EPSILON * max(1, abs(a), abs(b))\""
      <<",\"background\":"<<quote(utf8(backgroundStatus_))<<",\"image\":"<<quote(utf8(imagePath_))<<",\"image_focal_px\":"<<recordedImageFocal_<<",\"original_camera\":"<<savedCameraRecord_
      <<",\"restored_camera\":"<<restoredCameraRecord_<<",\"points\":[";
    for(size_t i=0;i<points_.size();++i){const auto& p=points_[i];CComVariant id=p.objectId;checked(id.ChangeType(VT_BSTR));if(i)o<<',';
        o<<"{\"id\":"<<quote("P"+std::to_string(p.number))<<",\"binding_status\":"<<quote(p.bindingStatus)<<",\"object_id\":"<<quote(utf8(id.bstrVal))<<",\"object_name\":"<<quote(utf8(p.name))
         <<",\"vertex_id\":"<<p.vertexId<<",\"api_coordinates\":"<<jsonVec(p.apiPoint)<<",\"transformed_coordinates\":"<<jsonVec(p.transformedPoint)
         <<",\"coordinate_convention\":\"pending_fixture_verification\",\"transform\":[";
        for(int j=0;j<16;++j){if(j)o<<',';o<<p.matrix[j];}o<<"]}";
    }o<<"],\"observations\":[";
    for(size_t i=0;i<observations_.size();++i){auto& v=observations_[i];if(i)o<<',';o<<"{\"camera\":"<<jsonCamera(v.camera)<<",\"world\":"<<jsonVec(v.world)
        <<",\"actual_physical_px\":["<<v.actual.x<<','<<v.actual.y<<"],\"viewport\":["<<v.width<<','<<v.height<<"],\"render_size\":["<<v.renderWidth<<','<<v.renderHeight<<"]}";}
    auto fits=fit(observations_);o<<"],\"fov_resolved\":"<<(resolved(fits)?"true":"false")<<",\"fov_candidates\":[";
    for(size_t i=0;i<fits.size();++i){if(i)o<<',';o<<"{\"name\":"<<quote(name(fits[i].convention))<<",\"max_error_px\":";
        if(std::isfinite(fits[i].maxError))o<<fits[i].maxError;else o<<"null";o<<'}';}
    auto array=[&](const char* key,const std::vector<std::string>& values,bool quoted){o<<"],\""<<key<<"\":[";for(size_t i=0;i<values.size();++i){if(i)o<<',';o<<(quoted?quote(values[i]):values[i]);}};
    array("measurements",sampleRecords_,false);array("camera_apply",cameraRecords_,false);array("errors",errors_,true);o<<"]\n}\n";
    return o.str();
}
void HostSession::OnTimer(UINT_PTR id) {
    if(id==1&&!servicing_){servicing_=true;
        Guard([&]{SyncDocument();pipe_.Poll([&](const std::string& data){return Request(data);});});
        if(overlay_.GetSafeHwnd())Guard([&]{try{UpdateOverlay();}catch(...){overlay_.Clear();imageFocal_=0;throw;}});
        servicing_=false;
    }CWnd::OnTimer(id);
}
HRESULT ProtoAppEvents::OnActiveDocChanged(IZDoc* next) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ActiveDocumentChanged(next);});return S_OK;}
HRESULT ProtoAppEvents::OnDocumentPreClosed(IZDoc* d) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ContextChanging(d);});return S_OK;}
HRESULT ProtoAppEvents::OnAppDestroyNotify() {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ContextChanging();});return S_OK;}
HRESULT ProtoAppEvents::OnActiveViewChanged(IZWindow*) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->ActiveViewChanged();});return S_OK;}
HRESULT ProtoSelectEvents::OnSelected(IZElement* e,IZMathPoint* p,long,long,long,eZEntityType t,VARIANT v) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->Select(e,p,t,v);});return S_OK;}
HRESULT ProtoDrawEvents::OnPostDraw(IZRender* r) {AFX_MANAGE_STATE(AfxGetStaticModuleState());if(owner)owner->Guard([&]{owner->Draw(r);});return S_OK;}

void HostSession::SyncDocument() {
    IZDocPtr active;checked(app_->get_ActiveDoc(&active));
    if(!sameObject(active,observedDoc_)){ContextChanging();observedDoc_=active;++generation_;}
}
std::string HostSession::SessionId() const {
    return instanceId_+":"+std::to_string(generation_);
}
nlohmann::json HostSession::Snapshot() {
    RefreshPoints();using nlohmann::json;json out=json::parse(Report());
    out["session"]=SessionId();out["capture_id"]=captureId_;out["captured"]=captured_;
    out["picking"]=bool(interactor_);out["test_camera"]=bool(test_);out["measuring"]=measuring_;
    out["logs"]=logs_;out["host_pid"]=GetCurrentProcessId();out["document"]=nullptr;out["camera"]=nullptr;
    out["saved_point_reconnect"]=true;
    out["point_delete_supported"]=true;out["point_clear_supported"]=true;out["point_markers_supported"]=true;
    out["replace_model_supported"]=true;
    out["scene_project_supported"]=true;
    out["photo_workflow_version"]=1;out["model_revision"]=modelRevision_;
    out["photo_opacity"]=overlay_.opacity/255.;
    out["adjusting_camera"]=adjusting_;out["manual_camera"]=json::parse(manualRecord_);
    out["photo_principal_px"]={imagePrincipal_.x,imagePrincipal_.y};
    out["projection_coordinate_rule"]="sdk_pixel_endpoints_truncate_then_physical_scale";
    if(overlay_.GetSafeHwnd()){auto r=overlay_.imageRect;out["photo_rectangle_physical"]={r.x,r.y,r.w,r.h};
        auto d=overlay_.drawnRect;out["photo_drawn_rectangle_physical"]={d.x,d.y,d.w,d.h};out["photo_render_size"]={lastRenderW_,lastRenderH_};}
    if(observedDoc_){CComBSTR name;checked(observedDoc_->get_Name(&name));out["document"]=utf8(name);}
    if(captured_&&cameras_){IZCameraPtr active;checked(cameras_->get_ActiveCamera(&active));
        out["camera"]=json::parse(jsonCameraState(ReadLiveCamera()));out["camera_state_source"]="active_pane";
        out["managed_camera"]=json::parse(jsonCameraState(ReadCamera(active)));
        CRect rect=clientRect(GraphicsWindow()),hostRect;::GetClientRect(GraphicsWindow(),&hostRect);
        out["viewport"]={rect.Width(),rect.Height()};out["host_client_size"]={hostRect.Width(),hostRect.Height()};out["dpi"]=windowDpi(GraphicsWindow());}
    return out;
}
std::string HostSession::Request(const std::string& text) {
    using nlohmann::json;json response;response["ok"]=false;
    try {
        if(GetCurrentThreadId()!=thread_)throw std::runtime_error("CAD call attempted outside host thread");
        auto request=json::parse(text);response["id"]=request.at("id");
        FILETIME ft;GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER time;time.LowPart=ft.dwLowDateTime;time.HighPart=ft.dwHighDateTime;
        auto now=(time.QuadPart-116444736000000000ULL)/10000;
        if(request.at("deadline_ms").get<unsigned long long>()<now)throw std::runtime_error("Request expired; no action performed");
        SyncDocument();auto command=request.at("command").get<std::string>();
        if(command!="status"&&request.at("session").get<std::string>()!=SessionId())throw std::runtime_error("Document changed; refresh and capture again");
        auto args=request.value("args",json::object());json fixture;
        if(command=="capture")Capture();
        else if(command=="save_scene"){
            if(!observedDoc_)throw std::runtime_error("Open an IronCAD scene first");
            IZSceneDocPtr activeScene=observedDoc_;if(!activeScene)throw std::runtime_error("Open an IronCAD scene first");
            if(args.contains("path")){auto path=args.at("path").get<std::string>();checked(observedDoc_->SaveAs(CComBSTR(CA2W(path.c_str(),CP_UTF8))));}
            else checked(observedDoc_->Save());
        }
        else if(command=="open_scene"){
            auto path=args.at("path").get<std::string>();CString filename(CA2W(path.c_str(),CP_UTF8));
            if(filename.Right(4).CompareNoCase(L".ics")!=0||GetFileAttributesW(filename)==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Saved IronCAD scene is missing");
            ClosePhoto();IZDocPtr opened;checked(app_->OpenFile(CComBSTR(filename),VARIANT_FALSE,&opened));
            if(!opened)throw std::runtime_error("Cannot open saved IronCAD scene");
            checked(app_->put_ActiveDoc(opened));SyncDocument();
        }
        else if(command=="test_coordinate_fixture")fixture=CoordinateFixture();
        else if(command=="reconnect")Reconnect(args.at("host"));
        else if(command=="replace_model"){
            const auto& ids=args.at("ids");
            if(!ids.is_array()||ids.empty()||ids.size()>1000)throw std::runtime_error("Invalid correspondence rows");
            std::vector<PickedPoint> pending;
            for(const auto& id:ids){
                PickedPoint point{};point.number=pointNumber(id.get<std::string>());
                for(const auto& old:pending)if(old.number==point.number)throw std::runtime_error("Duplicate point ID");
                point.objectId=long(0);point.bindingStatus="needs_reconnection";pending.push_back(point);
            }
            Capture();points_=std::move(pending);++modelRevision_;manualRecord_="null";
            for(const auto& point:points_)nextPointNumber_=std::max(nextPointNumber_,point.number+1);
            markersVisible_=true;checked(scene_->Redraw());
            Log(L"Replacement model captured. Connect each photo correspondence to a vertex on this model.");
        }
        else if(command=="pick")Pick();
        else if(command=="rebind_point"){
            auto id=args.at("id").get<std::string>();size_t index=points_.size();
            for(size_t i=0;i<points_.size();++i)if(id=="P"+std::to_string(points_[i].number))index=i;
            if(index==points_.size())throw std::runtime_error("Unknown correspondence");Pick();replacePoint_=index;
        }
        else if(command=="clear_points"){
            if(args.at("capture_id").get<unsigned long>()!=captureId_)throw std::runtime_error("Capture changed; refresh before clearing points");
            if(adjusting_)throw std::runtime_error("Save or cancel camera adjustment first");
            if(test_||overlay_.GetSafeHwnd())ClosePhoto();
            StopPicking();points_.clear();nextPointNumber_=1;++captureId_;++modelRevision_;
            sampleRecords_.clear();measuring_=false;manualCamera_=false;manualRecord_="null";
            markersVisible_=true;if(scene_)checked(scene_->Redraw());
            Log(L"All point pairs cleared; numbering restarts at P1.");
        }
        else if(command=="delete_point"){
            if(args.at("capture_id").get<unsigned long>()!=captureId_)throw std::runtime_error("Capture changed; refresh before deleting a point");
            if(adjusting_)throw std::runtime_error("Save or cancel camera adjustment first");
            const auto number=pointNumber(args.at("id").get<std::string>());
            auto point=std::find_if(points_.begin(),points_.end(),[&](const PickedPoint& p){return p.number==number;});
            if(point==points_.end())throw std::runtime_error("Unknown correspondence");
            if(test_||overlay_.GetSafeHwnd())ClosePhoto();
            StopPicking();points_.erase(point);++modelRevision_;sampleRecords_.clear();measuring_=false;manualRecord_="null";
            if(points_.empty()){nextPointNumber_=1;++captureId_;}
            markersVisible_=true;if(scene_)checked(scene_->Redraw());
            Log(L"Selected point pair deleted; remaining point numbers retained.");
        }
        else if(command=="stop_pick")StopPicking();
        else if(command=="restore")Restore();
        else if(command=="close_photo")ClosePhoto();
        else if(command=="photo_opacity"){
            const double value=args.at("opacity").get<double>();
            if(!std::isfinite(value)||value<0||value>1)throw std::runtime_error("Photo opacity must be between 0 and 1");
            overlay_.opacity=static_cast<BYTE>(std::round(value*255));UpdateOverlay();
        }
        else if(command=="adjust_camera")AdjustCamera(args.at("action").get<std::string>(),args);
        else if(command=="apply"){
            auto vec=[&](const char* key){auto a=args.at(key).get<std::vector<double>>();if(a.size()!=3)throw std::runtime_error("Expected three coordinates");return Vec3{a[0],a[1],a[2]};};
            Apply(Camera{vec("position"),vec("direction"),vec("up"),args.at("field_sdk").get<double>()});
        }else if(command=="measure"){CheckContext();measuring_=true;checked(scene_->Redraw());}
        else if(command=="photo"){auto path=args.at("path").get<std::string>();Photo(CString(CA2W(path.c_str(),CP_UTF8)),args.at("focal_px").get<double>(),args.value("principal_px",json()));}
        else if(command=="background"){auto path=args.at("path").get<std::string>();Background(CString(CA2W(path.c_str(),CP_UTF8)));}
        else if(command!="status")throw std::runtime_error("Unknown command");
        response["state"]=Snapshot();if(!fixture.is_null())response["state"]["coordinate_fixture"]=fixture;response["ok"]=true;
    }catch(const _com_error& e){CString msg;msg.Format(L"COM error 0x%08X",unsigned(e.Error()));response["error"]=utf8(msg);Log(msg);}
    catch(const std::exception& e){response["error"]=e.what();Log(CString(CA2W(e.what(),CP_UTF8)));}
    catch(CException* e){wchar_t msg[512]={};e->GetErrorMessage(msg,512);e->Delete();response["error"]=utf8(msg);Log(msg);}
    catch(...){response["error"]="Host operation failed";}
    if(!response["ok"].get<bool>())errors_.push_back(response.value("error",std::string("Host operation failed")));
    return response.dump();
}
void HostSession::LaunchGui() {
    // Python forwards repeat launches to the existing GUI, including across host restarts.
    if(guiProcess_){CloseHandle(guiProcess_);guiProcess_=nullptr;}
    wchar_t module[MAX_PATH]={};GetModuleFileNameW(_Module.GetModuleInstance(),module,MAX_PATH);
    CString bin(module);bin=bin.Left(bin.ReverseFind(L'\\'));
    CString root=bin.Left(bin.ReverseFind(L'\\'))+L"\\ICAPI\\PhotoMatchProto";
    CString python=root+L"\\.venv\\Scripts\\pythonw.exe",app=root+L"\\gui\\app.py";
    CString command;command.Format(L"\"%s\" \"%s\" --host %lu",python.GetString(),app.GetString(),GetCurrentProcessId());
    CString packaged=bin+L"\\PhotoMatch\\PhotoMatch.exe";
    if(GetFileAttributesW(packaged)!=INVALID_FILE_ATTRIBUTES){
        python=packaged;root=bin+L"\\PhotoMatch";
        command.Format(L"\"%s\" --host %lu",python.GetString(),GetCurrentProcessId());
    }
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    BOOL ok=CreateProcessW(python,command.GetBuffer(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root,&startup,&process);command.ReleaseBuffer();
    if(!ok)throw std::runtime_error("PhotoMatch GUI could not start. Install the end-user package or run scripts/setup-gui.ps1.");
    CloseHandle(process.hThread);guiProcess_=process.hProcess;
}
