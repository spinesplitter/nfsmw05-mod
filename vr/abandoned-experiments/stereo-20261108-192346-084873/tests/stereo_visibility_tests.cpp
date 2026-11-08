#include "../src/stereo_visibility.h"
#include <assert.h>
#include <limits>
#include <random>
#include <stdio.h>

static M4 Projection(float offset, float skew=0) {
    M4 m{};
    m.m[0][0]=1; m.m[1][1]=1;
    m.m[2][0]=skew; m.m[2][2]=10.0f/9.9f; m.m[2][3]=1;
    m.m[3][0]=offset; m.m[3][2]=-1.0f/9.9f;
    return m;
}
// Independent reference: transform every corner to homogeneous clip space.
static bool Reference(const M4& m,V3 lo,V3 hi) {
    bool allOutside[6]={true,true,true,true,true,true};
    for(int corner=0;corner<8;++corner) {
        double point[4]={corner&1 ? hi.x:lo.x,corner&2 ? hi.y:lo.y,corner&4 ? hi.z:lo.z,1};
        double clip[4]{};
        for(int c=0;c<4;++c) for(int r=0;r<4;++r) clip[c]+=point[r]*m.m[r][c];
        double d[6]={clip[3]+clip[0],clip[3]-clip[0],clip[3]+clip[1],clip[3]-clip[1],clip[2],clip[3]-clip[2]};
        for(int p=0;p<6;++p) allOutside[p]=allOutside[p] && d[p]<0;
    }
    for(bool outside:allOutside) if(outside) return false;
    return true;
}
int main() {
    M4 left=Projection(0.032f),right=Projection(-0.032f);
    StereoVisibility stereo(left,right);
    assert(stereo.Mask({0,0,1},{0,0,1})==3);
    assert(stereo.Mask({0.21f,0,0.2f},{0.21f,0,0.2f})==2);
    assert(stereo.Mask({-0.21f,0,0.2f},{-0.21f,0,0.2f})==1);
    assert(stereo.Mask({100,0,1},{101,1,2})==0);
    assert(stereo.Mask({0,0,-2},{0,0,-1})==0);
    assert(stereo.Mask({-1,-1,-1},{1,1,1})==3);
    assert(stereo.Mask({2,0,0},{1,0,0})==3);
    float nan=std::numeric_limits<float>::quiet_NaN();
    assert(stereo.Mask({nan,0,0},{1,1,1})==3);
    M4 invalid{};
    assert(StereoVisibility(invalid,invalid).Mask({100,0,1},{101,1,2})==3);
    assert(StereoVisibility(invalid,right).Mask({100,0,1},{101,1,2})==1);
    invalid=left; invalid.m[0][0]=nan;
    assert(StereoVisibility(invalid,right).Mask({100,0,1},{101,1,2})==1);
    std::mt19937 random(12345);
    std::uniform_real_distribution<float> position(-20,20),extent(0,2),angle(-1,1);
    unsigned compared=0;
    for(int pose=0;pose<50;++pose) {
        float a=angle(random);
        M4 view{}; view.m[0][0]=cosf(a); view.m[0][2]=sinf(a);
        view.m[1][1]=1; view.m[2][0]=-sinf(a); view.m[2][2]=cosf(a);
        view.m[3][0]=position(random); view.m[3][1]=position(random); view.m[3][3]=1;
        M4 l=MMul(view,Projection(0.032f,angle(random))),r=MMul(view,Projection(-0.032f,angle(random)));
        StereoVisibility query(l,r);
        for(int sample=0;sample<2000;++sample) {
            V3 lo={position(random),position(random),position(random)};
            V3 hi=lo+V3{extent(random),extent(random),extent(random)};
            unsigned expected=(Reference(l,lo,hi)?1:0)|(Reference(r,lo,hi)?2:0);
            unsigned actual=query.Mask(lo,hi);
            assert((actual & expected)==expected); // never discard a reference candidate
            ++compared;
        }
    }
    printf("PASS: stereo visibility, eye-only candidates, invalid inputs, %u randomized bounds\n",compared);
}