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
    auto& edits=EditsFor("SKIN_PICKER_TEST");
    std::vector<std::array<uint8_t,kEntrySize>> parts;
    Check(!SkinParts(fingers,solid,parts),"no skin marks leaves native draw alone");
    edits.skin.insert(TriId(3,4,5));
    uint8_t original[kEntrySize]; memcpy(original,fingers,kEntrySize);
    Check(SkinParts(fingers,solid,parts),"skin selection splits native material");
    Check(parts.size()==3,"unmarked/skin/unmarked runs remain separate");
    Check(memcmp(parts[1].data(),entries,0x18)==0,"selected run uses skin shader and textures");
    Check(memcmp(parts[0].data(),fingers,0x18)==0 && memcmp(parts[2].data(),fingers,0x18)==0,
        "unselected runs keep original material");
    Check(At<int>(parts[1].data(),kEntryFirstIndex)==3 && At<int>(parts[1].data(),kEntryTris)==1 &&
        At<int>(parts[1].data(),kEntryNumIndices)==3,"skin run keeps correct geometry range");
    Check(memcmp(parts[1].data()+0x18,original+0x18,kEntryTris-0x18)==0,
        "skin run preserves geometry flags and vertex declaration");
    Check(memcmp(fingers,original,kEntrySize)==0,"streamed draw entry is never modified");
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
    strcpy(const_cast<char*>(EditsPath()),oldPath);
    Check(DeleteFileA(temporary)!=0,"temporary editor ini removed");
    puts("SKIN PICKER TESTS PASSED");
    return 0;
}