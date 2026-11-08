// Frozen pre-hoisting reference for differential tests only.
bool ReferencePoseMarkedArms(void* ib, unsigned first, unsigned triangles, int base,
                           unsigned stride, unsigned positionOffset, int normalOffset,
                           unsigned char* vertices, size_t bytes) {
    TraceDetailScope poseTiming(TRACE_DRIVER_POSE);
    auto draw = s_armDraws.find(ib);
    if (draw == s_armDraws.end() || !vertices || stride < 12 || positionOffset + 12 > stride ||
        (!NativeSteeringActive() && s_armHover.progress <= 0.0001f)) return false;
    const DriverIK::Rig* rig = DriverRig();
    if (!rig) return false;
    SolidMesh mesh;
    if (!ReadMesh(draw->second.solid, mesh) || first > (unsigned)mesh.numIndices ||
        triangles > ((unsigned)mesh.numIndices - first) / 3) return false;
    auto info = s_solids.find(draw->second.solid);
    if (info == s_solids.end()) return false;
    const auto& edits = EditsFor(SolidName(info->second));
    const auto& marks = edits.arms;
    std::set<unsigned> selected, automaticOnly;
    for (unsigned t = 0; t < triangles; ++t) {
        const uint16_t* v = mesh.indices + first + t * 3;
        bool manual=marks.count(TriId(v[0], v[1], v[2]))!=0;
        if (manual || (!edits.native.count(TriId(v[0],v[1],v[2])) && info->second.driver &&
            (AutomaticDriverFingerIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3) ||
             AutomaticDriverArmIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3))))
            for (int j = 0; j < 3; ++j) if ((int)v[j] + base >= 0) {
                selected.insert(v[j] + base);
                if(!manual) automaticOnly.insert(v[j]+base);
            }
    }
    // Explicit editor marks retain their meaning, even if adjacent automatic
    // triangles share their vertices.
    for(unsigned t=0;t<triangles;++t) {
        const uint16_t* v=mesh.indices+first+t*3;
        if(marks.count(TriId(v[0],v[1],v[2])))
            for(int j=0;j<3;++j) if((int)v[j]+base>=0) automaticOnly.erase(v[j]+base);
    }
    // Shared sleeve/body seam vertices must not drag adjacent torso triangles.
    // Inspect all entries, not just this draw's subrange.
    if(info->second.driver && info->second.driver->rig.ok) {
        const Driver& driver=*info->second.driver;
        for(int i=0;i<mesh.n && i<(int)driver.body.size() && i<(int)driver.arms.size();++i) {
            uint8_t* entry=mesh.entries+i*kEntrySize;
            int begin=At<int>(entry,kEntryFirstIndex), count=At<int>(entry,kEntryTris);
            if(begin<0 || count<0 || begin>mesh.numIndices || count>(mesh.numIndices-begin)/3) continue;
            for(int t=0;t<count;++t) {
                const uint16_t* v=mesh.indices+begin+t*3;
                if(t>=driver.body[i] && t<driver.body[i]+driver.arms[i] &&
                    !edits.native.count(TriId(v[0],v[1],v[2]))) continue;
                for(int j=0;j<3;++j) if((int)v[j]+base>=0 && automaticOnly.count(v[j]+base))
                    selected.erase(v[j]+base);
            }
        }
    }
    std::vector<V3> head, tail;
    DriverArmPose(draw->second.world, head, tail);
    if (head.empty()) return false;
    auto point = [](const float* p) { return CustomDriver::Point{p[0], p[1], p[2]}; };
    bool changed = false;
    for (unsigned v : selected) {
        int localVertex = (int)v - base;
        if ((size_t)v * stride + positionOffset + 12 > bytes || localVertex < 0 || localVertex >= mesh.numVerts) continue;
        auto p = point(mesh.pos(localVertex));
        float automaticWeight=automaticOnly.count(v) && info->second.driver && info->second.driver->rig.ok ?
            NativeArmWeight(*rig,p) : 1.0f;
        if(automaticWeight<=0) continue;
        // Bind to the nearest arm segment on either side, not to a global Y threshold.
        float best = 1e9f; int nearest = -1;
        for (int side = 0; side < 2; ++side) {
            int candidates[4] = {rig->arms[side*3],rig->arms[side*3+1],rig->arms[side*3+2],rig->Find(side ? "JNT_hand_tip.R" : "JNT_hand_tip.L")};
            for (int b : candidates) if (b >= 0) {
                auto a = point(rig->bones[b].head), d = point(rig->bones[b].tail) - a;
                float t = std::max(0.0f, std::min(1.0f, CustomDriver::Dot(p-a,d) / std::max(1e-10f,CustomDriver::Dot(d,d))));
                float distance = CustomDriver::Length(p-(a+d*t));
                if (distance < best) { best = distance; nearest = (side==ControlSide() || NativeSteeringActive()) ? b : -1; }
            }
        }
        if (nearest < 0) continue;
        int b = nearest;
        auto restHead = point(rig->bones[b].head), restDir = point(rig->bones[b].tail)-restHead;
        CustomDriver::Point posedHead{head[b].x,head[b].y,head[b].z};
        CustomDriver::Point posedDir{tail[b].x-head[b].x,tail[b].y-head[b].y,tail[b].z-head[b].z};
        float cuffRoll=0;
        if(NativeSteeringActive()) for(int side=0;side<2;++side) {
            if(b!=rig->arms[side*3+1] || (side==ControlSide() && (s_armControlBlend>0 || s_armControlOverride))) continue;
            float along=CustomDriver::Dot(p-restHead,restDir)/std::max(1e-10f,CustomDriver::Dot(restDir,restDir));
            int hand=rig->arms[side*3+2];
            auto restHand=point(rig->bones[hand].tail)-point(rig->bones[hand].head);
            CustomDriver::Point posedHand{tail[hand].x-head[hand].x,tail[hand].y-head[hand].y,tail[hand].z-head[hand].z};
            M4 inv=MInvRigid(draw->second.world);
            V3 axis=ToWorld(ToWorld(s_nativeWheelPivot+s_nativeWheelAxis,s_nativeDriverBody),inv)-
                ToWorld(ToWorld(s_nativeWheelPivot,s_nativeDriverBody),inv);
            cuffRoll=DriverSteering::SleeveCuffRoll(side,restDir,posedDir,restHand,posedHand,
                {axis.x,axis.y,axis.z},NativeGrip(*rig,side,draw->second.world).roll)*
                DriverSteering::CuffWeight(along);
        }
        auto deformDirection = [&](CustomDriver::Point value) {
            if (NativeSteeringActive()) for (int side=0;side<2;++side) {
                if (b!=rig->arms[side*3+2] && b!=rig->Find(side ? "JNT_hand_tip.R" : "JNT_hand_tip.L")) continue;
                auto grip=NativeGrip(*rig,side,draw->second.world);
                float roll=grip.roll*(side==ControlSide() ? 1-s_armControlBlend : 1);
                M4 inv=MInvRigid(draw->second.world);
                V3 axis=ToWorld(ToWorld(s_nativeWheelPivot+s_nativeWheelAxis,s_nativeDriverBody),inv)-
                    ToWorld(ToWorld(s_nativeWheelPivot,s_nativeDriverBody),inv);
                CustomDriver::Point k{axis.x,axis.y,axis.z};
                return CustomDriver::Rotate(CustomDriver::Twist(value,k,roll),
                    CustomDriver::Twist(restDir,k,roll),posedDir);
            }
            auto rotated=CustomDriver::Rotate(value,restDir,posedDir);
            return cuffRoll!=0 ? CustomDriver::Twist(rotated,posedDir,cuffRoll) : rotated;
        };
        float* dst = (float*)(vertices + (size_t)v*stride + positionOffset);
        auto result = CustomDriver::Mix(p,posedHead + deformDirection(p-restHead),automaticWeight);
        memcpy(dst,&result,12);
        if (normalOffset >= 0 && (unsigned)normalOffset+12 <= stride) {
            float* normal = (float*)(vertices+(size_t)v*stride+normalOffset);
            auto original=point(normal);
            auto rotated = CustomDriver::Unit(CustomDriver::Mix(original,deformDirection(original),automaticWeight));
            memcpy(normal,&rotated,12);
        }
        changed = true;
    }
    return changed;
}
