// Reuse the synthetic game layout/stubs, then measure actual production hits.
#define main CockpitRegressionMain
#include "cockpit_tests.cpp"
#undef main
#include <new>
#include <type_traits>

static size_t allocations;
void* operator new(size_t n) {
    ++allocations;
    if (void* p = malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }

int main() {
    static_assert(std::is_trivially_destructible<SolidIdentity>::value,
                  "Cache identity must not own containers");
    CockpitRegressionMain();
    static uint8_t solid[256] = {};
    strcpy((char*)solid + kSolidName, "CACHE_TEST_ROAD");
    SolidInfo* expected = GetSolid(solid);
    size_t before = allocations;
    for (int i = 0; i < 10000; ++i)
        Check(GetSolid(solid) == expected, "stable cached solid identity");
    size_t hitAllocations = allocations - before;
    Check(hitAllocations == 0, "10000 production cache hits allocate nothing");
    // Bytes after the name terminator are not part of a streamed model's identity.
    expected->hashSeen = 12345;
    solid[kSolidName + strlen("CACHE_TEST_ROAD") + 1] = 0x7f;
    Check(GetSolid(solid)->hashSeen == 12345, "trailing name bytes do not rebuild the cache");
    // Unmodified traffic/props must not read their index payload just to enter
    // the cockpit cache. A protected payload makes an accidental scan fail.
    void* protectedIndices = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    Check(protectedIndices != nullptr, "protected index payload allocated");
    static uint8_t prop[256] = {}, plat[64] = {};
    strcpy((char*)prop + kSolidName, "BREAKABLE_TRAFFIC_CACHE_TEST");
    At<uint8_t*>(prop, kSolidPlat) = plat;
    At<uint16_t*>(plat, kPlatIndices) = (uint16_t*)protectedIndices;
    At<int>(plat, kPlatNumIndices) = 2048;
    SolidInfo* propInfo = GetSolid(prop);
    Check(propInfo->indexHash == 0, "ordinary prop needs no index hash baseline");
    Check(GetSolid(prop) == propInfo, "ordinary prop cache hit does not read indices");
    strcpy((char*)prop + kSolidName, "BREAKABLE_RELOADED_CACHE_TEST");
    Check(GetSolid(prop)->indexHash == 0, "reused prop address needs no index scan");
    Check(VirtualFree(protectedIndices, 0, MEM_RELEASE) != 0, "protected index payload released");
    s_solids.erase(prop);
    puts("PASS: traffic/breakable miss, hit and address reuse never scan index payload");
    before = allocations;
    for (int i = 0; i < 10000; ++i) { SolidInfo previousTemporary; }
    printf("PASS: 10000 cached lookups: %u allocations; previous full temporary: %u allocations\n",
           (unsigned)hitAllocations, (unsigned)(allocations - before));
    return 0;
}
