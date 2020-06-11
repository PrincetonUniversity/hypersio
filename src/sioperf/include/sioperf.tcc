#include <iostream>
#include <iomanip>
#include <cmath>
#include "defines.h"
#include "sioperf.h"
#include "config.h"
#include "dbg.h"

using namespace std;

template <template <typename, typename> class IOTLB_ROW_T>
SIOPerf<IOTLB_ROW_T>::SIOPerf(string cfg_path, string trace_prefix, size_t tqueue_size, bool isOOO) {
    _sid_context_map = new unordered_map<sid_t, CCEvent *>;
    _paddr_slpte_map = new unordered_map<hwaddr, SlpteEvent *>;

    // Log files names
    _trace_prefix = trace_prefix;
    string hypersio_home = string(getHyperSIOHome());
    _iova_trace_path    = hypersio_home + BUILD_FOLDER + trace_prefix + "_" + IOVA_TRACE_NAME;
    _cc_data_path       = hypersio_home + BUILD_FOLDER + trace_prefix + "_" + CC_DATA_NAME;
    _slpt_data_path     = hypersio_home + BUILD_FOLDER + trace_prefix + "_" + SLPT_DATA_NAME;
    _cache_config_path  = cfg_path;

    // Statistics
    _trans_cnt = 0;
    _stat_pkt_drop_cnt = 0;

    _trans_in_flight_max = tqueue_size;
    _trans_in_flight = 0;

    _tqueue_ooo = isOOO;
    _cc_fill_simevents = ScheduledSimEvents(isOOO);
    _pcache_trans_simevents = ScheduledSimEvents(isOOO);
    _pcache_fill_simevents = ScheduledSimEvents(isOOO);
    _iotlb_fill_simevents = ScheduledSimEvents(isOOO);

    // Create a map cache_name:cache_configuration
    _cache_cfg_map[IOTLB_CACHE_NAME] = new CacheConfig();
    _cache_cfg_map[CONTEXT_CACHE_NAME] = new CacheConfig();
    _cache_cfg_map[L3PT_CACHE_NAME] = new CacheConfig();
    _cache_cfg_map[L2PT_CACHE_NAME] = new CacheConfig();
    _cache_cfg_map[L1PT_CACHE_NAME] = new CacheConfig();
    _cache_cfg_map[PREFETCH_CACHE_NAME] = new CacheConfig();
    // Set cache configurations from a file
    ReadCacheConfig();

    // Translation Caches
    _cc_cache = new ContextCache<CacheRowLFU, sid_t>("CC", CACHE_CONTEXT,
                                                    _cache_cfg_map[CONTEXT_CACHE_NAME]->size,
                                                    _cache_cfg_map[CONTEXT_CACHE_NAME]->assoc,
                                                    _cache_cfg_map[CONTEXT_CACHE_NAME]->groups);
        
    _iotlb = new AddrCache<IOTLB_ROW_T, AddrDidKey>("IOTLB", CACHE_L1,
                                                    _cache_cfg_map[IOTLB_CACHE_NAME]->size,
                                                    _cache_cfg_map[IOTLB_CACHE_NAME]->assoc,
                                                    _cache_cfg_map[IOTLB_CACHE_NAME]->groups);


    _l2_pcache = new AddrCache<CacheRowLRU, AddrDidKey>("L2PAGE", CACHE_L2,
                                                    _cache_cfg_map[L2PT_CACHE_NAME]->size,
                                                    _cache_cfg_map[L2PT_CACHE_NAME]->assoc,
                                                    _cache_cfg_map[L2PT_CACHE_NAME]->groups);

    _l3_pcache = new AddrCache<CacheRowLRU, AddrDidKey>("L3PAGE", CACHE_L3,
                                                    _cache_cfg_map[L3PT_CACHE_NAME]->size,
                                                    _cache_cfg_map[L3PT_CACHE_NAME]->assoc,
                                                    _cache_cfg_map[L3PT_CACHE_NAME]->groups);

    _prefetcher = new Prefetcher(_cache_cfg_map[PREFETCH_CACHE_NAME]->size,
                                 _cache_cfg_map[PREFETCH_CACHE_NAME]->histlen,
                                 _cache_cfg_map[PREFETCH_CACHE_NAME]->assoc);

}

template <template <typename, typename> class IOTLB_ROW_T>
SIOPerf<IOTLB_ROW_T>::~SIOPerf() {
    // Delete Context Cache Data Structure
    for (auto it = _sid_context_map->begin(); it != _sid_context_map->end(); it++) {
        delete it->second;
    }
    delete _sid_context_map;

    // Delete SLPT Data Structure
    for (auto it = _paddr_slpte_map->begin(); it != _paddr_slpte_map->end(); it ++) {
        delete it->second;
    }
    delete _paddr_slpte_map;

    for (auto it = _cache_cfg_map.begin(); it != _cache_cfg_map.end(); it++) {
        delete it->second;
    }

    for (auto it = _did_flowstat_map.begin(); it != _did_flowstat_map.end(); it++) {
        delete it->second;
    }

    delete _iotlb;
    delete _cc_cache;
    delete _l3_pcache;
    delete _l2_pcache;
}

template <template <typename, typename> class IOTLB_ROW_T>
int SIOPerf<IOTLB_ROW_T>::SetupEnv()
{
    int rv;

    LOG_INFO("Reading Context Data from: %s\n", _cc_data_path.c_str());
    rv = ReadCCData();
    if (rv) {
        LOG_ERR("Failed to read Context Data\n");
        goto read_data_err;
    }
    
    LOG_INFO("Reading SLPT Data from: %s\n", _slpt_data_path.c_str());
    rv = ReadSLPTData();
    if (rv) {
        LOG_ERR("Failed to read SLPT Data\n");
        goto read_data_err;
    }

    return 0;
    
read_data_err:
    return 1;
}


template <template <typename, typename> class IOTLB_ROW_T>
int SIOPerf<IOTLB_ROW_T>::PlayIOVATrace()
{
    int rv;
    event_t event_type;
    TranslateEvent *event_ptr;
    TranslateEvent *translated_ev_ptr;
    ifstream transl_stream(_iova_trace_path, ios::binary);
    bool trace_ended;

    rv = SetupEnv();
    if (rv) {
        LOG_ERR("Can't setup environment for simulation!\n");
        return 1;
    }

    // Play IOVA trace
    if (!transl_stream) {
        LOG_ERR("Can't open IOVA trace file: %s\n", _iova_trace_path.c_str());
        return 1;
    }

    transl_stream.seekg(0, ios::beg);
    // Read events until read method sets eof flag
    LOG_INFO("Playing Translation Trace from: %s\n", _iova_trace_path.c_str());

    trace_ended = false;
    while (true) {
        // Start new Translation
        // Model every link pkt by issuing transations in bundle
        if ((_simtime.Get() % _pkt_interarrival_time_ns) == 0) {
            // drop pkt translation bundle if there is no space in a queue
            if (_trans_in_flight < _trans_in_flight_max) {
                for (auto i = 0; i < TRANSLATIONS_PER_PKT; i++ ) {
                    transl_stream.read(reinterpret_cast<char *>(&event_type), sizeof(event_t));
                    if (transl_stream.eof()) {
                        trace_ended = true;
                        break;
                    }
                    switch (event_type) {
                        case TRANSLATE_EVENT:
                            event_ptr = new TranslateEvent();
                            event_ptr->ReadFromFile(transl_stream);
                            Translate(event_ptr);
                            _trans_cnt++;
                            _trans_in_flight++;
                            break;
                        default:
                            LOG_ERR("Read an unknown event!\n");
                            return 1;
                            break;
                    }
                }
            } else {
                _stat_pkt_drop_cnt++;
            }
        }

        if (trace_ended) {
            break;
        }

        // Check if any translation was done in current cycle
        translated_ev_ptr = RunCycle();
        if(translated_ev_ptr != nullptr) {
            _trans_in_flight--;
            if (_trans_in_flight < 0) {
                LOG_ERR("BUG: Translation in flight is less than 0!\n");
                return -1;
            }
            UpdateStat(translated_ev_ptr);
            delete translated_ev_ptr;
        }

        // Go to next cycles
        _simtime.Add(1);
    }

    // Drain all remained translations
    LOG_INFO("\n\nWaiting for translations to drain...\n\n");
    while (_trans_in_flight > 0) {
        translated_ev_ptr = RunCycle();
        if (translated_ev_ptr != nullptr) {
            _trans_in_flight--;
            UpdateStat(translated_ev_ptr);
            delete translated_ev_ptr;
        }
        _simtime.Add(1);
    }

    return 0;
}

template <template <typename, typename> class IOTLB_ROW_T>
TranslateEvent *SIOPerf<IOTLB_ROW_T>::RunCycle()
{
    bool iotlb_hit, l2p_hit, l3p_hit, prefetch_hit;
    bool alloc_in_cache;
    bool suggest_prefetch;
    sid_t prefetch_sid;
    did_t tr_ev_did;
    timecnt_t time_now = _simtime.Get();
    timecnt_t trans_time;
    SimEvent cc_fill_se, pcache_se, pcache_fill_se, iotlb_fill_se;
    vector<SimEvent> cc_fill_se_v, pcache_se_v, pcache_fill_se_v, iotlb_fill_se_v;
    TranslateEvent *tr_event_ptr;
    vector<TranslateEvent> prefetch_te_v;

    _cc_fill_simevents.PickReady(time_now, &cc_fill_se_v);
    while (!cc_fill_se_v.empty()) {
        cc_fill_se = cc_fill_se_v.back();
        cc_fill_se_v.pop_back();

        // Allocate an entry in Context Cache
        tr_event_ptr = cc_fill_se.GetEvent();
        _cc_cache->Translate(tr_event_ptr, true);
        DEBUG("@%lu: CC read\n", time_now);

        // Translation requests to IOTLB are fed from CC fill queue
        iotlb_hit = _iotlb->Translate(tr_event_ptr, false);
        prefetch_hit = _prefetcher->Translate(tr_event_ptr);
        // Check if current access triggers prefetch hint

        // IOTLB hit
        if (iotlb_hit) {
            DEBUG("@%lu: IOTLB hit!\n", time_now);
            trans_time = SIM_LAT_IOTLB_HIT;
            tr_event_ptr->AddTransTime(trans_time);
            _iotlb_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
        // IOTLB miss
        } else  {
            if (prefetch_hit) {
                trans_time = SIM_LAT_PREFETCH_HIT;
                tr_event_ptr->AddTransTime(trans_time);
                _iotlb_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
            } else {
                trans_time = SIM_LAT_PCIE;
                tr_event_ptr->AddTransTime(trans_time);
                _pcache_trans_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time, false);
            }
        }
        
        // Prefetch hint is used on both hit and miss
        suggest_prefetch = _prefetcher->Probe(tr_event_ptr, &prefetch_sid, prefetch_te_v);
        if (suggest_prefetch) {
            trans_time = SIM_LAT_PCIE + SIM_LAT_DDR;
            TranslateEvent *pref_event_ptr;
            for (auto it = prefetch_te_v.begin(); it != prefetch_te_v.end(); it++) {
                pref_event_ptr = new TranslateEvent(TRANSLATE_EVENT, it->GetSID(), it->GetIOVA(), it->GetDID(),
                                   it->GetSLPTE(), it->GetMask(), it->GetLvl());
                pref_event_ptr->SetPrefetch(true);
                pref_event_ptr->AddTransTime(trans_time);
                _pcache_trans_simevents.ScheduleEvent(pref_event_ptr, time_now + trans_time, true);
            }
        }
    }

    // Accesses to Page Caches are parallel
    _pcache_trans_simevents.PickReady(time_now, &pcache_se_v);
    while (!pcache_se_v.empty()) {
        pcache_se = pcache_se_v.back();
        pcache_se_v.pop_back();

        DEBUG("@%lu Accessing Page Caches\n", time_now);
        tr_event_ptr = pcache_se.GetEvent();
        l2p_hit = _l2_pcache->Translate(tr_event_ptr, false);
        l3p_hit = _l3_pcache->Translate(tr_event_ptr, false);
        if (l2p_hit) {
            // Reading PFN for 2M page from cache
            if (tr_event_ptr->GetLvl() == 2) {
                trans_time = SIM_LAT_PCACHE_HIT + SIM_LAT_PCIE;
                tr_event_ptr->AddTransTime(trans_time);
                _iotlb_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
            // Have to access memory to get PFN for 4k page
            } else {
                if (tr_event_ptr->GetLvl() > 2) {
                    LOG_WARN("Wow, unexpected level 0x%lx!\n", tr_event_ptr->GetLvl());
                    tr_event_ptr->Print();
                } else {
                    trans_time = SIM_LAT_DDR*(2 - tr_event_ptr->GetLvl());
                    tr_event_ptr->AddTransTime(trans_time);
                    _pcache_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
                }
            }
        // L2 page cache miss
        } else if (l3p_hit) {
            // 1GB page
            if (tr_event_ptr->GetLvl() == 3) {
                trans_time = SIM_LAT_PCACHE_HIT;
                tr_event_ptr->AddTransTime(trans_time);
                _iotlb_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
            } else {
                if (tr_event_ptr->GetLvl() > 3) {
                    LOG_WARN("Wow, unexpected level 0x%lx!\n", tr_event_ptr->GetLvl());
                    tr_event_ptr->Print();
                } else {
                    trans_time = SIM_LAT_DDR*(3 - tr_event_ptr->GetLvl());
                    tr_event_ptr->AddTransTime(trans_time);
                    _pcache_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
                }
            }
        // L3 page cache miss
        } else {
            // walking all levels
            trans_time = SIM_LAT_DDR*(tr_event_ptr->GetLvl());
            tr_event_ptr->AddTransTime(trans_time);
            _pcache_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time - 1, false);
            DEBUG("@%lu Creating Page Cache fill @%lu\n", time_now, time_now + trans_time);
        }

    }

    // Page Cache Fills
    _pcache_fill_simevents.PickReady(time_now, &pcache_fill_se_v);
    while (!pcache_fill_se_v.empty()) {
        pcache_fill_se = pcache_fill_se_v.back();
        pcache_fill_se_v.pop_back();

        tr_event_ptr = pcache_fill_se.GetEvent();
        tr_ev_did = tr_event_ptr->GetDID();
        // L2 page cache
        alloc_in_cache = _l2_pcache->IsDIDCacheable(tr_ev_did);
        _l2_pcache->Translate(tr_event_ptr, alloc_in_cache);
        // L3 page cache
        alloc_in_cache = _l3_pcache->IsDIDCacheable(tr_ev_did);
        _l3_pcache->Translate(tr_event_ptr, alloc_in_cache);
        
        trans_time = SIM_LAT_PCIE;
        tr_event_ptr->AddTransTime(trans_time);
        _iotlb_fill_simevents.ScheduleEvent(tr_event_ptr, time_now + trans_time, false);
        DEBUG("@%lu Creating IOTLB Fill @%lu\n", time_now, time_now + trans_time);
    }

    // IOTLB Fills
    _iotlb_fill_simevents.PickReady(time_now, &iotlb_fill_se_v);
    while (!iotlb_fill_se_v.empty()) {
        iotlb_fill_se = iotlb_fill_se_v.back();
        iotlb_fill_se_v.pop_back();

        tr_event_ptr = iotlb_fill_se.GetEvent();
        
        if (tr_event_ptr->isPrefetch()) {
            _prefetcher->Fill(tr_event_ptr);
            delete tr_event_ptr;
            return nullptr;
        } else {
            tr_ev_did = tr_event_ptr->GetDID();
            alloc_in_cache = _iotlb->IsDIDCacheable(tr_ev_did);
            _iotlb->Translate(tr_event_ptr, alloc_in_cache);
            DEBUG("@%lu IOTLB Fill\n", time_now);
            return tr_event_ptr;
        }
        
    }

    return nullptr;
}


template <template <typename, typename> class IOTLB_ROW_T>
void SIOPerf<IOTLB_ROW_T>::Translate(TranslateEvent *e)
{
    bool cc_hit;
    size_t trans_time = 0;
    timecnt_t time_now = _simtime.Get();

    // Before looking up page caches,
    // we always must get DID first
    DEBUG("Tranlating:\n");
    #ifdef DEBUG_BUILD
        e->Print();
    #endif
    cc_hit = _cc_cache->Translate(e, false);
    e->SetTransStartTime(time_now);
    if (!cc_hit) {
        trans_time = SIM_LAT_PCIE_RTT + MEM_LAT_CE_READ;
        DEBUG("@%lu: Creating SimEvent @%lu for CC miss\n", time_now, time_now + trans_time);
        e->AddTransTime(trans_time);
        _cc_fill_simevents.ScheduleEvent(e, time_now + trans_time, false);
    } else {
        DEBUG("@%lu: Creating SimEvent @%lu for CC hit\n", time_now, time_now + trans_time);
        trans_time = SIM_LAT_CC_HIT;
        e->AddTransTime(trans_time);
        _cc_fill_simevents.ScheduleEvent(e, time_now + trans_time, false);
    }
}


template <template <typename, typename> class IOTLB_ROW_T>
int SIOPerf<IOTLB_ROW_T>::ReadCCData()
{
    CCEvent *ccevent_ptr;

    ifstream cc_data_stream(_cc_data_path, ios::binary);
    if (!cc_data_stream) {
        LOG_ERR("Can't open Context Cache Data file: %s!\n", _cc_data_path.c_str());
        return 1;
    }

    cc_data_stream.seekg(0, ios::beg);
    while(true) {
        ccevent_ptr = new CCEvent();
        ccevent_ptr->ReadFromFile(cc_data_stream);
        // hit EOF while reading an event
        // the last event is not complete, so discard it
        if (cc_data_stream.eof()) {
            break;
        }
        (*_sid_context_map)[ccevent_ptr->GetSID()] = ccevent_ptr;
    }

    return 0;
}

template <template <typename, typename> class IOTLB_ROW_T>
int SIOPerf<IOTLB_ROW_T>::ReadSLPTData()
{
    SlpteEvent *slptevent_ptr;

    ifstream slpt_data_stream(_slpt_data_path, ios::binary);
    if (!slpt_data_stream) {
        LOG_ERR("Can't open SLPT Data file: %s!\n", _slpt_data_path.c_str());
        return 1;
    }

    slpt_data_stream.seekg(0, ios::beg);
    while(true) {
        slptevent_ptr = new SlpteEvent();
        slptevent_ptr->ReadFromFile(slpt_data_stream);
        // discard last event if we hit EOF
        if (slpt_data_stream.eof()) {
            break;
        }
        (*_paddr_slpte_map)[slptevent_ptr->GetSlpteAddr()] = slptevent_ptr;
    }

    return 0;
}

template <template <typename, typename> class IOTLB_ROW_T>
void SIOPerf<IOTLB_ROW_T>::PrintStat()
{
    // Every pkt involves multiple accesses to main memory, and as a result
    // multiple tranlations
    double pkt_num = _trans_cnt / TRANSLATIONS_PER_PKT;
    // Time is in [s*10^(-9)], BW in [bps*10^(6)]
    size_t mbps  = (size_t) (pkt_num * DEFAULT_PKT_SIZE_BYTES * 8 * 1000) / (_simtime.Get());

    std::cout << "------------------------------------------\n";
    std::cout << "Time elapsed :        " << _simtime.Get() << " ns\n";
    std::cout << "Requests translated:  " << _trans_cnt << "\n";
    std::cout << "Pkts dropped:         " << _stat_pkt_drop_cnt << "\n";
    std::cout << "[Mb/s] for " << DEFAULT_PKT_SIZE_BYTES << "B pkts: " << mbps << "\n";
    std::cout << "------------------------------------------\n";


    // Statistics per flow
    std::cout << "Per Flow Statistics:\n";
    for (auto it = _did_flowstat_map.begin(); it != _did_flowstat_map.end(); it++) {
        std::cout << "DID: 0x" << hex << setw(6) << it->first << dec << ", ";
        (it->second)->PrintStat();
    }
    std::cout << "------------------------------------------\n";    

    if (_cc_cache) {
        _cc_cache->PrintStat();
    }

    if (_iotlb) {
        _iotlb->PrintStat();
    }


    if (_l2_pcache) {
        _l2_pcache->PrintStat();
    }

    if (_l3_pcache) {
        _l3_pcache->PrintStat();
    }

    _prefetcher->PrintStat();
}

template <template <typename, typename> class IOTLB_ROW_T>
int SIOPerf<IOTLB_ROW_T>::ReadCacheConfig()
{
    ifstream cfg_stream(_cache_config_path);
    string line;
    size_t colon_pos;
    string cname;
    size_t cfgval;
    string substr;

    if (!cfg_stream) {
        LOG_ERR("Can't open cache configuration file: %s\n", _cache_config_path.c_str());
        return 1;
    }

    LOG_INFO("Reading Configuration from %s\n", _cache_config_path.c_str());
    cfg_stream.seekg(0, ios::beg);
    while (getline(cfg_stream, line)) {
        // skip lines starting with '//', which are comments
        if (line.compare(0,2,"//") == 0) {
            continue;
        }

        colon_pos = line.find(":");
        // No colon - Parsing Cache name
        if (colon_pos == string::npos) {
            substr = line.substr(0,colon_pos-1);
            // valid cache name
            if (_cache_cfg_map.find(substr) != _cache_cfg_map.end()) {
                cname = substr;
            } else {
                LOG_ERR("Unknown Cache Name: %s\n", line.c_str());
                return 1;
            }
        // Parsing Cache configuration
        } else {
            if (line.compare(0,4,"size") == 0) {
                cfgval = stoul(line.substr(colon_pos+1, line.size()-1));
                _cache_cfg_map[cname]->size = cfgval;
            } else if (line.compare(0,5,"assoc") == 0) {
                cfgval = stoul(line.substr(colon_pos+1, line.size()-1));
                _cache_cfg_map[cname]->assoc = cfgval;
            } else if (line.compare(0,6,"groups") == 0) {
                cfgval = stoul(line.substr(colon_pos+1, line.size()-1));
                _cache_cfg_map[cname]->groups = cfgval;
            } else if (line.compare(0,7,"histlen") == 0) {
                cfgval = stoul(line.substr(colon_pos+1, line.size()-1));
                _cache_cfg_map[cname]->histlen = cfgval;
            }else {
                LOG_ERR("Unknown configuration: %s\n", line.c_str());
                return 1;
            }
        }
    }

    return 0;
}

// Update per Flow Statistics
template <template <typename, typename> class IOTLB_ROW_T>
void SIOPerf<IOTLB_ROW_T>::UpdateStat(TranslateEvent *te_ptr)
{

    DEBUG("Translation for DID %d was completed in %lu ns\n", te_ptr->GetDID(), te_ptr->GetTransTime());
    
    // Create an entry if is doesn't exist
    if (_did_flowstat_map.find(te_ptr->GetDID()) == _did_flowstat_map.end()) {
        _did_flowstat_map[te_ptr->GetDID()] = new FlowStat(te_ptr);
    } else {
    // Update an existing entry
        (_did_flowstat_map[te_ptr->GetDID()])->Update(te_ptr);
    }

}

template <template <typename, typename> class IOTLB_ROW_T>
void SIOPerf<IOTLB_ROW_T>::SetLinkSpeed(uint64_t gbps)
{
    _pkt_interarrival_time_ns = ((DEFAULT_PKT_SIZE_BYTES)*8)/gbps;
    _link_bw_gbps = gbps;
}


template <template <typename, typename> class IOTLB_ROW_T>
void SIOPerf<IOTLB_ROW_T>::ConfigSelectiveAlloc(bool salloc, size_t tnum)
{
    vector<did_t> possible_did_v;

    _salloc = salloc;

    // Vector with all possible DIDs
    for (did_t i = 0; i < tnum; i++) {
        possible_did_v.push_back(i);
    }

    // Always allocate in Context Cache
    _cc_cache->ConfigSelectiveAlloc(false, &possible_did_v);
    _iotlb->ConfigSelectiveAlloc(salloc, (size_t) 0);
    _l2_pcache->ConfigSelectiveAlloc(salloc, _iotlb->GetGroupNum());
    _l3_pcache->ConfigSelectiveAlloc(salloc, _iotlb->GetGroupNum() + _l2_pcache->GetGroupNum());
}

template <template <typename, typename> class IOTLB_ROW_T>
void SIOPerf<IOTLB_ROW_T>::PrintConfig()
{
    std::cout << "------------------------------------------\n";
    std::cout << "SIOPERF Configuration\n";
    std::cout << "------------------------------------------\n";
    std::cout << "TRANSL QUEUE ENTR : " << _trans_in_flight_max << "\n";
    std::cout << "TRANSL QUEUE OOO  : " << boolToString(_tqueue_ooo) << "\n";
    std::cout << "SELECTIVE ALLOC   : " << boolToString(_salloc) << "\n";

    _cc_cache->PrintConfig();
    _iotlb->PrintConfig();
    _l2_pcache->PrintConfig();
    _l3_pcache->PrintConfig();
}