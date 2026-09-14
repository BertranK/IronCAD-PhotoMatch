#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace photomatch {
struct Vec3 { double x, y, z; };
struct Pixel { double x, y; };
struct Rect { double x, y, w, h; };
// A floating model projection is usable as world output only after checking
// the current render frame against analytical and integer-world projections.
inline bool floatingProjectionAgrees(Pixel value,Pixel worldInteger,Pixel analytical,double sx,double sy) {
    if(!(sx>0&&sy>0)||!std::isfinite(sx)||!std::isfinite(sy)
        ||!std::isfinite(value.x)||!std::isfinite(value.y)
        ||!std::isfinite(worldInteger.x)||!std::isfinite(worldInteger.y)
        ||!std::isfinite(analytical.x)||!std::isfinite(analytical.y))return false;
    return std::hypot(value.x-analytical.x,value.y-analytical.y)<=0.01
        &&std::abs(std::trunc(value.x/sx)*sx-worldInteger.x)<1e-6
        &&std::abs(std::trunc(value.y/sy)*sy-worldInteger.y)<1e-6;
}
inline Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
inline Vec3 unit(Vec3 a) {
    double n=std::sqrt(dot(a,a));
    if (!finite(a)||n<1e-12) throw std::runtime_error("Invalid camera basis");
    return {a.x/n,a.y/n,a.z/n};
}
struct Camera { Vec3 position, direction, up; double field; };
// COM camera setters normalize vectors and can change their final floating-point bits.
inline bool sameCameraValue(double a,double b) {
    return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=16*std::numeric_limits<double>::epsilon()*(std::max)(1.0,(std::max)(std::abs(a),std::abs(b)));
}
inline bool sameCameraValue(Vec3 a,Vec3 b) {
    return sameCameraValue(a.x,b.x)&&sameCameraValue(a.y,b.y)&&sameCameraValue(a.z,b.z);
}
inline bool sameCameraValue(const Camera& a,const Camera& b) {
    return sameCameraValue(a.position,b.position)&&sameCameraValue(a.direction,b.direction)&&sameCameraValue(a.up,b.up)&&sameCameraValue(a.field,b.field);
}
enum class Axis { Horizontal, Vertical, Minimum, Maximum };
struct Convention { Axis axis; bool degrees; bool halfAngle; };
inline std::string name(Convention c) {
    const char* axes[]={"horizontal","vertical","minimum","maximum"};
    return std::string(axes[int(c.axis)])+(c.degrees?"_degrees":"_radians")+(c.halfAngle?"_half":"_full");
}
inline double extent(Convention c, double w, double h) {
    switch(c.axis) {case Axis::Horizontal:return w; case Axis::Vertical:return h;
        case Axis::Minimum:return (std::min)(w,h); default:return (std::max)(w,h);}
}
inline double fullRadians(double field, Convention c) {
    return field*(c.degrees?3.14159265358979323846/180.0:1.0)*(c.halfAngle?2.0:1.0);
}
inline Pixel project(Vec3 point, Camera camera, Convention convention, double w, double h) {
    double angle=fullRadians(camera.field,convention);
    if(w<=0||h<=0||!std::isfinite(angle)||angle<=0||angle>=3.14159265358979323846)
        throw std::runtime_error("Invalid viewport or field of view");
    Vec3 f=unit(camera.direction), r=unit(cross(f,camera.up)), u=unit(cross(r,f));
    Vec3 d=sub(point,camera.position); double depth=dot(d,f);
    if(!finite(point)||depth<=1e-12) throw std::runtime_error("Point is behind camera");
    double focal=extent(convention,w,h)/(2*std::tan(angle/2));
    return {w/2+focal*dot(d,r)/depth,h/2-focal*dot(d,u)/depth};
}
// IronCAD's SDK view transform uses pixel endpoints, then truncates in render pixels.
// Keep render pixels separate from physical pixels when Windows virtualizes the host DPI.
inline Pixel projectSdk(Vec3 point,Camera camera,Convention convention,double rw,double rh,double pw,double ph,bool integer=true) {
    if(rw<=2||rh<=2||pw<=0||ph<=0)throw std::runtime_error("Invalid render extent");
    Pixel p=project(point,camera,convention,rw-1,rh-1);
    p.x*=(rw-2)/(rw-1);p.y*=(rh-2)/(rh-1);
    if(integer){p.x=std::trunc(p.x);p.y=std::trunc(p.y);}
    return {p.x*pw/rw,p.y*ph/rh};
}
inline Rect sdkImageRect(double iw,double ih,double rw,double rh,double pw,double ph,Pixel principal) {
    if(iw<=0||ih<=0||rw<=2||rh<=2||pw<=0||ph<=0)throw std::runtime_error("Invalid image viewport");
    if(!std::isfinite(principal.x)||!std::isfinite(principal.y))throw std::runtime_error("Invalid image principal point");
    double w=(rw-2)*pw/rw,h=(rh-2)*ph/rh;
    double spanX=2*(std::max)(std::abs(principal.x),std::abs(iw-principal.x));
    double spanY=2*(std::max)(std::abs(principal.y),std::abs(ih-principal.y));
    double s=(std::min)(w/spanX,h/spanY);
    return {w/2-principal.x*s,h/2-principal.y*s,iw*s,ih*s};
}
inline Rect sdkImageRect(double iw,double ih,double rw,double rh,double pw,double ph) {
    return sdkImageRect(iw,ih,rw,rh,pw,ph,{iw/2,ih/2});
}
inline double sdkImageField(double focal,double iw,double ih,double rw,double rh,double pw,double ph,Convention c,Pixel principal) {
    Rect box=sdkImageRect(iw,ih,rw,rh,pw,ph,principal);
    if(!std::isfinite(focal)||focal<=0)throw std::runtime_error("Invalid focal length");
    double kx=(rw-2)/(rw-1)*pw/rw,ky=(rh-2)/(rh-1)*ph/rh;
    // Uniform image scaling; minimize corner distance from SDK endpoint anisotropy.
    double f=focal*box.w/iw*(iw*iw*kx+ih*ih*ky)/(iw*iw*kx*kx+ih*ih*ky*ky);
    double rad=2*std::atan(extent(c,rw-1,rh-1)/(2*f));
    return rad/(c.halfAngle?2:1)/(c.degrees?3.14159265358979323846/180:1);
}
inline double sdkImageField(double focal,double iw,double ih,double rw,double rh,double pw,double ph,Convention c) {
    return sdkImageField(focal,iw,ih,rw,rh,pw,ph,c,{iw/2,ih/2});
}
inline Rect contain(double iw,double ih,double vw,double vh) {
    if(!(iw>0&&ih>0&&vw>0&&vh>0)) throw std::runtime_error("Invalid image dimensions");
    double s=(std::min)(vw/iw,vh/ih);
    return {(vw-iw*s)/2,(vh-ih*s)/2,iw*s,ih*s};
}
inline double imageField(double focal,double iw,double ih,double vw,double vh,Convention c) {
    if(!std::isfinite(focal)||focal<=0) throw std::runtime_error("Invalid focal length");
    Rect box=contain(iw,ih,vw,vh);
    double rad=2*std::atan(extent(c,vw,vh)/(2*focal*box.w/iw));
    return rad/(c.halfAngle?2:1)/(c.degrees?3.14159265358979323846/180:1);
}
struct Observation { Camera camera; Vec3 world; Pixel actual; double width,height; double renderWidth=0,renderHeight=0; };
struct Fit { Convention convention; double maxError; };
inline std::vector<Fit> fit(const std::vector<Observation>& observations) {
    std::vector<Fit> fits;
    if(observations.empty()) return fits;
    for(int a=0;a<4;++a) for(int d=0;d<2;++d) for(int h=0;h<2;++h) {
        Convention c{Axis(a),d!=0,h!=0}; double error=0;
        try { for(const auto& o:observations) {
            Pixel p=o.renderWidth>0?projectSdk(o.world,o.camera,c,o.renderWidth,o.renderHeight,o.width,o.height):project(o.world,o.camera,c,o.width,o.height);
            if(!std::isfinite(o.actual.x)||!std::isfinite(o.actual.y)) throw std::runtime_error("Invalid observation");
            error=(std::max)(error,std::hypot(p.x-o.actual.x,p.y-o.actual.y));
        }} catch(const std::exception&) {error=std::numeric_limits<double>::infinity();}
        fits.push_back({c,error});
    }
    std::sort(fits.begin(),fits.end(),[](const Fit& a,const Fit& b){return a.maxError<b.maxError;});
    return fits;
}
inline bool resolved(const std::vector<Fit>& fits) {
    return fits.size()>1&&fits[0].maxError<=1.0&&fits[1].maxError>1.0;
}
}
