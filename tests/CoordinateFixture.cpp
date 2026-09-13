#include "../src/HostSession.h"
using namespace photomatch;

// Explicit diagnostic command, never invoked by the product GUI. All geometry
// is created in a new scene; no existing document geometry is changed.
nlohmann::json HostSession::CoordinateFixture() {
    using nlohmann::json;
    if(captured_)Restore();
    IZDocPtr created;checked(app_->CreateNewDoc(Z_SCENE,VARIANT_FALSE,VARIANT_TRUE,CComBSTR(L""),VARIANT_TRUE,&created));
    checked(app_->put_ActiveDoc(created));SyncDocument();
    IZSceneDocPtr scene=created;if(!scene)throw std::runtime_error("Fixture requires a new scene");
    long count=0;checked(scene->GetChildrenElementsCount(&count));
    if(count)throw std::runtime_error("Fixture template must contain no model elements");
    IZPartPtr single,nested;
    // CreateBlockPart returns E_NOTIMPL in SDK 29.0.2.20605. Use the
    // profile/extrusion path demonstrated by the SDK SelectionTool sample.
    auto makeBox=[&](){
        IZProfilePtr profile;checked(scene->CreateProfile(&profile));
        std::array<Vec3,4> corners={Vec3{-.02,-.01,0},Vec3{.02,-.01,0},Vec3{.02,.01,0},Vec3{-.02,.01,0}};
        auto xy=[](Vec3 p){CComVariant v;v.vt=VT_ARRAY|VT_R8;v.parray=SafeArrayCreateVector(VT_R8,0,2);
            double* data=nullptr;checked(SafeArrayAccessData(v.parray,reinterpret_cast<void**>(&data)));
            data[0]=p.x;data[1]=p.y;checked(SafeArrayUnaccessData(v.parray));return v;};
        for(long i=0;i<4;++i){auto a=xy(corners[i]),b=xy(corners[(i+1)%4]);long id=0;checked(profile->CreateLine(a,b,i+1,&id));}
        IZPartPtr part;checked(scene->CreatePart(&part));IZPartFeatureMgrPtr features=part;
        IZExtrudeFeaturePtr block;checked(features->CreateExtrudeFeature(Z_UNITE,VARIANT_FALSE,.015,.015,0,profile,Z_FEATURE_PROFILE_ABSORB,&block));
        checked(part->Update());return part;
    };
    single=makeBox();nested=makeBox();
    IZElementPtr singleElement=single,nestedElement=nested;
    checked(singleElement->put_Name(CComBSTR(L"PhotoMatch single 40x20x30 mm")));
    checked(nestedElement->put_Name(CComBSTR(L"PhotoMatch nested 40x20x30 mm")));
    IZAssemblyPtr outer,inner;checked(scene->CreateAssembly(&outer));checked(outer->CreateSubAssembly(&inner));
    checked(inner->AddChild(nestedElement));
    auto setMatrix=[](IZSceneElement* element,std::array<double,16> data){
        IZMathMatrixPtr matrix;checked(matrix.CreateInstance(__uuidof(ZMathMatrix)));
        checked(matrix->SetDataCOM(data.data()));checked(element->SetTransformToParent(matrix));
    };
    IZSceneElementPtr singleScene=single,nestedScene=nested,innerScene=inner,outerScene=outer;
    setMatrix(singleScene,{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
    setMatrix(nestedScene,{0,0,-1,0, 0,1,0,0, 1,0,0,0, .005,.003,.002,1});
    setMatrix(innerScene,{1,0,0,0, 0,0,1,0, 0,-1,0,0, .02,.01,.03,1});
    setMatrix(outerScene,{0,1,0,0, -1,0,0,0, 0,0,1,0, .1,.2,.3,1});
    checked(outer->Update());checked(scene->Redraw());
    Capture();
    json report={{"tolerance_model_units",1e-9},{"box_size_model_units",{.04,.02,.03}},
                 {"expected_nested_mapping","(.092-x, .225+z, .333+y)"},{"cases",json::array()}};
    for(int which=0;which<2;++which){
        IZPartPtr part=which?nested:single;IZElementPtr element=part;
        CComVariant bodies;checked(part->GetBodies(VARIANT_TRUE,&bodies));auto list=items(bodies);
        if(list.size()!=1)throw std::runtime_error("Fixture box must have one body");
        IZBodyPtr body=unknown(list[0]);IZBodyVertexPtr vertices=body;
        CComVariant ids;checked(body->GetVertexIds(&ids));auto vertexIds=items(ids);
        if(vertexIds.size()!=8)throw std::runtime_error("Fixture box must have eight vertices");
        IZSceneElementPtr elementScene=part;IZMathMatrixPtr global;checked(elementScene->GetTransformToGlobal(&global));
        json rows=json::array();double maxError=0,cornerError=0;
        for(auto id:vertexIds){
            checked(id.ChangeType(VT_I4));CComVariant value;checked(vertices->GetPosition(id.lVal,&value));
            Vec3 local=vector3(value),actual=transform(local,global);
            Vec3 expected=which?Vec3{.092-local.x,.225+local.z,.333+local.y}:local;
            Vec3 delta=sub(actual,expected);double error=std::sqrt(dot(delta,delta));maxError=std::max(maxError,error);
            cornerError=std::max(cornerError,std::max({std::abs(std::abs(local.x)-.02),std::abs(std::abs(local.y)-.01),std::abs(std::abs(local.z)-.015)}));
            rows.push_back({{"vertex_id",id.lVal},{"local",json::parse(jsonVec(local))},
                {"expected_global",json::parse(jsonVec(expected))},{"actual_global",json::parse(jsonVec(actual))},{"error",error}});
        }
        // Exercise the same ID-based coordinate retrieval used by vertex events.
        Select(element,nullptr,Z_ENTITY_VERTEX,ids);
        report["cases"].push_back({{"name",which?"nested_rotations_and_translations":"single_identity"},
            {"vertices",rows},{"max_global_error",maxError},{"max_local_corner_error",cornerError},
            {"passed",maxError<=1e-9&&cornerError<=1e-9}});
    }
    report["passed"]=report["cases"][0]["passed"].get<bool>()&&report["cases"][1]["passed"].get<bool>();
    return report;
}
