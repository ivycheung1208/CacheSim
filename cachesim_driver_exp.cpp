#include <cstdio>
#include <cstdlib>
#include <cstring>
//#include <unistd.h>
#include "XGetopt.h"
#include "cachesim.hpp"

void print_help_and_exit(void) {
    printf("cachesim [OPTIONS] < traces/file.trace\n");
    printf("  -c C\t\tTotal size in bytes is 2^C\n");
    printf("  -b B\t\tSize of each block in bytes is 2^B\n");
    printf("  -s S\t\tNumber of blocks per set is 2^S\n");
    printf("  -v V\t\tNumber of blocks in victim cache\n");
    printf("  -k K\t\tPrefetch Distance");
	printf("  -h\t\tThis helpful output\n");
    exit(0);
}

void print_statistics(cache_stats_t* p_stats);

int main(int argc, char* argv[]) {
	//uint64_t test;
	//test = (1 << 3) - 1;
	//printf("%" PRIu64 "\n", test);
    int opt;
    uint64_t c = DEFAULT_C;
    uint64_t b = DEFAULT_B;
    uint64_t s = DEFAULT_S;
    uint64_t v = DEFAULT_V;
	uint64_t k = DEFAULT_K;
    FILE* fin  = stdin;
	FILE* fout = stdout;
	char inputfile[100];
	char outputfile[100];

    /* Read arguments */ 
    while(-1 != (opt = getopt(argc, argv, "c:b:s:i:v:k:h"))) {
        switch(opt) {
		case 'i':
			strcpy(inputfile, optarg);
			strcpy(outputfile, optarg);
			strcat(outputfile, ".out");
			break;
		case 'c':
            c = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'v':
            v = atoi(optarg);
            break;
		case 'k':
			k = atoi(optarg);
			break;
        case 'h':
            /* Fall through */
        default:
            print_help_and_exit();
            break;
        }
    }

/*	printf("Cache Settings\n");
	printf("C: %" PRIu64 "\n", c);
	printf("B: %" PRIu64 "\n", b);
	printf("S: %" PRIu64 "\n", s);
	printf("V: %" PRIu64 "\n", v);
	printf("K: %" PRIu64 "\n", k);
	printf("\n");
*/

	fout = fopen(outputfile, "w");
	fprintf(fout, "%s:\n\n", inputfile);

	double AAT_min = AAT_MAX;
	uint64_t AAT_min_c = DEFAULT_C;
	uint64_t AAT_min_b = DEFAULT_B;
	uint64_t AAT_min_s = DEFAULT_S;
	uint64_t AAT_min_v = DEFAULT_V;
	uint64_t AAT_min_k = DEFAULT_K;

	for (c = 12; c <= 15; ++c) {
		for (b = 3; b <= 6; ++b) {
			for (s = 0; s <= c - b; ++s) {
//				for (v = 0; v <= 4; ++v) {
//					for (k = 0; k <= 4; ++k) {

						printf("%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t", c, b, s, v, k);
						fprintf(fout, "%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t", c, b, s, v, k);

						/* calculate memory budge */
						uint64_t data_storage = (1 << b) * 8;
						//	printf("data storage: %" PRIu64 "\n", data_storage);
						uint64_t cache_memory = (1 << (c - b)) * (64 - c + s + 1 + data_storage);
						//	printf("cache memory: %" PRIu64 "\n", cache_memory);
						uint64_t vc_memory = v * (64 - b + 1 + data_storage);
						//	printf("vc memory: %" PRIu64 "\n", vc_memory);
						double total_memory_kb = (cache_memory + vc_memory) / double((1 << 10) * 8);
						printf("%f\t", total_memory_kb);
						fprintf(fout, "%f\t", total_memory_kb);

						/* skip if memory limitation exceeded */
						if (total_memory_kb > 48) {
							printf("\n");
							fprintf(fout, "\n");
							continue;
						}

						/* Setup the cache */
						setup_cache(c, b, s, v, k);

						/* Setup statistics */
						cache_stats_t stats;
						memset(&stats, 0, sizeof(cache_stats_t));

						/* Begin reading the file */
						fin = fopen(inputfile, "r");
						char rw;
						uint64_t address;
						while (!feof(fin)) {
							int ret = fscanf(fin, "%c %" PRIx64 "\n", &rw, &address);
							if (ret == 2) {
								cache_access(rw, address, &stats);
							}
						}
						fclose(fin);

						complete_cache(&stats);

						printf("%f\n", stats.avg_access_time);
						fprintf(fout, "%f\n", stats.avg_access_time);

						// update optimal setting
						if (stats.avg_access_time < AAT_min) {
							AAT_min = stats.avg_access_time;
							AAT_min_c = c;
							AAT_min_b = b;
							AAT_min_s = s;
							AAT_min_v = v;
							AAT_min_k = k;
						}

//					}
//				}
			}
		}
	}

	printf("\nBest AAT: %f\n", AAT_min);
	fprintf(fout, "\nBest AAT: %f\n", AAT_min);
	printf("Setting: %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
		AAT_min_c, AAT_min_b, AAT_min_s, AAT_min_v, AAT_min_k);
	fprintf(fout, "Setting: %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
		AAT_min_c, AAT_min_b, AAT_min_s, AAT_min_v, AAT_min_k);
	fclose(fout);

	return 0;
}

void print_statistics(cache_stats_t* p_stats) {
	printf("\n");
	printf("Cache Statistics\n");
    printf("Accesses: %" PRIu64 "\n", p_stats->accesses);
    printf("Reads: %" PRIu64 "\n", p_stats->reads);
    printf("Read misses: %" PRIu64 "\n", p_stats->read_misses);
    printf("Read misses combined: %" PRIu64 "\n", p_stats->read_misses_combined);
    printf("Writes: %" PRIu64 "\n", p_stats->writes);
    printf("Write misses: %" PRIu64 "\n", p_stats->write_misses);
    printf("Write misses combined: %" PRIu64 "\n", p_stats->write_misses_combined);
    printf("Misses: %" PRIu64 "\n", p_stats->misses);
    printf("Writebacks: %" PRIu64 "\n", p_stats->write_backs);
	printf("Victim cache misses: %" PRIu64 "\n", p_stats->vc_misses);
	printf("Prefetched blocks: %" PRIu64 "\n", p_stats->prefetched_blocks);
	printf("Useful prefetches: %" PRIu64 "\n", p_stats->useful_prefetches);
	printf("Bytes transferred to/from memory: %" PRIu64 "\n", p_stats->bytes_transferred);
	printf("Hit Time: %f\n", p_stats->hit_time);
    printf("Miss Penalty: %" PRIu64 "\n", p_stats->miss_penalty);
    printf("Miss rate: %f\n", p_stats->miss_rate);
	printf("Average access time (AAT): %f\n", p_stats->avg_access_time);
}
