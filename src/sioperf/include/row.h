#ifndef __ROW_H__
#define __ROW_H__

#include <unordered_map>
#include <vector>
#include "defines.h"
#include "event.h"

template <typename KEY_T, typename ENTRY_T>
class CacheRow
{
public:
    CacheRow(size_t row_size);
    virtual ~CacheRow();
    virtual bool AddEntry(KEY_T k, ENTRY_T v);
    virtual void InvKey(KEY_T k);
    virtual bool SearchKey(KEY_T k);
    virtual void Print();
    size_t GetAccessStat();
    virtual void AddAccess();
protected:
    virtual KEY_T GetVictimKey() = 0;
    std::unordered_map<KEY_T , ENTRY_T> _entries;
    size_t _max_size;
    size_t _access_stat;
};

template <>
bool CacheRow<sid_t, hwaddr>::AddEntry(sid_t k, hwaddr v);

//-----------------------------------------------------------------------------
// LRU
//-----------------------------------------------------------------------------
template <typename KEY_T, typename ENTRY_T>
class CacheRowLRU : public CacheRow<KEY_T, ENTRY_T>
{
public:
    CacheRowLRU(size_t size);
    virtual ~CacheRowLRU();
    virtual KEY_T GetVictimKey();
    virtual bool AddEntry(KEY_T k, ENTRY_T v);
    virtual bool SearchKey(KEY_T k);
    virtual void InvKey(KEY_T k);
protected:
    std::vector<KEY_T> _age_key_fifo;
private:
    // need a vector since we might want to erase an element
    // from the middle
};


//-----------------------------------------------------------------------------
// LRU with Pinned Addresses
//-----------------------------------------------------------------------------
template <typename KEY_T, typename ENTRY_T>
class CacheRowLRUPin : public CacheRowLRU<KEY_T, ENTRY_T>
{
public:
    CacheRowLRUPin(size_t size);
    virtual ~CacheRowLRUPin();
    // virtual KEY_T GetVictimKey();
    virtual bool AddEntry(KEY_T k, ENTRY_T v);
    // virtual bool SearchKey(KEY_T k);
    // virtual void InvKey(KEY_T k);
    virtual void SetPinnedPages(std::vector<hwaddr> pinned_v);
private:
    std::vector<hwaddr> _pinned_addr_v;
    // std::vector<KEY_T> _age_key_fifo;
};


//-----------------------------------------------------------------------------
// LFU Row
//-----------------------------------------------------------------------------
template <typename KEY_T, typename ENTRY_T>
class CacheRowLFU : public CacheRow<KEY_T, ENTRY_T>
{
public:
    CacheRowLFU(size_t size);
    virtual ~CacheRowLFU();
    virtual KEY_T GetVictimKey();
    virtual bool AddEntry(KEY_T k, ENTRY_T v);
    virtual bool SearchKey(KEY_T k);
    virtual void InvKey(KEY_T k);
protected:
    std::unordered_map<KEY_T, uint16_t> _key_freq_map;
    uint16_t _max_freq_cnt = 15;
};

//-----------------------------------------------------------------------------
// NAT Row
//-----------------------------------------------------------------------------
template <typename KEY_T, typename ENTRY_T>
class CacheRowNAT : public CacheRow<KEY_T, ENTRY_T>
{
public:
    CacheRowNAT(size_t size);
    virtual ~CacheRowNAT();
    virtual KEY_T GetVictimKey();
    virtual bool SearchKey(KEY_T k);
    virtual void Print();
protected:
private:
    // need a vector since we might want to erase an element
    // from the middle
};

#endif