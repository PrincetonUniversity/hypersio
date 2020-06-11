#include <algorithm>
#include "prefetcher.h"

using namespace std;

Prefetcher::Prefetcher(size_t buff_size, size_t history_len, size_t prefetch_dist) {
    _buffer = new AddrCache<CacheRowLRU, AddrDidKey>("Prefetch Buffer", CACHE_L1,
                                                    buff_size, buff_size, 1);

    _is_prev_set = false;
    _is_prediction_set = false;
    _lastaddr_num = history_len;
    _buffer_size = buff_size;
    _global_sid_hist_len = prefetch_dist;

    _stat = {};
}

Prefetcher::~Prefetcher() {
    delete _buffer;
}

bool Prefetcher::Probe(TranslateEvent *event, sid_t *sid_for_prefetch, vector<TranslateEvent> &hwaddr_for_prefetch_v)
{
    map<sid_t, TableEntry>::iterator it;
    map<sid_t, HistoryEntry>::iterator sid_lastaddr_it;

    sid_t current_sid = event->GetSID();
    _stat.access_num++;

    // Collect stat about distance between the same sid
    bool intersid_cnt_found = false;
    for (auto it2 = _stat.intersid_cnt.begin(); it2 != _stat.intersid_cnt.end(); it2++) {
        if (it2->first == current_sid) {
            intersid_cnt_found = true;
            it2->second = 0;
        } else {
            it2->second++;
        }
    }
    if (!intersid_cnt_found) {
        _stat.intersid_cnt[current_sid] = 0;
    }

    
    // Read a list of last accessed addr for a sid
    sid_lastaddr_it = _sid_history_map.find(current_sid);
    if (sid_lastaddr_it == _sid_history_map.end()) {
        HistoryEntry history_entry;
        vector<TranslateEvent> te_v;
        history_entry.te_v = te_v;
        history_entry.hit_cnt = 0;
        history_entry.miss_cnt = 0;
        _sid_history_map[current_sid] = history_entry;
    }
    // Search for current page addr in the list of last accessed addr for a SID
    bool found_in_history = false;
    for (auto it = _sid_history_map[current_sid].te_v.begin(); it != _sid_history_map[current_sid].te_v.end(); it++) {
        if (event->GetPageAddr() == (*it).GetPageAddr()) {
            found_in_history = true;
            break;
        }
    }
    if (found_in_history) {
        // LOG_INFO("Hit in addr 0x%lx in history for SID 0x%x\n", event->GetPageAddr(), event->GetDID());
        _sid_history_map[current_sid].hit_cnt++;
    } else {
        _sid_history_map[current_sid].miss_cnt++;
    }
    // Record last accessed addr by a sid
    if (_sid_history_map[current_sid].te_v.size() == _lastaddr_num) {
        _sid_history_map[current_sid].te_v.erase(_sid_history_map[current_sid].te_v.begin());
    }
    TranslateEvent saved_event = TranslateEvent(TRANSLATE_EVENT, event->GetSID(), event->GetIOVA(), event->GetDID(),
                                                event->GetSLPTE(), event->GetMask(), event->GetLvl());
    _sid_history_map[current_sid].te_v.push_back(saved_event);




    // Check prediction
    if (_is_prediction_set) {
        if (current_sid == _prediction_sid) {
            _stat.correct_sid_prediction++;
        } else {
            _stat.incorrect_sid_prediction++;
        }
    }

    // Lookup if we can predict next sid
    it = _sid_nsid_map.find(current_sid);
    if (it != _sid_nsid_map.end()) {
        if (true) {
            _prediction_sid = (it->second).sid;
            *sid_for_prefetch = _prediction_sid;
            hwaddr_for_prefetch_v = _sid_history_map[_prediction_sid].te_v;
            _is_prediction_set = true;
            _stat.made_prediction++;
        } else {
            _is_prediction_set = false;
        }
    } else {
        _is_prediction_set = false;
    }
    
    // Add mapping sid -> next sid
    if (_is_prev_set) {
        if (_prev_sid != current_sid) {
            it = _sid_nsid_map.find(_prev_sid);
            if (it == _sid_nsid_map.end()) {
                _stat.new_sid_mapping++;
                TableEntry te = {current_sid, 0};
                _sid_nsid_map.insert(pair<sid_t, TableEntry>(_prev_sid, te));
            } else {
                (it->second).cnt++;
            }
        }

        if (_prev_sid == current_sid) {
            _stat.trans_to_same_sid++;
        } else {
            _stat.trans_to_diff_sid++;
        }
    }

    if (_global_sid_hist.size() == _global_sid_hist_len) {
        _prev_sid = _global_sid_hist.front();
        _global_sid_hist.erase(_global_sid_hist.begin());
        _is_prev_set = true;
    }
    _global_sid_hist.push_back(current_sid);

    return _is_prediction_set;
}

void Prefetcher::Fill(TranslateEvent *te)
{
    bool was_before;
    was_before = _buffer->Translate(te, true);
    if (was_before) {
        _stat.fill_the_same++;
    }
    _stat.buffer_fill++;
}

bool Prefetcher::Translate(TranslateEvent *te)
{
    bool hit = _buffer->Translate(te, false);
    if (hit) {
        _stat.buffer_hit++;
    } else {
        _stat.buffer_miss++;
    }

    return hit;
}


void Prefetcher::PrintStat(void)
{
    cout << "======================================================\n";
    cout << "Prefetcher Statistics\n";
    cout << "======================================================\n";
    cout << "Accesses                   : " << _stat.access_num << "\n";
    cout << "Correct SID Predictions    : " << _stat.correct_sid_prediction << "\n";
    cout << "Incorrect SID Predictions  : " << _stat.incorrect_sid_prediction << "\n";
    cout << "Predictions Made           : " << _stat.made_prediction << "\n";
    cout << "New SID mappings           : " << _stat.new_sid_mapping << "\n";
    cout << "Trans to the same SID      : " << _stat.trans_to_same_sid << "\n";
    cout << "Trans to diff SID          : " << _stat.trans_to_diff_sid << "\n";
    cout << "Prefetch distance          : " << _global_sid_hist_len << "\n";
    cout << "Addr history length        : " << _lastaddr_num << "\n";
    for (auto it = _sid_history_map.begin(); it != _sid_history_map.end(); it++) {
        cout << "SID 0x" << hex << it->first << dec <<  " History Stat      : h " << it->second.hit_cnt << " m " << it->second.miss_cnt << "\n";
    }
    cout << "======================================================\n";
    cout << "Prefetcher Buffer Statistics\n";
    cout << "======================================================\n";
    cout << "Buffer size                : " << _buffer_size << "\n";
    cout << "Fill the same entry        : " << _stat.fill_the_same << "\n";
    cout << "Buffer fill                : " << _stat.buffer_fill << "\n";
    cout << "Buffer hit                 : " << _stat.buffer_hit << "\n";
    cout << "Buffer miss                : " << _stat.buffer_miss << "\n";
}