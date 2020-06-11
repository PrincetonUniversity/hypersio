#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>
#include <cstdlib>
#include "config.h"
#include "trace.h"
#include "eventparser.h"
#include "event.h"
#include "dbg.h"
#include "common.h"

using namespace std;

Trace::Trace(size_t consec_events, std::vector<sid_t> sid_v) {
    _did_tr_event_map = new unordered_map<did_t, vector<Event *>* >;
    _sid_context_map = new unordered_map<sid_t, CCEvent *>;
    _paddr_slpt_map = new unordered_map<hwaddr, SlpteEvent *>;
    _did_log_trace_v = new vector<did_t>;
    _constructed_iova_event_v = new vector<Event *>;
    _consec_events_per_did = consec_events;
    _valid_sid_v = sid_v;

    string hypersio_home = string(getHyperSIOHome());
    _iova_trace_path    = hypersio_home + BUILD_FOLDER + IOVA_TRACE_NAME;
    _cc_data_path       = hypersio_home + BUILD_FOLDER + CC_DATA_NAME;
    _slpt_data_path     = hypersio_home + BUILD_FOLDER + SLPT_DATA_NAME;

    _global_did_cnt = 0;
}


Trace::~Trace()
{
    vector<EventParser *>::iterator pars_it;
    unordered_map<did_t, vector<Event *>* >::iterator map_it;
    vector<Event *> *event_q_ptr;

    // delete all parsers
    for (pars_it = _event_pars.begin(); pars_it != _event_pars.end(); pars_it ++) {
        delete (*pars_it);
    }

    //----------------------------------------------------------
    // Freeing map of translation events
    // for each did remove all translation events
    for (map_it = _did_tr_event_map->begin(); map_it != _did_tr_event_map->end(); map_it++) {
        event_q_ptr = map_it->second;
        while (!event_q_ptr->empty()) {
            delete event_q_ptr->back();
            event_q_ptr->pop_back();
        }
        delete event_q_ptr;
    }
    // delete has with dids
    delete _did_tr_event_map;

    //----------------------------------------------------------
    // Freeing map of contexts
    for (auto it = _sid_context_map->begin(); it != _sid_context_map->end(); it++) {
        delete it->second;
    }
    delete _sid_context_map;

    //-----------------------------------------------------------
    // Freeing map of slpte
    for (auto it = _paddr_slpt_map->begin(); it != _paddr_slpt_map->end(); it++) {
        delete it->second;
    }
    delete _paddr_slpt_map;

    delete _did_log_trace_v;
    delete _constructed_iova_event_v;
}


void Trace::SetIOVATracePath(string path)
{
    _iova_trace_path = path;
}

int Trace::ParseLog(const char *fname, uint logid, log_t ltype)
{
    // Because of the same device is mapped to different domains
    // in different traces, we have to reset valid did sets for all parsers
    // before parsing a log
    for (auto it = _event_pars.begin(); it != _event_pars.end(); it++) {
        (*it)->ResetValidDid();
    }

    if (ltype == TEXT_LOG) {
        return ParseTextLog(fname, logid);
    } else {
        LOG_ERR("Uknown log_type for the trace!\n");
        return false;
    }
}

bool Trace::ReadLogs(string log_base, size_t tntnum, size_t maxDevPerlog, log_t ltype)
{
    uint log_cnt = 0;
    uint dev_num_in_log;
    int read_did_num;
    vector<did_t> valid_did_v;
    string log_name;

    while (tntnum > 0) {
        // Number of valid devices in a current log
        if (tntnum > maxDevPerlog) {
            dev_num_in_log = maxDevPerlog;
        } else {
            dev_num_in_log = tntnum;
        }

        // Set valid SIDs based of number of valid devices
        valid_did_v.clear();
        for (size_t i = 0; i < dev_num_in_log; i++) {
            valid_did_v.push_back(FIRST_DEV_ID + i);
        }
        SetValidSid(sidVFromDevV(valid_did_v));
        LOG_INFO("Setting %u valid devices in log #%d\n", dev_num_in_log, log_cnt);

        // Read a log
        string log_folder_path;
        char *hypersio_home = getHyperSIOHome();
        if (hypersio_home == NULL) {
            return false;
        } else {
            log_folder_path = string(hypersio_home) + LOG_FOLDER;
        }
        log_name = log_folder_path + log_base + "_" + to_string(log_cnt) + ".txt";
        read_did_num = ParseLog(log_name.c_str(), log_cnt, ltype);
        if (read_did_num == -1) {
            LOG_ERR("Failed to read log %s\n", log_name.c_str());
            return false;
        }
        if ((uint)read_did_num != maxDevPerlog) {
            LOG_WARN("Encountered %d devices while expecting maximum %lu devices in log %s\n",
                     read_did_num, maxDevPerlog, log_name.c_str());
        }
        tntnum -= read_did_num;
        log_cnt++;
    }


    return true;
}

int Trace::ParseTextLog(const char *fname, uint logid)
{
    ifstream fin(fname);
    string line;
    uint event_cnt;
    uint ignored_cnt;
    Event *event;
    unordered_set<did_t> seen_did_set;

    if (!fin.is_open()) {
        LOG_ERR("Can't open file %s\n", fname);
        return -1;
    }

    event_cnt = 0;
    ignored_cnt = 0;
    LOG_INFO("Parsing file: %s\n", fname);
    while (getline(fin, line)) {
        event = ParseLine(&line);
        if (event) {
            PushEvent(event, logid);
            event_cnt++;
            did_t curr_did = event->GetDID();
            if (seen_did_set.find(curr_did) == seen_did_set.end()) {
                seen_did_set.insert(curr_did);
            }
        } else {
            ignored_cnt++;
        }
    }
    size_t seen_did_num = seen_did_set.size();
    LOG_INFO("Parsed %u events from %lu devices\n", event_cnt, seen_did_num);
    LOG_INFO("Ignored %d events\n", ignored_cnt);

    return seen_did_num;
}

bool Trace::ParseBinaryLog(const char *fname, uint logid)
{
    event_t event_type;
    did_t did;
    sid_t sid;
    hwaddr iova, mask, slpte;
    lvl_t lvl;
    Event *event;
    uint event_cnt;
    uint ignored_cnt;

    ifstream flog(fname, ios::binary);
    if (!flog) {
        LOG_ERR("Can't open log: %s\n", fname);
        return false;
    }

    flog.seekg(0, ios::beg);
    event_cnt = 0;
    ignored_cnt = 0;
    LOG_INFO("Parsing file: %s\n", fname);
    // read until read method sets eof flag
    while (true) {
        // Make sure we are reading Translate Event
        flog.read(reinterpret_cast<char *>(&event_type), sizeof(event_t));
        if (flog.eof()) {
            break;
        }
        if (event_type != TRANSLATE_EVENT) {
            LOG_ERR("Expected Translate Event type!\n");
            continue;
        }
        // Read all fields
        // Format must match with Qemu!
        flog.read(reinterpret_cast<char *>(&sid), sizeof(sid));
        flog.read(reinterpret_cast<char *>(&did), sizeof(did));
        flog.read(reinterpret_cast<char *>(&iova), sizeof(iova));
        flog.read(reinterpret_cast<char *>(&mask), sizeof(mask));
        flog.read(reinterpret_cast<char *>(&lvl), sizeof(lvl));
        flog.read(reinterpret_cast<char *>(&slpte), sizeof(slpte));
        
        // Check if translation was from a device we care about
        if (!binary_search(_valid_sid_v.begin(), _valid_sid_v.end(), sid)) {
            ignored_cnt++;
            continue;
        } else {
            event = new TranslateEvent(TRANSLATE_EVENT, sid, iova, did, slpte, mask, lvl);
            PushEvent(event, logid);
            event_cnt++;
        }
    }

    LOG_INFO("Saved %d events\n", event_cnt);
    LOG_INFO("Ignored %d events\n", ignored_cnt);

    return true;
}

void Trace::AddEventParser(EventParser *parser)
{
    parser->SetValidSid(_valid_sid_v);
    _event_pars.push_back(parser);
}

void Trace::SetValidSid(vector<sid_t> sid_v)
{
    for (auto it = _event_pars.begin(); it != _event_pars.end(); it++) {
        (*it)->SetValidSid(sid_v);
    }
    _valid_sid_v = sid_v;
}

Event *Trace::ParseLine(string *line)
{
    vector<EventParser *>::iterator it;
    Event *event;

    for (it = _event_pars.begin(); it != _event_pars.end(); it++) {
        event = (*it)->ParseLine(line);
        if (event) {
            return event;
        }
    }

    return nullptr;
}


void Trace::PushEvent(Event *event, uint logid)
{
    unordered_map<did_t, vector<Event *>* >::const_iterator fres;
    unordered_map<sid_t, CCEvent *>::const_iterator fres_sid;
    unordered_map<hwaddr, SlpteEvent *>::const_iterator fres_slpte;
    unordered_map<did_t, did_t>::const_iterator fres_did;
    CCEvent *ccevent_ptr;
    SlpteEvent *slptevent_ptr;
    did_t did, unique_did;
    sid_t sid;
    hwaddr slpte_addr;   
   
    // Setting DID in global order for better partitioning
    unique_did = event->GetDID() + logid*MAX_DOMAINS_PER_LOG;
    fres_did = _unique_did_order_map.find(unique_did);
    if (fres_did == _unique_did_order_map.end()) {
        _unique_did_order_map[unique_did] = _global_did_cnt;
        _global_did_cnt++;
        LOG_INFO("Creating mapping UID: %u GID: %u\n", unique_did, _unique_did_order_map[unique_did]);
    }
    
    event->AdjustDid(_unique_did_order_map[unique_did]);

    event->AdjustSid(logid);
    switch(event->GetType()) {
        case TRANSLATE_EVENT:
            
            did = event->GetDID();
            fres = _did_tr_event_map->find(did);
            // the first translation event for this did
            if (fres == _did_tr_event_map->end()) {
                (*_did_tr_event_map)[did] = new vector<Event *>;
            }         
            (*_did_tr_event_map)[did]->push_back(event);
            _did_log_trace_v->push_back(did);
            break;
        case CC_EVENT:
            ccevent_ptr = static_cast<CCEvent *>(event);
            sid = ccevent_ptr->GetSID();
            fres_sid = _sid_context_map->find(sid);
            if (fres_sid != _sid_context_map->end()) {
                if ((fres_sid->second->GetHigh() != ccevent_ptr->GetHigh()) ||
                    (fres_sid->second->GetLow() != ccevent_ptr->GetLow())) {
                    LOG_WARN("Context entry for SID 0x%x is changed in the trace!\n", sid);
                }
            }
            (*_sid_context_map)[sid] = ccevent_ptr;
            break;
        case SLPTE_EVENT:
            slptevent_ptr = static_cast<SlpteEvent *>(event);
            slpte_addr = slptevent_ptr->GetSlpteAddr();
            fres_slpte = _paddr_slpt_map->find(slpte_addr);
            if (fres_slpte != _paddr_slpt_map->end()) {
                // since offset is fixed in width and less than page size,
                // need to check only the base
                if ((fres_slpte->second->GetBaseAddr() != slptevent_ptr->GetBaseAddr()) ||
                    (fres_slpte->second->GetDID() != slptevent_ptr->GetDID()) ||
                    (fres_slpte->second->GetLvl() != slptevent_ptr->GetLvl())) {
                    LOG_WARN("New SLPTE conflicts with an existing one!\n");
                }
            }
            (*_paddr_slpt_map)[slpte_addr] = slptevent_ptr;
            break;
        default:
            LOG_ERR("Trying to add an unknown event to a trace!\n");
            break;
    }
}

void Trace::SetNextIOVAAccessTime(void)
{
    // Get distance for future use
    uint64_t event_cnt = 0;
    map<AddrDidKey, TranslateEvent *> key_event_map;
    map<AddrDidKey, TranslateEvent *>::iterator map_it;
    AddrDidKey key;
    TranslateEvent *te_ptr;
    for (auto it = _constructed_iova_event_v->begin(); it != _constructed_iova_event_v->end(); it++) {
        te_ptr = static_cast<TranslateEvent *>(*it);
        key.addr = te_ptr->GetIOVA() & te_ptr->GetMask();
        key.did = te_ptr->GetDID();
        map_it = key_event_map.find(key);
        if (map_it != key_event_map.end()) {
            map_it->second->SetNextAccessTime(event_cnt);
        }
        key_event_map[key] = te_ptr;
        event_cnt++;
    }
}

int Trace::SaveIOVATrace(string order) {
    int trans_cnt = 0;

    LOG_INFO("Saving IOVA trace of %lu domains to a file %s\n", _did_tr_event_map->size(), _iova_trace_path.c_str());
    if (order.compare("orig") == 0) {
        trans_cnt = BuildIOVATraceOrig();
    } else if (order.compare("rr") == 0) {
        trans_cnt = BuildIOVATraceRR();
    } else if (order.compare("rand") == 0) {
        trans_cnt = BuildIOVATraceRand();
    } else {
        LOG_ERR("'%s' translation event order is unknown!\n", order.c_str());
        return -1;
    }

    SetNextIOVAAccessTime();

    // Save Events to a file
    ofstream iova_trace_stream(_iova_trace_path, ios::binary);
    if (!iova_trace_stream.is_open()) {
        LOG_ERR("Failed to open stream file: %s", _iova_trace_path.c_str());
        return -1;
    }
    for (auto it = _constructed_iova_event_v->begin(); it != _constructed_iova_event_v->end(); it++) {
        (*it)->WriteToFile(iova_trace_stream);        
    }

    LOG_INFO("Saved %d IOVA translation events\n", trans_cnt);
    return trans_cnt;
}

size_t Trace::BuildIOVATraceOrig() {
    did_t did;
    map<did_t, vector<Event *>::iterator> did_it;
    size_t event_cnt = 0;

    // make a map of did:iterators
    for (auto kv = _did_tr_event_map->begin(); kv != _did_tr_event_map->end(); kv++) {
        did = kv->first;
        did_it[did] = (*_did_tr_event_map)[did]->begin();
    }

    // Since Events are stored in original order for each did,
    // using original did trace writes all events in original order
    for (auto it = _did_log_trace_v->begin(); it != _did_log_trace_v->end(); it++) {
        did = *it;
        _constructed_iova_event_v->push_back(*did_it[did]);
        event_cnt++;
        did_it[did]++;
    }

    return event_cnt;
}

size_t Trace::BuildIOVATraceRR()
{
    vector<did_t> did_v;
    vector<did_t> empty_did_v;
    map<did_t, vector<Event *>::iterator> did_it;
    map<did_t, vector<Event *>::iterator> did_it_end;
    did_t did;
    size_t event_cnt = 0;

    // make a vector of available dids'
    // map of did:event_iterator
    for (auto kv = _did_tr_event_map->begin(); kv != _did_tr_event_map->end(); kv++) {
        did = kv->first;
        did_v.push_back(did);
        did_it[did] = (*_did_tr_event_map)[did]->begin();
        did_it_end[did] = (*_did_tr_event_map)[did]->end();
    }
    // sort dids to make it consistent between runs
    sort(did_v.begin(), did_v.end());
    
    bool one_did_is_over = false;
    // write until there are did with available events
    while (did_v.size() > 0) {
        // write events for available did in RR manner
        DEBUG("DID_V size: %lu\n", did_v.size());
        empty_did_v.clear();
        auto did_v_it = did_v.begin();
        while (did_v_it != did_v.end()) {
            did = *did_v_it;
            for (size_t e_cnt = 0; e_cnt < _consec_events_per_did; e_cnt++) {
                if (did_it[did] == did_it_end[did]) {
                    // all events for did are read
                    // add did to a vector of to be removed dids
                    empty_did_v.push_back(did);
                    DEBUG("Removing %d\n", did);
                    one_did_is_over = true;
                    // got to the next did
                    break;
                } else {
                    DEBUG("%lu: Writing an event for domain %d\n", event_cnt, did);
                    _constructed_iova_event_v->push_back(*did_it[did]);
                    event_cnt++;
                    // Move one event forward
                    did_it[did]++;
                }
            }
            // go to the next did in a vector
            did_v_it++;
        }
        if (ALL_DID_END_SAME_TIME && one_did_is_over) {
            LOG_INFO("Ending trace generation after first did stream is over\n");
            break;
        }
        // Erase dids which ran out of events
        for (auto it = empty_did_v.begin(); it != empty_did_v.end(); it++) {
            did_v.erase(remove(did_v.begin(), did_v.end(), *it), did_v.end());
        }
    }

    return event_cnt;
}

size_t Trace::BuildIOVATraceRand()
{
    did_t did;
    vector<did_t> did_v;
    map<did_t, vector<Event *>::iterator> did_it;
    map<did_t, vector<Event *>::iterator> did_it_end;
    size_t event_cnt = 0;
    int rand_ind;

    // Make a vector of available DIDs
    // Map did:current_event and
    // Map did:last_event
    for (auto kv = _did_tr_event_map->begin(); kv != _did_tr_event_map->end(); kv++) {
        did = kv->first;
        did_v.push_back(did);
        did_it[did] = (*_did_tr_event_map)[did]->begin();
        did_it_end[did] = (*_did_tr_event_map)[did]->end();
    }
    // sort dids to make it consistent between runs
    sort(did_v.begin(), did_v.end());

    srand(RANDOM_SEED);
    // write until there are did with available events
    while (did_v.size() > 0) {
        rand_ind = rand() % did_v.size();
        did = did_v[rand_ind];
        // for selected DID write certain number of events to trace
        for (size_t e_cnt = 0; e_cnt < _consec_events_per_did; e_cnt++) {
            if (did_it[did] == did_it_end[did]) {
                did_v.erase(remove(did_v.begin(), did_v.end(), did), did_v.end());
                break;
            } else {
                _constructed_iova_event_v->push_back(*did_it[did]);
                event_cnt++;
                // Move one event forward for selected did
                did_it[did]++;
            }
        }
    }

    return event_cnt;
}


void Trace::SaveCCData()
{
    ofstream cc_data_stream(_cc_data_path, ios::binary);
    size_t sid_cnt = 0;

    for (auto kv = _sid_context_map->begin(); kv != _sid_context_map->end(); kv++) {
        kv->second->WriteToFile(cc_data_stream);
        sid_cnt++;
    }

    LOG_INFO("Saved CC data for %lu SIDs to file: %s\n", sid_cnt, _cc_data_path.c_str());
}

void Trace::SaveSLPTData()
{
    ofstream slpt_data_stream(_slpt_data_path, ios::binary);
    size_t cnt = 0;

    for (auto kv = _paddr_slpt_map->begin(); kv != _paddr_slpt_map->end(); kv++) {
        kv->second->WriteToFile(slpt_data_stream);
        cnt++;
    }

    LOG_INFO("Saved %lu SLPT entries for different levels to: %s\n", cnt, _slpt_data_path.c_str());
}

void Trace::UpdateTracePaths(string order, string test_name)
{
    size_t used_did_num = _did_tr_event_map->size();
    string order_suff;
    string prefix;

    order_suff = order + to_string(_consec_events_per_did) + "_";
    
    prefix = test_name + "_tnt_" + to_string(used_did_num) + "_" + order_suff;

    string hypersio_home = string(getHyperSIOHome());
    _iova_trace_path = hypersio_home + BUILD_FOLDER + prefix + IOVA_TRACE_NAME;
    _cc_data_path = hypersio_home + BUILD_FOLDER + prefix + CC_DATA_NAME;
    _slpt_data_path = hypersio_home + BUILD_FOLDER + prefix + SLPT_DATA_NAME; 
}

int Trace::SaveTrace(string order, string test_name)
{
    UpdateTracePaths(order, test_name);
    if (SaveIOVATrace(order) == -1) {
        return -1;
    };
    SaveCCData();
    SaveSLPTData();

    return 0;
}