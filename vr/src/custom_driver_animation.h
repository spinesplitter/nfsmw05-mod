#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace CustomDriver {
struct Point {
    float x=0, y=0, z=0;
    Point operator+(Point b) const { return {x+b.x,y+b.y,z+b.z}; }
    Point operator-(Point b) const { return {x-b.x,y-b.y,z-b.z}; }
    Point operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline float Dot(Point a, Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Point Cross(Point a, Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float Length(Point a) { return std::sqrt(Dot(a,a)); }
inline Point Unit(Point a, Point fallback={1,0,0}) { float n=Length(a); return n>1e-7f ? a*(1/n) : fallback; }
inline Point Mix(Point a, Point b, float t) { return a*(1-t)+b*t; }
inline Point Twist(Point p, Point axis, float radians) {
    axis=Unit(axis);
    float c=std::cos(radians), s=std::sin(radians);
    return p*c+Cross(axis,p)*s+axis*(Dot(axis,p)*(1-c));
}
inline Point Rotate(Point p, Point from, Point to) {
    from=Unit(from); to=Unit(to);
    float c=std::max(-1.0f,std::min(1.0f,Dot(from,to)));
    Point v=Cross(from,to);
    if(c < -0.9999f) { Point axis=Unit(Cross(from,std::fabs(from.z)<0.9f ? Point{0,0,1}:Point{0,1,0})); return axis*(2*Dot(axis,p))-p; }
    return p+Cross(v,p)+Cross(v,Cross(v,p))*(1/(1+c));
}
// Prepared operators retain Rotate/Twist's arithmetic and degeneracy rules.
// Prepare once per bone/pose, rather than normalizing and taking trig per vertex.
struct PreparedRotation {
    Point cross, axis;
    float reciprocal=0;
    bool opposite=false;
    PreparedRotation(Point from, Point to) {
        from=Unit(from); to=Unit(to);
        float c=std::max(-1.0f,std::min(1.0f,Dot(from,to)));
        cross=Cross(from,to);
        opposite=c < -0.9999f;
        if(opposite) axis=Unit(Cross(from,std::fabs(from.z)<0.9f ? Point{0,0,1}:Point{0,1,0}));
        else reciprocal=1/(1+c);
    }
    Point Apply(Point p) const {
        if(opposite) return axis*(2*Dot(axis,p))-p;
        return p+Cross(cross,p)+Cross(cross,Cross(cross,p))*reciprocal;
    }
};
struct PreparedTwist {
    Point axis;
    float cosine, sine;
    PreparedTwist(Point direction, float radians) : axis(Unit(direction)),
        cosine(std::cos(radians)), sine(std::sin(radians)) {}
    Point Apply(Point p) const {
        return p*cosine+Cross(axis,p)*sine+axis*(Dot(axis,p)*(1-cosine));
    }
};
struct Controls {
    Point centre, axis{1,0,0}, shifter, handbrake;
    float radius=0, degrees=0, shift=0, brake=0;
    float handSpread=1;
    bool wheel=false, hasShifter=false, hasHandbrake=false;
};
inline void GateUnselectedControls(Controls& controls, bool shifterSelected, bool handbrakeSelected) {
    if (!shifterSelected) {
        controls.hasShifter = false;
        controls.shift = 0;
    }
    if (!handbrakeSelected) {
        controls.hasHandbrake = false;
        controls.brake = 0;
    }
}
struct BonePose { Point head, tail; };
struct Weight { uint32_t bone; float weight; Point offset; };
struct AnimationRig {
    std::vector<BonePose> rest;
    std::vector<std::vector<Weight>> weights;
    std::vector<Point> paths;
    uint32_t arms[6]={}; // upper arm, forearm, hand, per side
    bool Load(const char* path, size_t vertexCount) {
        rest.clear(); weights.clear(); paths.clear();
        FILE* f=fopen(path,"rb"); if(!f) return false;
        uint32_t h[4]={};
        auto read=[&](void* p,size_t n) { return fread(p,1,n,f)==n; };
        bool ok=read(h,sizeof(h)) && h[0]==0x32415244 && h[1]>0 && h[1]<=64 && h[2]==vertexCount && h[3]>=2 && h[3]<=1001;
        if(ok) {
            rest.resize(h[1]); weights.resize(h[2]); paths.resize(h[3]*2);
            ok=read(arms,sizeof(arms)) && read(rest.data(),rest.size()*sizeof(BonePose)) && read(paths.data(),paths.size()*sizeof(Point));
            for(auto b:arms) ok=ok && b<rest.size();
            for(auto& row:weights) {
                uint32_t n=0;
                if(!ok || !read(&n,4) || n<1 || n>16) { ok=false; break; }
                row.resize(n); ok=read(row.data(),n*sizeof(Weight));
                float sum=0;
                for(auto w:row) { ok=ok && w.bone<rest.size() && std::isfinite(w.weight) && w.weight>=0 && std::isfinite(w.offset.x) && std::isfinite(w.offset.y) && std::isfinite(w.offset.z); sum+=w.weight; }
                ok=ok && std::fabs(sum-1)<0.001f;
            }
            for(auto b:rest) for(auto p:{b.head,b.tail}) ok=ok && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && Length(b.tail-b.head)>1e-6f;
            for(auto p:paths) ok=ok && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
            ok=ok && fgetc(f)==EOF;
        }
        fclose(f); if(!ok) { rest.clear(); weights.clear(); paths.clear(); } return ok;
    }
    Point Path(float degrees,int side) const {
        float t=std::max(0.0f,std::min(1.0f,0.5f-degrees/900.0f))*(paths.size()/2-1);
        size_t i=std::min(size_t(t),paths.size()/2-2);
        return Mix(paths[i*2+side],paths[(i+1)*2+side],t-float(i));
    }
    float GripRoll(float degrees,int side) const {
        // Unwrap each sampled radial angle before interpolation, including
        // the +/-pi seam. Canonical path coordinates are Y-left, Z-up.
        float t=std::max(0.0f,std::min(1.0f,0.5f-degrees/900.0f))*(paths.size()/2-1);
        float neutral=float(paths.size()/2-1)*0.5f;
        float previous=std::atan2(paths[side].z,paths[side].y), total=0, value=0, zero=0;
        for(size_t i=1;i<paths.size()/2;i++) {
            float phase=std::atan2(paths[i*2+side].z,paths[i*2+side].y);
            float delta=std::atan2(std::sin(phase-previous),std::cos(phase-previous));
            if(t>=float(i-1) && t<=float(i)) value=total+delta*(t-float(i-1));
            if(neutral>=float(i-1) && neutral<=float(i)) zero=total+delta*(neutral-float(i-1));
            total+=delta; previous=phase;
        }
        return value-zero;
    }
    void Solve(std::vector<BonePose>& pose, int side, Point target, Point forward) const {
        auto u=arms[side*3], f=arms[side*3+1], h=arms[side*3+2];
        Point shoulder=rest[u].head;
        float upper=Length(rest[u].tail-shoulder), lower=Length(rest[f].tail-rest[f].head), hand=Length(rest[h].tail-rest[h].head);
        Point wrist=target-forward*hand, dir=Unit(wrist-shoulder);
        float d=std::max(std::fabs(upper-lower)+1e-5f,std::min(upper+lower-1e-5f,Length(wrist-shoulder)));
        wrist=shoulder+dir*d;
        // A fixed down/out pole avoids the fitted elbow hint flipping across the
        // reach axis during a reversal. The original fitted bone lengths remain intact.
        Point hint={0,side==0 ? 0.35f : -0.35f,-1};
        Point pole=Unit(hint-dir*Dot(hint,dir),Unit(Cross(dir,{0,0,1}),{0,1,0}));
        float along=(upper*upper-lower*lower+d*d)/(2*d);
        Point elbow=shoulder+dir*along+pole*std::sqrt(std::max(0.0f,upper*upper-along*along));
        pose[u]={shoulder,elbow}; pose[f]={elbow,wrist}; pose[h]={wrist,wrist+forward*hand};
    }
    Point SteeringTarget(const Controls& c, int side) const {
        Point forward=Unit(c.axis), left=Unit(Cross({0,0,1},forward),{0,1,0}), up=Unit(Cross(forward,left));
        Point p=Path(c.degrees,side);
        float spread=std::isfinite(c.handSpread) ? std::max(0.5f,std::min(1.5f,c.handSpread)) : 1.0f;
        return c.centre+(forward*p.x+(left*p.y+up*p.z)*spread)*c.radius;
    }
    template<class Vertex> void Pose(const std::vector<Vertex>& source, std::vector<Vertex>& output,const Controls& c) const {
        output=source; if(rest.empty() || !c.wheel || c.radius<=0) return;
        auto pose=rest;
        float rolls[2]={};
        Point forward=Unit(c.axis);
        for(int side=0;side<2;side++) {
            Point target=SteeringTarget(c,side);
            rolls[side]=GripRoll(c.degrees,side);
            if(side==1) {
                if(c.hasHandbrake && c.brake>0) target=Mix(target,c.handbrake,std::max(0.0f,std::min(1.0f,c.brake)));
                else if(c.hasShifter) target=Mix(target,c.shifter,std::max(0.0f,std::min(1.0f,c.shift)));
                float action=c.hasHandbrake && c.brake>0 ? c.brake : (c.hasShifter ? c.shift : 0);
                rolls[side]*=1-std::max(0.0f,std::min(1.0f,action));
            }
            auto hand=arms[side*3+2];
            Point grip=Twist(Rotate(rest[hand].tail-rest[hand].head,{1,0,0},forward),forward,rolls[side]);
            Solve(pose,side,target,Unit(grip));
        }
        std::vector<PreparedRotation> rotations;
        rotations.reserve(rest.size());
        for(size_t b=0;b<rest.size();++b)
            rotations.emplace_back(rest[b].tail-rest[b].head,pose[b].tail-pose[b].head);
        PreparedRotation handRotation({1,0,0},forward);
        PreparedTwist handTwists[2]={{forward,rolls[0]},{forward,rolls[1]}};
        for(size_t i=0;i<output.size();i++) {
            Point p;
            for(auto w:weights[i]) {
                Point offset=rotations[w.bone].Apply(w.offset);
                for(int side=0;side<2;side++) if(w.bone==arms[side*3+2])
                    offset=handTwists[side].Apply(handRotation.Apply(w.offset));
                p=p+(pose[w.bone].head+offset)*w.weight;
            }
            output[i].x=p.x; output[i].y=p.y; output[i].z=p.z;
        }
    }
};
}