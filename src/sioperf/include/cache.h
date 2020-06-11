#ifndef __CACHE_H__
#define __CACHE_H__

#include <fstream>
#include <unordered_set>
#include <string>
#include "defines.h"
#include "event.h"
#include "common.h"
#include "row.h"
#include "row.tcc"

struct CacheConfig {
    size_t size;
    size_t assoc;
    size_t groups;
    size_t histlen;
};

enum CacheLevel {
    CACHE_L1 = 1,
    CACHE_L2,
    CACHE_L3,
    CACHE_CONTEXT
};

struct CacheEntry {
    hwaddr      slpte;
    uint64_t    next_acc_time;
};

template <template <typename, typename> class SET_T, typename KEY_T> 
class BaseCache
{
public:
    BaseCache(std::string name, CacheLevel cl, size_t size, size_t assoc, size_t groups);
    virtual ~BaseCache();
    bool Translate(TranslateEvent *trans_event, bool alloc);
    void PrintStat();
    void PrintConfig();
    size_t GetGroupNum();
    void ConfigSelectiveAlloc(bool salloc, std::vector<did_t> *);
    void ConfigSelectiveAlloc(bool salloc, size_t start_did);
    bool IsDIDCacheable(did_t did);
    void SetPinnedPages(std::vector<hwaddr> pinned_v_ptr);
protected:
    // Variables
    std::string _name;
    CacheLevel _lvl;
    hwaddr _addr_mask;
    size_t _size, _wnum, _snum, _sets_per_gr;
    size_t _groups_num;
    size_t _bp_index, _bp_grid, _bp_frid;
    std::vector<SET_T<KEY_T, CacheEntry> *> _set_v;
    size_t _ind_shift;
    std::unordered_set<did_t> _cacheable_did_set;
    bool _salloc;
    // Methods
    size_t GetIndex(TranslateEvent&);
    virtual bool SetHit(TranslateEvent *, SET_T<KEY_T, CacheEntry> *) = 0;
    virtual bool AddEntry(TranslateEvent *, SET_T<KEY_T, CacheEntry> *) = 0;
    // Statistics
    uint64_t _miss_st;
    uint64_t _hit_st;
    uint64_t _confl_st;
    std::unordered_set<did_t> _seen_did_st;
    std::vector<hwaddr> _pinned_v;
};


template <template <typename, typename> class SET_T, typename KEY_T>
class ContextCache : public BaseCache<SET_T, KEY_T>
{
public:
    ContextCache(std::string name, CacheLevel cl, size_t size, size_t assoc, size_t groups);
    virtual ~ContextCache();
private:
    virtual bool SetHit(TranslateEvent *, SET_T<KEY_T, CacheEntry> *);
    virtual bool AddEntry(TranslateEvent *, SET_T<KEY_T, CacheEntry> *);
};


template <template <typename, typename> class SET_T, typename KEY_T>
class AddrCache : public BaseCache<SET_T, KEY_T>
{
public:
    AddrCache(std::string name, CacheLevel cl, size_t size, size_t assoc, size_t groups);
    virtual ~AddrCache();
private:
    AddrDidKey ConstructAddrDidKey(hwaddr iova, hwaddr mask, did_t did, uint64_t nat);
    virtual bool SetHit(TranslateEvent *, SET_T<KEY_T, CacheEntry> *);
    virtual bool AddEntry(TranslateEvent *, SET_T<KEY_T, CacheEntry> *);
};

#include "cache.tcc"

#endif