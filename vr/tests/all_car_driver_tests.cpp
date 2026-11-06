#define main CockpitAllCarBaseline
#include "cockpit_tests.cpp"
#undef main

int main(int argc,char** argv) {
    Check(argc==3,"rig path and car name required");
    Check(DriverIK::LoadFile(argv[1],&s_driverIK),"load per-car fitted rig");
    s_driverIKTried=true;
    s_head.car=argv[2]; s_head.valid=true; s_head.worldId=123;
    Driver installed; installed.ok=true; installed.car=s_head.car;
    installed.rig=s_driverIK; installed.rigTried=true;
    uint8_t solid[1]{};
    s_solids[solid].driver=&installed;
    s_nativeWheelCar=123; s_nativeWheelValid=true;
    s_nativeDriverBody=Identity(); s_driverIKWorld=Identity(); s_driverIKWorldValid=true;
    auto p=[](const float* v){return CustomDriver::Point{v[0],v[1],v[2]};};
    auto l=p(s_driverIK.bones[s_driverIK.arms[2]].head);
    auto r=p(s_driverIK.bones[s_driverIK.arms[5]].head);
    auto centre=(l+r)*0.5f;
    auto shoulder=p(s_driverIK.bones[s_driverIK.arms[0]].head);
    auto opposite=p(s_driverIK.bones[s_driverIK.arms[3]].head);
    auto chest=(shoulder+opposite)*0.5f;
    Near(NativeArmWeight(s_driverIK,chest),0,"torso receives zero automatic arm weight");
    Near(NativeArmWeight(s_driverIK,{centre.x,centre.y,centre.z-0.35f}),0,"jeans receive zero automatic arm weight");
    Check(NativeArmWeight(s_driverIK,l)>0.9f,"left wrist remains bound");
    Check(NativeArmWeight(s_driverIK,r)>0.9f,"right wrist remains bound");
    s_nativeWheelPivot={centre.x+0.07f,centre.y,centre.z-0.065f};
    s_nativeWheelAxis={0.9701f,0,-0.2425f};
    std::vector<V3> heads,tails,previous;
    for(int step=-4500;step<=4500;++step) {
        g_steeringWheelAngle=step*0.1f;
        DriverArmPose(Identity(),heads,tails);
        Check(heads.size()==s_driverIK.bones.size(),"pose every fitted rig");
        for(int side=0;side<2;++side) {
            auto grip=NativeGrip(s_driverIK,side,Identity());
            if(side==ControlSide() && grip.release>0) {
                int u=s_driverIK.arms[side*3], f=s_driverIK.arms[side*3+1], h=s_driverIK.arms[side*3+2];
                auto wrist=grip.wrist;
                CustomDriver::Point elbow;
                Check(DriverSteering::Solve(p(s_driverIK.bones[u].head),p(s_driverIK.bones[f].head),
                    p(s_driverIK.bones[h].head),wrist,elbow),"lift bend plane solves");
                Near(CustomDriver::Length(elbow-CustomDriver::Point{heads[f].x,heads[f].y,heads[f].z}),0,
                    "lift never blends toward fixed model-space elbow");
            }
            if(side!=ControlSide()) Near(grip.release,0,"non-control hand never releases");
            for(int j=0;j<2;++j) {
                int b=s_driverIK.arms[side*3+j];
                auto extent=CustomDriver::Point{tails[b].x-heads[b].x,tails[b].y-heads[b].y,tails[b].z-heads[b].z};
                Near(CustomDriver::Length(extent),CustomDriver::Length(p(s_driverIK.bones[b].tail)-p(s_driverIK.bones[b].head)),"per-car bone lengths preserved");
                Check(std::isfinite(heads[b].x)&&std::isfinite(heads[b].y)&&std::isfinite(heads[b].z),"finite fitted pose");
                if(!previous.empty()) Check(CustomDriver::Length({heads[b].x-previous[b].x,heads[b].y-previous[b].y,heads[b].z-previous[b].z})<0.015f,"continuous fitted arm sweep");
            }
        }
        previous=heads;
    }
    int control=ControlSide(), other=1-control;
    Check(control==((s_head.car=="CLIO" || s_head.car=="MONARO")?0:1),"right-hand-drive cars select left control hand");
    g_steeringWheelAngle=control==0?-200:200;
    Check(NativeGrip(s_driverIK,control,Identity()).release>0.99f,"correct hand lifts during transfer");
    DriverArmPose(Identity(),heads,tails);
    auto otherWrist=heads[s_driverIK.arms[other*3+2]];
    s_shifterHoverValid=true;
    s_shifterHoverWorld={centre.x-0.20f,centre.y+(control?-0.15f:0.15f),centre.z-0.15f};
    UpdateDriverHover(0.02f,1);
    ++g_cockpitShiftUp;
    UpdateDriverHover(0.02f,1.02);
    Check(s_armControlOverride,"shift controls active per car");
    DriverArmPose(Identity(),heads,tails);
    auto actual=heads[s_driverIK.arms[other*3+2]];
    Near(CustomDriver::Length({actual.x-otherWrist.x,actual.y-otherWrist.y,actual.z-otherWrist.z}),0,"shifting never moves opposite grip");
    Driver membership; membership.rig=s_driverIK; membership.body={2}; membership.arms={3};
    uint8_t entry[kEntrySize]{}; At<int>(entry,kEntryFirstIndex)=12;
    Check(!AutomaticDriverArmIndex(membership,entry,1,15),"body excluded from automatic rigging");
    Check(AutomaticDriverArmIndex(membership,entry,1,18),"sleeves included without manual marks");
    Check(!AutomaticDriverArmIndex(membership,entry,1,27),"head excluded from automatic rigging");
    s_head.car="MONARO";
    Check(ControlSide()==0,"Monaro uses left control hand");
    g_steeringWheelAngle=200;
    Near(SteeringDegrees(),-200,"Monaro mirrors steering transfer");
    s_head.car="CLIO";
    Check(ControlSide()==0,"Clio uses left control hand");
    s_head.car="GALLARDO";
    Check(ControlSide()==1,"Gallardo retains right control hand");
    s_head.car=argv[2];
    // Check measured targets using the real per-car hand extent and handedness.
    s_nativeWheelValid=false; s_controlVehicleValid=false;
    s_driverIKWorld=Identity(); s_driverIKWorldValid=true;
    s_shifterHoverValid=false; s_handbrakeGripValid=true;
    s_armShiftPending=false; s_armShiftUntil=-1; g_cockpitHandbrake=1;
    const auto& controlBone=s_driverIK.bones[s_driverIK.arms[control*3+2]];
    CustomDriver::Point handExtent{controlBone.tail[0]-controlBone.head[0],
        controlBone.tail[1]-controlBone.head[1],controlBone.tail[2]-controlBone.head[2]};
    for(int placement=0;placement<2;++placement) {
        s_handbrakeGripWorld={centre.x+0.02f*placement,centre.y+0.03f*placement,centre.z-0.12f};
        for(int i=0;i<100;++i) UpdateDriverHover(0.02f,20+placement*3+i*0.02);
        auto tip=s_armHover.wrist+handExtent;
        Near(CustomDriver::Length(tip-CustomDriver::Point{s_handbrakeGripWorld.x,
            s_handbrakeGripWorld.y,s_handbrakeGripWorld.z}),0,"rig hand tip tracks actual control placement on either control side");
    }
    g_cockpitHandbrake=0;
    s_nativeWheelValid=true;
    // Deliberately over-broad automatic range reproduces the visibility
    // classifier selecting torso/jeans. Exercise the actual vertex write path.
    uint8_t geometry[256]{}, platform[128]{}, drawEntry[kEntrySize]{}, format[16]{};
    FakeIb buffer;
    uint16_t indices[]={0,1,2,3,4,5,6,7,8};
    CustomDriver::Point source[9];
    for(int i=0;i<3;++i) {
        source[i]=chest+CustomDriver::Point{0.001f*i,0,0};
        source[3+i]={centre.x+0.001f*i,centre.y,centre.z-0.35f};
        source[6+i]=l+CustomDriver::Point{0.001f*i,0,0};
    }
    At<uint8_t*>(geometry,kSolidPlat)=platform;
    At<int>(platform,kPlatNumEntries)=1;
    At<uint8_t*>(platform,kPlatEntries)=drawEntry;
    At<uint16_t*>(platform,kPlatIndices)=indices;
    At<int>(platform,kPlatNumIndices)=9;
    At<IDirect3DIndexBuffer9*>(platform,kPlatIndexBuffer)=&buffer;
    At<int>(format,8)=12;
    At<uint8_t*>(drawEntry,kEntryFormat)=format;
    At<CustomDriver::Point*>(drawEntry,kEntryVertices)=source;
    At<int>(drawEntry,kEntryVertexBytes)=sizeof(source);
    At<int>(drawEntry,kEntryTris)=3;
    auto& meshInfo=s_solids[geometry];
    meshInfo.driver=&installed; strcpy(meshInfo.name,"BODY_ISOLATION_DRIVER_A");
    installed.body={0}; installed.arms={3}; installed.heads={0};
    s_armDraws[&buffer]={geometry,Identity()};
    s_armControlBlend=0; s_armControlOverride=false; s_armHover.Reset();
    g_steeringWheelAngle=45;
    CustomDriver::Point output[9]; memcpy(output,source,sizeof(source));
    Check(Cockpit_PoseMarkedArms(&buffer,0,3,0,12,0,-1,(unsigned char*)output,sizeof(output)),"automatic sleeves deform");
    Check(memcmp(output,source,6*sizeof(CustomDriver::Point))==0,"torso and jeans remain byte-identical under steering");
    Check(memcmp(output+6,source+6,3*sizeof(CustomDriver::Point))!=0,"wrist still steers while body is fixed");
    printf("PASS: %s fitted rig full 900-degree sweep, %s control hand, arm membership and shift isolation\n",argv[2],control?"right":"left");
}