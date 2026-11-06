#define main ElbowAuditBaseline
#include "cockpit_tests.cpp"
#undef main

int main(int argc,char** argv) {
    Check(argc==3,"rig path and car name required");
    Check(DriverIK::LoadFile(argv[1],&s_driverIK),"load audit rig");
    s_driverIKTried=true; s_head.car=argv[2]; s_head.valid=true; s_head.worldId=123;
    Driver driver; driver.ok=true; driver.car=s_head.car; driver.rig=s_driverIK; driver.rigTried=true;
    uint8_t solid[1]{}; s_solids[solid].driver=&driver;
    s_nativeWheelCar=123; s_nativeWheelValid=true;
    s_nativeDriverBody=Identity(); s_driverIKWorld=Identity(); s_driverIKWorldValid=true;
    auto p=[](const float* v) { return CustomDriver::Point{v[0],v[1],v[2]}; };
    using namespace CustomDriver;
    Point centre=(p(s_driverIK.bones[s_driverIK.arms[2]].head)+p(s_driverIK.bones[s_driverIK.arms[5]].head))*0.5f;
    s_nativeWheelPivot={centre.x+0.07f,centre.y,centre.z-0.065f};
    s_nativeWheelAxis={0.9701f,0,-0.2425f};
    char config[MAX_PATH];
    const char* game="T:\\Games\\ModWanted\\driverNeed For Speed Most Wanted Black Edition - Copy\\";
    snprintf(config,sizeof(config),"%sscripts\\nfsmw_vr_driver.ini",game);
    char value[128]{};
    GetPrivateProfileStringA("AnimatedDriver",argv[2],"",value,sizeof(value),config);
    float enabled=1, handFix=0;
    sscanf(value,"%f,%f",&enabled,&handFix);
    g_cfg.animatedDriverHandFix=handFix;
    snprintf(config,sizeof(config),"%sscripts\\nfsmw_vr_wheel.ini",game);
    GetPrivateProfileStringA("WheelPosition",argv[2],"0,0",value,sizeof(value),config);
    float right=0,up=0; sscanf(value,"%f,%f",&right,&up);
    s_nativeWheelOffset={0,-right,up};
    s_nativeWheelPivot=s_nativeWheelPivot+s_nativeWheelOffset;
    printf("CONFIG %s handFix=%.1f wheelRight=%.4f wheelUp=%.4f\n",argv[2],handFix,right,up);
    for(int side=0;side<2;++side) {
        Point shoulder=p(s_driverIK.bones[s_driverIK.arms[side*3]].head);
        Point restElbow=p(s_driverIK.bones[s_driverIK.arms[side*3+1]].head);
        Point restWrist=p(s_driverIK.bones[s_driverIK.arms[side*3+2]].head);
        float maxStep=0, maxPoleStep=0, minProjection=100, worstAngle=0, worstBlend=0;
        float maxUp=-100, maxOut=-100;
        for(int blendStep=0;blendStep<=10;++blendStep) {
            float blend=blendStep*0.1f;
            if(side!=ControlSide() && blendStep) break;
            Point previous{},previousPole{}; bool havePrevious=false;
            for(int step=-4500;step<=4500;++step) {
                g_steeringWheelAngle=step*0.1f;
                auto grip=NativeGrip(s_driverIK,side,Identity());
                // Fixed representative console target; not measured car geometry.
                Point target={centre.x-0.20f,centre.y+(side?-0.15f:0.15f),centre.z-0.15f};
                Point wrist=Mix(grip.wrist,target,blend),elbow;
                float quiet=0; // match the transported bend plane used by DriverArmPose
                Point dir=Unit(wrist-shoulder);
                Point hint=Mix(Rotate(restElbow-shoulder,Unit(restWrist-shoulder),dir),restElbow-shoulder,quiet);
                minProjection=std::min(minProjection,Length(hint-dir*Dot(hint,dir)));
                Check(DriverSteering::Solve(shoulder,restElbow,restWrist,wrist,elbow,{},0,quiet),"solve audit pose");
                dir=Unit(wrist-shoulder);
                Point pole=Unit(elbow-shoulder-dir*Dot(elbow-shoulder,dir));
                if(havePrevious) {
                    float distance=Length(elbow-previous);
                    if(distance>maxStep) { maxStep=distance; worstAngle=g_steeringWheelAngle; worstBlend=blend; }
                    maxPoleStep=std::max(maxPoleStep,std::acos(std::max(-1.0f,std::min(1.0f,Dot(pole,previousPole))))*57.29578f);
                }
                maxUp=std::max(maxUp,elbow.z-shoulder.z);
                maxOut=std::max(maxOut,(elbow.y-shoulder.y)*(side?-1.0f:1.0f));
                previous=elbow; previousPole=pole; havePrevious=true;
            }
        }
        printf("%s side=%d step_mm=%.3f pole_step_deg=%.3f min_hint_mm=%.3f worst_angle=%.1f blend=%.1f up_mm=%.1f outward_mm=%.1f\n",
            argv[2],side,maxStep*1000,maxPoleStep,minProjection*1000,worstAngle,worstBlend,maxUp*1000,maxOut*1000);
    }
}