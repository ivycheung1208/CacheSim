#include "cachesim.hpp"

// implementation of cache access
cache_access_t CacheSim::cacheAccess(char rw, uint64_t address) {
	cache_access_t result;
	
	// address decoder
	const uint64_t addrTag = address >> (c - s);
	const unsigned int addrIdx = ((address >> b) & ((1 << (c - s - b)) - 1));
#if DEBUGL1
	cout << hex;
	cout << "Address: " << address << " Index: " << addrIdx << " Tag: " << addrTag << " ";
#endif

	// probe the L1 cache
	list<CacheNode>::iterator l1beg = cacheSets[addrIdx].begin();
	while (l1beg != cacheSets[addrIdx].end() && l1beg->tag != addrTag) {
#if DEBUGL1
		cout << l1beg->tag << " ";
#endif
		++l1beg;
	}
	if (l1beg != cacheSets[addrIdx].end()) { // hit
#if DEBUGL1
		cout << " Hit!" << endl;
#endif
		// hit on a prefetched block
		if (l1beg->isPrefetch) {
			// first hit on a prefetched block, update count of useful prefetches
#if DEBUGL1
			cout << "Pref hit: " << addrTag << " " << addrIdx << endl;
#endif
			if (!l1beg->usefulPrefetch) {
				l1beg->usefulPrefetch = true;
				++result.useful_prefetches;
			}
			// else ;
			// don't do anything if hit on an already hit prefetched block
			// neither updating useful count nor promote to MRU
		}
		// hit on a normal block, promote to MRU position
		else
			cacheSets[addrIdx].splice(cacheSets[addrIdx].begin(), cacheSets[addrIdx], l1beg);
	}

	// ========== victim cache logic ===============

	// probe the VC cache
	else if (v) { // L1 miss with vc enabled
#if DEBUGL1
		cout << " Miss!" << endl;
#endif
#if DEBUGVC
		cout << hex << endl;
		cout << "Index: " << addrIdx << " Tag: " << addrTag << " Miss!" << endl;
		cout << "VC: ";
		for (auto it : victimCache) cout << (it.tag << (c-b-s)) + it.idx << "; ";
#endif
		++result.misses;
		list<CacheNode>::iterator vcbeg = victimCache.begin(); // iterator in vimtim cache
		while (vcbeg != victimCache.end() && (vcbeg->idx != addrIdx || vcbeg->tag != addrTag)) {
			++vcbeg;
		}
		// L1 insertion from VC
		if (vcbeg != victimCache.end()) { // vc hit
#if DEBUGVC
			cout << "VC Hit!" << endl;
			cout << "VC swap: " << (vcbeg->tag << (c - b - s)) + vcbeg->idx;
			cout << " with " << (cacheSets[addrIdx].back().tag << (c - b - s)) + cacheSets[addrIdx].back().idx << endl;
#endif
			// switch hit block in vc with LRU block in L1
			// cacheSets[addrIdx] must be full, otherwise current block wouldn't be found in vc
			// thus cacheSets[addrIdx].back() exists
			CacheNode temp = *vcbeg;
			*vcbeg = cacheSets[addrIdx].back(); // replace the VC block with the LRU block in the L1 cache
			cacheSets[addrIdx].pop_back();
			cacheSets[addrIdx].push_front(temp); // insert the block from VC as the MRU block in the L1 cache
		}
		// L1 insertion from main memory
		else { // vc miss
#if DEBUGVC
			cout << "VC Miss!" << endl;
#endif
			++result.vc_misses;
			if (cacheSets[addrIdx].size() == setCap && victimCache.size() == v) {
#if DEBUGVC
				cout << "VC evict: " << (victimCache.front().tag << (c-b-s)) + victimCache.front().idx << endl;
#endif
				if (victimCache.front().dirty) ++result.writebacks; // writeback if the evicted block from VC is dirty
				victimCache.pop_front(); // evict the first block from VC
			}
			if (cacheSets[addrIdx].size() == setCap) {
#if DEBUGVC
				cout << "VC insert: " << (cacheSets[addrIdx].back().tag << (c-b-s)) + cacheSets[addrIdx].back().idx << endl;
#endif
				victimCache.push_back(cacheSets[addrIdx].back()); // insert the LRU block in the L1 cache into VC
				cacheSets[addrIdx].pop_back(); // evict the LRU block from L1 cache
			}
			cacheSets[addrIdx].push_front(CacheNode(addrTag, addrIdx));
		}
	}
	// ========== end of victim cache ===============

	// L1 insertion from main memory
	else { // L1 miss with vc disabled
#if DEBUGL1
		cout << " Miss!" << endl;
#endif
		++result.misses;
		++result.vc_misses;
		if (cacheSets[addrIdx].size() == setCap) {
			if (cacheSets[addrIdx].back().dirty) ++result.writebacks; // writeback if dirty bit is true
			cacheSets[addrIdx].pop_back();
		}
		cacheSets[addrIdx].push_front(CacheNode(addrTag, addrIdx));
	}

	// update dirty bit
	if (rw == WRITE) cacheSets[addrIdx].front().dirty = true; // set dirty bit whenever write to a block

	// ========== prefetch logic after insertion =====

	// PREFETCH WITHOUT VC AT THIS TIME. LRU PREFETCHED BLOCKS ARE DIRECTLY EVICTED FROM L1

	// examine prefecher when there's an access miss (L1 or VC)
	if (k && result.misses) {
		// shifted block address shifted! without LSBs for block offset
		bool d_sign = (address >> b) > last_miss;
		uint64_t d = d_sign ? (address >> b) - last_miss : last_miss - (address >> b);

		// check prefether
		if (d_sign == stride_sign && d == pending_stride) {
#if DEBUGPREF
			cout << "Prefetch begin! d = " << (d_sign ? "+" : "-") << d << endl;
			cout << "Miss block: " << (addrTag << (c-b-s)) + addrIdx << endl;
#endif
			uint64_t prefetch_block = (address >> b);
			uint64_t prefetch_tag;
			unsigned int prefetch_index;

			// prefetch K blocks
			for (int i = 0; i != k; ++i) {
				// calculate prefetch address
				if (d_sign)
					prefetch_block += d;
				else
					prefetch_block -= d;
				prefetch_index = (prefetch_block & ((1 << (c - s - b)) - 1));
				prefetch_tag = prefetch_block >> (c - s - b);
#if DEBUGPREF
				cout << "Pref block: " << prefetch_tag << " " << prefetch_index << endl;
#endif

				// check whether it already exists in the cache!
				list<CacheNode>::iterator prefbeg = cacheSets[prefetch_index].begin();
				while (prefbeg != cacheSets[prefetch_index].end() && prefbeg->tag != prefetch_tag) ++prefbeg;

				// doesn't exist, prefetch
				if (prefbeg == cacheSets[prefetch_index].end()) {
					if (cacheSets[prefetch_index].size() == setCap) { // evict LRU block
						if (cacheSets[prefetch_index].back().dirty)
							++result.writebacks;
						cacheSets[prefetch_index].pop_back();
					}
					// insert as LRU? could be kicked out immediately when inserting the next pref
					cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, prefetch_index, true));
					++result.prefetch_blocks;
				}
			}
		}

		// update prefecher info
		pending_stride = d;
		stride_sign = d_sign;
		last_miss = address >> b;
	}
	// ========== end of prefetch ====================

	return result;
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
	cache_access_t result = cacheSim.cacheAccess(rw, address);
	switch (rw) {
	case READ:
		++p_stats->reads;
		p_stats->read_misses += result.misses;
		p_stats->read_misses_combined += result.vc_misses;
		break;
	case WRITE:
		++p_stats->writes;
		p_stats->write_misses += result.misses;
		p_stats->write_misses_combined += result.vc_misses;
		break;
	default:
		break;
	}
	p_stats->write_backs += result.writebacks;
	p_stats->prefetched_blocks += result.prefetch_blocks;
	p_stats->useful_prefetches += result.useful_prefetches;
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
	p_stats->bytes_transferred = (1 << cacheSim.getB()) * (p_stats->vc_misses + p_stats->write_backs + p_stats->prefetched_blocks);
	// calculate AAT
	p_stats->hit_time = 2 + 0.2 * cacheSim.getS();
	p_stats->miss_rate = (double)p_stats->misses / p_stats->accesses;
	p_stats->miss_penalty = 200;
	double vc_miss_rate = (double)p_stats->vc_misses / p_stats->accesses;
	p_stats->avg_access_time = p_stats->hit_time + vc_miss_rate * p_stats->miss_penalty;
}
