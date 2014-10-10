#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>

#include <vector>
#include <list>

using std::vector;
using std::list;

// return struct for cache access function
struct cache_access_t {
	int misses;
	int vc_misses;
	int writebacks;
	int useful_prefetches;
	uint64_t prefetch_blocks;
	cache_access_t() : misses(0), vc_misses(0), writebacks(0), useful_prefetches(0), prefetch_blocks(0) {}
};

// class for cache simulation
class CacheSim {
public:
	CacheSim() : c(0), b(0), s(0), v(0), k(0), set_capacity(0) {}
	CacheSim(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) : c(c), b(b), s(s), v(v), k(k),
		// # blocks per set: 2 ^ s -> set capacity
		set_capacity(1 << s),
		// total # blocks: 2 ^ (c - b)
		// # sets: 2 ^ (c - b - s)
		cacheSets(vector<list<CacheNode>>(1 << (c - b - s))),
		// prefetcher variables initialized to zero
		last_miss(0), pending_stride(0), stride_sign(true) {}
	cache_access_t cacheAccess(char rw, uint64_t address); // member function that performs cache access
	uint64_t getC() { return c; } // read-only
	uint64_t getB() { return b; } // read-only
	uint64_t getS() { return s; } // read-only
	uint64_t getV() { return v; } // read-only
	uint64_t getK() { return k; } // read-only
private:
	uint64_t c, b, s, v, k;
	uint64_t set_capacity; // associativity (2^s)
	// struct for L1 cache block, stores only tag
	struct CacheNode {
		uint64_t tag;
		bool dirty;
		bool isPrefetch;
		CacheNode() : tag(0), dirty(false), isPrefetch(false) {}
		// fetch block from main memory: store tag value
		CacheNode(uint64_t addrTag) : tag(addrTag), dirty(false), isPrefetch(false) {}
		// fetch block from victim cache: preserve dirty bit and prefetch bit (manually)
		CacheNode(uint64_t addrTag, bool dir, bool pref) : tag(addrTag), dirty(dir), isPrefetch(pref) {}
	};
	// struct for victim cache block, stores both tag and index
	struct VCNode {
		uint64_t tag;
		unsigned int idx;
		bool dirty;
		bool isPrefetch;
		VCNode() : tag(0), idx(0), dirty(false), isPrefetch(false) {}
		// fetch block from L1 cache: store both tag and index value, preserve dirty bit and prefetch bit
		VCNode(CacheNode block, unsigned int index) : tag(block.tag), idx(index),
			dirty(block.dirty), isPrefetch(block.isPrefetch) {}
	};
	// block containers: cacheSets, victimCache
	// each list represents a set in L1 cache, MRU block resides at the front and LRU at the back
	// the huge vector holds all the lists
	vector<list<CacheNode>> cacheSets; 
	// oldest block resides at the front and newest at the back (always insert from the back!)
	list<VCNode> victimCache;
	// prefetcher variables: last_miss, pending_stride, stride_sign
	uint64_t last_miss;
	uint64_t pending_stride;
	bool stride_sign; // 1 is positive and 0 is negative
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

/** Status of dirty bit */
static const bool     CLEAN = false;
static const bool     DIRTY = true;

/** Status of prefetch bit */
static const bool     PREFETCH = true;
static const bool     NONPREFETCH = false;

#endif /* CACHESIM_HPP */
