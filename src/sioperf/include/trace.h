#ifndef __TRACE_H__
#define __TRACE_H__

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <string>
#include <fstream>
#include "defines.h"
#include "eventparser.h"
#include "event.h"
#include "cache.h"

class Trace
{
public:
    Trace(size_t , std::vector<sid_t>);
    ~Trace();
    int  ParseLog(const char *fname, uint logid, log_t);
    bool ReadLogs(std::string log_base, size_t tntnum, size_t maxDevPerlog, log_t ltype);
    void AddEventParser(EventParser *parser);
    void PushEvent(Event *event, uint logid);
    int  SaveTrace(std::string order, std::string tname);
    void SetIOVATracePath(std::string);
    void SetValidSid(std::vector<sid_t> sid_v);
private:
    size_t _consec_events_per_did;
    std::vector<sid_t> _valid_sid_v;
    // Trace paths
    std::string _iova_trace_path;
    std::string _cc_data_path;
    std::string _slpt_data_path;
    // Trace has three data structures constructed while reading a log
    // 1) did : transl_event map
    // 2) Context Cache translations
    // 3) SLPT translations
    std::unordered_map<did_t, std::vector<Event *>* > *_did_tr_event_map;
    std::unordered_map<sid_t, CCEvent *> *_sid_context_map;
    std::unordered_map<hwaddr, SlpteEvent *> *_paddr_slpt_map;
    std::unordered_map<did_t, did_t> _unique_did_order_map;
    did_t _global_did_cnt;
    // Trace of did accesses for saving events in their original order
    std::vector<did_t > *_did_log_trace_v;
    // Constructed vector of events
    std::vector<Event *> *_constructed_iova_event_v;
    // Vector of parsers
    std::vector<EventParser *> _event_pars;
    int ParseTextLog(const char *, uint);
    bool ParseBinaryLog(const char *, uint);
    Event *ParseLine(std::string *line);
    void UpdateTracePaths(std::string order, std::string tname);
    int  SaveIOVATrace(std::string order);
    size_t BuildIOVATraceRR();
    size_t BuildIOVATraceRand();
    size_t BuildIOVATraceOrig();
    void SaveCCData();
    void SaveSLPTData();
    void SetNextIOVAAccessTime();
};

#endif