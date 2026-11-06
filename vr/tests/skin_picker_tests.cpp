#define main CockpitRegressionMain
#include "cockpit_tests.cpp"
#undef main

int main() {
    uint8_t solid[256]{}, plat[128]{}, entries[2*kEntrySize]{}, format[16]{};
    uint16_t indices[] = {0,1,2, 3,4,5, 0,4,2};
    float vertices[18]{};
    FakeIb ib;
    At<uint8_t*>(solid,kSolidPlat)=plat;
    At<int>(plat,kPlatNumEntries)=2;
    At<uint8_t*>(plat,kPlatEntries)=entries;
    At<uint16_t*>(plat,kPlatIndices)=indices;
    At<int>(plat,kPlatNumIndices)=9;
    At<IDirect3DIndexBuffer9*>(plat,kPlatIndexBuffer)=&ib;
    At<int>(format,8)=12;
    At<uint8_t*>(entries,kEntryFormat)=format;
    At<float*>(entries,kEntryVertices)=vertices;
    At<int>(entries,kEntryVertexBytes)=sizeof(vertices);
    uint8_t* fingers=entries+kEntrySize;
    memset(entries,0x11,0x18);
    memset(fingers,0x22,0x18);
    At<int>(fingers,kEntryTris)=3;
    At<int>(fingers,kEntryNumIndices)=9;
    Driver driver;
    auto& info=s_solids[solid]; info.driver=&driver;
    strcpy(info.name,"SKIN_PICKER_TEST");
    s_armDraws[&ib]={solid,M4{}};
    char drawName[128]{}; int drawEntry=-1;
    Check(Cockpit_DriverDrawInfo(&ib,0,3,drawName,sizeof(drawName),&drawEntry) && drawEntry==1 &&
        !strcmp(drawName,"SKIN_PICKER_TEST"),"GPU capture identifies registered driver entry without active steering");
    Check(!Cockpit_DriverDrawInfo(&ib,8,3,drawName,sizeof(drawName),&drawEntry),"GPU capture rejects out-of-range draw");
    Check(!Cockpit_DriverDrawInfo(nullptr,0,3,drawName,sizeof(drawName),&drawEntry),"GPU capture ignores unrelated buffers");
    Check(!Cockpit_DriverDrawInfo(&ib,0,3,nullptr,0,&drawEntry),"GPU capture validates output storage");
    s_armDraws.erase(&ib);
    auto& edits=EditsFor("SKIN_PICKER_TEST");
    {
        s_armDraws[&ib]={solid,M4{}};
        float stream[6][4]{};
        for (auto& v:stream) { v[0]=123.0f; v[1]=0.4f; v[2]=-0.93f; v[3]=456.0f; }
        edits.skin.insert(TriId(3,4,5));
        std::vector<uint16_t> drawIndices;
        Check(!Cockpit_DriverDrawWork(&ib,0,1,0,drawIndices) && drawIndices.empty(),
            "unaffected driver draw skips GPU stream readback");
        Check(Cockpit_DriverDrawWork(&ib,3,1,2,drawIndices) &&
            drawIndices==std::vector<uint16_t>({5,6,7}),
            "skin draw uses live CPU indices with base vertex applied");
        Check(!Cockpit_DriverDrawWork(&ib,3,1,-10,drawIndices) && drawIndices.empty(),
            "invalid remapping skips replacement draw");
        Check(!Cockpit_DriverDrawWork(nullptr,3,1,0,drawIndices),
            "unregistered buffers never request driver readback");
        Check(Cockpit_RepairSkinUV(&ib,0,3,0,16,4,(unsigned char*)stream,sizeof(stream)),
            "skin atlas V repaired without active steering");
        Check(fabs(stream[3][2]-0.07f)<0.00001f && stream[0][2]==-0.93f &&
            stream[3][0]==123.0f && stream[3][1]==0.4f && stream[3][3]==456.0f,
            "only selected V changes; other vertices and attributes preserved");
        Check(!Cockpit_RepairSkinUV(&ib,0,3,0,16,4,(unsigned char*)stream,sizeof(stream)),
            "atlas correction is idempotent and leaves positive V unchanged");
        stream[3][2]=-2.0f; stream[4][2]=-1.0f; stream[5][2]=0.0f;
        Check(Cockpit_RepairSkinUV(&ib,3,1,0,16,4,(unsigned char*)stream,sizeof(stream)) &&
            stream[3][2]==-2.0f && stream[4][2]==0.0f && stream[5][2]==0.0f,
            "only native negative atlas interval is decoded");
        Check(!Cockpit_RepairSkinUV(&ib,8,1,0,16,4,(unsigned char*)stream,sizeof(stream)) &&
            !Cockpit_RepairSkinUV(&ib,0,1,0,16,12,(unsigned char*)stream,sizeof(stream)),
            "UV repair rejects invalid draw ranges and declarations");
        edits.skin.clear(); edits.black.insert(TriId(3,4,5));
        driver.body={0,0}; driver.arms={0,3}; driver.heads={0,0};
        stream[3][2]=-0.93f;
        Check(!Cockpit_RepairSkinUV(&ib,3,1,0,16,4,(unsigned char*)stream,sizeof(stream)),
            "explicit black triangle excluded from automatic UV repair");
        edits.black.clear();
        Check(Cockpit_RepairSkinUV(&ib,3,1,0,16,4,(unsigned char*)stream,sizeof(stream)),
            "automatic finger triangles share atlas correction");
        driver.body.clear(); driver.arms.clear(); driver.heads.clear();
        s_armDraws.erase(&ib);
    }
    uint32_t resolvedTextures[5] = {101,102,103,104,105};
    CaptureDriverSkin(solid,fingers,77,reinterpret_cast<uint32_t>(resolvedTextures),88);
    Check(!driver.skinBindingValid,"secondary entry cannot replace hand skin binding");
    CaptureDriverSkin(solid,entries,77,reinterpret_cast<uint32_t>(resolvedTextures),88);
    resolvedTextures[0]=999;
    Check(driver.skinBindingValid && driver.skinShader==77 && driver.skinAux==88 &&
        driver.skinTextures[0]==101 && driver.skinTextures[4]==105,
        "resolved hand shader textures and auxiliary binding copied independently of caller stack");
    std::vector<std::array<uint8_t,kEntrySize>> parts;
    Check(!SkinParts(fingers,solid,parts),"no skin marks leaves native draw alone");
    edits.skin.insert(TriId(3,4,5));
    uint8_t original[kEntrySize]; memcpy(original,fingers,kEntrySize);
    std::vector<bool> skinRuns;
    Check(SkinParts(fingers,solid,parts,&skinRuns),"skin selection splits native material");
    Check(skinRuns.size()==3 && !skinRuns[0] && skinRuns[1] && !skinRuns[2],
        "only marked run receives resolved hand texture arguments");
    Check(parts.size()==3,"unmarked/skin/unmarked runs remain separate");
    Check(memcmp(parts[1].data(),entries,0x18)==0,"selected run uses skin shader and textures");
    Check(memcmp(parts[0].data(),fingers,0x18)==0 && memcmp(parts[2].data(),fingers,0x18)==0,
        "unselected runs keep original material");
    Check(At<int>(parts[1].data(),kEntryFirstIndex)==3 && At<int>(parts[1].data(),kEntryTris)==1 &&
        At<int>(parts[1].data(),kEntryNumIndices)==3,"skin run keeps correct geometry range");
    Check(memcmp(parts[1].data()+0x18,original+0x18,kEntryTris-0x18)==0,
        "skin run preserves geometry flags and vertex declaration");
    Check(memcmp(fingers,original,kEntrySize)==0,"streamed draw entry is never modified");
    edits.skin.clear(); parts.clear();
    driver.body={0,0}; driver.arms={0,3}; driver.heads={0,0};
    Check(SkinParts(fingers,solid,parts),"secondary finger material repaired without picker marks");
    Check(parts.size()==1 && memcmp(parts[0].data(),entries,0x18)==0,
        "automatic fingers use this car's main skin material");
    Check(memcmp(fingers,original,kEntrySize)==0,"automatic fix never changes streamed material");
    uint8_t copied[kEntrySize]; memcpy(copied,fingers,kEntrySize);
    At<int>(copied,kEntryFirstIndex)=3; At<int>(copied,kEntryTris)=1;
    parts.clear();
    Check(SkinParts(copied,solid,parts),"copied animated finger range receives skin too");
    edits.black.insert(TriId(3,4,5)); parts.clear();
    Check(!SkinParts(copied,solid,parts),"explicit black marking overrides automatic finger fix");
    edits.black.clear(); driver.body[1]=20; parts.clear();
    Check(!SkinParts(fingers,solid,parts),"unrelated secondary body material is not recoloured");
    driver.body[1]=0;
    uint8_t otherSolid[256]{}, otherPlat[128]{}, otherEntries[2*kEntrySize]{};
    memcpy(otherPlat,plat,sizeof(plat)); memcpy(otherEntries,entries,sizeof(entries));
    memset(otherEntries,0x33,0x18);
    At<uint8_t*>(otherSolid,kSolidPlat)=otherPlat;
    At<uint8_t*>(otherPlat,kPlatEntries)=otherEntries;
    auto& otherInfo=s_solids[otherSolid]; otherInfo.driver=&driver;
    strcpy(otherInfo.name,"OTHER_CAR_DRIVER"); parts.clear();
    Check(SkinParts(otherEntries+kEntrySize,otherSolid,parts),"another car's fingers repaired automatically");
    Check(memcmp(parts[0].data(),otherEntries,0x18)==0,
        "skin material never leaks between cars");
    s_solids.erase(otherSolid);
    parts.clear(); At<int>(fingers,kEntryFirstIndex)=100;
    Check(!SkinParts(fingers,solid,parts),"invalid index range rejected");
    char directory[MAX_PATH], temporary[MAX_PATH], oldPath[MAX_PATH];
    Check(GetTempPathA(MAX_PATH,directory)!=0 && GetTempFileNameA(directory,"skn",0,temporary)!=0,
        "temporary editor ini created");
    strcpy(oldPath,EditsPath()); strcpy(const_cast<char*>(EditsPath()),temporary);
    s_pick.solid=solid; s_pick.name="SKIN_PICKER_TEST";
    s_brush.clear(); s_brush.push_back(TriId(3,4,5));
    edits.skin.clear(); edits.black.insert(TriId(3,4,5));
    RunEditCommand(18);
    Check(edits.skin.count(TriId(3,4,5)) && edits.black.empty(),"skin command replaces black marking");
    char saved[128]{};
    GetPrivateProfileStringA("SKIN_PICKER_TEST","Skin","",saved,sizeof(saved),temporary);
    Check(!ParseTris(saved).empty(),"skin marking persisted in editor ini");
    RunEditCommand(6);
    Check(edits.skin.empty() && edits.black.count(TriId(3,4,5)),"undo restores previous material marking");
    RunEditCommand(18); RunEditCommand(5);
    Check(edits.skin.empty() && edits.black.empty(),"restore clears skin override");
    At<int>(fingers,kEntryFirstIndex)=0;
    driver.ok=true;
    driver.body={0,3}; driver.arms={0,0}; driver.heads={0,0};
    edits.arms.clear(); edits.skin.clear(); edits.show.clear();
    edits.black.insert(TriId(3,4,5));
    info.tris.clear();
    for (int t=0; t<3; ++t) {
        TriRef tri{}; tri.entry=1;
        for (int j=0; j<3; ++j) tri.v[j]=indices[t*3+j];
        tri.id=TriId(tri.v[0],tri.v[1],tri.v[2]); info.tris.push_back(tri);
    }
    TriRef body{}; body.entry=0; body.id=TriId(10,11,12); info.tris.push_back(body);
    info.trisDirty=false;
    s_pick.id=TriId(3,4,5);
    s_brush.clear();
    for (const TriRef& tri : info.tris) if (tri.entry==1) s_brush.push_back(tri.id);
    RunEditCommand(19);
    Check(edits.arms.size()==3 && edits.skin.size()==3 && edits.show.size()==3 && edits.black.empty(),
        "second entry really populates Arms Skin Show and clears conflicting black marks");
    GetPrivateProfileStringA("SKIN_PICKER_TEST","Arms","",saved,sizeof(saved),temporary);
    Check(ParseTris(saved).size()==3,"automatic arm list persisted");
    GetPrivateProfileStringA("SKIN_PICKER_TEST","Skin","",saved,sizeof(saved),temporary);
    Check(ParseTris(saved).size()==3,"automatic skin list persisted");
    parts.clear();
    Check(SkinParts(fingers,solid,parts) && parts.size()==1 && memcmp(parts[0].data(),entries,0x18)==0,
        "saved second-entry skin marks use main driver material without classification");
    Check(!edits.arms.count(body.id) && !edits.skin.count(body.id),"merge never marks body entry");
    RunEditCommand(6);
    Check(edits.arms.empty() && edits.skin.empty() && edits.black.count(TriId(3,4,5)),
        "single undo restores all merge changes");
    info.tris={body}; info.trisDirty=false; s_pick.id=body.id;
    s_brush.assign(1,body.id);
    RunEditCommand(19);
    Check(edits.arms.count(body.id) && edits.skin.count(body.id) && edits.show.count(body.id),
        "merge honours selected driver triangles even outside hard-coded entry 1");
    Check(edits.arms.size()==1 && edits.skin.size()==1,"merge never expands beyond highlighted selection");
    RunEditCommand(6);
    s_brush.assign(1,TriId(3,4,5)); s_pick.id=s_brush[0];
    RunEditCommand(19);
    Check(edits.arms.size()==1 && edits.arms.count(s_pick.id) && edits.skin.count(s_pick.id),
        "single-triangle merge adds exactly the same arm membership as Mark as arms");
    RunEditCommand(5);
    Check(!edits.arms.count(s_pick.id) && !edits.skin.count(s_pick.id) &&
        !edits.show.count(s_pick.id) && edits.native.count(s_pick.id),
        "restore clears merge and persists automatic merge opt-out");
    GetPrivateProfileStringA("SKIN_PICKER_TEST","Native","",saved,sizeof(saved),temporary);
    Check(ParseTris(saved).size()==1,"restore opt-out saved for next launch");
    RunEditCommand(19);
    Check(!edits.native.count(s_pick.id) && edits.arms.count(s_pick.id),
        "explicit merge re-enables a restored triangle");
    strcpy(const_cast<char*>(EditsPath()),oldPath);
    Check(DeleteFileA(temporary)!=0,"temporary editor ini removed");
    puts("SKIN PICKER TESTS PASSED");
    return 0;
}