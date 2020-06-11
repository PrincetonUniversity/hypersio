#ifndef __CONFIG_H__
#define __CONFIG_H__

#define     PAGE_LEVEL_SHIFT    9
#define     MAX_PAGE_LEVEL      2
#define     PAGE_4K_SHIFT       12

#define     LEVEL_SHIFT(l)      (((l)-1)*PAGE_LEVEL_SHIFT+PAGE_4K_SHIFT)
#define     MAX_PAGE_LEVEL      2

#define     MASK_FROM_BIT_WIDTH(bw) ((1<<bw)-1)

#define     SLPTE_SIZE          8

// Note: assuming only 4KB and 2MB pages

#define     CACHE_CONTEXT_IND_SHIFT 0

#define     CACHE_ADDR_SHIFT        16
// Infrastructure defines
#define     MAX_DOMAINS_PER_LOG     128

#define     CONSEC_EVENTS_PER_DID   TRANSLATIONS_PER_PKT

#define     DEFAULT_TRANS_IN_FLIGHT_MAX     1
#define     DEFAULT_SELECTIVE_ALLOC         false
#define     DEFAULT_TQUEUE_OOO              true

#define     DEFAULT_PARALLEL_BUILD          false

#define     ALL_DID_END_SAME_TIME           true


enum log_t {
    TEXT_LOG,
    BINARY_LOG
};

// folders relative to HYPERSIO_HOME
const std::string LOG_FOLDER ("/build/qemu_logs/");
const std::string BUILD_FOLDER ("/build/hypersio_traces/");
const std::string DEFAULT_TRACE_PREFIX ("");
const std::string IOVA_TRACE_NAME ("iotlb_trace.bin");
const std::string CC_DATA_NAME ("cc_data.bin");
const std::string SLPT_DATA_NAME ("slpt_data.bin");
const std::string CACHE_CONFIG_NAME ("cache.cfg");
const std::string DEFAULT_IOVATRACE_ORDER ("rr");
const std::string DEFAULT_LOG_EXT ("txt");
const long LOG_NUM = 1;
const log_t DEFAULT_LOG_TYPE = BINARY_LOG;
const size_t DEFAULT_TENANT_NUM = 0;
const size_t DEFAULT_DEV_PER_LOG_MAX = 0;



const std::string IOTLB_CACHE_NAME      ("IOTLB");
const std::string CONTEXT_CACHE_NAME    ("CC");
const std::string L3PT_CACHE_NAME       ("L3PAGE");
const std::string L2PT_CACHE_NAME       ("L2PAGE");
const std::string L1PT_CACHE_NAME       ("L1PAGE");
const std::string PREFETCH_CACHE_NAME   ("PREFETCH");

#endif