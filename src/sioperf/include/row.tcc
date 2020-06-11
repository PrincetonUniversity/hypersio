#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <string>
#include "row.h"
#include "dbg.h"

template <typename KEY_T, typename ENTRY_T>
CacheRow<KEY_T, ENTRY_T>::CacheRow(size_t rsize)
{
    _max_size = rsize;
    _access_stat = 0;
}

template <typename KEY_T, typename ENTRY_T>
CacheRow<KEY_T, ENTRY_T>::~CacheRow()
{
    _entries.clear();
}

// Returns 
// 'true' if there was a conflict,
// 'false' otherwise
template <typename KEY_T, typename ENTRY_T>
bool CacheRow<KEY_T, ENTRY_T>::AddEntry(KEY_T k, ENTRY_T v)
{
    KEY_T victim_k;
    bool was_confict;

    // always accessing just added entry
    // _access_stat += 1;
    // check if we have to evict an entry before
    // adding a new one

    was_confict = false;
    if (_entries.size() == _max_size) {
        victim_k = GetVictimKey();
        InvKey(victim_k);
        was_confict = true;
    }
    _entries[k] = v;
    return was_confict;
}

template <typename KEY_T, typename ENTRY_T>
void CacheRow<KEY_T, ENTRY_T>::InvKey(KEY_T k)
{
    _entries.erase(k);
}

template <typename KEY_T, typename ENTRY_T>
bool CacheRow<KEY_T, ENTRY_T>::SearchKey(KEY_T k)
{
    typename std::unordered_map<KEY_T, ENTRY_T>::const_iterator found = _entries.find(k);
    if (found == _entries.end()) {
        return false;
    } else {
        // Accessing an existing entry
        // _access_stat += 1;
        return true;
    }
}

template <typename KEY_T, typename ENTRY_T>
void CacheRow<KEY_T, ENTRY_T>::Print()
{
    typename std::unordered_map<KEY_T, ENTRY_T>::iterator it;
    
    if (_entries.size() == 0) {
        std::cout << "Row is empty!\n";
        return;
    }
    std::cout << "Not implemented!\n";
}

template <typename KEY_T, typename ENTRY_T>
void CacheRow<KEY_T, ENTRY_T>::AddAccess()
{
    _access_stat++;
}

template <typename KEY_T, typename ENTRY_T>
size_t CacheRow<KEY_T, ENTRY_T>::GetAccessStat()
{
    return _access_stat;
}

//===================================================
// LRU row

template <typename KEY_T, typename ENTRY_T>
CacheRowLRU<KEY_T, ENTRY_T>::CacheRowLRU(size_t size) : CacheRow<KEY_T, ENTRY_T>(size)
{}

template <typename KEY_T, typename ENTRY_T>
CacheRowLRU<KEY_T, ENTRY_T>::~CacheRowLRU()
{}

template <typename KEY_T, typename ENTRY_T>
bool CacheRowLRU<KEY_T, ENTRY_T>::AddEntry(KEY_T k, ENTRY_T v)
{
    bool rv;

    rv = CacheRow<KEY_T, ENTRY_T>::AddEntry(k, v);
    _age_key_fifo.push_back(k);

    return rv;
}

template <typename KEY_T, typename ENTRY_T>
bool CacheRowLRU<KEY_T, ENTRY_T>::SearchKey(KEY_T k)
{
    bool found;

    found = CacheRow<KEY_T, ENTRY_T>::SearchKey(k);
    if (found) {
        // Update LRU order in case of access
        _age_key_fifo.erase(remove(_age_key_fifo.begin(), _age_key_fifo.end(), k), _age_key_fifo.end());
        _age_key_fifo.push_back(k);
    }
    return found;
}

template <typename KEY_T, typename ENTRY_T>
void CacheRowLRU<KEY_T, ENTRY_T>::InvKey(KEY_T k)
{
    CacheRow<KEY_T, ENTRY_T>::InvKey(k);
    _age_key_fifo.erase(remove(_age_key_fifo.begin(), _age_key_fifo.end(), k), _age_key_fifo.end());
}

template <typename KEY_T, typename ENTRY_T>
KEY_T CacheRowLRU<KEY_T, ENTRY_T>::GetVictimKey()
{
    KEY_T k =  _age_key_fifo.front();

    return k;
}

//===================================================
// LRU row with Pinning
template <typename KEY_T, typename ENTRY_T>
CacheRowLRUPin<KEY_T, ENTRY_T>::CacheRowLRUPin(size_t size) : CacheRowLRU<KEY_T, ENTRY_T>(size)
{}

template <typename KEY_T, typename ENTRY_T>
CacheRowLRUPin<KEY_T, ENTRY_T>::~CacheRowLRUPin()
{}


template <typename KEY_T, typename ENTRY_T>
bool CacheRowLRUPin<KEY_T, ENTRY_T>::AddEntry(KEY_T k, ENTRY_T v)
{
    KEY_T victim_k;
    bool was_confict;

    was_confict = false;
    if (CacheRow<KEY_T, ENTRY_T>::_entries.size() == CacheRow<KEY_T, ENTRY_T>::_max_size) {
        victim_k = CacheRowLRU<KEY_T, ENTRY_T>::GetVictimKey();
        was_confict = true;
   
        std::vector<hwaddr>::iterator it;
        it = find(_pinned_addr_v.begin(), _pinned_addr_v.end(), victim_k.addr);
        if (it != _pinned_addr_v.end()) {
            return was_confict;
        } else {
            CacheRowLRU<KEY_T, ENTRY_T>::InvKey(victim_k);
        }
    }

    CacheRow<KEY_T, ENTRY_T>::_entries[k] = v;
    CacheRowLRU<KEY_T, ENTRY_T>::_age_key_fifo.push_back(k);
    return was_confict;
}

template <typename KEY_T, typename ENTRY_T>
void CacheRowLRUPin<KEY_T, ENTRY_T>::SetPinnedPages(std::vector<hwaddr> addr_v)
{
    for (auto it = addr_v.begin(); it != addr_v.end(); it++) {
        _pinned_addr_v.push_back(*it);
    }

}


//===================================================
// LFU Row

template <typename KEY_T, typename ENTRY_T>
CacheRowLFU<KEY_T, ENTRY_T>::CacheRowLFU(size_t size) : CacheRow<KEY_T, ENTRY_T>(size)
{
}

template <typename KEY_T, typename ENTRY_T>
CacheRowLFU<KEY_T, ENTRY_T>::~CacheRowLFU()
{}


template <typename KEY_T, typename ENTRY_T>
bool CacheRowLFU<KEY_T, ENTRY_T>::AddEntry(KEY_T k, ENTRY_T v)
{
    bool rv;

    rv = CacheRow<KEY_T, ENTRY_T>::AddEntry(k, v);
    _key_freq_map[k] = 0;

    return rv;
}

template <typename KEY_T, typename ENTRY_T>
bool CacheRowLFU<KEY_T, ENTRY_T>::SearchKey(KEY_T k)
{
    bool found;

    found = CacheRow<KEY_T, ENTRY_T>::SearchKey(k);
    if (found) {
        // Update LFU counter
        _key_freq_map[k]++;
        if (_key_freq_map[k] == _max_freq_cnt) {
            // Divide all values by 2
            for (auto it = _key_freq_map.begin(); it != _key_freq_map.end(); it++) {
                it->second = it->second >> 1;
            }
        }

    }
    return found;
}

template <typename KEY_T, typename ENTRY_T>
void CacheRowLFU<KEY_T, ENTRY_T>::InvKey(KEY_T k)
{
    CacheRow<KEY_T, ENTRY_T>::InvKey(k);
    _key_freq_map.erase(k);
}


template <typename KEY_T, typename ENTRY_T>
KEY_T CacheRowLFU<KEY_T, ENTRY_T>::GetVictimKey()
{
    uint16_t min_cnt = _max_freq_cnt;
    std::vector<KEY_T> key_with_min_cnt;

    for (auto it = _key_freq_map.begin(); it != _key_freq_map.end(); it++) {
        if (it->second == min_cnt) {
            key_with_min_cnt.push_back(it->first);
        } else if (it->second < min_cnt) {
            min_cnt = it->second;
            key_with_min_cnt.erase(key_with_min_cnt.begin(), key_with_min_cnt.end());
            key_with_min_cnt.push_back(it->first);
        }
    }

    int randIndex = rand() % key_with_min_cnt.size();
    return key_with_min_cnt[randIndex];
}


//========================================================================
// NAT (Next Access Time) Row

template <typename KEY_T, typename ENTRY_T>
CacheRowNAT<KEY_T, ENTRY_T>::CacheRowNAT(size_t size) : CacheRow<KEY_T, ENTRY_T>(size)
{}

template <typename KEY_T, typename ENTRY_T>
CacheRowNAT<KEY_T, ENTRY_T>::~CacheRowNAT()
{}

template <typename KEY_T, typename ENTRY_T>
bool CacheRowNAT<KEY_T, ENTRY_T>::SearchKey(KEY_T k)
{
    typename std::unordered_map<KEY_T, ENTRY_T>::iterator it = CacheRow<KEY_T, ENTRY_T>::_entries.find(k);

    if (it == CacheRow<KEY_T, ENTRY_T>::_entries.end()) {
        return false;
    } else {
        // update next access time
        (it->second).next_acc_time = k.nat;
        return true;
    }
}

template <typename KEY_T, typename ENTRY_T>
KEY_T CacheRowNAT<KEY_T, ENTRY_T>::GetVictimKey()
{
    uint64_t nat_max = 0;
    KEY_T k;

    for (auto it = CacheRow<KEY_T, ENTRY_T>::_entries.begin(); it != CacheRow<KEY_T, ENTRY_T>::_entries.end(); it++) {
        if (it->second.next_acc_time > nat_max) {
            k = it->first;
            nat_max = it->second.next_acc_time;
        }
    }

    return k;
}


template <typename KEY_T, typename ENTRY_T>
void CacheRowNAT<KEY_T, ENTRY_T>::Print()
{
    for (auto it = CacheRow<KEY_T, ENTRY_T>::_entries.begin(); it != CacheRow<KEY_T, ENTRY_T>::_entries.end(); it++) {
        LOG_INFO("Key: addr 0x%lx, did: 0x%x; nat: %lu\n", it->first.addr, it->first.did, it->second.next_acc_time);
    }
}