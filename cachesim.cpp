#include "cachesim.hpp"

// implementation of cache access funciton
cache_access_t CacheSim::cacheAccess(char rw, uint64_t address) {
	cache_access_t result;
	
	// address decoder
	const uint64_t addrTag = address >> (c - s);
	const unsigned int addrIdx = ((address >> b) & ((1 << (c - s - b)) - 1));

	// probe the L1 cache
	list<CacheNode>::iterator l1beg = cacheSets[addrIdx].begin(); // iterator in L1 cache set
	while (l1beg != cacheSets[addrIdx].end() && l1beg->tag != addrTag) ++l1beg;

	// hit on block in L1 cache
	if (l1beg != cacheSets[addrIdx].end()) {
		// check whether it's the first hit on a prefetched block
		if (l1beg->isPrefetch == PREFETCH) {
			// update useful prefetch count and reset prefetch bit
			++result.useful_prefetches;
			l1beg->isPrefetch = NONPREFETCH;
		}
		// promote to MRU position on hit
		cacheSets[addrIdx].splice(cacheSets[addrIdx].begin(), cacheSets[addrIdx], l1beg);
	}

	// miss in L1, vc disabled: fetch from main memory, insert as MRU and evict the LRU block when cache set is full
	else if (!v) {
		// update miss count and vc miss count
		++result.misses;
		++result.vc_misses;
		// evict LRU block when L1 cache set is full, check dirty bit and update writeback count
		if (cacheSets[addrIdx].size() == set_capacity) {
			if (cacheSets[addrIdx].back().dirty) ++result.writebacks;
			cacheSets[addrIdx].pop_back();
		}
		// fetch block from main memory and insert at the MRU position of L1 cache set
		cacheSets[addrIdx].push_front(CacheNode(addrTag));
	}

	// ========== victim cache implementation ===============

	// miss in L1, vc enabled: probe the victim cache
	else {
		// update miss count
		++result.misses;
		list<VCNode>::iterator vcbeg = victimCache.begin(); // iterator in vimtim cache
		while (vcbeg != victimCache.end() && (vcbeg->idx != addrIdx || vcbeg->tag != addrTag)) ++vcbeg;

		// hit on block in VC, swap it with the LRU block from corresponding L1 cache set and promote to MRU position
		if (vcbeg != victimCache.end()) {
			// check whether its a prefetch block in VC
			if (vcbeg->isPrefetch) {
				++result.useful_prefetches;
				vcbeg->isPrefetch = false;
			}
			// swap hit block in vc with LRU block in L1 and then make it MRU
			// cacheSets[addrIdx] must be full, otherwise the hit block wouldn't be found in vc
			VCNode temp = *vcbeg;
			// move the LRU block in L1 cache to VC
			*vcbeg = VCNode(cacheSets[addrIdx].back(), addrIdx);
			cacheSets[addrIdx].pop_back();
			// insert the hit block in VC to L1 cache at the MRU position
			cacheSets[addrIdx].push_front(CacheNode(temp.tag, temp.dirty, NONPREFETCH));
		}

		// miss in VC, fetch the block from main memory and insert into L1 cache, update VC correspondingly
		else {
			// update vc miss count
			++result.vc_misses;
			if (cacheSets[addrIdx].size() == set_capacity) {
				// evict the oldest block when VC is full, check dirty bit and update writeback count
				if (victimCache.size() == v) {
					if (victimCache.front().dirty) ++result.writebacks;
					victimCache.pop_front();
				}
				// move LRU block from L1 to VC when L1 cache set is full
				victimCache.push_back(VCNode(cacheSets[addrIdx].back(), addrIdx));
				cacheSets[addrIdx].pop_back();
			}
			// fetch block from main memory and insert at MRU position of L1 cache set
			cacheSets[addrIdx].push_front(CacheNode(addrTag));
		}
	}
	// ========== end of victim cache implementation ===============

	// set dirty bit per write access (the accessed block will be in the MRU position)
	if (rw == WRITE) cacheSets[addrIdx].front().dirty = DIRTY;



	// =============== prefetch implementation ===============

	// check prefetcher when there's an L1 miss (even it hits in vc)
	if (k && result.misses) {
		// calculate prefetch stride (block offset bits are discarded)
		bool d_sign = (address >> b) > last_miss;
		uint64_t d = d_sign ? (address >> b) - last_miss : last_miss - (address >> b);

		// check prefether
		if (d_sign == stride_sign && d == pending_stride) {
			// update prefetch blocks count
			result.prefetch_blocks += k;
			// initialize prefetch address, index and tag (prefetch address is set to the miss block address)
			uint64_t prefetch_addr = (address >> b); // block address with offset bits discarded
			uint64_t prefetch_tag;
			unsigned int prefetch_index;

			// prefetch K blocks
			for (int i = 0; i != k; ++i) {
				// calculate prefetch address, index and tag
				if (d_sign)
					prefetch_addr += d;
				else
					prefetch_addr -= d;
				prefetch_index = (prefetch_addr & ((1 << (c - s - b)) - 1));
				prefetch_tag = prefetch_addr >> (c - s - b);

				// check whether it already exists in the cache
				list<CacheNode>::iterator prefbeg = cacheSets[prefetch_index].begin();
				while (prefbeg != cacheSets[prefetch_index].end() && prefbeg->tag != prefetch_tag) ++prefbeg;

				// if the block is already in L1 cache, don't do anything

				// if the block is not in L1, check whether it's in VC, or prefetch when VC is disabled
				if (prefbeg == cacheSets[prefetch_index].end()) {

					// vc disabled: evict LRU block when cache set is full, then prefetch into LRU position in L1 cache set
					if (!v) {
						if (cacheSets[prefetch_index].size() == set_capacity) {
							if (cacheSets[prefetch_index].back().dirty)
								++result.writebacks;
							cacheSets[prefetch_index].pop_back();
						}
						cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, CLEAN, PREFETCH));
					}

					// VC enabled: check whether the block is already in VC
					else {
						list<VCNode>::iterator prefvcbeg = victimCache.begin();
						while (prefvcbeg != victimCache.end() && (prefvcbeg->idx != prefetch_index || prefvcbeg->tag != prefetch_tag))
							++prefvcbeg;
						// if the block is in VC, swap it with the LRU block in L1 cache set and set prefetch bit
						if (prefvcbeg != victimCache.end()) {
							VCNode temp = *prefvcbeg;
							*prefvcbeg = VCNode(cacheSets[prefetch_index].back(), prefetch_index);
							// preserve dirty bit and set prefetch bit to true when insert into L1 cache
							cacheSets[prefetch_index].back() = CacheNode(temp.tag, temp.dirty, PREFETCH);
						}

						// if the block is not in VC, prefetch from main memory
						// replace the LRU block with the prefetched block and set prefetch bit
						// the LRU block goes into VC, and the oldest block in VC is evicted when VC is full
						else {
							if (cacheSets[prefetch_index].size() == set_capacity) {
								// evict the oldest block when VC is full, check dirty bit and update writeback count
								if (victimCache.size() == v) {
									if (victimCache.front().dirty) ++result.writebacks;
									victimCache.pop_front();
								}
								// move the LRU block from L1 to VC when L1 cache set is full
								victimCache.push_back(VCNode(cacheSets[prefetch_index].back(), prefetch_index));
								cacheSets[prefetch_index].pop_back();
							}
							// prefetch from main memory and insert at the LRU position
							cacheSets[prefetch_index].push_back(CacheNode(prefetch_tag, CLEAN, PREFETCH));
						}
					}
				}
			}
		}

		// update prefetcher variables
		pending_stride = d;
		stride_sign = d_sign;
		last_miss = address >> b;
	}
	// =============== end of prefetch implementation ===============

	return result;
}

// Global object that simulates the cache
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
