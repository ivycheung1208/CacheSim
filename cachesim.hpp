#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>

#include <vector>
#include <list>

// #include <iostream>

using std::vector;
using std::list;

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
	CacheSim() : c(0), b(0), s(0), v(0), k(0), set_capacity(0) {}
	CacheSim(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) : c(c), b(b), s(s), v(v), k(k),
		// # blocks per set: 2 ^ s -> set capacity
		set_capacity(1 << s),
		// total # blocks: 2 ^ (c - b)
		// # sets: 2 ^ (c - b - s)
		cacheSets(vector<list<CacheNode>>(1 << (c - b - s))),
		// initial prefetch values set to zero
		last_miss(0), pending_stride(0), stride_sign(true) {}
	cache_access_t cacheAccess(char rw, uint64_t address);
	uint64_t getB() { return b; }
	uint64_t getS() { return s; }
private:
	uint64_t c, b, s, v, k;
	uint64_t set_capacity;
	struct CacheNode { // block in L1 cache, saves index storage
		uint64_t tag;
		bool dirty;
		bool isPrefetch;
		CacheNode() : tag(0), dirty(false), isPrefetch(false) {}
		// fetch from main memory, only need to supply tag value
		CacheNode(uint64_t addrTag) : tag(addrTag), dirty(false), isPrefetch(false) {}
		// fetch from victim cache
		CacheNode(uint64_t addrTag, bool dir, bool pref) : tag(addrTag), dirty(dir), isPrefetch(pref) {}
	};
	struct VCNode { // block in victim cache
		uint64_t tag;
		unsigned int idx;
		bool dirty;
		bool isPrefetch;
		VCNode() : tag(0), idx(0), dirty(false), isPrefetch(false) {}
		VCNode(CacheNode block, unsigned int index) : tag(block.tag), idx(index),
			dirty(block.dirty), isPrefetch(block.isPrefetch) {}
	};
	vector<list<CacheNode>> cacheSets;
	list<VCNode> victimCache;
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

static const double AAT_MAX = 1000;

/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';

/** Status of dirty bit */
static const bool     CLEAN = false;
static const bool     DIRTY = true;

/** Status of prefetch bit */
static const bool     PREFETCH = true;
static const bool     NONPREFETCH = false;

#endif /* CACHESIM_HPP */
