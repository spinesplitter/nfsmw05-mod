#define main CockpitRegressionMain
#include "cockpit_tests.cpp"
#undef main

int main(int argc, char** argv) {
    // Two differently placed knobs and longitudinal brake handles, plus a floor
    // and dashboard decoy. Run the production detector on each mesh.
    for(int car=0;car<2;++car) {
        std::vector<V3> vertices;
        std::vector<uint16_t> indices;
        auto box=[&](V3 lo,V3 hi) {
            unsigned base=(unsigned)vertices.size();
            for(int i=0;i<8;++i) vertices.push_back({i&1?hi.x:lo.x,i&2?hi.y:lo.y,i&4?hi.z:lo.z});
            const int faces[]={0,1,3,0,3,2,4,6,7,4,7,5,0,4,5,0,5,1,2,3,7,2,7,6,0,2,6,0,6,4,1,5,7,1,7,3};
            for(int i:faces) indices.push_back((uint16_t)(base+i));
        };
        float x=car?0.40f:0.25f, y=car?-0.07f:0.03f, z=car?0.55f:0.43f;
        box({x-0.025f,y-0.025f,z-0.06f},{x+0.025f,y+0.025f,z});
        box({-0.20f,y,0.28f},{0.05f,y+0.025f,0.34f});
        box({-0.6f,-0.7f,0.1f},{0.6f,0.7f,0.12f});
        box({0.60f,-0.2f,0.3f},{0.75f,0.2f,0.7f});
        SolidMesh mesh; mesh.verts=(uint8_t*)vertices.data(); mesh.stride=sizeof(V3);
        mesh.numVerts=(int)vertices.size(); mesh.indices=indices.data(); mesh.numIndices=(int)indices.size();
        ControlMarks picked; DetectConsoleControls(mesh,picked);
        Check(picked.shift && picked.handbrake,"both console controls detected");
        Check(fabsf(picked.shifter.x-x)<1e-5f && fabsf(picked.shifter.y-y)<1e-5f && fabsf(picked.shifter.z-z)<1e-5f,"knob top centre follows each car placement");
        Check(picked.brake.x>0 && fabsf(picked.brake.z-0.34f)<1e-5f,"brake targets grip instead of hinge");
        picked.shifter={9,8,7}; picked.shift=true;
        DetectConsoleControls(mesh,picked);
        Check(picked.shifter.x==9,"manual control target wins over detection");
    }
    // Optional offline dump from the installed geometry reader, testing the
    // exact production detector against real car meshes without game injection.
    if(argc==2) {
        FILE* f=fopen(argv[1],"rb"); Check(f!=nullptr,"open geometry dump");
        unsigned count; Check(fread(&count,4,1,f)==1,"read geometry count");
        for(unsigned i=0;i<count;++i) {
            char name[128]{}; unsigned nv,ni;
            Check(fread(name,128,1,f)==1 && fread(&nv,4,1,f)==1 && fread(&ni,4,1,f)==1,"read mesh header");
            Check(nv<65536 && ni<1000000,"geometry bounds");
            std::vector<V3> v(nv); std::vector<uint16_t> indices(ni);
            Check(fread(v.data(),sizeof(V3),nv,f)==nv && fread(indices.data(),2,ni,f)==ni,"read mesh data");
            SolidMesh mesh; mesh.verts=(uint8_t*)v.data(); mesh.stride=sizeof(V3); mesh.numVerts=nv;
            mesh.indices=indices.data(); mesh.numIndices=ni;
            ControlMarks picked; DetectConsoleControls(mesh,picked);
            printf("%s shift=%d (%.3f %.3f %.3f) brake=%d (%.3f %.3f %.3f)\n",name,
                picked.shift,picked.shifter.x,picked.shifter.y,picked.shifter.z,
                picked.handbrake,picked.brake.x,picked.brake.y,picked.brake.z);
        }
        fclose(f);
    }
    puts("PASS: per-car control geometry detection and manual override");
}