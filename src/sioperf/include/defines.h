#ifndef __IOTLBPERF_H__
#define __IOTLBPERF_H__

#include <string>
#include "config.h"


typedef uint64_t hwaddr;
typedef uint16_t did_t;
typedef uint16_t sid_t;
typedef uint16_t fld_t; // field type
typedef unsigned long int lvl_t;
typedef uint64_t timecnt_t;

// Latency Parameters in [ns]
#define SIM_LAT_PCIE            450
#define SIM_LAT_PCIE_RTT        (2*SIM_LAT_PCIE)
// from Translation Agent (TA)
#define SIM_LAT_DDR             50
#define SIM_LAT_MEM_RTT         (SIM_LAT_PCIE_RTT+SIM_LAT_DDR)
#define SIM_LAT_IOTLB_HIT       2
#define SIM_LAT_PREFETCH_HIT    2
#define SIM_LAT_CC_HIT          1
#define SIM_LAT_PTW_NUM         24
#define SIM_LAT_TRANLATION      (SIM_LAT_PCIE_RTT+SIM_LAT_PTW_NUM*SIM_LAT_DDR)
#define SIM_LAT_PCACHE_HIT      5

#define MEM_ACC_PER_CE          2
#define MEM_LAT_CE_READ         (MEM_ACC_PER_CE*SIM_LAT_DDR)

// Size in Bytes
#define DEFAULT_PKT_SIZE_BYTES  1542

#define LINK_BANDWIDTH_GBPS     200

#define PKT_INTERARRIVAL_TIME_NS    ((DEFAULT_PKT_SIZE_BYTES*8)/(LINK_BANDWIDTH_GBPS))

#define TRANSLATIONS_PER_PKT    3

#define RANDOM_SEED         1

// Log Parameters
#define FIRST_DEV_ID            2

struct AddrDidKey {
    hwaddr      addr;
    did_t       did;
    uint64_t    nat;

    bool operator==(const AddrDidKey& other) const
    {
        return (addr == other.addr
                && did == other.did);
    }

    bool operator<(const AddrDidKey& other) const
    {
        if (did != other.did) {
            return did < other.did;
        } else {
            return addr < other.addr;
        }
    }

    AddrDidKey& operator=(AddrDidKey const& other)
    {
        addr = other.addr;
        did = other.did;
        nat = other.nat;
        return *this;
    }
};

namespace std {

    template <>
    struct hash<AddrDidKey>
    {
        std::size_t operator()(const AddrDidKey& k) const
        {
            return (k.addr << CACHE_ADDR_SHIFT) | k.did;
        }
    };
}

#endif