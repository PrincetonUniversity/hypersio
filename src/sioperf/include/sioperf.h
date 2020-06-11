#ifndef __SIOPERF_H__
#define __SIOPERF_H__

#include <unordered_map>
#include <string>
#include <map>
#include <string>
#include <queue>
#include "event.h"
#include "cache.h"
#include "common.h"
#include "simevent.h"
#include "prefetcher.h"
#include "common.h"

// Top level class for scalable I/O performance simulator
template <template <typename, typename> class IOTLB_ROW_T>
class SIOPerf
{
public:
    SIOPerf(std::string config_name, std::string trace_pref, size_t tq_size, bool isOOO);
    ~SIOPerf();
    int PlayIOVATrace();
    void PrintStat();
    void ConfigSelectiveAlloc(bool salloc, size_t tnum);
    void PrintConfig();
    void SetLinkSpeed(uint64_t gbps);
private:
    // Methods
    int SetupEnv();
    int ReadCCData();
    int ReadSLPTData();
    int ReadCacheConfig();
    void Translate(TranslateEvent *);
    TranslateEvent *RunCycle();
    void UpdateStat(TranslateEvent *);
    // Variables
    std::unordered_map<sid_t, CCEvent *> *_sid_context_map;
    std::unordered_map<hwaddr, SlpteEvent *> *_paddr_slpte_map;
    uint64_t _pkt_interarrival_time_ns;
    uint64_t _link_bw_gbps;
    // Trace paths
    std::string _trace_prefix;
    std::string _iova_trace_path;
    std::string _cc_data_path;
    std::string _slpt_data_path;
    std::string _cache_config_path;
    // Caches
    std::map<std::string, CacheConfig *> _cache_cfg_map;
    ContextCache<CacheRowLFU, sid_t> *_cc_cache= nullptr;
    AddrCache<IOTLB_ROW_T, AddrDidKey> *_iotlb = nullptr;
    AddrCache<CacheRowLRU, AddrDidKey> *_l3_pcache = nullptr;
    AddrCache<CacheRowLRU, AddrDidKey> *_l2_pcache = nullptr;
    // Prefetcher
    Prefetcher *_prefetcher;
    // Event Queues for Caches
    ScheduledSimEvents _cc_fill_simevents;
    ScheduledSimEvents _pcache_trans_simevents;
    ScheduledSimEvents _pcache_fill_simevents;
    ScheduledSimEvents _iotlb_fill_simevents;
    size_t _trans_in_flight;
    size_t _trans_in_flight_max;
    bool _salloc;
    bool _tqueue_ooo;
    // Statistics
    TimeCnt _simtime = TimeCnt();
    size_t _trans_cnt;
    size_t _stat_pkt_drop_cnt;
    std::unordered_map<did_t, FlowStat*> _did_flowstat_map;
};

#endif