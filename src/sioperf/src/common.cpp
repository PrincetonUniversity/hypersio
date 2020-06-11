#include <iostream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include "common.h"
#include "dbg.h"
#include "config.h"

using namespace std;

hwaddr pageLevelToPageMask(lvl_t level) {
    if (level == 0) {
        LOG_ERR("Page level can't be 0!");
        return 0;
    }

    return ~((1<<LEVEL_SHIFT(level)) - 1);
}

lvl_t pageMaskToLevel(hwaddr mask) {
    if (mask == pageLevelToPageMask(1)) {
        return 1;
    } else if (mask == pageLevelToPageMask(2)) {
        return 2;
    } else if (mask == pageLevelToPageMask(3)) {
        return 3;
    } else if (mask == pageLevelToPageMask(4)) {
        return 4;
    } else {
        LOG_ERR("Can't convert mask 0x%lx to any level!\n", mask);
        return 0;
    }
}

/* Attention: Doesn't check for valid input! */
vector<did_t> expandRangeOption(const string opt) {
    size_t pos, rangelen;
    string delim("-");
    string substr;
    did_t start, end, i;
    vector<did_t> did_v;

    pos = opt.find(delim);
    if (pos != string::npos) {
        substr = opt.substr(0, pos);
        start = stoul(substr,nullptr);
        substr = opt.substr(pos+1, opt.length()-pos-1);
        end = stoul(substr,nullptr);
    } else {
        start = stoul(opt,nullptr);
        end = start;
    }
    rangelen = end - start + 1;

    for (i = 0; i < rangelen; i++) {
        did_v.push_back(start++);
    }

    return did_v;
}

vector<sid_t> sidVFromDevV(const vector<did_t> did_v) {
    vector<sid_t> sid_v;

    for (auto it = did_v.begin(); it != did_v.end(); it++) {
        sid_v.push_back(*it << 3);
    }

    return sid_v;
}

string boolToString(bool v)
{
    if (v) {
        return string("YES");
    } else {
        return string("NO");
    }
}


//-----------------------------------------
// TimeCnt
//-----------------------------------------
TimeCnt::TimeCnt() {
    _time = 0;
}

TimeCnt::~TimeCnt() {}

void TimeCnt::Add(timecnt_t t) {
    _time += t;
}

timecnt_t TimeCnt::Get() {
    return _time;
}

void TimeCnt::Reset() {
    _time = 0;
}


//-----------------------------------------
// FlowStat
//-----------------------------------------
FlowStat::FlowStat(TranslateEvent *tr) {
    _first_trans_start = tr->GetTransStartTime();
    _last_trans_end = tr->GetTransEndTime();
    _trans_cnt = 1;
    _pkt_size = DEFAULT_PKT_SIZE_BYTES;
}

FlowStat::~FlowStat() {}

void FlowStat::Update(TranslateEvent *tr) {
    _trans_cnt++;
    
    // Have to check start/end time because translation requests
    // can be finished out of order
    if (tr->GetTransStartTime() < _first_trans_start) {
        _first_trans_start = tr->GetTransStartTime();
    }

    if (tr->GetTransEndTime() > _last_trans_end) {
        _last_trans_end = tr->GetTransEndTime();
    }
}

timecnt_t FlowStat::GetTransTime() {
    return _last_trans_end - _first_trans_start;
}

size_t FlowStat::GetTransCnt() {
    return _trans_cnt;
}

// BW in Mb/s
size_t FlowStat::GetBW() {
    double pkt_num = _trans_cnt / TRANSLATIONS_PER_PKT;
    return (size_t) (_pkt_size*8*pkt_num*1000)/(GetTransTime());
}


void FlowStat::PrintStat() {
    cout << "Time: " << GetTransTime() << " ns";
    cout << ", Trans #: " << setw(6) << GetTransCnt();
    cout << ", BW " << setw(6) << GetBW() << " Mb/s" << "\n";
}

//-----------------------------------------
// Get Environment variables
//-----------------------------------------
char* getHyperSIOHome() {
    char *hypersio_home = getenv("HYPERSIO_HOME");
    if (hypersio_home == NULL)
        LOG_ERR("HYPERSIO_HOME environment variable is not set!");
    return hypersio_home;
}

