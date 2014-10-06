#include "cachesim.hpp"

// implementation of cache access
// return 0 if it's a hit, no WB in this case, update MRU block and block map
// return 2 if it's a miss and no WB, return 3 if miss and WB,
// remove LRU block if necessary, insert new block as MRU and update block map
int CacheSim::cacheAccess(char rw, uint64_t address) {
	bool hit = false, writeback = false;
	unsigned int addrIdx = (address >> b & ((1 << (c - s - b)) - 1));
	uint64_t addrTag = address >> (c - s);
	list<CacheNode>::iterator beg = cacheSets[addrIdx].begin();
	while (beg != cacheSets[addrIdx].end() && beg->tag != addrTag) ++beg;
	if (beg != cacheSets[addrIdx].end()) { // hit
		hit = true;
		cacheSets[addrIdx].splice(cacheSets[addrIdx].begin(), cacheSets[addrIdx], beg);
	}
	else { // miss
		if (cacheSets[addrIdx].size() == setCap) {
			writeback = cacheSets[addrIdx].back().dirty; // writeback if dirty bit is true
			cacheSets[addrIdx].pop_back();
		}
		cacheSets[addrIdx].push_front(CacheNode(addrTag));
	}
	if (rw == WRITE) cacheSets[addrIdx].front().dirty = true; // set dirty bit whenever write to a block
	if (hit) return HIT;
	else if (!writeback) return MISS_NWB;
	else return MISS_WB;
}

// Global object that simulates the whole cache
CacheSim cacheSim;

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @c The total number of bytes for data storage is 2^C
 * @b The size of a single cache line in bytes is 2^B
 * @s The number of blocks in each set is 2^S
 * @v The number of blocks in the victim cache is V
 * @k The prefetch distance is K
 */
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) {
	cacheSim = CacheSim(c, b, s, v, k);
}
/**
 * Subroutine that simulates the cache one trace event at a time.
 * XXX: You're responsible for completing this routine
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {
	int mode = cacheSim.cacheAccess(rw, address);
	switch (rw) { // update # read / writes
	case READ:
		++p_stats->reads;
		if (mode != HIT) ++p_stats->read_misses;
		break;
	case WRITE:
		++p_stats->writes;
		if (mode != HIT) ++p_stats->write_misses;
		break;
	default:
		break;
	}
	if (mode == MISS_WB) // update # write backs
		++p_stats->write_backs;
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
	p_stats->accesses = p_stats->reads + p_stats->writes;
	p_stats->misses = p_stats->read_misses + p_stats->write_misses;
	p_stats->vc_misses = p_stats->read_misses_combined + p_stats->write_misses_combined;
}
