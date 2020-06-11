#ifndef __EVENT_H__
#define __EVENT_H__

#include <fstream>
#include "defines.h"

enum event_t 
{
    UNKNOWN_EVENT,
    TRANSLATE_EVENT,
    CC_EVENT,
    SLPTE_EVENT
};

class Event
{
public:
    Event(event_t type, hwaddr iova, did_t did);
    virtual ~Event();
    virtual void WriteToFile(std::ofstream &);
    virtual void ReadFromFile(std::ifstream &);
    virtual void Print();
    did_t GetDID();
    hwaddr GetIOVA();
    event_t GetType();
    void AdjustDid(uint id);
    void AdjustSid(uint logid);
protected:
    event_t _type;
    hwaddr _iova;
    did_t _did;
    sid_t _sid;
};

class TranslateEvent : public Event
{
public:
    TranslateEvent(event_t type, sid_t sid, hwaddr iova, did_t did, hwaddr slpte, hwaddr mask, lvl_t lvl);
    TranslateEvent();
    virtual ~TranslateEvent();
    sid_t GetSID();
    hwaddr GetMask();
    hwaddr GetSLPTE();
    hwaddr GetPageAddr();
    lvl_t GetLvl();
    void Print();
    virtual void WriteToFile(std::ofstream&);
    virtual void ReadFromFile(std::ifstream&);
    void AddTransTime(timecnt_t t);
    void SetTransStartTime(timecnt_t t);
    timecnt_t GetTransStartTime();
    timecnt_t GetTransEndTime();
    timecnt_t GetTransTime();
    void SetNextAccessTime(uint64_t t);
    uint64_t GetNextAccessTime();
    void SetPrefetch(bool );
    bool isPrefetch();
private:
    hwaddr _slpte, _mask;
    lvl_t _lvl;
    timecnt_t _trans_start_time, _trans_end_time;
    uint64_t _next_access_time;
    bool _is_prefetch;
};


class CCEvent : public Event
{
public:
    CCEvent(event_t type, sid_t sid, did_t did, hwaddr high, hwaddr low, fld_t gen);
    CCEvent();
    virtual ~CCEvent();
    virtual void WriteToFile(std::ofstream &);
    virtual void ReadFromFile(std::ifstream &);
    virtual void Print();
    sid_t GetSID();
    hwaddr GetHigh();
    hwaddr GetLow();
protected:
    hwaddr _high, _low;
    fld_t _gen;
};


class SlpteEvent : public Event
{
public:
    SlpteEvent(event_t type, did_t did, hwaddr high, hwaddr low,
               hwaddr iova, lvl_t lvl, hwaddr paddr, hwaddr slpte);
    SlpteEvent();
    virtual ~SlpteEvent();
    virtual void WriteToFile(std::ofstream &);
    virtual void ReadFromFile(std::ifstream &);
    virtual void Print();
    hwaddr GetSlpteAddr();
    lvl_t GetLvl();
    hwaddr GetBaseAddr();
protected:
    did_t _did;
    hwaddr _ce_hi, _ce_lo;
    hwaddr _iova, _paddr, _slpte;
    hwaddr _slpte_addr;
    lvl_t _lvl;
};


#endif