#ifndef __PREFETCHER_H__
#define __PREFETCHER_H__

#include <map>
#include <vector>
#include "event.h"
#include "cache.h"
#include "event.h"

struct TableEntry {
    sid_t       sid;
    uint16_t    cnt;
};

struct HistoryEntry {
    std::vector<TranslateEvent>     te_v;
    size_t                  hit_cnt;
    size_t                  miss_cnt;
};

struct PrefetcherStat {
    uint64_t                    access_num;
    uint64_t                    correct_sid_prediction;
    uint64_t                    incorrect_sid_prediction;
    uint64_t                    made_prediction;
    uint64_t                    new_sid_mapping;
    uint64_t                    trans_to_diff_sid;
    uint64_t                    trans_to_same_sid;
    uint64_t                    fill_the_same;
    uint64_t                    buffer_fill;
    uint64_t                    buffer_hit;
    uint64_t                    buffer_miss;
    std::map<sid_t, uint64_t>   intersid_cnt;
};

class Prefetcher
{
public:
    Prefetcher(size_t buff_size, size_t histlen, size_t pref_dist);
    ~Prefetcher();
    bool Probe(TranslateEvent *, sid_t *, std::vector<TranslateEvent> &);
    void PrintStat(void);
    void Fill(TranslateEvent *te);
    bool Translate(TranslateEvent *te);
private:
    // SID - nSID mapping - in memory
    std::map<sid_t, TableEntry> _sid_nsid_map;
    std::map<sid_t, HistoryEntry> _sid_history_map;
    size_t _lastaddr_num;
    AddrCache<CacheRowLRU, AddrDidKey> *_buffer = nullptr;
    sid_t _prev_sid;
    sid_t _prediction_sid;
    size_t _buffer_size;
    bool _is_prev_set;
    bool _is_prediction_set;
    PrefetcherStat _stat;
    std::vector<sid_t> _global_sid_hist;
    size_t _global_sid_hist_len;
};

#endif