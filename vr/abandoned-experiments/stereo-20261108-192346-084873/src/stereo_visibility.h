#pragma once
#include "vrmath.h"
#include <cmath>

// Conservative WORLD-space AABB query for D3D's row-vector clip convention:
// -w <= x,y <= w, 0 <= z <= w. Supply original view-projection matrices,
// before depth splitting; these are not suitable for occlusion or LOD decisions.
// No object pointers/results are cached, so no frame or streaming lifetime leaks.
class StereoVisibility {
    struct Plane { double v[4]; };
    Plane planes_[2][6]{};
    bool valid_[2]{};
public:
    StereoVisibility(const M4& left, const M4& right) {
        const M4* eyes[2] = {&left,&right};
        for (int eye=0;eye<2;++eye) {
            const M4& m=*eyes[eye];
            bool valid=true;
            for (int r=0;r<4;++r)
                for (int c=0;c<4;++c) valid=valid && std::isfinite(m.m[r][c]);
            for (int p=0;p<6;++p) {
                for (int r=0;r<4;++r) {
                    double w=m.m[r][3];
                    planes_[eye][p].v[r] = p==4 ? m.m[r][2] :
                        p==5 ? w-m.m[r][2] : w+(p%2 ? -1.0 : 1.0)*m.m[r][p/2];
                }
                const auto& v=planes_[eye][p].v;
                // Degenerate/infinite-far projections fall back to visible.
                valid=valid && (v[0]*v[0]+v[1]*v[1]+v[2]*v[2]>1e-24);
            }
            valid_[eye]=valid;
        }
    }
    // Bit 0: left candidate; bit 1: right candidate; zero: outside both.
    // False positives are allowed. Invalid inputs must never hide an object.
    unsigned Mask(V3 minimum, V3 maximum) const {
        double lo[3]={minimum.x,minimum.y,minimum.z};
        double hi[3]={maximum.x,maximum.y,maximum.z};
        for(int a=0;a<3;++a)
            if(!std::isfinite(lo[a]) || !std::isfinite(hi[a]) || lo[a]>hi[a]) return 3;
        unsigned mask=0;
        for(int eye=0;eye<2;++eye) {
            bool visible=true;
            if(valid_[eye]) for(const Plane& plane:planes_[eye]) {
                double distance=plane.v[3], scale=std::fabs(plane.v[3])+1.0;
                for(int a=0;a<3;++a) {
                    double term=plane.v[a]*(plane.v[a]>=0 ? hi[a] : lo[a]);
                    distance+=term;
                    scale+=std::fabs(term);
                }
                // Keep touching/near-boundary bounds, including float noise.
                if(distance < -1e-6*scale) { visible=false; break; }
            }
            if(visible) mask|=1u<<eye;
        }
        return mask;
    }
};