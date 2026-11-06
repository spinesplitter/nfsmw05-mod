#define main CockpitRegressionMain
#include "cockpit_tests.cpp"
#undef main

int main() {
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