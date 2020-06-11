#ifndef __SIMEVENT_H__
#define __SIMEVENT_H__

#include <queue>
#include "defines.h"
#include "event.h"

class SimEvent
{
public:
    SimEvent(TranslateEvent *e, timecnt_t t);
    SimEvent(TranslateEvent *e, timecnt_t t, bool prefetch);
    SimEvent();
    ~SimEvent();
    TranslateEvent *GetEvent();
    timecnt_t GetTime();
    bool isPrefetch();
private:
    TranslateEvent *_e_ptr;
    timecnt_t _t;
    bool _is_prefetch;
};

class SimEventComparison
{
public:
    SimEventComparison() {};
    bool operator() (SimEvent& lhs, SimEvent& rhs) const
    {
        return (lhs.GetTime() > rhs.GetTime());
    }
};

class ScheduledSimEvents
{
public:
    ScheduledSimEvents() {};
    ScheduledSimEvents(bool isOOO);
    ~ScheduledSimEvents() {};
    void ScheduleEvent(TranslateEvent *e, timecnt_t t, bool is_prefetch);
    int PickReady(timecnt_t tnow, std::vector<SimEvent> *se_v);
private:
    bool _isOOO;
    std::priority_queue<SimEvent, std::vector<SimEvent>, SimEventComparison> _simevent_pq; 
    std::queue<SimEvent> _simevent_v; 
};

#endif