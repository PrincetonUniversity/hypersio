#include "row.h"

template <>
bool CacheRow<sid_t, hwaddr>::AddEntry(sid_t k, hwaddr v)
{
    sid_t victim_k;
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