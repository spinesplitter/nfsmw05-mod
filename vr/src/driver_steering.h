#pragma once
#include "custom_driver_animation.h"

// Absolute wheel degrees, not frame time. Only the right hand transfers its grip.
// The fitted native wrist is the neutral grip; no replacement mesh or hand spread.
namespace DriverSteering {
using namespace CustomDriver;
struct Grip {
    Point wrist, direction;
    float roll = 0, release = 0;
};
inline float Phase(float degrees, float begin, float end) {
    float t = std::max(0.0f, std::min(1.0f, (degrees-begin)/(end-begin)));
    return t*t*t*(t*(t*6-15)+10); // zero velocity and acceleration at phase boundaries
}
inline float TransferSettle(float degrees) {
    return degrees<0 ? Phase(-degrees,270,310) : Phase(degrees,270,325);
}
inline Grip Evaluate(Point wrist, Point direction, Point pivot, Point axis,
                      float degrees, int side, float handFix = 0) {
    if (std::isfinite(handFix) && handFix != 0) {
        Point normal = Unit(axis);
        Point up = Unit(Point{0,0,1}-normal*Dot({0,0,1},normal), {0,1,0});
        Point radial = wrist-pivot;
        Point planar = radial-normal*Dot(radial,normal);
        Point lateral = planar-up*Dot(planar,up);
        float radius = Length(planar);
        if (radius>1e-6f && Length(lateral)>1e-6f) {
            float elevation = std::atan2(Dot(planar,up),Length(lateral));
            float requested = elevation+std::max(-60.0f,std::min(60.0f,handFix))*0.01745329252f;
            // Never move a grip across the top/bottom into the other hand's side.
            float delta = std::max(-1.483529864f,std::min(1.483529864f,requested))-elevation;
            float turn = Dot(Cross(normal,planar),up)>=0 ? delta : -delta;
            auto grip = Evaluate(pivot+Twist(radial,axis,turn),Twist(direction,axis,turn),
                pivot,axis,degrees,side);
            grip.roll += turn; // deformation rolls relative to the original fitted hand
            return grip;
        }
    }
    if (!std::isfinite(degrees)) degrees = 0;
    degrees = std::max(-450.0f, std::min(450.0f, degrees));
    float roll = degrees * 0.01745329252f;
    Point target = pivot + Twist(wrist - pivot, axis, roll);
    if (side != 1 || (degrees >= -100 && degrees <= 90))
        return {target, Twist(direction, axis, roll), roll, 0};
    Point normal = Unit(axis);
    Point up = Unit(Point{0,0,1}-normal*Dot({0,0,1},normal), {0,1,0});
    Point radial = wrist-pivot;
    float radius = Length(radial-normal*Dot(radial,normal));
    if (degrees>0) {
        // Detach from wheel rotation, wait bottom-right, then make contact at
        // +325. Only after contact does the target resume rotating with the rim.
        Point right = Unit(Cross(normal,up),{0,-1,0});
        Point rim = Unit(right-up)*radius + normal*Dot(radial,normal);
        float rimRoll = std::atan2(Dot(normal,Cross(radial,rim)),
            Dot(radial-normal*Dot(radial,normal),rim-normal*Dot(rim,normal)));
        float depart = Phase(degrees,90,165);
        float move = Phase(degrees,90,165);
        float contact = Phase(degrees,270,325);
        Point lift = up*(radius*0.15f)-normal*(radius*0.30f)+right*(radius*0.25f);
        Point waiting = pivot+rim-normal*(radius*0.75f)+right*(radius*0.50f);
        Point lifted = pivot+Twist(radial,axis,90*0.01745329252f)+lift;
        Point released = Mix(Mix(target,lifted,depart),waiting,move)
            -normal*(radius*0.60f*depart*(1-move));
        float attachedRoll = rimRoll+(degrees-325)*0.01745329252f;
        Point attached = pivot+Twist(radial,axis,attachedRoll);
        float wristRoll = roll+(rimRoll-roll)*depart;
        wristRoll += (attachedRoll-wristRoll)*contact;
        return {Mix(released,attached,contact),Twist(direction,axis,wristRoll),
            wristRoll,depart*(1-contact)};
    }
    // Negative steering moves diagonally right/down into a bottom-right wait.
    // Reversals retrace the same path; wheel tracking resumes at -310.
    Point right = Unit(Cross(normal,up),{0,-1,0});
    // The measured wheel axis may point either way. Its cross product is not
    // a reliable driver-right reference; use the fitted neutral right grip.
    Point fittedRight = radial-normal*Dot(radial,normal)-up*Dot(radial,up);
    if (Dot(right,fittedRight)<0) right=right*-1;
    Point rim = Unit(right-up)*radius + normal*Dot(radial,normal);
    float rimRoll = std::atan2(Dot(normal,Cross(radial,rim)),
        Dot(radial-normal*Dot(radial,normal),rim-normal*Dot(rim,normal)));
    float depart = Phase(-degrees,100,180);
    float contact = TransferSettle(degrees);
    Point waiting = pivot+rim-normal*(radius*0.75f)+right*(radius*0.50f);
    Point released = Mix(target,waiting,depart)
        -normal*(radius*0.60f*depart*(1-depart));
    float attachedRoll = rimRoll+(degrees+310)*0.01745329252f;
    Point attached = pivot+Twist(radial,axis,attachedRoll);
    float wristRoll = roll+(rimRoll-roll)*depart;
    wristRoll += (attachedRoll-wristRoll)*contact;
    return {Mix(released,attached,contact),Twist(direction,axis,wristRoll),
        wristRoll,depart*(1-contact)};
}

// Soft reach keeps a small elbow bend and approaches the limit with a continuous
// first derivative. The fitted neutral pose is inside the unchanged region.
inline bool Solve(Point shoulder, Point restElbow, Point restWrist, Point& wrist,
                  Point& elbow, Point elbowOffset = {}, float poleTurn = 0,
                  float quietElbow = 0) {
    float upper = Length(restElbow - shoulder), lower = Length(restWrist - restElbow);
    float distance = Length(wrist - shoulder);
    if (upper < 1e-6f || lower < 1e-6f || !std::isfinite(distance)) return false;
    Point restDir = Unit(restWrist - shoulder);
    Point dir = Unit(wrist - shoulder, restDir);
    float total = upper + lower;
    float maximum = std::min(total - 1e-5f,
        std::max(total * 0.98f, Length(restWrist - shoulder)));
    float minimum = std::fabs(upper - lower) + std::min(0.005f, std::min(upper,lower)*0.1f);
    float width = std::max(1e-5f, std::min(0.03f,
        (maximum - Length(restWrist - shoulder)) * 0.5f));
    float start = maximum - width;
    if (distance > start)
        distance = maximum - width * std::exp(-(distance-start)/width);
    // Smooth the inner limit too: avoid a folded-arm singularity near shoulder.
    float innerWidth = std::min(0.01f, (maximum-minimum)*0.1f);
    if (distance < minimum + innerWidth)
        distance = minimum + innerWidth * std::exp((distance-minimum-innerWidth)/innerWidth);
    wrist = shoulder + dir * distance;
    // Project the fitted elbow onto the valid IK circle: the nearest possible
    // elbow to its fitted position, rather than rotating it around the shoulder.
    Point hint = Mix(Rotate(restElbow - shoulder, restDir, dir),restElbow-shoulder,quietElbow);
    Point fittedPole = Unit(hint - dir * Dot(hint, dir), Unit(Cross(dir, {0,0,1}), {0,1,0}));
    // Bias the pole, never displace the solved elbow or change either bone length.
    Point bias = elbowOffset - dir * Dot(elbowOffset,dir);
    Point pole = Unit(fittedPole + bias * (0.5f / std::max(0.1f,Length(bias))), fittedPole);
    if (poleTurn != 0) pole = Twist(pole,dir,poleTurn);
    float along = (upper * upper - lower * lower + distance * distance) / (2 * distance);
    elbow = shoulder + dir * along + pole * std::sqrt(std::max(0.0f, upper * upper - along * along));
    return true;
}
inline bool Elbow(Point shoulder, Point restElbow, Point restWrist, Point wrist, Point& elbow) {
    return Solve(shoulder,restElbow,restWrist,wrist,elbow);
}
}