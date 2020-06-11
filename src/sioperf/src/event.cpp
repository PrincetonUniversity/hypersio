#include <iostream>
#include <stdio.h>
#include "event.h"
#include "config.h"
#include "slpt.h"
#include "dbg.h"

using namespace std;

Event::Event(event_t type, hwaddr iova, did_t did)
{
    _type = type;
    _iova = iova;
    _did = did;
    _sid = 0;
}

Event::~Event() {}

void Event::WriteToFile(ofstream& fou)
{
    LOG_ERR("Not implemented!\n");   
}

void Event::ReadFromFile(ifstream& fin)
{
    LOG_ERR("Not implemented!\n");   
}

void Event::Print()
{
    LOG_ERR("Not implemented!\n");   
}

did_t Event::GetDID(void)
{
    return _did;
}

hwaddr Event::GetIOVA(void)
{
    return _iova;
}

event_t Event::GetType(void)
{
    return _type;
}

void Event::AdjustDid(uint id)
{
    _did = id;
}

void Event::AdjustSid(uint logid)
{
    _sid = _sid + logid*MAX_DOMAINS_PER_LOG;
}

/* TranslateEvent Methods */

TranslateEvent::TranslateEvent(event_t type, sid_t sid, hwaddr iova, did_t did, hwaddr slpte, hwaddr mask, lvl_t lvl) :
    Event(type, iova, did)
{
    _slpte = slpte;
    _mask = mask;
    _sid = sid;
    _lvl = lvl;
    _trans_start_time = 0;
    _trans_end_time = 0;
    // sets to a maximum unsigned value
    _next_access_time = -1;
    _is_prefetch = false;
}

TranslateEvent::TranslateEvent() :
    Event(UNKNOWN_EVENT, 0, 0)
{
    _trans_start_time = 0;
    _trans_end_time = 0;
    // sets to a maximum unsigned value
    _next_access_time = -1;
    _is_prefetch = false;
}

sid_t TranslateEvent::GetSID()
{
    return _sid;
}

hwaddr TranslateEvent::GetMask(void)
{
    return _mask;
}

hwaddr TranslateEvent::GetSLPTE(void)
{
    return _slpte;
}

lvl_t TranslateEvent::GetLvl()
{
    return _lvl;
}

hwaddr TranslateEvent::GetPageAddr(void)
{
    return _iova & _mask;
}

void TranslateEvent::SetPrefetch(bool val)
{
    _is_prefetch = val;
}

bool TranslateEvent::isPrefetch()
{
    return _is_prefetch;
}

TranslateEvent::~TranslateEvent() {}

void TranslateEvent::Print()
{
    LOG_INFO("Translate Event: did 0x%x, iova 0x%lx, mask 0x%lx, slpte 0x%lx, lvl 0x%lx, nat %lu\n",
             _did, _iova, _mask, _slpte, _lvl, _next_access_time);
}

void TranslateEvent::WriteToFile(ofstream& fout)
{
    fout.write(reinterpret_cast<char *>(&_type),                sizeof(event_t));
    fout.write(reinterpret_cast<char *>(&_sid),                 sizeof(sid_t));
    fout.write(reinterpret_cast<char *>(&_did),                 sizeof(did_t));
    fout.write(reinterpret_cast<char *>(&_iova),                sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_mask),                sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_lvl),                 sizeof(lvl_t));
    fout.write(reinterpret_cast<char *>(&_slpte),               sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_next_access_time),    sizeof(uint64_t));
}

// Read Translate Event from 'fin' to 'event'
void TranslateEvent::ReadFromFile(ifstream& fin)
{
    fin.read(reinterpret_cast<char *>(&_sid),               sizeof(sid_t));
    fin.read(reinterpret_cast<char *>(&_did),               sizeof(did_t));
    fin.read(reinterpret_cast<char *>(&_iova),              sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_mask),              sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_lvl),               sizeof(lvl_t));
    fin.read(reinterpret_cast<char *>(&_slpte),             sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_next_access_time),  sizeof(uint64_t));
}

void TranslateEvent::SetTransStartTime(timecnt_t t)
{
    _trans_start_time = t;
    _trans_end_time = t;
}

void TranslateEvent::AddTransTime(timecnt_t t)
{
    _trans_end_time += t;
}

timecnt_t TranslateEvent::GetTransStartTime()
{
    return _trans_start_time;
}

timecnt_t TranslateEvent::GetTransEndTime()
{
    return _trans_end_time;
}

timecnt_t TranslateEvent::GetTransTime()
{
    return _trans_end_time - _trans_start_time;
}

void TranslateEvent::SetNextAccessTime(uint64_t t)
{
   _next_access_time = t;
}

uint64_t TranslateEvent::GetNextAccessTime(void)
{
    return _next_access_time;
}

//-------------------------------------------------------------------------
// CCEvent methods
//-------------------------------------------------------------------------
CCEvent::CCEvent(event_t type, sid_t sid, did_t did, hwaddr high, hwaddr low, fld_t gen) :
    Event(type, 0, did)
{
    _high = high;
    _low = low;
    _sid = sid;
    _gen = gen;
}

CCEvent::CCEvent() :
    Event(UNKNOWN_EVENT, 0, 0)
{
    // empty
}

CCEvent::~CCEvent() {}

sid_t CCEvent::GetSID()
{
    return _sid;
}

hwaddr CCEvent::GetHigh()
{
    return _high;
}

hwaddr CCEvent::GetLow()
{
    return _low;
}

void CCEvent::WriteToFile(ofstream& fout)
{
    fout.write(reinterpret_cast<char *>(&_sid),  sizeof(sid_t));
    fout.write(reinterpret_cast<char *>(&_high), sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_low),  sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_gen),  sizeof(fld_t));
}

void CCEvent::ReadFromFile(ifstream& fin)
{
    fin.read(reinterpret_cast<char *>(&_sid),   sizeof(sid_t));
    fin.read(reinterpret_cast<char *>(&_high),  sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_low),   sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_gen),   sizeof(fld_t));
}

void CCEvent::Print()
{
    fprintf(stderr, "CCEvent: SID 0x%x, high 0x%lx, low 0x%lx, gen 0x%x\n",
            _sid, _high, _low, _gen);
}

//-------------------------------------------------------------------------
// SlpteEvent methods
//-------------------------------------------------------------------------
SlpteEvent::SlpteEvent(event_t type, did_t did, hwaddr hi, hwaddr lo,
                       hwaddr iova, lvl_t lvl, hwaddr paddr, hwaddr slpte)
    : Event(type, iova, did)
{
    _did = did;
    _ce_hi = hi;
    _ce_lo = lo;
    _iova = iova;
    _lvl = lvl;
    _paddr = paddr;
    _slpte = slpte;
    _slpte_addr = paddr + addrLvlOffset(iova, lvl);
}

SlpteEvent::SlpteEvent() :
    Event(UNKNOWN_EVENT, 0, 0)
{
    // empty
}

SlpteEvent::~SlpteEvent() {}

void SlpteEvent::WriteToFile(ofstream& fout)
{
    fout.write(reinterpret_cast<char *>(&_did),         sizeof(did_t));
    fout.write(reinterpret_cast<char *>(&_slpte_addr),  sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_lvl),         sizeof(lvl_t));
    fout.write(reinterpret_cast<char *>(&_slpte),       sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_iova),        sizeof(hwaddr));
    fout.write(reinterpret_cast<char *>(&_paddr),       sizeof(hwaddr));
}

void SlpteEvent::ReadFromFile(ifstream& fin)
{
    fin.read(reinterpret_cast<char *>(&_did),           sizeof(did_t));
    fin.read(reinterpret_cast<char *>(&_slpte_addr),    sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_lvl),           sizeof(lvl_t));
    fin.read(reinterpret_cast<char *>(&_slpte),         sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_iova),          sizeof(hwaddr));
    fin.read(reinterpret_cast<char *>(&_paddr),         sizeof(hwaddr));
}

void SlpteEvent::Print()
{
    fprintf(stderr, "SLPTE: DID 0x%x, slpte_addr 0x%lx, iova 0x%lx, base_addr 0x%lx, lvl 0x%lx, slpte 0x%lx\n",
            _did, _slpte_addr, _iova, _paddr, _lvl, _slpte);
}

hwaddr SlpteEvent::GetSlpteAddr()
{
    return _slpte_addr;
}

hwaddr SlpteEvent::GetBaseAddr()
{
    return _paddr;
}

lvl_t SlpteEvent::GetLvl()
{
    return _lvl;
}