#include "dbg.h"
#include "config.h"
#include "common.h"
#include "event.h"
#include <unordered_set>
#include <cmath>

// Note:
// not a cache in a sense of usual data/intstruction cache
// since entry has only one entry
template <template <typename, typename> class SET_T, typename KEY_T>
BaseCache<SET_T, KEY_T>::BaseCache(std::string name, CacheLevel clvl,
                    size_t size, size_t assoc, size_t groups)
{
    if (assoc != 0) {
        if (size % assoc) {
            LOG_ERR("Size %lu is not divisible by associativity %lu!\n", size, assoc);
        }
    }
    _name = name;
    _size = size;
    _wnum = assoc;
    if (assoc != 0) {
        _snum = size / assoc;
    } else {
        _snum = 0;
    }
    if (_snum % groups) {
        LOG_ERR("%s: Can not equally partition %lu sets into %lu groups\n", _name.c_str(), _snum, groups);
    }
    _sets_per_gr = _snum / groups;
    _groups_num = groups;

    _salloc = false;
    // Statistics
    _miss_st = 0;
    _hit_st = 0;
    _confl_st = 0;
    _addr_mask = pageLevelToPageMask(clvl);

    // Bitwidths
    _bp_index = static_cast<int>(log2(_snum));
    _bp_grid = static_cast<int>(log2(groups));
    _bp_frid = _bp_index - _bp_grid;

    // Create rows
    for (size_t i = 0; i < _snum; i++) {
        SET_T<KEY_T, CacheEntry> *row = new SET_T<KEY_T, CacheEntry>(_wnum);
        _set_v.push_back(row);
    }

    _lvl = clvl;
    switch(clvl) {
        case CACHE_CONTEXT:
            // Assuming Context Cache is fully associative
            if (_snum > 1) {
                LOG_ERR("Expected Fully Associative or Empty Context Cache!\n");
            }
            _ind_shift = CACHE_CONTEXT_IND_SHIFT;
            break;
        case CACHE_L1:
        case CACHE_L2:
            _ind_shift = LEVEL_SHIFT(CACHE_L2);
            break;
        case CACHE_L3:
            _ind_shift = LEVEL_SHIFT(CACHE_L3);
            break;
        default:
            LOG_ERR("Unexpected Cache Level! Setting to default one: %d\n", CACHE_L1);
            _lvl = CACHE_L1;
            _ind_shift = LEVEL_SHIFT(1);
            break;
    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
BaseCache<SET_T, KEY_T>::~BaseCache()
{
    for (auto it = _set_v.begin(); it != _set_v.end(); it++) {
        delete *it;
    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
bool BaseCache<SET_T, KEY_T>::Translate(TranslateEvent *trans_event, bool alloc)
{
    SET_T<KEY_T, CacheEntry> *set;
    size_t ind;
    did_t did;
    bool is_confl;

    // cache size of 0 is equivalent to lack of the cache
    if (_snum == 0) {
        return false;
    }
    
    ind = GetIndex(*trans_event);
    // Access Cache Set
    set = _set_v[ind];

    // search from the largest page since we expect
    // more those than small ones
    if (SetHit(trans_event, set)) {
        if (alloc) {
            _hit_st++;
            set->AddAccess();
        }
        return true;
    }

    // If we haven't found translation, we install it
    if (alloc) {
        _miss_st++;

        is_confl = AddEntry(trans_event, set);
        if (is_confl) {
            _confl_st++;
        }
        set->AddAccess();
        
        // Add DID to a map of seen DIDs for statistics
        did = trans_event->GetDID();
        if (_seen_did_st.find(did) == _seen_did_st.end()) {
            _seen_did_st.insert(did);
        }
    }


    return false;
}

// Private methods
template <template <typename, typename> class SET_T, typename KEY_T>
size_t BaseCache<SET_T, KEY_T>::GetIndex(TranslateEvent& event)
{
    size_t unique_access_bits;
    size_t gr_id, access_id, index;

    switch (_lvl) {
        case CACHE_CONTEXT:
            unique_access_bits = event.GetSID() >> _ind_shift;
            break;
        default:
            unique_access_bits = event.GetIOVA() >> _ind_shift;
            break;
    }

    // Group partitioning based on DID
    gr_id = event.GetDID() & MASK_FROM_BIT_WIDTH(_bp_grid);
    access_id = unique_access_bits & MASK_FROM_BIT_WIDTH(_bp_frid);
    index = (gr_id << _bp_frid) | access_id;
    return index;
}

template <template <typename, typename> class SET_T, typename KEY_T>
void BaseCache<SET_T, KEY_T>::PrintStat()
{
    std::cout << "------------------------------------------\n";
    std::cout << _name << " Statistics\n";
    std::cout << "------------------------------------------\n";
    std::cout << "HIT               : " << _hit_st << "\n";
    std::cout << "MISS              : " << _miss_st << "\n";
    std::cout << "CONFLICT          : " << _confl_st << "\n";
    std::cout << "# DOMAINS USED    : " << _seen_did_st.size() << "\n";
    std::cout << "Row Stat :\n[";
    
    size_t total_access = 0;
    size_t set_access;
    for (auto it = _set_v.begin(); it != _set_v.end(); it++) {
        if (it != _set_v.begin()) {
            std::cout << ", ";
        }
        set_access = (*it)->GetAccessStat();
        std::cout << set_access;
        total_access += set_access;
    }
    std::cout << "]\n";
    if (total_access != (_hit_st + _miss_st)) {
        LOG_WARN("Row Statistics doesn't add up to hit + miss\n");
    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
size_t BaseCache<SET_T, KEY_T>::GetGroupNum()
{
    return _groups_num;
}

template <template <typename, typename> class SET_T, typename KEY_T>
void BaseCache<SET_T, KEY_T>::ConfigSelectiveAlloc(bool salloc, size_t start_did)
{
    _salloc = salloc;

    for (size_t i = 0; i < _groups_num; i++) {
        _cacheable_did_set.insert(start_did+i);
    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
void BaseCache<SET_T, KEY_T>::ConfigSelectiveAlloc(bool salloc, std::vector<did_t> *did_v)
{
    _salloc = salloc;
    size_t c_stride;
    std::vector<did_t> cdid_v;
    did_t did;
    

    if (_salloc) {
        if (did_v->size() <= _groups_num) {
            for (auto it = did_v->begin(); it != did_v->end(); it++) {
                did = *it;
                _cacheable_did_set.insert(did);
                cdid_v.push_back(did);
            }
        } else {
            c_stride = (did_v->size() - 1) / _groups_num + 1;
            LOG_INFO("Cacheable stride: %lu\n", c_stride);
            for (size_t i = 0; i < did_v->size(); i++) {
                if ((i % c_stride) == 0) {
                    did = *(did_v->begin() + i);
                    _cacheable_did_set.insert(did);
                    cdid_v.push_back(did);
                }
            }
        }

        // Remove cacheable DIDs from input vector
        for (auto it = cdid_v.begin(); it != cdid_v.end(); it++) {
            did_v->erase(remove(did_v->begin(), did_v->end(), *it), did_v->end());
        }

        // in case of number of DIDs not divisible by _groups_num
        // take more DIDs from the rest of DID vector
        for (auto it = did_v->begin(); it != did_v->end(); it++) {
            if (cdid_v.size() == _groups_num) {
                break;
            }
            did = (*it);
            _cacheable_did_set.insert(did);
            cdid_v.push_back(did);
        }

        for (auto it = cdid_v.begin(); it != cdid_v.end(); it++) {
            did_v->erase(remove(did_v->begin(), did_v->end(), *it), did_v->end());
        }

    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
bool BaseCache<SET_T, KEY_T>::IsDIDCacheable(did_t did)
{
    if (_salloc) {
        return (_cacheable_did_set.find(did) != _cacheable_did_set.end());
    } else {
        return true;
    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
void BaseCache<SET_T, KEY_T>::PrintConfig()
{
    std::cout << "------------------------------------------\n";
    std::cout << _name << " Configuration\n";
    std::cout << "------------------------------------------\n";
    std::cout << "SIZE              : " << _size << "\n";
    std::cout << "ASSOCIATIVITY     : " << _wnum << "\n";
    std::cout << "GROUPS            : " << _groups_num << "\n";
    std::cout << "CACHEALE DIDs     : ";
    if (_salloc) {
        std::cout << _cacheable_did_set.size() << "\n";
        std::cout << "{ ";
        for (auto it = _cacheable_did_set.begin(); it != _cacheable_did_set.end(); it++) {
            if (it != _cacheable_did_set.begin()) {
                std::cout << ", ";
            }
            std::cout << *it;
        }
        std::cout << " }\n";
    } else {
        std::cout << "ALL\n";
    }
}

template <template <typename, typename> class SET_T, typename KEY_T>
void BaseCache<SET_T, KEY_T>::SetPinnedPages(std::vector<hwaddr> pinned_v)
{
    for (auto setit = _set_v.begin(); setit != _set_v.end(); setit++) {
        (*setit)->SetPinnedPages(pinned_v);
    }

}


//-----------------------------------------------------------------------------
// Context Cache
//-----------------------------------------------------------------------------
template <template <typename, typename> class SET_T, typename KEY_T>
ContextCache<SET_T, KEY_T>::ContextCache(std::string name, CacheLevel cl, size_t size, size_t assoc, size_t groups)
    : BaseCache<SET_T, KEY_T>(name, cl, size, assoc, groups)
{

}

template <template <typename, typename> class SET_T, typename KEY_T>
ContextCache<SET_T, KEY_T>::~ContextCache()
{
    // empty
}


template <template <typename, typename> class SET_T, typename KEY_T>
bool ContextCache<SET_T, KEY_T>::SetHit(TranslateEvent *e, SET_T<KEY_T, CacheEntry> *set)
{
    sid_t key = e->GetSID(); 

    return set->SearchKey(key);
}

template <template <typename, typename> class SET_T, typename KEY_T>
bool ContextCache<SET_T, KEY_T>::AddEntry(TranslateEvent *e, SET_T<KEY_T, CacheEntry> *set)
{
    sid_t key = e->GetSID();
    CacheEntry  ce;

    ce.slpte = e->GetSLPTE();
    return set->AddEntry(key, ce);
}



//-----------------------------------------------------------------------------
// Address Cache
//-----------------------------------------------------------------------------
template <template <typename, typename> class SET_T, typename KEY_T>
AddrCache<SET_T, KEY_T>::AddrCache(std::string name, CacheLevel cl, size_t size, size_t assoc, size_t groups)
    : BaseCache<SET_T, KEY_T>(name, cl, size, assoc, groups)
{

}

template <template <typename, typename> class SET_T, typename KEY_T>
AddrCache<SET_T, KEY_T>::~AddrCache()
{
    // empty
}



template <template <typename, typename> class SET_T, typename KEY_T>
bool AddrCache<SET_T, KEY_T>::SetHit(TranslateEvent *e, SET_T<KEY_T, CacheEntry> *set)
{
    hwaddr mask;
    AddrDidKey key;

    if (BaseCache<SET_T, KEY_T>::_lvl == CACHE_L1) {
            mask = e->GetMask();
            key = ConstructAddrDidKey(e->GetIOVA(), mask, e->GetDID(), e->GetNextAccessTime());
            return set->SearchKey(key);
    } else {
        key = ConstructAddrDidKey(e->GetIOVA(), BaseCache<SET_T, KEY_T>::_addr_mask, e->GetDID(), e->GetNextAccessTime());
        return set->SearchKey(key);
    }

    return false;
}

template <template <typename, typename> class SET_T, typename KEY_T>
AddrDidKey AddrCache<SET_T, KEY_T>::ConstructAddrDidKey(hwaddr iova, hwaddr mask, did_t did, uint64_t nat)
{
    AddrDidKey key;
    
    key.addr = iova & mask;
    key.did = did;
    key.nat = nat;

    return key;
}

template <template <typename, typename> class SET_T, typename KEY_T>
bool AddrCache<SET_T, KEY_T>::AddEntry(TranslateEvent *e, SET_T<KEY_T, CacheEntry> *set)
{
    hwaddr mask;
    CacheEntry ce;

    if (BaseCache<SET_T, KEY_T>::_lvl == CACHE_L1) {
        mask = e->GetMask();
    } else {
        mask = BaseCache<SET_T, KEY_T>::_addr_mask;
    }

    AddrDidKey key = ConstructAddrDidKey(e->GetIOVA(), mask, e->GetDID(), e->GetNextAccessTime());
    ce.slpte = e->GetSLPTE();
    ce.next_acc_time = e->GetNextAccessTime();
    return set->AddEntry(key, ce);
}
