#ifndef __COMMON_H__
#define __COMMON_H__

#include <vector>
#include "defines.h"
#include "config.h"
#include "event.h"

hwaddr pageLevelToPageMask(lvl_t level);
lvl_t pageMaskToLevel(hwaddr mask);
std::vector<did_t> expandRangeOption(const std::string);
std::vector<sid_t> sidVFromDevV(const std::vector<did_t>);
std::string boolToString(bool v);
char* getHyperSIOHome();


class TimeCnt {
public:
    TimeCnt();
    ~TimeCnt();
    void Add(timecnt_t t);
    timecnt_t Get();
    void Reset();
private:
    timecnt_t _time;
};

class FlowStat {
public:
    FlowStat(TranslateEvent *te);
    ~FlowStat();
    void Update(TranslateEvent *te);
    timecnt_t GetTransTime();
    size_t GetTransCnt();
    size_t GetBW();
    void PrintStat();
private:
    timecnt_t _first_trans_start, _last_trans_end;
    size_t _trans_cnt;
    size_t _pkt_size;
};

#endif