#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>

#include <vector>
#include <list>

#include <iostream>

using namespace std;

class CacheSim {
public:
	CacheSim() : c(0), b(0), s(0), v(0), k(0), setCap(0) {}
	CacheSim(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) : c(c), b(b), s(s), v(v), k(k),
		// total # blocks: 2 ^ (c - b)
		// # blocks per set: 2 ^ s -> CacheSet capacity
		// # sets: 2 ^ (c - b - s)
		setCap(1 << s), cacheSets(vector<list<CacheNode>>(1 << (c - b - s), list<CacheNode>(1 << s))) {}
	int cacheAccess(char rw, uint64_t address);
private:
	uint64_t c, b, s, v, k;
	uint64_t setCap;
	struct CacheNode {
		uint64_t tag;
		bool dirty;
		CacheNode() : tag(0), dirty(0) {}
		CacheNode(uint64_t addrTag) : tag(addrTag), dirty(false) {}
	};
	vector<list<CacheNode>> cacheSets;
//	vector<CacheNode> victimCache;
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
   
	uint64_t hit_time;
    uint64_t miss_penalty;
    double   miss_rate;
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
static const int     HIT = 0;
static const int     MISS_NWB = 1;
static const int     MISS_WB = 2;

#endif /* CACHESIM_HPP */
