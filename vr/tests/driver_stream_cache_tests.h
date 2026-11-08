#include "../src/driver_stream_cache.h"

static void TestDriverStreamCache() {
    DriverStreamCache cache;
    unsigned char buffer[4096];
    for (size_t i=0;i<sizeof(buffer);++i) buffer[i]=(unsigned char)i;
    unsigned reads=0;
    auto read=[&](std::vector<unsigned char>& bytes) {
        ++reads;
        bytes.assign(buffer,buffer+sizeof(buffer));
        return true;
    };
    // Increasing, decreasing and interleaved split ranges all share ONE read.
    for (size_t drawBytes : {32,128,2048,64,4096,256}) {
        auto* source=cache.Get(buffer,0,32,sizeof(buffer),drawBytes,read);
        Check(source && source->size()==sizeof(buffer),"cache stores complete stream on first small draw");
        std::vector<unsigned char> copy(source->begin(),source->begin()+drawBytes);
        Check(copy.size()==drawBytes && !memcmp(copy.data(),buffer,drawBytes),"bounded CPU copy retains exact source bytes");
        copy[0]^=255;
        Check((*source)[0]==buffer[0],"per-draw UV/pose writes never mutate cached source");
    }
    Check(reads==1,"growing split draws cause exactly one stream readback");
    // The next eye shares unchanged source data, but never shares posed CPU copies.
    cache.Get(buffer,0,32,sizeof(buffer),4096,read);
    Check(reads==1,"second eye reuses unchanged source without another readback");
    Check(!cache.Get(buffer,0,32,sizeof(buffer),0,read),"empty draw never reads stream");
    Check(!cache.Get(buffer,0,32,sizeof(buffer),8192,read),"invalid draw never reads stream");
    Check(!cache.Get(buffer,0,32,sizeof(buffer),33,read),"partial vertex draw rejected");
    Check(reads==1,"invalid ranges do not invoke reader");
    cache.Clear();
    buffer[0]=99;
    auto* next=cache.Get(buffer,0,32,sizeof(buffer),32,read);
    Check(reads==2 && next && (*next)[0]==99,"eye reset rereads current animation bytes");
    cache.Get(buffer,0,16,sizeof(buffer),32,read);
    Check(reads==3,"different stride is a distinct stream identity");
    auto offsetRead=[&](std::vector<unsigned char>& bytes) {
        ++reads; bytes.assign(buffer+32,buffer+sizeof(buffer)); return true;
    };
    auto* offset=cache.Get(buffer,32,32,sizeof(buffer)-32,32,offsetRead);
    Check(reads==4 && offset && (*offset)[0]==buffer[32],"stream offset uses correct snapshot");
    auto smallerRead=[&](std::vector<unsigned char>& bytes) {
        ++reads; bytes.assign(buffer,buffer+2048); return true;
    };
    cache.Get(buffer,0,32,2048,32,smallerRead);
    Check(reads==5,"changed buffer size does not reuse old snapshot");
    unsigned char otherBuffer=0;
    cache.Get(&otherBuffer,0,32,sizeof(buffer),32,read);
    Check(reads==6,"different buffer does not reuse snapshot");
    cache.Clear();
    auto fail=[&](std::vector<unsigned char>&) { ++reads; return false; };
    Check(!cache.Get(buffer,0,32,sizeof(buffer),32,fail),"failed lock allows native fallback");
    auto shortRead=[&](std::vector<unsigned char>& bytes) { ++reads; bytes.resize(32); return true; };
    Check(!cache.Get(buffer,0,32,sizeof(buffer),32,shortRead),"partial snapshot cannot enter cache");
    Check(cache.Get(buffer,0,32,sizeof(buffer),4096,read)!=nullptr,"failed reads do not poison subsequent valid read");
    unsigned before=reads;
    buffer[0]=77;
    cache.Invalidate(buffer);
    next=cache.Get(buffer,0,32,sizeof(buffer),32,read);
    Check(reads==before+1 && next && (*next)[0]==77,"write between eyes invalidates source at same address");
    cache.Invalidate(&otherBuffer);
    cache.Get(buffer,0,32,sizeof(buffer),32,read);
    Check(reads==before+1,"unrelated buffer write preserves cached source");
    cache.Invalidate(buffer);
    buffer[0]=88;
    next=cache.Get(buffer,0,32,sizeof(buffer),32,read);
    Check(reads==before+2 && next && (*next)[0]==88,"released buffer address reuse cannot return old bytes");
    puts("PASS: full-stream snapshot, one read across growing split draws, CPU-copy isolation, invalidation and fallback");
}