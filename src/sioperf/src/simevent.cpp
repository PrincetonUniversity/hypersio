#include "simevent.h"
#include "dbg.h"

using namespace std;

SimEvent::SimEvent(TranslateEvent *e, timecnt_t t)
{
    _e_ptr = e;
    _t = t;
    _is_prefetch = false;
}

SimEvent::SimEvent(TranslateEvent *e, timecnt_t t, bool prefetch)
{
    _e_ptr = e;
    _t = t;
    _is_prefetch = prefetch;
}


SimEvent::SimEvent()
{
    // empty
}

SimEvent::~SimEvent()
{
    // empty
}

TranslateEvent *SimEvent::GetEvent()
{
    return _e_ptr;
}

timecnt_t SimEvent::GetTime()
{
    return _t;
}

bool SimEvent::isPrefetch()
{
    return _is_prefetch;
}

//-----------------------------------------------------------------
ScheduledSimEvents::ScheduledSimEvents(bool isOOO)
{
    _isOOO = isOOO;
}


void ScheduledSimEvents::ScheduleEvent(TranslateEvent *te, timecnt_t t, bool is_prefetch)
{
    SimEvent se = SimEvent(te, t, is_prefetch);
    // use priority queue for scheduling revent OOO
    if (_isOOO) {
        _simevent_pq.push(se);
    // use FIFO queue for scheduling event in-order
    } else {
        _simevent_v.push(se);
    }
}

int ScheduledSimEvents::PickReady(timecnt_t tnow, vector<SimEvent> *se_v)
{
    int rdy_cnt;

    if (_isOOO) {
        if (_simevent_pq.empty()) {
            rdy_cnt = 0;
        } else {
            SimEvent se = _simevent_pq.top();
            if (tnow >= se.GetTime()) {
                _simevent_pq.pop();
                se_v->push_back(se);
                rdy_cnt++;
            }
        }
    } else {
        if (_simevent_v.empty()) {
            rdy_cnt = 0;
        } else {
            SimEvent se = _simevent_v.front();
            if (tnow >= se.GetTime()) {
                _simevent_v.pop();
                se_v->push_back(se);
                rdy_cnt++;
            }
        }
    }

    return rdy_cnt;
}