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
