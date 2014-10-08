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
			// update useful prefetch bit and increment count
			++result.useful_prefetches;
			l1beg->isPrefetch = false;
		}

		// update dirty bit on write hit
		if (rw == WRITE) l1beg->dirty = true;

		// promote to MRU position on hit
		cacheSets[addrIdx].splice(cacheSets[addrIdx].begin(), cacheSets[addrIdx], l1beg);
	}



	// ========== victim cache logic ===============

	// L1 miss when vc enabled: probe the VC cache
	else if (v) {
#if DEBUGL1
		cout << " Miss!" << endl;
#endif
		++result.misses;
#if DEBUGVC
		cout << hex << endl;
		cout << "Index: " << addrIdx << " Tag: " << addrTag << " Miss!" << endl;
		cout << "VC: ";
		for (auto it : victimCache) cout << (it.tag << (c-b-s)) + it.idx << "; ";
#endif
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
			for (auto it : victimCache) cout << (it.tag << (c - b - s)) + it.idx << "; ";
#endif
			// check whether its a prefetch block in VC
			if (vcbeg->isPrefetch) {
				++result.useful_prefetches;
				vcbeg->isPrefetch = false;
			}
			// update dirty bit
			if (rw == WRITE) vcbeg->dirty = true;
			// swap hit block in vc with LRU block in L1 and then make it MRU
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

			// evict block from VC when both VC and L1 cache set are full
			if (cacheSets[addrIdx].size() == setCap && victimCache.size() == v) {
#if DEBUGVC
				cout << "VC evict: " << (victimCache.front().tag << (c-b-s)) + victimCache.front().idx << endl;
#endif
				if (victimCache.front().dirty) ++result.writebacks; // writeback if the evicted block from VC is dirty
				victimCache.pop_front(); // evict the first block from VC
			}

			// move LRU block from L1 to VC when L1 cache set is full and VC is not
			if (cacheSets[addrIdx].size() == setCap) {
#if DEBUGVC
				cout << "VC insert: " << (cacheSets[addrIdx].back().tag << (c-b-s)) + cacheSets[addrIdx].back().idx << endl;
#endif
				victimCache.push_back(cacheSets[addrIdx].back()); // insert the LRU block in the L1 cache into VC
#if DEBUGVC
				for (auto it : victimCache) cout << (it.tag << (c - b - s)) + it.idx << "; ";
#endif
				cacheSets[addrIdx].pop_back(); // evict the LRU block from L1 cache
			}

			// fetch block from main memory to L1 cache
			cacheSets[addrIdx].push_front(CacheNode(addrTag, addrIdx));

			// alternative fetch approach!
/*			if (cacheSets[addrIdx].size() != setCap) {
				cacheSets[addrIdx].push_front(CacheNode(addrTag, addrIdx));
			}
			else {
				CacheNode temp = cacheSets[addrIdx].back();
				cacheSets[addrIdx].pop_back();
				cacheSets[addrIdx].push_front(CacheNode(addrTag, addrIdx));
				if (victimCache.size() != v) {
					victimCache.push_back(temp);
				}
				else {
					if (victimCache.front().dirty) ++result.writebacks;
					victimCache.pop_front();
					victimCache.push_back(temp);
				}
			}*/

			// update the dirty bit after insertion
			if (rw == WRITE) cacheSets[addrIdx].front().dirty = true;
		}
	}
	// ========== end of victim cache ===============



	// L1 miss when vc disabled
	// L1 insertion directly from main memory
	else {
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
		// update dirty bit after fetch the block
		if (rw == WRITE) cacheSets[addrIdx].front().dirty = true;
	}



	// ========== prefetch logic after insertion =====

	// 1) Prefetch with VC disabled first. LRU block from L1 is directly evicted from cache
	
	// 2) Prefetch with VC enabled: LRU block from L1 goes into VC and VC performs FIFO

	// check prefecher when there's an access miss (L1 or VC)
	if (k && result.misses) {
		// shifted block address shifted! without LSBs for block offset
		bool d_sign = (address >> b) > last_miss;
		uint64_t d = d_sign ? (address >> b) - last_miss : last_miss - (address >> b);

		// check prefether
		if (d_sign == stride_sign && d == pending_stride) {
			// increment prefetched_blocks K times every time the prefetcher is triggered
			result.prefetch_blocks += k;
#if DEBUGPREF
			cout << "Prefetch begin! d = " << (d_sign ? "+" : "-") << d << endl;
			cout << "Miss block: " << (addrTag << (c-b-s)) + addrIdx << endl;
#endif
			// initialize prefetch address, index and tag (set at the missing address)
			uint64_t prefetch_addr = (address >> b);
			uint64_t prefetch_tag;
			unsigned int prefetch_index;

			// prefetch K blocks
			for (int i = 0; i != k; ++i) {
				// calculate prefetch address
				if (d_sign)
					prefetch_addr += d;
				else
					prefetch_addr -= d;
				prefetch_index = (prefetch_addr & ((1 << (c - s - b)) - 1));
				prefetch_tag = prefetch_addr >> (c - s - b);
#if DEBUGPREF
				cout << "Pref block: " << prefetch_tag << " " << prefetch_index << endl;
#endif

				// first check whether it already exists in the cache
				list<CacheNode>::iterator prefbeg = cacheSets[prefetch_index].begin();
				while (prefbeg != cacheSets[prefetch_index].end() && prefbeg->tag != prefetch_tag) ++prefbeg;

				// if the block is already in L1 cache, don't do anything
#if DEBUGPREF
				if (prefbeg != cacheSets[prefetch_index].end())
					std::cout << "Prefetch found in L1!" << endl;
#endif

				// with VC enabled, if the block is not in L1, check whether it exists in the VC
				if (v && prefbeg == cacheSets[prefetch_index].end()) {
					list<CacheNode>::iterator prefvcbeg = victimCache.begin();
#if DEBUGPREF
					std::cout << "Prefetch address: " << (prefetch_tag << (c - b - s)) + prefetch_index << endl;
					std::cout << "Check VC: ";
					for (auto it : victimCache)
						std::cout << (it.tag << (c - b - s)) + it.idx << "; ";
					std::cout << endl;
#endif
					while (prefvcbeg != victimCache.end() && (prefvcbeg->idx != prefetch_index || prefvcbeg->tag != prefetch_tag))
						++prefvcbeg;
					
					// if the block is in VC, swap with corresponding LRU block in L1 cache and set prefetch bit
					if (prefvcbeg != victimCache.end()) {
#if DEBUGPREF
						std::cout << "Prefetch found in VC!" << endl;
						// Never hit in the default setting!
#endif
						CacheNode temp = *prefvcbeg;
						*prefvcbeg = cacheSets[prefetch_index].back();
						cacheSets[prefetch_index].pop_back();
						cacheSets[prefetch_index].push_back(temp);
						cacheSets[prefetch_index].back().isPrefetch = true;
					}
					
					// if the block is not in L1 or VC, prefetch from main memory
					// replace the LRU block with the prefetched block and set the prefech bit
					// LRU block goes into VC following FIFO
					else {
						// evict block from VC when both VC and L1 cache set are full
						if (victimCache.size() == v && cacheSets[prefetch_index].size() == setCap) {
							if (victimCache.front().dirty) ++result.writebacks;
							victimCache.pop_front();
						}
						// move LRU block to VC when L1 cache set is full and VC is not
						if (cacheSets[prefetch_index].size() == setCap) {
// ***** should be push_back!! *****
							victimCache.push_back(cacheSets[prefetch_index].back());
							cacheSets[prefetch_index].pop_back();
						}
						// insert prefetched block at LRU position
						cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, prefetch_index));
						cacheSets[prefetch_index].back().isPrefetch = true;

						// alternative prefetch approach
/*						if (cacheSets[prefetch_index].size() != setCap) {
							cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, prefetch_index));
							cacheSets[prefetch_index].back().isPrefetch = true;
						}
						else {
							CacheNode temp = cacheSets[prefetch_index].back();
							cacheSets[prefetch_index].pop_back();
							cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, prefetch_index));
							cacheSets[prefetch_index].back().isPrefetch = true;
							if (victimCache.size() != v) {
								victimCache.push_back(temp);
							}
							else {
								if (victimCache.front().dirty) ++result.writebacks;
								victimCache.pop_front();
								victimCache.push_back(temp);
							}
						}*/
					}	
				}

				// with VC disabled, evict LRU when cache set is full, then prefetch
				else if (prefbeg == cacheSets[prefetch_index].end()) {
					if (cacheSets[prefetch_index].size() == setCap) {
						if (cacheSets[prefetch_index].back().dirty)
							++result.writebacks;
						cacheSets[prefetch_index].pop_back();
					}
					cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, prefetch_index));
					cacheSets[prefetch_index].back().isPrefetch = true;
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
