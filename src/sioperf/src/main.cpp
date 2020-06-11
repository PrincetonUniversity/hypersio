#include <iostream>
#include <regex>
#include <string>
#include <fstream>
#include <string>
#include <thread>
#include "optparser.h"
#include "trace.h"
#include "event.h"
#include "defines.h"
#include "common.h"
// #include "row.h"
#include "cache.h"
#include "sioperf.h"
#include "sioperf.tcc"
#include "dbg.h"

using namespace std;

void setupTrace(Trace *trace)
{
    TranslateEventParser *transl_event_pars = new TranslateEventParser;
    CCEventParser *cc_event_pars = new CCEventParser;
    SlpteEventParser *slpte_even_pars = new SlpteEventParser;

    // transl_event_pars.AddMatch("(vtd_iotlb_page_\\w+).*sid (0x[a-fA-F0-9]) iova (0x[a-fA-F0-9]+) slpte (0x[a-fA-F0-9]+) level (0x[a-fA-F0-9]+) domain (0x[a-fA-F0-9]+))");
    transl_event_pars->AddMatch(".*(vtd_iotlb_page_update).*\
sid\\s+(0x[[:xdigit:]]+)\\s+\
iova\\s+(0x[[:xdigit:]]+)\\s+\
slpte\\s+(0x[[:xdigit:]]+)\\s+\
level\\s+(0x[[:xdigit:]]+)\\s+\
domain\\s+(0x[[:xdigit:]]+)\
.*");
    transl_event_pars->AddMatch(".*(vtd_iotlb_page_hit).*\
sid\\s+(0x[[:xdigit:]]+)\\s+\
iova\\s+(0x[[:xdigit:]]+)\\s+\
pmask\\s+(0x[[:xdigit:]]+)\\s+\
slpte\\s+(0x[[:xdigit:]]+)\\s+\
domain\\s+(0x[[:xdigit:]]+)\
.*");


    // Context Cache Event Matches
    cc_event_pars->AddMatch(".*(vtd_iotlb_cc_update).*\
bus\\s+(0x[[:xdigit:]]+)\\s+\
devfn\\s+(0x[[:xdigit:]]+)\\s+\
high\\s+(0x[[:xdigit:]]+)\\s+\
low\\s+(0x[[:xdigit:]]+)\\s+\
gen\\s+([[:xdigit:]]+)\
.*");

    // SLPTE Fetch Event Matches
    slpte_even_pars->AddMatch(".*(vtd_slpte_fetch).*\
high\\s+(0x[[:xdigit:]]+)\\s+\
low\\s+(0x[[:xdigit:]]+)\\s+\
iova\\s+(0x[[:xdigit:]]+)\\s+\
level\\s+(0x[[:xdigit:]]+)\\s+\
addr\\s+(0x[[:xdigit:]]+)\\s+\
slpte\\s+(0x[[:xdigit:]]+)\
.*");

    trace->AddEventParser(transl_event_pars);
    trace->AddEventParser(cc_event_pars);
    trace->AddEventParser(slpte_even_pars);
}

void printHelp() {
    cout << "Usage:\n";
    cout << "--build OR --run >\n";
    cout << "   --build [--tenantnum <uint>] [--lbase <string>] [--ltype <string>] \n";
    cout << "       --tenantnum <uint>   - number of tenants\n";
    cout << "       --maxdevperlog <uint> - maximum number of devices per log file\n";
    cout << "       --lbase <string>    - base name for logs (excluding \"_X.log\") at the end\n";
    cout << "       --ltype <string>    - log type. Possible options:\n";
    cout << "                               txt, bin\n";
    cout << "       --order  <rr|orig|rand>  - order of IOVA tranlation requests in a trace\n";
    cout << "                             rr - Round Robin, N consec / device\n";
    cout << "                             orig - Order from the log, without any reodering\n";
    cout << "       --consec <int>      - number of consecutive events from a single device\n";
    cout << "       --tname <string>    - name of a test which was used to generate Qemu traces\n";
    cout << "   --run --size <int> --assoc <int> --epg <int>\n";
    cout << "       --config <string>   - path to cache configuration file";
    cout << "       --prefix <string>   - prefix for traces\n\n";
    cout << "       --tenantnum <int>   - number of tenants\n";
}


int main(int argc, char *argv[]) {
    // Initialize parameters to default values
    string log_base;
    size_t consec_event = CONSEC_EVENTS_PER_DID;
    log_t log_type = DEFAULT_LOG_TYPE;
    string log_ext = DEFAULT_LOG_EXT;
    size_t tqueue_size = DEFAULT_TRANS_IN_FLIGHT_MAX;
    size_t dev_per_log_max = DEFAULT_DEV_PER_LOG_MAX;
    
    Trace *trace = nullptr;
    vector<did_t> valid_did_v;
    vector<sid_t> valid_sid_v;
    string log_name;
    size_t tenant_num = DEFAULT_TENANT_NUM;
    string cfg_path = CACHE_CONFIG_NAME;
    string trace_prefix = DEFAULT_TRACE_PREFIX;
    OptParser *optparser = new OptParser();
    string *opt_val = new string();
    uint64_t link_bw_gbps = LINK_BANDWIDTH_GBPS;
    string iova_trace_order = DEFAULT_IOVATRACE_ORDER;
    bool selective_alloc = DEFAULT_SELECTIVE_ALLOC;
    bool tqueue_ooo = DEFAULT_TQUEUE_OOO;
    string tname;
    string iotlbrepl;

    vector<thread> parse_log_threads;
    vector<string> log_names;
    vector<int> log_ids;


    bool parse_log_succ = true;

    // Try to parse command line options
    if (!optparser->Parse(argc, argv)) {
        goto exit_on_error;
    }

    // Build a trace out of logs
    if (optparser->GetOpt("build", opt_val)) {

        optparser->GetOpt("tenantnum", opt_val);
        tenant_num = stoul(*opt_val);
        LOG_INFO("Target number of tenants: %lu\n", tenant_num);
        
        optparser->GetOpt("maxdevperlog", opt_val);
        dev_per_log_max = stoul(*opt_val);
        LOG_INFO("Maximum number of devices per log: %lu\n", dev_per_log_max);

        optparser->GetOpt("lbase", &log_base);
        LOG_INFO("Setting log_base to: %s\n", log_base.c_str());

        optparser->GetOpt("tname", &tname);
        LOG_INFO("Test name: %s\n", tname.c_str());

        if (optparser->GetOpt("order", opt_val)) {
            iova_trace_order = *opt_val;
        }
        LOG_INFO("Using '%s' order for IOVA translation events\n", iova_trace_order.c_str());

        if (optparser->GetOpt("consec", opt_val)) {
            consec_event = stoul(*opt_val);
        }
        LOG_INFO("Setting consecutive number of events per device to: %lu\n", consec_event);

        // Read type of the log for future parsing
        if (optparser->GetOpt("ltype", &log_ext)) {
            if (log_ext.compare("txt") == 0) {
                log_type = TEXT_LOG;
            } else if (log_ext.compare("bin") == 0) {
                log_type = BINARY_LOG;
            } else {
                LOG_WARN("Unknown log type! Using default one\n");
            }
        }

        LOG_INFO("Building a trace\n");
        // Build device list for parsing
        valid_sid_v = sidVFromDevV(valid_did_v);
        trace = new Trace(consec_event, valid_sid_v);
        setupTrace(trace);
        
        if (!(trace->ReadLogs(log_base, tenant_num, dev_per_log_max, log_type))) {
            parse_log_succ = false;
        }

        // Write Trace to files
        if (trace->SaveTrace(iova_trace_order, tname) == -1) {
            goto exit_on_error;
        }
    } else if (optparser->GetOpt("run", opt_val)) {
        if (optparser->GetOpt("config", opt_val)) {
            cfg_path = *opt_val;
        }

        if (optparser->GetOpt("prefix", opt_val)) {
            trace_prefix = *opt_val;
        }

        if (optparser->GetOpt("tqueue", opt_val)) {
            tqueue_size = stoul(*opt_val);
        }

        if (optparser->GetOpt("inorder", opt_val)) {
           tqueue_ooo = false; 
        }
        LOG_INFO("Setting Translation Queue size to %lu entries\n", tqueue_size);

        if (optparser->GetOpt("salloc", opt_val)) {
            selective_alloc = true;
        }
        if (selective_alloc) {
            LOG_INFO("Selective allocation based on DID\n");
        } else {
            LOG_INFO("Translation Allocation in all caches\n");
        }

        optparser->GetOpt("tenantnum", opt_val);
        tenant_num = stoul(*opt_val);
        LOG_INFO("Expected number of tenants: %lu\n", tenant_num);

        if (optparser->GetOpt("iotlbrepl", opt_val)) {
            iotlbrepl = *opt_val;
        }
        LOG_INFO("IOTLB Replacement Policy: %s\n", iotlbrepl.c_str());

        if (optparser->GetOpt("linkbwgbps", opt_val)) {
            link_bw_gbps = stoul(*opt_val);
        }
        LOG_INFO("Link Bandwidth: %lu Gb/s\n", link_bw_gbps);


        int rv;

        if (iotlbrepl == "lru") {
            SIOPerf<CacheRowLRU> *sioperf = nullptr;
            sioperf = new SIOPerf<CacheRowLRU>(cfg_path, trace_prefix, tqueue_size, tqueue_ooo);
            sioperf->ConfigSelectiveAlloc(selective_alloc, tenant_num);
            sioperf->SetLinkSpeed(link_bw_gbps);
            sioperf->PrintConfig();
            rv = sioperf->PlayIOVATrace();
            if (rv == 0) {
                sioperf->PrintStat();
            }
        } else if (iotlbrepl == "lfu") {
            SIOPerf<CacheRowLFU> *sioperf = nullptr;
            sioperf = new SIOPerf<CacheRowLFU>(cfg_path, trace_prefix, tqueue_size, tqueue_ooo);
            sioperf->ConfigSelectiveAlloc(selective_alloc, tenant_num);
            sioperf->SetLinkSpeed(link_bw_gbps);
            sioperf->PrintConfig();
            rv = sioperf->PlayIOVATrace();
            if (rv == 0) {
                sioperf->PrintStat();
            }
        } else if (iotlbrepl == "oracle") {
            SIOPerf<CacheRowNAT> *sioperf = nullptr;
            sioperf = new SIOPerf<CacheRowNAT>(cfg_path, trace_prefix, tqueue_size, tqueue_ooo);
            sioperf->ConfigSelectiveAlloc(selective_alloc, tenant_num);
            sioperf->SetLinkSpeed(link_bw_gbps);
            sioperf->PrintConfig();
            rv = sioperf->PlayIOVATrace();
            if (rv == 0) {
                sioperf->PrintStat();
            }
        }

    } else {
        LOG_ERR("Incorrect options!\n");
        printHelp();
    }

exit_on_error:
    if (trace) {
        delete trace;
    }
    delete optparser;
    delete opt_val;

    if (!parse_log_succ) {
        return 1;
    } else {
        return 0;
    }
}
