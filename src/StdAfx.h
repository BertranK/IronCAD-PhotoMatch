#pragma once
#define NOMINMAX
#define _ATL_APARTMENT_THREADED
#include <afxwin.h>
#include <afxdlgs.h>
#include <afxdisp.h>
#include <algorithm>
namespace Gdiplus {using std::min;using std::max;}
#include <atlbase.h>
#include <atlcom.h>
#include <atlimage.h>
#include <comdef.h>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>
#include <functional>
#include <memory>
#pragma warning(push)
#pragma warning(disable:4192 4278)
#import <ICApiIronCAD.tlb> no_namespace, named_guids, raw_interfaces_only, raw_native_types
#pragma warning(pop)
#include "../core/Projection.h"
extern CComModule _Module;
inline void checkedApi(HRESULT hr,const char* operation) {
    if(FAILED(hr)) {
        std::ostringstream message;
        message<<operation<<" failed (COM 0x"<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill('0')<<static_cast<unsigned long>(hr)<<')';
        throw std::runtime_error(message.str());
    }
}
#define checked(operation) checkedApi((operation),#operation)
inline bool sameObject(IUnknown* a,IUnknown* b) {
    if(!a||!b) return a==b;
    CComPtr<IUnknown> x,y; checked(a->QueryInterface(IID_PPV_ARGS(&x))); checked(b->QueryInterface(IID_PPV_ARGS(&y)));
    return x==y;
}
inline CComVariant xyz(photomatch::Vec3 p) {
    CComVariant v; v.vt=VT_ARRAY|VT_R8; v.parray=SafeArrayCreateVector(VT_R8,0,3);
    if(!v.parray) _com_issue_error(E_OUTOFMEMORY);
    double data[]={p.x,p.y,p.z}; for(LONG i=0;i<3;++i) checked(SafeArrayPutElement(v.parray,&i,&data[i]));
    return v;
}
inline std::vector<CComVariant> items(const VARIANT& value) {
    if(!(value.vt&VT_ARRAY)||!value.parray||SafeArrayGetDim(value.parray)!=1) throw std::runtime_error("Expected one-dimensional SAFEARRAY");
    LONG lo=0,hi=-1;checked(SafeArrayGetLBound(value.parray,1,&lo));checked(SafeArrayGetUBound(value.parray,1,&hi));
    std::vector<CComVariant> result;
    for(LONG i=lo;i<=hi;++i) {
        CComVariant v; VARTYPE type=value.vt&VT_TYPEMASK;
        if(type==VT_VARIANT) checked(SafeArrayGetElement(value.parray,&i,&v));
        else {v.vt=type;switch(type){
            case VT_R8:checked(SafeArrayGetElement(value.parray,&i,&v.dblVal));break;
            case VT_R4:checked(SafeArrayGetElement(value.parray,&i,&v.fltVal));break;
            case VT_I4:checked(SafeArrayGetElement(value.parray,&i,&v.lVal));break;
            case VT_UNKNOWN:checked(SafeArrayGetElement(value.parray,&i,&v.punkVal));break;
            case VT_DISPATCH:checked(SafeArrayGetElement(value.parray,&i,&v.pdispVal));break;
            default:throw std::runtime_error("Unsupported SAFEARRAY type");
        }} result.push_back(v);
    } return result;
}
inline photomatch::Vec3 vector3(const VARIANT& value) {
    auto a=items(value); if(a.size()!=3) throw std::runtime_error("Expected three coordinates");
    for(auto& v:a) checked(v.ChangeType(VT_R8));
    photomatch::Vec3 p{a[0].dblVal,a[1].dblVal,a[2].dblVal};
    if(!photomatch::finite(p)) throw std::runtime_error("Non-finite coordinates");return p;
}
inline IUnknown* unknown(const VARIANT& v) {
    if(v.vt==VT_UNKNOWN&&v.punkVal)return v.punkVal;
    if(v.vt==VT_DISPATCH&&v.pdispVal)return v.pdispVal;
    throw std::runtime_error("Expected COM object");
}
inline photomatch::Vec3 pointData(IZMathPoint* p) {CComVariant v;checked(p->get_Data(&v));return vector3(v);}
inline photomatch::Vec3 transform(photomatch::Vec3 p,IZMathMatrix* matrix) {
    IZMathPointPtr point;checked(point.CreateInstance(__uuidof(ZMathPoint)));checked(point->put_Data(xyz(p)));
    checked(point->TransformBy(matrix));return pointData(point);
}
inline std::string utf8(const wchar_t* text) { CW2A s(text?text:L"",CP_UTF8);return std::string(s); }
inline std::string quote(const std::string& value) {
    std::ostringstream out;out<<'"';for(unsigned char c:value){switch(c){case '"':out<<"\\\"";break;case '\\':out<<"\\\\";break;
        case '\n':out<<"\\n";break;case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;
        default:if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;else out<<char(c);}}
    out<<'"';return out.str();
}
inline std::string jsonVec(photomatch::Vec3 p) {std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<'['<<p.x<<','<<p.y<<','<<p.z<<']';return o.str();}
