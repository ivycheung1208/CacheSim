#include "cachesim.hpp"

// implementation of cache access within a set
// return true if it's a hit, update MRU block and block map
// return false if it's a miss, remove LRU block if necessary,
// insert new block as MRU and update block map
bool CacheSet::setAccess(uint64_t addrTag) {
/*	if (setMap.find(addrTag) != setMap.end()) { // hit
		setBlocks.splice(setBlocks.begin(), setBlocks, setMap[addrTag]);
		setMap[addrTag] = setBlocks.begin();
		return true;
	}
	else { // miss
		if (setBlocks.size() == capacity) { // remove LRU block
			setMap.erase(setBlocks.back().tag);
			setBlocks.pop_back();
		}
		setBlocks.push_front(CacheNode(addrTag));
		setMap[addrTag] = setBlocks.begin();
		return false;
	}*/
	list<CacheNode>::iterator beg = setBlocks.begin();
	while (beg != setBlocks.end() && beg->tag != addrTag) ++beg;
	if (beg != setBlocks.end()) { // hit
		setBlocks.splice(setBlocks.begin(), setBlocks, beg);
		return true;
	}
	else { // miss
		if (setBlocks.size() == capacity) setBlocks.pop_back();
		setBlocks.push_front(CacheNode(addrTag));
		return false;
	}
}

bool CacheSim::cacheAccess(char rw, uint64_t address) {
	unsigned int addrIdx = (address >> b & ((1 << (c - s - b)) - 1));
//	cout << "Index: " << addrIdx << endl;
	uint64_t addrTag = address >> (c - s);
	return cacheSets[addrIdx].setAccess(addrTag);
/*	switch (rw) {
	case READ:
		if (hit) return 0;
		else return 1;
		break;
	case WRITE:
		if (hit) return 2;
		else return 3;
		break;
	default:
		return -1;
		break;
	}*/
}

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
//	cout << cacheSim.c << " " << cacheSim.b << " " << cacheSim.s << endl;
//	cout << cacheSim.cacheSets.size() << endl;
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
	++p_stats->accesses; // update # accesses
	bool mode = cacheSim.cacheAccess(rw, address);
	switch (rw) { // update # read / writes
	case READ:
		++p_stats->reads;
		if (!mode) ++p_stats->read_misses;
		break;
	case WRITE:
		++p_stats->writes;
		if (!mode) ++p_stats->write_misses;
		break;
	}
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
}
