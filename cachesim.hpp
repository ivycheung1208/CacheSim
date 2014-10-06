#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>

#include <vector>
#include <list>

#include <iostream>

using namespace std;

// Debug mode for L1 cache
#ifndef DEBUGL1
#define DEBUGL1 0
#endif

// Debug mode for victim cache
#ifndef DEBUGVC
#define DEBUGVC 0
#endif

// Debug mode for prefetch
#ifndef DEBUGPREF
#define DEBUGPREF 0
#endif

struct cache_access_t {
	int misses;
	int vc_misses;
	int writebacks;
	int useful_prefetches;
	uint64_t prefetch_blocks;
	cache_access_t() : misses(0), vc_misses(0), writebacks(0), useful_prefetches(0), prefetch_blocks(0) {}
};

class CacheSim {
public:
	CacheSim() : c(0), b(0), s(0), v(0), k(0), setCap(0) {}
	CacheSim(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) : c(c), b(b), s(s), v(v), k(k),
		// # blocks per set: 2 ^ s -> set capacity
		setCap(1 << s),
		// total # blocks: 2 ^ (c - b)
		// # sets: 2 ^ (c - b - s)
//		cacheSets(vector<list<CacheNode>>(1 << (c - b - s), list<CacheNode>(1 << s))),
		cacheSets(vector<list<CacheNode>>(1 << (c - b - s))),
		// initial prefetch values set to zero
		last_miss(0), pending_stride(0), stride_sign(true) {}
	cache_access_t cacheAccess(char rw, uint64_t address);
	uint64_t getB() { return b; }
	uint64_t getS() { return s; }
private:
	uint64_t c, b, s, v, k;
	uint64_t setCap;
	struct CacheNode {
		uint64_t tag;
		unsigned int idx;
		bool dirty;
		bool isPrefetch;
		bool usefulPrefetch;
		CacheNode() : tag(0), dirty(0), isPrefetch(0), usefulPrefetch(0) {}
		CacheNode(uint64_t addrTag, unsigned int addrIdx) : tag(addrTag), idx(addrIdx),
			dirty(false), isPrefetch(false), usefulPrefetch(false) {}
		CacheNode(uint64_t addrTag, unsigned int addrIdx, bool pref) : tag(addrTag), idx(addrIdx),
			dirty(false), isPrefetch(pref), usefulPrefetch(false) {}
	};
	vector<list<CacheNode>> cacheSets;
	list<CacheNode> victimCache;
	uint64_t last_miss;
	uint64_t pending_stride;
	bool stride_sign; // 1 for positive and 0 for negative
};

struct cache_stats_t {
    uint64_t accesses;
    uint64_t reads;
    uint64_t read_misses;
    uint64_t read_misses_combined;
    uint64_t writes;
    uint64_t write_misses;
    uint64_t write_misses_combined;
    uint64_t misses;
	uint64_t write_backs;
	uint64_t vc_misses;
	uint64_t prefetched_blocks;
	uint64_t useful_prefetches;
	uint64_t bytes_transferred; 
   
	double   hit_time;
	double   miss_rate;
	uint64_t miss_penalty;
    double   avg_access_time;
};

void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k);
void cache_access(char rw, uint64_t address, cache_stats_t* p_stats);
void complete_cache(cache_stats_t *p_stats);

static const uint64_t DEFAULT_C = 15;   /* 32KB Cache */
static const uint64_t DEFAULT_B = 5;    /* 32-byte blocks */
static const uint64_t DEFAULT_S = 3;    /* 8 blocks per set */
static const uint64_t DEFAULT_V = 4;    /* 4 victim blocks */
static const uint64_t DEFAULT_K = 2;	/* 2 prefetch distance */

/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';

/** Return value of cache access MODE. */
static const int     HIT_NORMAL = 0;
static const int     HIT_USEFUL = 1;
static const int     VC_HIT = 2;
static const int     MISS_NWB = 3;
static const int     MISS_WB = 4;

#endif /* CACHESIM_HPP */
