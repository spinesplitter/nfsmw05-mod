#include "../src/custom_driver.h"
#include "../src/custom_driver_animation.h"
#include <cstdlib>
#define Check(ok) do { if(!(ok)) { printf("FAILED at line %d: %s\n",__LINE__,#ok); std::exit(1); } } while(0)
int main() {
    auto twisted=CustomDriver::Twist({0,1,0},{1,0,0},3.141592653589793f/2);
    Check(std::fabs(twisted.y)<1e-6f && std::fabs(twisted.z-1)<1e-6f);
    CustomDriver::AnimationRig seam;
    for(int d=160;d<=200;d+=10) {
        float a=d*3.141592653589793f/180;
        seam.paths.push_back({0,std::cos(a),std::sin(a)});
        seam.paths.push_back({0,-std::cos(a),-std::sin(a)});
    }
    for(int side=0;side<2;side++) {
        Check(std::fabs(seam.GripRoll(0,side))<1e-6f);
        Check(std::fabs(seam.GripRoll(-450,side)-20*3.141592653589793f/180)<1e-5f);
    }
    CustomDriver::Asset asset;
    CustomDriver::AnimationRig rig;
    Check(asset.Load("..\\..\\drivermod\\driver-runtime.bin"));
    Check(rig.Load("..\\..\\drivermod\\driver-animation.bin",asset.vertices.size()));
    Check(rig.Path(0,0).y>0.99f && rig.Path(0,1).y< -0.99f);
    CustomDriver::Controls c;
    c.wheel=true; c.radius=0.16f; c.centre={0.55f,0,-0.20f};
    c.shifter={0.35f,-0.25f,-0.45f}; c.handbrake={0.25f,-0.25f,-0.5f};
    // Every sampled source path survives fitting, including release/regrip at
    // large angles. Only wheel-plane displacement scales; axial depth does not.
    for (int d=-450;d<=450;d++) for (int side=0;side<2;side++) {
        c.degrees=float(d);
        auto p=rig.Path(c.degrees,side);
        for (float spread : {0.5f,1.0f,1.5f}) {
            c.handSpread=spread;
            auto target=rig.SteeringTarget(c,side)-c.centre;
            Check(std::fabs(target.x-p.x*c.radius)<1e-6f);
            Check(std::fabs(target.y-p.y*c.radius*spread)<1e-6f);
            Check(std::fabs(target.z-p.z*c.radius*spread)<1e-6f);
        }
    }
    c.handSpread=1;
    std::vector<CustomDriver::Vertex> a,b,previous;
    for(int d=-450;d<=450;d++) {
        c.degrees=float(d);
        rig.Pose(asset.vertices,a,c); rig.Pose(asset.vertices,b,c);
        Check(a.size()==asset.vertices.size());
        for(size_t i=0;i<a.size();i++) {
            Check(std::isfinite(a[i].x) && std::isfinite(a[i].y) && std::isfinite(a[i].z));
            Check(a[i].x==b[i].x && a[i].y==b[i].y && a[i].z==b[i].z);
            Check(a[i].u==asset.vertices[i].u && a[i].v==asset.vertices[i].v);
            if(!previous.empty()) Check(CustomDriver::Length({a[i].x-previous[i].x,a[i].y-previous[i].y,a[i].z-previous[i].z})<0.10f);
        }
        for(int side=0;side<2;side++) {
            auto poses=rig.rest;
            rig.Solve(poses,side,{10,2,3},{1,0,0});
            for(int j=0;j<3;j++) {
                auto i=rig.arms[side*3+j];
                Check(std::fabs(CustomDriver::Length(poses[i].tail-poses[i].head)-CustomDriver::Length(rig.rest[i].tail-rig.rest[i].head))<1e-4f);
                if(j<2) Check(CustomDriver::Length(poses[i].tail-poses[rig.arms[side*3+j+1]].head)<1e-6f);
            }
        }
        previous=a;
    }
    c.degrees=0; rig.Pose(asset.vertices,a,c);
    c.hasShifter=true; c.shift=1; rig.Pose(asset.vertices,b,c);
    float motion=0;
    for(size_t i=0;i<a.size();i++) motion+=std::fabs(a[i].x-b[i].x)+std::fabs(a[i].z-b[i].z);
    Check(motion>0.1f);
    c.hasHandbrake=true; c.brake=1; rig.Pose(asset.vertices,b,c);
    for(auto v:b) Check(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z));
    Check(!rig.Load("missing-animation.bin",asset.vertices.size()));
    puts("PASS: rig payload, 901 steering poses, held-input determinism, UV preservation, continuity, unreachable IK bone lengths/connections, shift and handbrake");
}