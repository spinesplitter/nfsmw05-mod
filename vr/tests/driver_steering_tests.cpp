#define main CockpitSteeringRegressionMain
#include "cockpit_tests.cpp"
#undef main

int main(int argc, char** argv) {
    CockpitSteeringRegressionMain();
    using namespace CustomDriver;
    Point axis{1,0,0}, pivot{}, right{0,-0.2f,0}, left{0,0.2f,0}, direction{1,0,0};
    for(int side=0;side<2;++side) for(float spread:{0.5f,1.f,1.1f,1.5f}) {
        Point rest=side ? right:left;
        for(int angle=-450;angle<=450;++angle) {
            auto grip=DriverSteering::Evaluate(rest,direction,pivot,axis,float(angle),side,0,spread);
            auto scaled=DriverSteering::Evaluate(rest*spread,direction,pivot,axis,float(angle),side);
            Near(Length(grip.wrist-scaled.wrist),0,"native spread matches scaled wheel-plane path through transfers");
            Near(grip.release,scaled.release,"spread preserves transfer timing");
        }
        auto grip=DriverSteering::Evaluate(rest,direction,pivot,axis,0,side,0,spread);
        Near(Length(grip.wrist),0.2f*spread,"spread scales native neutral separation");
    }
    for(int side=0;side<2;++side) {
        Point fore{0.22f,side ? 0.06f : -0.06f,0.10f}, hand{0.075f,0,0.025f};
        float previous=0;
        for(int step=-22500;step<=22500;++step) {
            float angle=step*0.02f;
            auto grip=DriverSteering::Evaluate(side ? right:left,hand,pivot,axis,angle,side);
            Point posedFore=Rotate(fore,hand,grip.direction);
            float roll=DriverSteering::CuffRoll(fore,posedFore,hand,grip.direction,axis,grip.roll);
            Check(std::isfinite(roll) && fabsf(roll)<=0.349066f,"cuff correction finite and at most 20 degrees across 900 degrees");
            if(step>-22500) Check(fabsf(roll-previous)<0.003f,"cuff rotation continuous through both regrips");
            previous=roll;
            Near(Length(Twist(posedFore,posedFore,roll)-posedFore),0,"cuff roll preserves wrist and elbow endpoints");
            Point radial=Unit(Cross(posedFore,{0,0,1}))*0.035f;
            Near(Length(Twist(radial,posedFore,roll)),Length(radial),"cuff radius unchanged; no sleeve scaling");
            Check(Length(Twist(radial,posedFore,roll)-radial)<0.0123f,"cuff surface correction bounded to 12.3 mm at 35 mm radius");
        }
    }
    Near(DriverSteering::CuffWeight(0.70f),0,"proximal 70 percent of forearm unchanged");
    Near(DriverSteering::CuffWeight(1),1,"full orientation correction only at cuff");
    Near(DriverSteering::CuffRoll({1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},0),0,
        "neutral cuff has no correction even with collinear hand and forearm");
    for(Point wheelAxis:{axis,axis*-1,Point{0.9701f,0,-0.2425f},Point{-0.9701f,0,0.2425f}}) {
        Point normal=Unit(wheelAxis);
        Point up=Unit(Point{0,0,1}-normal*Dot({0,0,1},normal));
        for(int side=0;side<2;++side) for(float fix:{-60.f,-25.f,0.f,25.f,60.f}) {
            Point rest=side ? right : left;
            auto neutral=DriverSteering::Evaluate(rest,direction,pivot,wheelAxis,0,side,fix);
            Near(Length(neutral.wrist-normal*Dot(neutral.wrist,normal)),Length(rest),
                "hand fix preserves wheel rim radius");
            Near(Dot(neutral.wrist,normal),Dot(rest,normal),"hand fix preserves rim plane");
            Check(Dot(neutral.wrist,rest)>0,"hand fix keeps each grip on its own side");
            if(fix!=0) Check(Dot(neutral.wrist,up)*fix>0,"hand fix positive up negative down for either axis sign");
            for(int a=-450;a<=450;++a) {
                auto g=DriverSteering::Evaluate(rest,direction,pivot,wheelAxis,float(a),side,fix);
                auto next=DriverSteering::Evaluate(rest,direction,pivot,wheelAxis,a+0.001f,side,fix);
                auto baseline=DriverSteering::Evaluate(rest,direction,pivot,wheelAxis,float(a),side);
                Near(g.release,baseline.release,"hand fix never changes transfer timing");
                Check(Length(g.wrist-next.wrist)<0.0001f,"hand fix transfer remains continuous");
                if(side==0 || (a>=-100 && a<=90))
                    Check(Length(g.wrist-Twist(neutral.wrist,wheelAxis,a*0.01745329252f))<1e-6f,
                        "hand fix tracks adjusted neutral grip without changing steering angle");
                if(side==1 && ((a>=165 && a<=270) || (a>=-270 && a<=-180))) {
                    auto waiting=DriverSteering::Evaluate(rest,direction,pivot,wheelAxis,a>0 ? 180.f : -180.f,side,fix);
                    Check(Length(g.wrist-waiting.wrist)<1e-6f,"hand fix preserves stationary waiting");
                    Check(Dot(g.wrist,up)<0,"hand fix keeps transfer wait bottom-right");
                }
                if(side==1 && (a==325 || a==-310)) {
                    Check(Length(g.wrist-baseline.wrist)<1e-6f,"hand fix preserves regrip contact position");
                    Check(Length(g.direction-baseline.direction)<1e-6f,"hand fix preserves contact orientation");
                }
            }
        }
    }
    for (int degrees=-450;degrees<=450;++degrees) {
        for (int side=0;side<2;++side) {
            Point rest=side ? right : left;
            auto grip=DriverSteering::Evaluate(rest,direction,pivot,axis,float(degrees),side);
            auto next=DriverSteering::Evaluate(rest,direction,pivot,axis,float(degrees)+0.001f,side);
            Check(Length(next.wrist-grip.wrist)<0.0001f,"continuous steering across former release boundaries");
            if(side==0 || (degrees>=-100 && degrees<=90)) {
                Check(Length(grip.wrist-Twist(rest,axis,degrees*0.01745329252f))<1e-6f,"left hand and neutral right tracking unchanged");
                Check(grip.release==0,"no release outside right transfer");
                Near(grip.roll,degrees*0.01745329252f,"unchanged tracking roll");
            }
            Check(grip.release>=0 && grip.release<=1,"bounded release weight");

        }
    }
    for(float a:{90.f,100.f,145.f,165.f,180.f,190.f,210.f,220.f,240.f,270.f,305.f,310.f,320.f,325.f,340.f}) for(float sign:{-1.f,1.f}) {
        auto before=DriverSteering::Evaluate(right,direction,pivot,axis,sign*(a-0.01f),1);
        auto at=DriverSteering::Evaluate(right,direction,pivot,axis,sign*a,1);
        auto after=DriverSteering::Evaluate(right,direction,pivot,axis,sign*(a+0.01f),1);
        Check(Length(after.wrist-before.wrist)<0.001f,"phase position continuous in both directions");
        Check(Length((after.wrist-at.wrist)-(at.wrist-before.wrist))<0.00001f,"phase velocity continuous");
        Check(Length(after.direction-before.direction)<0.001f,"phase wrist orientation continuous");
    }
    for(int a=91;a<=270;++a) {
        auto positive=DriverSteering::Evaluate(right,direction,pivot,axis,float(a),1);
        Near(positive.release,DriverSteering::Phase(float(a),90,165),"positive release spreads smoothly over 90 to 165");
        if(a>=165) {
            auto waiting=DriverSteering::Evaluate(right,direction,pivot,axis,165,1);
            Check(Length(positive.wrist-waiting.wrist)<1e-6f,"released hand waits independently of wheel rotation");
            Check(positive.release==1,"waiting hand is fully released");
        }
        auto l=DriverSteering::Evaluate(left,direction,pivot,axis,float(a),0);
        Check(Length(l.wrist-positive.wrist)>0.04f,"transfer wrist stays clear of left grip");
    }
    auto wait=DriverSteering::Evaluate(right,direction,pivot,axis,270,1);
    auto contact=DriverSteering::Evaluate(right,direction,pivot,axis,325,1);
    Check(wait.release==1 && contact.release==0,"positive hand waits until 270 and regrips at 325");
    for(int a=270;a<=325;++a) {
        auto grip=DriverSteering::Evaluate(right,direction,pivot,axis,float(a),1);
        Near(grip.release,1-DriverSteering::Phase(float(a),270,325),
            "positive regrip eases over 270 to 325");
        Near(DriverSteering::TransferSettle(float(a)),1-grip.release,
            "positive settle and regrip weights agree");
    }
    Check(wait.wrist.y<0 && wait.wrist.z<0 && contact.wrist.y<0 && contact.wrist.z<0,
        "positive waiting and regrip positions are bottom right");
    for(int a=325;a<=450;++a) {
        auto g=DriverSteering::Evaluate(right,direction,pivot,axis,float(a),1);
        Check(Length(g.wrist-Twist(contact.wrist,axis,(a-325)*0.01745329252f))<1e-6f,
            "regripped hand resumes wheel tracking only after 325");
    }
    auto negativeWait=DriverSteering::Evaluate(right,direction,pivot,axis,-180,1);
    auto negativeContact=DriverSteering::Evaluate(right,direction,pivot,axis,-310,1);
    Check(negativeWait.release==1 && negativeContact.release==0,
        "negative hand fully waits at 180 and completes contact at 310");
    for(int a=101;a<180;++a) {
        auto g=DriverSteering::Evaluate(right,direction,pivot,axis,float(-a),1);
        float depart=DriverSteering::Phase(float(a),100,180);
        Point tracked=Twist(right,axis,-a*0.01745329252f);
        Point expected=Mix(tracked,negativeWait.wrist,depart)
            -axis*(0.2f*0.60f*depart*(1-depart));
        Check(Length(g.wrist-expected)<1e-6f,
            "negative release takes direct diagonal to bottom-right without upward detour");
    }
    for(Point wheelAxis:{axis,axis*-1,Point{0.9701f,0,-0.2425f},Point{-0.9701f,0,0.2425f}}) {
        auto normal=Unit(wheelAxis);
        auto up=Unit(Point{0,0,1}-normal*Dot({0,0,1},normal));
        auto fittedRight=Unit(right-normal*Dot(right,normal)-up*Dot(right,up));
        for(float angle:{-180.f,-270.f,-295.f,-310.f}) {
            auto g=DriverSteering::Evaluate(right,direction,pivot,wheelAxis,angle,1);
            Check(Dot(g.wrist,fittedRight)>0,
                "negative wait and regrip stay driver-right for either wheel-axis sign");
            Check(Dot(g.wrist,up)<0,"negative wait and contact stay bottom-right");
        }
    }
    Check(negativeWait.wrist.y<0 && negativeWait.wrist.z<0 &&
        negativeContact.wrist.y<0 && negativeContact.wrist.z<0,
        "negative wait and regrip are bottom right");
    for(int a=100;a<=450;++a) {
        auto g=DriverSteering::Evaluate(right,direction,pivot,axis,float(-a),1);
        Near(g.release,DriverSteering::Phase(float(a),100,180)*
            (1-DriverSteering::Phase(float(a),270,310)),"negative release and contact timing");
        if(a>=180 && a<=270)
            Check(Length(g.wrist-negativeWait.wrist)<1e-6f,"negative hand waits independently of wheel rotation");
        if(a>=310)
            Check(Length(g.wrist-Twist(negativeContact.wrist,axis,(310-a)*0.01745329252f))<1e-6f,
                "negative regrip tracks wheel after minus 310");
    }
    Point elbow;
    Check(DriverSteering::Elbow({0,0,0},{0.3f,0,-0.2f},{0.6f,0,0},{0.5f,0.1f,0},elbow),"reachable IK target");
    Check(fabsf(Length(elbow)-sqrtf(0.13f))<1e-5f && fabsf(Length(Point{0.5f,0.1f,0}-elbow)-sqrtf(0.13f))<1e-5f,"IK preserves arm lengths");
    s_nativeWheelValid=true; s_nativeWheelCar=s_head.worldId;
    s_head.car="GALLARDO";
    s_driverIKTried=true; s_driverIK.ok=true; s_driverIK.bones.resize(8);
    for(int side=0;side<2;++side) {
        for(int j=0;j<3;++j) s_driverIK.arms[side*3+j]=side*4+j;
        for(int j=0;j<4;++j) {
            auto& bone=s_driverIK.bones[side*4+j];
            bone.head[0]=(j-1)*0.4f; bone.tail[0]=j*0.4f;
            bone.head[1]=bone.tail[1]=side ? -0.3f : 0.3f;
        }
    }
    s_head.valid=true; s_driverIKWorldValid=true; s_driverIKWorld=Identity();
    s_shifterHoverValid=true; s_shifterHoverWorld={0.3f,-0.5f,-0.1f};
    s_nativeWheelPivot={0.8f,0,0}; s_nativeWheelAxis={1,0,0}; s_curBody=Identity();
    s_nativeDriverBody=Identity();
    Check(fabsf(DriverHoverMotion::clearance-0.05f)<1e-6f,
        "shifter and handbrake hover clearance lowered by 3 cm to 5 cm");
    for(const char* car : {"GALLARDO","CLIO"}) {
        s_head.car=car;
        for(int side=0;side<2;++side) {
            for(float angle : {-220.0f,0.0f,210.0f}) {
                g_steeringWheelAngle=angle;
                s_nativeWheelOffset={};
                auto original=NativeGrip(s_driverIK,side,Identity());
                V3 shift={0,0.035f,-0.02f};
                s_nativeWheelOffset=shift; s_nativeWheelPivot=s_nativeWheelPivot+shift;
                auto moved=NativeGrip(s_driverIK,side,Identity());
                Check(Length(moved.wrist-original.wrist-Point{shift.x,shift.y,shift.z})<1e-5f,
                    "both handedness paths follow relocated wheel through steering and transfer");
                s_nativeWheelPivot=s_nativeWheelPivot-shift;
                s_nativeWheelOffset={};
            }
        }
    }
    s_head.car="GALLARDO";
    s_armHover.Reset(); s_armControlBlend=0; s_armControlOverride=false;
    s_armShiftPending=false; s_armShiftUntil=-1; g_cockpitHandbrake=0;
    g_steeringWheelAngle=210;
    UpdateDriverHover(0.02f);
    auto grip=NativeGrip(s_driverIK,1,Identity());
    Check(Length(s_armHover.wrist-grip.wrist)<1e-5f,"free hand follows fixed wheel grip");
    ++g_cockpitShiftUp;
    auto before=s_armHover.wrist;
    UpdateDriverHover(0.02f);
    Check(s_armControlOverride && Length(s_armHover.wrist-before)<=DriverHoverMotion::speed*0.02f+1e-5f,"shift interrupts steering without teleport");
    double after=GetTickCount64()*0.001+10;
    for(int i=0;i<200;++i) UpdateDriverHover(0.02f,after+i*0.02);
    Check(Length(s_armHover.wrist-grip.wrist)<1e-5f && s_armControlBlend==0,"control returns to current rotating grip");
    g_steeringWheelAngle=-210;
    UpdateDriverHover(0.02f,after+5);
    std::vector<V3> heads,tails;
    DriverArmPose(Identity(),heads,tails);
    auto l=NativeGrip(s_driverIK,0,Identity());
    Point solvedLeft=l.wrist, solvedElbow;
    DriverSteering::Solve({-0.4f,0.3f,0},{0,0.3f,0},{0.4f,0.3f,0},solvedLeft,solvedElbow);
    Check(Length(Point{heads[2].x,heads[2].y,heads[2].z}-solvedLeft)<1e-5f,"native left hand uses reach-limited steering");
    s_handbrakeGripValid=true; s_handbrakeGripWorld={0.2f,-0.5f,-0.2f};
    g_cockpitHandbrake=1;
    before=s_armHover.wrist;
    UpdateDriverHover(0.02f,after+6);
    Check(s_armControlOverride && Length(s_armHover.wrist-before)<=DriverHoverMotion::speed*0.02f+1e-5f,"brake interrupts steering continuously");
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f,after+6+i*0.02);
    ++g_cockpitShiftDown;
    UpdateDriverHover(0.02f,after+9);
    Check(s_armShiftPending,"shift interrupts held brake during steering");
    for(int i=0;i<200;++i) UpdateDriverHover(0.02f,after+9+i*0.02);
    Check(s_armControlOverride && !s_armShiftPending,"held brake restored after shift while steering");
    g_cockpitHandbrake=0;
    for(int i=0;i<100;++i) UpdateDriverHover(0.02f,after+14+i*0.02);
    auto r=NativeGrip(s_driverIK,1,Identity());
    Check(Length(s_armHover.wrist-r.wrist)<1e-5f && s_armControlBlend==0,"brake releases to live negative steering grip");
    static uint8_t solid[256]{}, plat[128]{}, entry[kEntrySize]{}, format[16]{};
    static FakeIb ib;
    static uint16_t indices[]={0,1,2,3,4,5};
    static float vertices[]={0.7f,-0.3f,0,0.8f,-0.3f,0,0.8f,-0.31f,0,
                             0.7f,0.3f,0,0.8f,0.3f,0,0.8f,0.31f,0};
    strcpy((char*)solid+kSolidName,"STEERING_TEST_DRIVER_A");
    At<uint8_t*>(solid,kSolidPlat)=plat;
    At<int>(plat,kPlatNumEntries)=1; At<uint8_t*>(plat,kPlatEntries)=entry;
    At<uint16_t*>(plat,kPlatIndices)=indices; At<int>(plat,kPlatNumIndices)=6;
    At<IDirect3DIndexBuffer9*>(plat,kPlatIndexBuffer)=&ib;
    At<int>(format,8)=12; At<uint8_t*>(entry,kEntryFormat)=format;
    At<float*>(entry,kEntryVertices)=vertices; At<int>(entry,kEntryVertexBytes)=sizeof(vertices);
    strcpy(s_solids[solid].name,"STEERING_TEST_DRIVER_A");
    auto& marks=EditsFor("STEERING_TEST_DRIVER_A").arms;
    marks.insert(TriId(0,1,2)); marks.insert(TriId(3,4,5));
    s_armDraws[&ib]={solid,Identity()};
    float output[18]; memcpy(output,vertices,sizeof(output));
    Check(Cockpit_HasMarkedArmDraw(&ib),"steering enables native draw without control reach");
    Check(Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)output,sizeof(output)),"native steering deforms marked draw");
    Check(memcmp(output,vertices,9*sizeof(float))!=0 && memcmp(output+9,vertices+9,9*sizeof(float))!=0,"both marked arms deform");
    marks.clear(); memcpy(output,vertices,sizeof(output));
    Check(!Cockpit_PoseMarkedArms(&ib,0,2,0,12,0,-1,(unsigned char*)output,sizeof(output)) && memcmp(output,vertices,sizeof(output))==0,"unmarked native mesh preserved during steering");
    s_nativeWheelCar++;
    Check(!NativeSteeringActive(),"car change rejects stale wheel measurement");
    s_nativeWheelCar=s_head.worldId;
    s_nativeDriverBody=Identity();
    g_steeringWheelAngle=90;
    auto baseline=NativeGrip(s_driverIK,1,Identity());
    M4 moved=Identity();
    moved.m[0][0]=0; moved.m[0][1]=1;
    moved.m[1][0]=-1; moved.m[1][1]=0;
    moved.m[3][0]=123; moved.m[3][1]=-456; moved.m[3][2]=25;
    s_nativeDriverBody=moved;
    s_curBody={}; // Actual render hook restores this before the deferred pose.
    auto deferred=NativeGrip(s_driverIK,1,moved);
    Check(Length(baseline.wrist-deferred.wrist)<0.0001f,"deferred pose independent of restored hook matrix");
    Check(Length(baseline.direction-deferred.direction)<0.0001f,"deferred wrist orientation follows retained body");
    if(argc>1) {
        Check(DriverIK::LoadFile(argv[1],&s_driverIK),"load installed native rig");
        s_nativeWheelPivot={0.4647f,0.4202f,0.5987f};
        s_nativeWheelAxis={0.9701f,0,-0.2425f};
        s_nativeDriverBody=moved; s_driverIKWorld=moved;
        g_steeringWheelAngle=0; s_armControlBlend=0; s_armHover.Reset();
        // Check actual solved wrist, not just the synthetic requested target.
        for(float fix:{-60.f,-25.f,25.f,60.f}) {
            g_cfg.animatedDriverHandFix=fix;
            std::vector<V3> previous;
            for(int a=-450;a<=450;++a) {
                g_steeringWheelAngle=float(a);
                DriverArmPose(moved,heads,tails);
                for(int side=0;side<2;++side) {
                    for(int j=0;j<2;++j) {
                        int b=s_driverIK.arms[side*3+j];
                        auto& rest=s_driverIK.bones[b];
                        Near(Length({tails[b].x-heads[b].x,tails[b].y-heads[b].y,tails[b].z-heads[b].z}),
                            Length({rest.tail[0]-rest.head[0],rest.tail[1]-rest.head[1],rest.tail[2]-rest.head[2]}),
                            "installed rig hand fix preserves arm bone lengths");
                        if(!previous.empty()) Check(Length({heads[b].x-previous[b].x,
                            heads[b].y-previous[b].y,heads[b].z-previous[b].z})<0.03f,
                            "installed rig hand fix elbow remains continuous");
                    }
                    auto grip=NativeGrip(s_driverIK,side,moved);
                    auto unadjusted=grip;
                    float setting=g_cfg.animatedDriverHandFix;
                    g_cfg.animatedDriverHandFix=0;
                    unadjusted=NativeGrip(s_driverIK,side,moved);
                    g_cfg.animatedDriverHandFix=setting;
                    Near(grip.release,unadjusted.release,"installed hand fix keeps transfer schedule");
                    if(a==0) Check(Length(grip.wrist-unadjusted.wrist)>0.001f,
                        "live per-car setting adjusts installed neutral grips");
                }
                previous=heads;
            }
        }
        g_cfg.animatedDriverHandFix=0;
        for(float axisSign:{1.f,-1.f}) {
            s_nativeWheelAxis={0.9701f*axisSign,0,-0.2425f*axisSign};
            auto normal=Unit(Point{s_nativeWheelAxis.x,s_nativeWheelAxis.y,s_nativeWheelAxis.z});
            auto up=Unit(Point{0,0,1}-normal*Dot({0,0,1},normal));
            const auto& rest=s_driverIK.bones[s_driverIK.arms[5]];
            Point pivot{s_nativeWheelPivot.x,s_nativeWheelPivot.y,s_nativeWheelPivot.z};
            Point radial=Point{rest.head[0],rest.head[1],rest.head[2]}-pivot;
            auto fittedRight=Unit(radial-normal*Dot(radial,normal)-up*Dot(radial,up));
            for(float angle:{-180.f,-270.f,-295.f,-310.f,-360.f,-400.f,-450.f}) {
                g_steeringWheelAngle=angle;
                DriverArmPose(moved,heads,tails);
                auto w=heads[s_driverIK.arms[5]];
                Point offset=Point{w.x,w.y,w.z}-pivot;
                if(angle>=-310) Check(Dot(offset,fittedRight)>0,
                    "installed rig solved wait and regrip stay driver-right for either axis sign");
                if(angle>=-310) Check(Dot(offset,up)<0,"installed rig wait and contact stay bottom-right");
                auto l=heads[s_driverIK.arms[2]];
                Check(Length(Point{w.x-l.x,w.y-l.y,w.z-l.z})>0.04f,
                    "installed rig regrip and final lock stay clear of left hand for either axis sign");
            }
        }
        s_nativeWheelAxis={0.9701f,0,-0.2425f};
        g_steeringWheelAngle=0;
        DriverArmPose(moved,heads,tails);
        for(int side=0;side<2;++side) {
            int wrist=s_driverIK.arms[side*3+2];
            Near(heads[wrist].x,s_driverIK.bones[wrist].head[0],"real rig neutral wrist x preserved");
            Near(heads[wrist].z,s_driverIK.bones[wrist].head[2],"real rig neutral wrist height preserved");
        }
        std::vector<V3> previousHeads;
        float largestStep=0, smallestClearance=100, clearanceAngle=0;
        float previousCuff[2]={}, largestCuffStep=0;
        int clearanceLeft=0, clearanceRight=0;
        for(int step=-22500;step<=22500;++step) {
            g_steeringWheelAngle=step*0.02f;
            DriverArmPose(moved,heads,tails);
            for(int side=0;side<2;++side) {
                int f=s_driverIK.arms[side*3+1], h=s_driverIK.arms[side*3+2];
                auto p=[](const float* v){return Point{v[0],v[1],v[2]};};
                Point restFore=p(s_driverIK.bones[f].tail)-p(s_driverIK.bones[f].head);
                Point restHand=p(s_driverIK.bones[h].tail)-p(s_driverIK.bones[h].head);
                Point posedFore{tails[f].x-heads[f].x,tails[f].y-heads[f].y,tails[f].z-heads[f].z};
                Point posedHand{tails[h].x-heads[h].x,tails[h].y-heads[h].y,tails[h].z-heads[h].z};
                float cuff=DriverSteering::CuffRoll(restFore,posedFore,restHand,posedHand,
                    {s_nativeWheelAxis.x,s_nativeWheelAxis.y,s_nativeWheelAxis.z},NativeGrip(s_driverIK,side,moved).roll);
                Check(std::isfinite(cuff) && fabsf(cuff)<=0.349066f,"installed rig cuff correction bounded across full sweep");
                if(step>-22500) {
                    largestCuffStep=std::max(largestCuffStep,fabsf(cuff-previousCuff[side]));
                    Check(fabsf(cuff-previousCuff[side])<0.01f,"installed rig cuff correction continuous through positive and negative regrips");
                }
                previousCuff[side]=cuff;
            }
            if(g_steeringWheelAngle>=90 || g_steeringWheelAngle<=-100) {
                int u=s_driverIK.arms[3], f=s_driverIK.arms[4], h=s_driverIK.arms[5];
                auto p=[](const float* v){return Point{v[0],v[1],v[2]};};
                auto target=NativeGrip(s_driverIK,1,moved).wrist;
                Point naturalElbow;
                Check(DriverSteering::Solve(p(s_driverIK.bones[u].head),p(s_driverIK.bones[f].head),
                    p(s_driverIK.bones[h].head),target,naturalElbow),"natural right elbow solves");
                Check(Length(naturalElbow-Point{heads[f].x,heads[f].y,heads[f].z})<1e-5f,
                    "lift keeps transported fitted bend plane without fixed-elbow blend");
            }
            if(step%5==0 && (g_steeringWheelAngle>=90 || g_steeringWheelAngle<=-100) &&
                (g_steeringWheelAngle<0 || g_steeringWheelAngle<=450)) {
                for(int lb:{s_driverIK.arms[0],s_driverIK.arms[1],s_driverIK.arms[2]})
                    for(int rb:{s_driverIK.arms[3],s_driverIK.arms[4],s_driverIK.arms[5]})
                        for(int a=0;a<=12;++a) for(int b=0;b<=12;++b) {
                            auto p=[](V3 v){return Point{v.x,v.y,v.z};};
                            auto l=Mix(p(heads[lb]),p(tails[lb]),a/12.f);
                            auto r=Mix(p(heads[rb]),p(tails[rb]),b/12.f);
                            if(Length(l-r)<smallestClearance) {
                                smallestClearance=Length(l-r); clearanceAngle=g_steeringWheelAngle;
                                clearanceLeft=lb; clearanceRight=rb;
                            }
                        }
            }
            for(int side=0;side<2;++side) {
                int upper=s_driverIK.arms[side*3], fore=s_driverIK.arms[side*3+1];
                for(int b:{upper,fore}) {
                    auto& rest=s_driverIK.bones[b];
                    Near(Length({tails[b].x-heads[b].x,tails[b].y-heads[b].y,tails[b].z-heads[b].z}),
                        Length({rest.tail[0]-rest.head[0],rest.tail[1]-rest.head[1],rest.tail[2]-rest.head[2]}),"real rig bone lengths preserved across full sweep");
                }
                if(!previousHeads.empty()) {
                    float delta=Length({heads[fore].x-previousHeads[fore].x,heads[fore].y-previousHeads[fore].y,heads[fore].z-previousHeads[fore].z});
                    largestStep=std::max(largestStep,delta);
                    if(delta>=0.003f) printf("Discontinuous elbow: side %d degrees %.1f step %.6f\n",side,g_steeringWheelAngle,delta);
                    Check(delta<0.003f,"elbow continuous across reach boundary and 70 degrees");
                }
            }
            previousHeads=heads;
        }
        printf("PASS: real rig 0.02-degree sweep, maximum elbow step %.6f m\n",largestStep);
        printf("PASS: installed rig cuff sweep maximum angular step %.6f radians\n",largestCuffStep);
        printf("Real rig sampled arm centreline clearance during transfer: %.6f m at %.1f degrees bones %d/%d\n",smallestClearance,clearanceAngle,clearanceLeft,clearanceRight);
        Check(smallestClearance>0.04f,"real rig transfer clears left arm centrelines");
        for(Point target:{Point{100,0,0},Point{-100,0,0},Point{-0.08f,0.222f,0.67f}}) {
            int u=s_driverIK.arms[3], f=s_driverIK.arms[4], h=s_driverIK.arms[5];
            auto p=[](const float* v){return Point{v[0],v[1],v[2]};};
            Point elbow, shoulder=p(s_driverIK.bones[u].head);
            Point restElbow=p(s_driverIK.bones[f].head), restWrist=p(s_driverIK.bones[h].head);
            Check(DriverSteering::Solve(shoulder,restElbow,restWrist,target,elbow,{0,-0.08f,-0.1f}),"unreachable target solved with brake pole bias");
            Near(Length(elbow-shoulder),Length(restElbow-shoulder),"unreachable upper arm never stretches");
            Near(Length(target-elbow),Length(restWrist-restElbow),"unreachable forearm never stretches");
        }
        s_armShiftPending=false; s_armShiftUntil=-1; g_cockpitHandbrake=0;
        g_steeringWheelAngle=180;
        UpdateDriverHover(0.02f,after+30);
        auto wheelWrist=s_armHover.wrist;
        s_shifterHoverWorld=ToWorld({0.35f,0.17f,0.45f},moved);
        ++g_cockpitShiftUp;
        UpdateDriverHover(0.02f,after+30.02);
        Check(s_armControlOverride && Length(s_armHover.wrist-wheelWrist)>0.001f,"real rig shifter moves hand with deferred car transform");
        for(int i=0;i<200;++i) UpdateDriverHover(0.02f,after+30.04+i*0.02);
        s_handbrakeGripWorld=ToWorld({0.2f,0.1f,0.3f},moved);
        g_cockpitHandbrake=1;
        auto beforeBrake=s_armHover.wrist;
        UpdateDriverHover(0.02f,after+35);
        Check(s_armControlOverride && Length(s_armHover.wrist-beforeBrake)>0.001f,"real rig brake moves hand with deferred car transform");
        puts("PASS: installed rig neutral height, steering, shift and brake under nonidentity deferred transform");
    }
    puts("PASS: right-only degree-driven transfer, phase continuity, left tracking, IK and shift interruption");
    return 0;
}