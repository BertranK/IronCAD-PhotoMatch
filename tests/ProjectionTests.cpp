#include "../core/Projection.h"
#include <iostream>
#include <cstdlib>
using namespace photomatch;
void check(bool ok,const char* message) { if(!ok) {std::cerr<<message<<'\n';std::exit(1);} }
int main() {
    check(sameCameraValue(Vec3{0.6743378971633767,0.6659468811939842,-0.3190347189214382},Vec3{0.6743378971633766,0.665946881193984,-0.31903471892143814}),"Accept observed COM camera normalization roundoff");
    check(!sameCameraValue(0.3711721030859038,0.3711721031859038),"Reject changed camera field");
    check(!sameCameraValue(1.6329955485418048,1.6329955585418048),"Reject changed camera position");
    check(!sameCameraValue(std::numeric_limits<double>::quiet_NaN(),0.0),"Reject invalid camera value");
    // Captured SDK output, independent of the implementation under test (150% Windows).
    const Pixel recorded[2][8]={{{362,415},{544,387},{417,292},{580,251},{398,387},{525,374},{362,251},{544,279}},
                               {{193,403},{348,380},{239,298},{379,263},{224,380},{333,368},{193,263},{348,286}}};
    Camera sdkCamera{{0,0,0},{0,0,1},{0,-1,0},1.1};Convention sdkConvention{Axis::Minimum,false,false};
    std::vector<Observation> sdkObservations;
    for(int view=0;view<2;++view)for(int i=0;i<8;++i){
        double rw=view?575:945,rh=669;Vec3 q{(i&1)?2.0:-2.0,(i&2)?-1.5:1.5,10.0+5*(i%3)};
        for(double scale:std::vector<double>{2.0/3,1.0,1.5,2.0}){
            Pixel expected{recorded[view][i].x*scale,recorded[view][i].y*scale};
            auto actual=projectSdk(q,sdkCamera,sdkConvention,rw,rh,rw*scale,rh*scale);
            check(std::hypot(actual.x-expected.x,actual.y-expected.y)<1e-9,"SDK raster and physical DPI mapping");
            sdkObservations.push_back({sdkCamera,q,expected,rw*scale,rh*scale,rw,rh});
        }
    }
    check(resolved(fit(sdkObservations)),"SDK raster convention resolves across aspects and DPI virtualization");
    for(auto viewport:std::vector<Pixel>{{945,669},{575,669},{3000,500},{500,3000}})
      for(auto image:std::vector<Pixel>{{1600,1000},{750,750},{4000,500}})for(double scale:std::vector<double>{2.0/3,1.0,1.5,2.0}){
        auto box=sdkImageRect(image.x,image.y,viewport.x,viewport.y,viewport.x*scale,viewport.y*scale);
        check(std::abs(box.w/box.h-image.x/image.y)<1e-9,"Photo aspect ratio is preserved");
        auto camera=sdkCamera;camera.field=sdkImageField(1000,image.x,image.y,viewport.x,viewport.y,viewport.x*scale,viewport.y*scale,sdkConvention);
        for(Pixel source:std::vector<Pixel>{{0,0},{image.x,0},{image.x,image.y},{0,image.y},{image.x/2,image.y/2}}){
            Vec3 world{(source.x-image.x/2)/1000, (source.y-image.y/2)/1000,1};
            auto actual=projectSdk(world,camera,sdkConvention,viewport.x,viewport.y,viewport.x*scale,viewport.y*scale,false);
            Pixel expected{box.x+source.x*box.w/image.x,box.y+source.y*box.h/image.y};
            check(std::hypot(actual.x-expected.x,actual.y-expected.y)<=1.0,"Photo corners stay within one physical pixel across resize and DPI");
        }
    }
    Camera c{{0,0,0},{0,0,1},{0,-1,0},60};
    Convention v{Axis::Vertical,true,false};
    Pixel p=project({0,0,10},c,v,1200,800);
    check(p.x==600&&p.y==400,"Principal point");
    p=project({1,1,10},c,v,1200,800);
    check(p.x>600&&p.y>400,"Right and down convention");
    std::vector<Observation> obs;
    for(auto size:std::vector<Pixel>{{1200,800},{800,1200}})
        for(Vec3 q:std::vector<Vec3>{{1,1,10},{-2,3,15},{3,-2,20}})
            obs.push_back({c,q,project(q,c,v,size.x,size.y),size.x,size.y});
    auto fits=fit(obs);
    check(resolved(fits)&&name(fits[0].convention)==name(v),"Identify FOV across aspect ratios");
    obs.resize(3); check(!resolved(fit(obs)),"Single aspect ratio must remain ambiguous");
    check(fit({}).empty(),"No observations must not pass");
    Rect box=contain(1600,900,800,800);
    check(box.w==800&&box.h==450&&box.y==175,"Letterbox");
    for(auto size:std::vector<Pixel>{{1200,800},{800,1200}}) {
        Rect b=contain(1600,900,size.x,size.y);
        c.field=imageField(1000,1600,900,size.x,size.y,v);
        Pixel projected=project({1,0,10},c,v,size.x,size.y);
        check(std::abs(projected.x-(b.x+900*b.w/1600))<1e-9,"Resize preserves image registration");
    }
    bool rejected=false;try{project({0,0,-1},c,v,800,800);}catch(...){rejected=true;}
    check(rejected,"Reject behind-camera point");
    c.up=c.direction;rejected=false;try{project({0,0,5},c,v,800,800);}catch(...){rejected=true;}
    check(rejected,"Reject parallel up vector");
    // Check every supported FOV convention using an independent pinhole formula.
    for(int axis=0;axis<4;++axis) for(int degrees=0;degrees<2;++degrees) for(int half=0;half<2;++half) {
        Convention target{Axis(axis),degrees!=0,half!=0};
        Camera cam{{0,0,0},{0,0,1},{0,-1,0},(degrees?60.0:3.14159265358979323846/3)/(half?2:1)};
        std::vector<Observation> samples;
        for(auto wh:std::vector<Pixel>{{1400,700},{700,1400}}) {
            double referenceExtent=axis==0?wh.x:axis==1?wh.y:axis==2?700:1400;
            double focal=referenceExtent*std::sqrt(3.0)/2;
            for(Vec3 q:std::vector<Vec3>{{2,1,10},{-3,2,18},{1,-3,20}})
                samples.push_back({cam,q,{wh.x/2+focal*q.x/q.z,wh.y/2+focal*q.y/q.z},wh.x,wh.y});
        }
        auto fitted=fit(samples);
        check(resolved(fitted)&&name(fitted[0].convention)==name(target),"All FOV modes identified independently");
        samples[0].actual.x+=20;
        check(!resolved(fit(samples)),"Outlier must prevent a false pass");
    }
    std::cout<<"Projection tests passed\n";
}
