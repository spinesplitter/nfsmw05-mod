#define main CockpitRegressionMain
#include "cockpit_tests.cpp"
#undef main
#include "../src/driver_stream_range.h"
#include "driver_pose_reference.h"
#include "driver_stream_cache_tests.h"

int main() {
    {
        DriverBindingCache cache;
        DriverIK::Rig rig; rig.bones.resize(8);
        for(int i=0;i<6;++i) rig.arms[i]=i;
        strcpy(rig.bones[6].name,"JNT_hand_tip.L");
        strcpy(rig.bones[7].name,"JNT_hand_tip.R");
        int builds=0;
        auto build=[&]() { ++builds; return DriverBindingCache::Binding{0.75f,2,0}; };
        CustomDriver::Point p{1,2,3};
        cache.Prepare(rig);
        Check(cache.Get(12,p,build).bone==2,"binding builds rest classification");
        for(int i=0;i<100;++i) { cache.Prepare(rig); cache.Get(12,p,build); }
        Check(builds==1,"unchanged rest binding reuses across draws and eyes");
        p.x+=0.1f; cache.Get(12,p,build);
        Check(builds==2,"live vertex changes invalidate same-address binding");
        rig.bones[0].tail[2]=1; cache.Prepare(rig); cache.Get(12,p,build);
        Check(builds==3,"live rest-bone change invalidates binding");
        std::swap(rig.arms[0],rig.arms[1]); cache.Prepare(rig); cache.Get(12,p,build);
        Check(builds==4,"arm-index changes invalidate binding");
        std::swap(rig.bones[6],rig.bones[7]); cache.Prepare(rig); cache.Get(12,p,build);
        Check(builds==5,"tip-name remapping invalidates binding");
        rig.bones.resize(9); cache.Prepare(rig); cache.Get(12,p,build);
        Check(builds==6,"rig replacement invalidates binding");
        cache.Get(65535,p,build); cache.Get(65535,p,build);
        Check(builds==7,"maximum native index is cached");
        cache.Get(65536,p,build); cache.Get(65536,p,build);
        Check(builds==9,"out-of-range indices never enter binding cache");
        puts("PASS: rest binding reuse, live vertices, rest rig, arm and tip remapping, replacement and bounds");
    }
    {
        DriverSeamCache cache;
        uint16_t payload[]={0,1,2};
        std::vector<int> layout={0,1,0,0};
        std::set<uint64_t> native;
        int builds=0;
        auto build=[&](std::vector<unsigned char>& mask) { ++builds; mask[payload[0]]=1; };
        Check(cache.Get(payload,3,4,layout,native,build)[0]==1,"seam cache builds exclusion mask");
        cache.Get(payload,3,4,layout,native,build);
        Check(builds==1,"unchanged mesh reuses seam classification");
        payload[0]=3;
        const auto& changed=cache.Get(payload,3,4,layout,native,build);
        Check(builds==2 && changed[3] && !changed[0],"same-address index mutation invalidates seams");
        layout[2]=1; cache.Get(payload,3,4,layout,native,build);
        Check(builds==3,"body/arm partition changes invalidate seams");
        native.insert(123); cache.Get(payload,3,4,layout,native,build);
        Check(builds==4,"live native editor marks invalidate seams");
        cache.Get(payload,3,5,layout,native,build);
        Check(builds==5,"vertex count changes invalidate seams");
        puts("PASS: seam cache reuse, address reuse, partition, editor and vertex-count invalidation");
    }
    {
        using namespace CustomDriver;
        unsigned seed=731;
        auto random=[&]() { seed=seed*1664525u+1013904223u; return ((int)(seed>>16)-32768)/8192.0f; };
        for(int i=0;i<10000;++i) {
            Point p{random(),random(),random()}, from{random(),random(),random()}, to{random(),random(),random()};
            if(i%4==0) to=from*-1;
            if(i%7==0) from={};
            if(i%11==0) to={};
            float angle=random();
            auto expected=Rotate(p,from,to), actual=PreparedRotation(from,to).Apply(p);
            Check(Length(expected-actual)<1e-6f,"prepared rotation matches original including antiparallel and degenerate axes");
            expected=Twist(p,from,angle); actual=PreparedTwist(from,angle).Apply(p);
            Check(Length(expected-actual)<1e-6f,"prepared twist matches original for randomized angles and axes");
        }
        puts("PASS: 10000 prepared rotation/twist differential cases");
    }
    TestDriverStreamCache();
    {
        DriverPoseSelection selection(8);
        selection.Add(5,false); selection.Add(2,true); selection.Add(5,true);
        selection.Add(2,false); selection.Add(7,false); selection.Add(8,true);
        selection.ExcludeAutomatic(5); selection.ExcludeAutomatic(2); selection.ExcludeAutomatic(7);
        const auto& ordered=selection.Ordered();
        Check(ordered==std::vector<unsigned>({2,5,7}),"selection deduplicates and preserves ascending vertex order");
        Check(selection.IsSelected(2) && selection.IsSelected(5),"manual priority survives either triangle order and seam exclusion");
        Check(!selection.IsAutomatic(2) && !selection.IsAutomatic(5),"shared manual vertices retain full deformation weight");
        Check(!selection.IsSelected(7),"automatic torso seam vertex stays fixed");
        DriverPoseSelection::BoneOverride sleeve;
        Check(!selection.GetBoneOverride(2,sleeve),"unmarked vertex has no sleeve binding override");
        selection.SetBoneOverride(2,4,1);
        Check(selection.GetBoneOverride(2,sleeve) && sleeve.bone==4 && sleeve.side==1,
            "marked sleeve cuff vertex can stay bound to its forearm instead of hand");
        selection.SetBoneOverride(2,5,1,2);
        selection.SetBoneOverride(2,6,0,1);
        Check(selection.GetBoneOverride(2,sleeve) && sleeve.bone==5 && sleeve.side==1,
            "hand-vertex binding takes priority over a shared sleeve cuff binding");
        Check(selection.IsSelected(2) && !selection.IsAutomatic(2),
            "sleeve binding override preserves manual hand-mark priority");
        DriverPoseSelection empty(0); empty.Add(0,true); empty.ExcludeAutomatic(0);
        Check(empty.Ordered().empty(),"empty mesh rejects all vertices");
        DriverPoseSelection full(70000); full.Add(65535,false); full.Add(65536,true);
        Check(full.Ordered()==std::vector<unsigned>({65535}),"selection supports complete 16-bit index range only");
        puts("PASS: contiguous pose selection, manual priority, seam exclusion and index bounds");
        unsigned seed=12345;
        for(int trial=0;trial<100;++trial) {
            DriverPoseSelection current(256);
            std::set<unsigned> selected, automatic, manual;
            for(int i=0;i<2000;++i) {
                seed=seed*1664525u+1013904223u;
                unsigned vertex=(seed>>16)&255;
                bool marked=(seed&7)==0;
                current.Add(vertex,marked); selected.insert(vertex);
                if(marked) manual.insert(vertex); else automatic.insert(vertex);
            }
            for(unsigned vertex:manual) automatic.erase(vertex);
            for(unsigned vertex=0;vertex<256;vertex+=3) {
                current.ExcludeAutomatic(vertex);
                if(automatic.count(vertex)) selected.erase(vertex);
            }
            std::vector<unsigned> actualSelection;
            for(unsigned vertex:current.Ordered()) if(current.IsSelected(vertex)) {
                actualSelection.push_back(vertex);
                Check(current.IsAutomatic(vertex)==(automatic.count(vertex)!=0),"selection weights match set reference");
            }
            Check(actualSelection==std::vector<unsigned>(selected.begin(),selected.end()),"randomized selection and seams match set reference");
        }
    }
    Check(DriverStreamBytes({},32,1024)==0,"empty driver stream rejected");
    Check(DriverStreamBytes({4,1,2},32,4096)==160,"stream copy bounded by highest referenced vertex");
    Check(DriverStreamBytes({4,1,2},32,159)==0,"truncated stream rejected");
    Check(DriverStreamBytes({65535},32,2097152)==2097152,"maximum 16-bit index supported");
    Check(DriverStreamBytes({0},0,1024)==0,"zero stride rejected");
    static uint8_t solid[256]{}, plat[128]{}, entry[kEntrySize]{}, format[16]{};
    static FakeIb ib;
    static uint16_t indices[] = {0,1,2,3,4,5};
    static float vertices[] = {0.7f,-0.3f,0, 0.8f,-0.3f,0, 0.8f,-0.31f,0,
                              0.7f,0.3f,0, 0.8f,0.3f,0, 0.8f,0.31f,0};
    strcpy((char*)solid+kSolidName,"ARM_TEST_DRIVER_A");
    At<uint8_t*>(solid,kSolidPlat)=plat;
    At<int>(plat,kPlatNumEntries)=1;
    At<uint8_t*>(plat,kPlatEntries)=entry;
    At<uint16_t*>(plat,kPlatIndices)=indices;
    At<int>(plat,kPlatNumIndices)=6;
    At<IDirect3DIndexBuffer9*>(plat,kPlatIndexBuffer)=&ib;
    At<int>(format,8)=12;
    At<uint8_t*>(entry,kEntryFormat)=format;
    At<float*>(entry,kEntryVertices)=vertices;
    At<int>(entry,kEntryVertexBytes)=sizeof(vertices);
    auto& info=s_solids[solid];
    strcpy(info.name,"ARM_TEST_DRIVER_A");
    auto& edits=EditsFor("ARM_TEST_DRIVER_A");
    edits.arms.insert(TriId(0,1,2)); edits.arms.insert(TriId(3,4,5));
    s_driverIKTried=true; s_driverIK.ok=true; s_driverIK.bones.resize(8);
    for(int side=0;side<2;++side) {
        float y=side ? -0.3f : 0.3f;
        for(int j=0;j<3;++j) s_driverIK.arms[side*3+j]=side*4+j;
        for(int j=0;j<4;++j) {
            auto& b=s_driverIK.bones[side*4+j];
            b.head[0]=(j-1)*0.4f; b.head[1]=y;
            b.tail[0]=j*0.4f; b.tail[1]=y;
        }
        strcpy(s_driverIK.bones[side*4+3].name,side ? "JNT_hand_tip.R" : "JNT_hand_tip.L");
    }
    s_armDraws[&ib]={solid,Identity()};
    Check(Cockpit_IsRegisteredDriverDraw(&ib),"registered driver passes cheap draw gate");
    Check(!Cockpit_IsRegisteredDriverDraw(nullptr),"unrelated buffer rejected before D3D queries");
    Check(!Cockpit_HasMarkedArmDraw(&ib),"UV-only registration does not require active animation");
    // Compare positions AND normals against the pre-optimization implementation
    // with live steering, both control sides, blend/override and transformed worlds.
    {
    struct Vertex { float p[3], n[3]; } expected[6], actual[6];
    s_nativeWheelValid=true; s_nativeWheelCar=s_head.worldId;
    s_nativeWheelPivot={0.8f,0,0}; s_nativeWheelAxis={1,0,0};
    s_nativeDriverBody=Identity();
    for (int side=0;side<2;++side) for (int angle : {-120,0,75}) for (int blend=0;blend<3;++blend) {
        s_head.car=side ? "CLIO" : "GALLARDO";
        g_steeringWheelAngle=(float)angle;
        s_armControlBlend=blend*0.5f; s_armControlOverride=blend==2;
        s_armHover.Update(0.1f,true,{0.8f,-0.3f,0},{0.5f,-0.2f,-0.22f});
        M4 world=Identity(); world.m[3][0]=0.15f; world.m[3][1]=-0.05f;
        s_armDraws[&ib].world=world;
        for (int i=0;i<6;++i) {
            memcpy(actual[i].p,vertices+i*3,12);
            actual[i].n[0]=0; actual[i].n[1]=0; actual[i].n[2]=1;
        }
        memcpy(expected,actual,sizeof(actual));
        bool oldChanged=ReferencePoseMarkedArms(&ib,0,2,0,sizeof(Vertex),0,12,(unsigned char*)expected,sizeof(expected));
        bool newChanged=Cockpit_PoseMarkedArms(&ib,0,2,0,sizeof(Vertex),0,12,(unsigned char*)actual,sizeof(actual));
        Check(oldChanged==newChanged,"optimized pose retains change result");
        for(int i=0;i<6;++i) for(int j=0;j<3;++j) {
            Check(std::isfinite(actual[i].p[j]) && fabsf(actual[i].p[j]-expected[i].p[j])<1e-6f,"optimized steering position matches reference");
            Check(std::isfinite(actual[i].n[j]) && fabsf(actual[i].n[j]-expected[i].n[j])<1e-6f,"optimized steering normal matches reference");
        }
        for(int base : {-1,2}) for(unsigned first : {0u,3u}) {
            Vertex oldSplit[8]{}, newSplit[8]{};
            for(int i=0;i<8;++i) {
                newSplit[i].p[0]=0.1f*i; newSplit[i].n[2]=1;
            }
            memcpy(oldSplit,newSplit,sizeof(oldSplit));
            bool oldSplitChanged=ReferencePoseMarkedArms(&ib,first,1,base,sizeof(Vertex),0,12,
                (unsigned char*)oldSplit,sizeof(oldSplit));
            bool newSplitChanged=Cockpit_PoseMarkedArms(&ib,first,1,base,sizeof(Vertex),0,12,
                (unsigned char*)newSplit,sizeof(newSplit));
            Check(oldSplitChanged==newSplitChanged,"split draw and signed base retain change result");
            for(int i=0;i<8;++i) for(int j=0;j<3;++j) {
                Check(fabsf(oldSplit[i].p[j]-newSplit[i].p[j])<1e-6f,"split/base pose matches reference");
                Check(fabsf(oldSplit[i].n[j]-newSplit[i].n[j])<1e-6f,"split/base normal matches reference");
            }
        }
    }
    s_head.car.clear(); s_nativeWheelValid=false; g_steeringWheelAngle=0;
    s_armControlBlend=0; s_armControlOverride=false; s_armHover.Reset();
    s_armDraws[&ib].world=Identity();
    puts("PASS: bounded stream ranges and differential steering pose/normal equivalence");
    }
    s_handbrakeGripWorld={0.5f,-0.2f,-0.3f}; s_handbrakeGripValid=true;
    s_armHover.Update(0.1f,true,{0.8f,-0.3f,0},{0.5f,-0.2f,-0.22f});
    float output[18]; memcpy(output,vertices,sizeof(output));
    Check(Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)output,sizeof(output)),"marked right arm posed");
    Check(memcmp(output,vertices,9*sizeof(float))!=0,"right arm moves toward lever");
    Check(memcmp(output+9,vertices+9,9*sizeof(float))==0,"marked left arm unchanged");
    edits.arms.clear(); memcpy(output,vertices,sizeof(output));
    Check(!Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)output,sizeof(output)),"no marks no deformation");
    Check(memcmp(output,vertices,sizeof(output))==0,"unmarked geometry unchanged");
    uint8_t automaticEntries[2*kEntrySize]{};
    memcpy(automaticEntries,entry,kEntrySize);
    memcpy(automaticEntries+kEntrySize,entry,kEntrySize);
    At<int>(automaticEntries+kEntrySize,kEntryFirstIndex)=0;
    At<int>(automaticEntries+kEntrySize,kEntryTris)=2;
    Driver automaticDriver;
    automaticDriver.body={0,0}; automaticDriver.arms={0,2}; automaticDriver.heads={0,0};
    info.driver=&automaticDriver;
    At<uint8_t*>(plat,kPlatEntries)=automaticEntries;
    At<int>(plat,kPlatNumEntries)=2;
    Check(Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)output,sizeof(output)),
        "separate fingers animate without manual arm marks");
    Check(memcmp(output,vertices,9*sizeof(float))!=0,"automatic right fingers follow control reach");
    Check(memcmp(output+9,vertices+9,9*sizeof(float))==0,"automatic left fingers stay neutral during right reach");
    automaticDriver.rig.ok=true;
    for(int state=0;state<4;++state) {
        if(state==1) edits.arms.insert(TriId(0,1,2));
        if(state==2) edits.native.insert(TriId(3,4,5));
        if(state==3) { edits.arms.clear(); edits.native.clear(); }
        float expectedSeam[18], actualSeam[18];
        memcpy(expectedSeam,vertices,sizeof(vertices));
        memcpy(actualSeam,vertices,sizeof(vertices));
        bool oldResult=ReferencePoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)expectedSeam,sizeof(expectedSeam));
        bool newResult=Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)actualSeam,sizeof(actualSeam));
        Check(oldResult==newResult,"cached automatic seams retain reference change result");
        for(int v=0;v<18;++v)
            Check(fabsf(expectedSeam[v]-actualSeam[v])<1e-6f,"cached seams match reference across live manual/native edits");
    }
    // Repeated full animation differential after warm cache, then mutate rest
    // geometry at the same address. Selection/control state must stay live.
    for(int mutation=0;mutation<4;++mutation) {
        if(mutation==1) vertices[0]+=0.0125f;
        if(mutation==2) s_driverIK.bones[0].tail[2]+=0.02f;
        if(mutation==3) { vertices[0]-=0.0125f; s_driverIK.bones[0].tail[2]-=0.02f; }
        for(int repeat=0;repeat<3;++repeat) {
            float expected[18], actual[18];
            memcpy(expected,vertices,sizeof(expected)); memcpy(actual,vertices,sizeof(actual));
            bool oldResult=ReferencePoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)expected,sizeof(expected));
            bool newResult=Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)actual,sizeof(actual));
            Check(oldResult==newResult,"warm and mutated bindings retain reference change result");
            for(int i=0;i<18;++i) Check(fabsf(expected[i]-actual[i])<1e-6f,"warm bindings match live rig and geometry reference");
        }
    }
    puts("PASS: warm-cache animation differential with live same-address rig and vertex mutations");
    automaticDriver.rig.ok=false;
    UpdateShownRuns(solid,info,&automaticDriver,true,true);
    Check(automaticDriver.shownRuns.size()==2 && automaticDriver.shownRuns[1].size()==1 &&
        automaticDriver.shownRuns[1][0].second==2,"automatic fingers visible with native arms hidden and no show marks");
    info.driver=nullptr;
    At<uint8_t*>(plat,kPlatEntries)=entry;
    At<int>(plat,kPlatNumEntries)=1;
    s_armHover.Reset();
    Check(!Cockpit_HasMarkedArmDraw(&ib),"release restores original native draw");
    s_armHover.Update(0.1f,true,{0.8f,-0.3f,0},{0.5f,-0.2f,-0.22f});
    Check(!Cockpit_PoseMarkedArms(&ib,99,2,0,12,0,-1,(unsigned char*)output,sizeof(output)),"bad draw range rejected");
    s_handbrakeGripValid=false;
    Check(Cockpit_HasMarkedArmDraw(&ib),"release and shifter pose do not require brake target");
    DriverHoverMotion motion;
    CustomDriver::Point rest{0.8f,-0.3f,0}, hover{0.5f,-0.2f,-0.22f};
    motion.Update(0.02f,true,rest,hover);
    Check(fabsf(CustomDriver::Length(motion.wrist-rest)-DriverHoverMotion::speed*0.02f)<1e-6f,"fixed speed first frame, no teleport");
    auto previous=motion.wrist;
    motion.Update(0.02f,true,rest,{0.1f,-0.2f,-0.4f});
    Check(fabsf(CustomDriver::Length(motion.wrist-previous)-DriverHoverMotion::speed*0.02f)<1e-6f,"moving target remains speed limited");
    previous=motion.wrist;
    motion.Update(0.02f,false,rest,hover);
    Check(CustomDriver::Length(motion.wrist-previous)<=DriverHoverMotion::speed*0.02f+1e-6f,"release speed limited");
    for(int i=0;i<100;++i) motion.Update(0.02f,true,rest,hover);
    Check(CustomDriver::Length(motion.wrist-hover)<1e-6f,"settles at hover without overshoot");
    for(int i=0;i<100;++i) motion.Update(0.02f,false,rest,hover);
    Check(motion.progress==0,"returns completely to rest");
    s_handbrakeGripValid=true;
    s_armHover.Update(0.1f,true,rest,hover);
    std::vector<V3> heads,tails;
    DriverArmPose(Identity(),heads,tails);
    Check(fabsf(heads[4].x-s_driverIK.bones[4].head[0])<1e-6f,"shoulder stays anchored during control reach");
    Check(fabsf((tails[6]-heads[6]).x-0.4f)<1e-5f,"forearm stays rigid without wrist rotation");
    Check(fabsf((tails[7]-heads[7]).x-0.4f)<1e-5f,"hand stays rigid without articulation");
    s_head.valid=true; s_driverIKWorldValid=true; s_driverIKWorld=Identity();
    s_handbrakeGripValid=false; s_shifterHoverValid=true; s_shifterHoverWorld={0.3f,-0.5f,-0.1f};
    s_armHover.Reset(); s_armShiftInitialized=false; g_cockpitHandbrake=0;
    UpdateDriverHover(0.02f);
    Check(s_armHover.progress==0,"no phantom shift on initialization");
    ++g_cockpitShiftUp; UpdateDriverHover(0.02f);
    Check(s_armHover.progress>0,"raw shift attempt reaches shifter without gear success");
    DriverArmPose(Identity(),heads,tails);
    Check(heads[6].x<0.4f && heads[6].y<-0.3f,"elbow moves backward and right");
    Check(fabsf(DriverHoverMotion::speed-1.375f*1.5f)<1e-6f,"reach speed increased exactly 50 percent");
    // Identical control targets isolate the handbrake-only clearance from target placement.
    s_handbrakeGripWorld=s_shifterHoverWorld; s_handbrakeGripValid=true;
    s_armShiftUntil=GetTickCount64()*0.001+100;
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f);
    auto shiftElbow=s_armHover.wrist;
    Check(fabsf(shiftElbow.x+0.1f)<1e-5f && fabsf(shiftElbow.y+0.5f)<1e-5f,"approved shift target unchanged");
    g_cockpitHandbrake=1;
    s_armShiftUntil=-1; s_armShiftPending=false;
    auto beforeBrake=s_armHover.wrist;
    UpdateDriverHover(0.02f);
    Check(CustomDriver::Length(s_armHover.wrist-beforeBrake)<=DriverHoverMotion::speed*0.02f+1e-6f,"extra brake clearance does not snap");
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f);
    Check(CustomDriver::Length(s_armHover.wrist-shiftElbow)<1e-5f,"same measured target gives same hand hover for brake and shift");
    DriverArmPose(Identity(),heads,tails);
    Check(fabsf(heads[6].z-shiftElbow.z)<1e-5f,"brake elbow clearance does not displace measured hand target");
    Check(fabsf(heads[7].z-heads[6].z)<1e-5f,"hand lowers together with elbow");
    for(int b:{4,5,6,7}) {
        auto& restBone=s_driverIK.bones[b];
        Check(fabsf(CustomDriver::Length({tails[b].x-heads[b].x,tails[b].y-heads[b].y,tails[b].z-heads[b].z})-
            CustomDriver::Length({restBone.tail[0]-restBone.head[0],restBone.tail[1]-restBone.head[1],restBone.tail[2]-restBone.head[2]}))<1e-5f,
            "control-only brake solve preserves every arm bone length");
    }
    Check(CustomDriver::Length({tails[4].x-heads[5].x,tails[4].y-heads[5].y,tails[4].z-heads[5].z})<1e-5f,"actual elbow joint stays connected");
    g_cockpitHandbrake=0;
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f);
    Check(CustomDriver::Length(s_armHover.brakeElbowOffset)<1e-6f,"shift has no extra elbow offset");
    // Held brake is interrupted immediately by shifting, then restored after completion.
    s_handbrakeGripWorld={0.1f,-0.1f,-0.3f};
    g_cockpitHandbrake=1;
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f);
    auto brakeWrist=s_armHover.wrist;
    ++g_cockpitShiftUp; UpdateDriverHover(0.02f);
    Check(s_armShiftPending && !s_shifterHandReady,"shift interrupts held brake before contact");
    Check(CustomDriver::Length(s_armHover.wrist-brakeWrist)>1e-6f,"shift starts without brake release");
    s_armShiftUntil=-1; // elapsed click-time hold must not discard queued transfer
    auto transferWrist=s_armHover.wrist;
    UpdateDriverHover(0.02f);
    Check(CustomDriver::Length(s_armHover.wrist-transferWrist)<=DriverHoverMotion::speed*0.02f+1e-6f &&
        !s_shifterHandReady,"brake to shifter starts continuously before contact");
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f);
    Check(s_shifterHandReady && !s_armShiftPending,"shift reaches contact before hold expires");
    auto atShifter=s_armHover.wrist;
    double afterHold=s_armShiftUntil+0.001;
    UpdateDriverHover(0.02f,afterHold);
    Check(!s_shifterHandReady && CustomDriver::Length(s_armHover.wrist-atShifter)<=DriverHoverMotion::speed*0.02f+1e-6f,
        "shifter to brake starts continuously and clears contact");
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f,afterHold+i*0.02);
    Check(CustomDriver::Length(s_armHover.wrist-brakeWrist)<1e-5f,"reverse transfer preserves brake endpoint");
    // A relocated control changes the endpoint, without moving the other hand.
    auto relocated=s_handbrakeGripWorld+V3{0.04f,0.03f,0.02f};
    s_handbrakeGripWorld=relocated;
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f,afterHold+2.1);
    auto expected=CustomDriver::Point{relocated.x-0.4f,relocated.y,relocated.z};
    Check(CustomDriver::Length(s_armHover.wrist-expected)<1e-5f,"hand follows relocated brake with no fixed-position clamp");
    s_handbrakeGripWorld={0.1f,-0.1f,-0.3f};
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f,afterHold+2.2);
    ++g_cockpitShiftDown;
    UpdateDriverHover(0.02f,afterHold+3);
    Check(s_armShiftPending && CustomDriver::Length(s_armHover.wrist-brakeWrist)>1e-6f,"downshift also interrupts held brake");
    g_cockpitHandbrake=0;
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f,afterHold+3+i*0.02);
    Check(s_armHover.progress==0,"brake released during shift returns to rest after completion");
    puts("PASS: arm isolation, 50 percent faster motion, shift interrupt and held brake return");
}