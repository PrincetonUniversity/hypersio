#include <regex>
#include <iostream>
#include <string>
#include <algorithm>
#include "eventparser.h"
#include "event.h"
#include "common.h"
#include "dbg.h"

using namespace std;

unordered_set<did_t> EventParser::_valid_did_set;

EventParser::EventParser() {}
EventParser::~EventParser() {}

void EventParser::AddMatch(const char *match_str)
{
    regex *m = new regex(match_str);
    _match_v.push_back(m);
}

void EventParser::SetValidSid(vector<sid_t> sid_v)
{
    _valid_sid_v = sid_v;
    sort(_valid_sid_v.begin(), _valid_sid_v.end());
}

bool EventParser::IsSidValid(sid_t sid)
{
    return binary_search(_valid_sid_v.begin(), _valid_sid_v.end(), sid);
}

void EventParser::AddValidDid(did_t did)
{
    _valid_did_set.insert(did);
}

void EventParser::ResetValidDid()
{
    _valid_did_set.clear();
}

bool EventParser::IsDidValid(did_t did)
{
    return _valid_did_set.find(did) != _valid_did_set.end();
}

/* TranslateEventParser Methods */

TranslateEventParser::~TranslateEventParser()
{
    vector<regex *>::iterator it;

    for (it = _match_v.begin(); it != _match_v.end(); it++) {
        delete (*it);
    }
}

Event *TranslateEventParser::ParseLine(const string *line)
{
    vector<regex *>::iterator it;
    smatch sm;
    string event_name;
    hwaddr iova, mask, slpte;
    did_t did;
    sid_t sid;
    lvl_t level;

    // try to match all patterns. Return an event after the first match
    for (it = _match_v.begin(); it != _match_v.end(); it++) {
        if (regex_match(*line, sm, **it)) {
            sid = stoul(sm[2], nullptr, 16);
            // Skip event if its SID is not among given ones
            if (!IsSidValid(sid)) {
                // Event corresponds to ignored device
                // don't parse event any further
                return nullptr;
            } else {
                event_name = sm[1];
                // Depending on matched event name, different sub-matches correspond
                // to different attributes
                if (!event_name.compare("vtd_iotlb_page_update")) {
                    iova = stoul(sm[3], nullptr, 16);
                    slpte = stoul(sm[4], nullptr, 16);
                    level = stoul(sm[5], nullptr, 16);
                    did = stoul(sm[6], nullptr, 16);
                    mask = pageLevelToPageMask(level);
                } else if (!event_name.compare("vtd_iotlb_page_hit")) {
                    iova = stoul(sm[3], nullptr, 16);
                    mask = stoul(sm[4], nullptr, 16);
                    slpte = stoul(sm[5], nullptr, 16);
                    did = stoul(sm[6], nullptr, 16);
                    level = pageMaskToLevel(mask);
                } else {
                    LOG_ERR("Don't know how to process event %s", event_name.c_str());
                    return nullptr;
                }
                
                return new TranslateEvent(TRANSLATE_EVENT, sid, iova, did, slpte, mask, level);
            }
        }
    }

    return nullptr;
}

// CCEventParser Methods
CCEventParser::~CCEventParser()
{
    vector<regex *>::iterator it;

    for (it = _match_v.begin(); it != _match_v.end(); it++) {
        delete (*it);
    }
}


Event *CCEventParser::ParseLine(const string *line) {
    vector<regex *>::iterator it;
    smatch sm;
    sid_t bus, devfn, sid;
    did_t did;
    fld_t gen;
    hwaddr high, low;

    // Try to match all patterns. Return an event after the first match
    for (it = _match_v.begin(); it != _match_v.end(); it++) {
        if (regex_match(*line, sm, **it)) {
            bus = stoul(sm[2], nullptr, 16);
            devfn = stoul(sm[3], nullptr, 16);
            sid = (bus << 8) | devfn;
            // Skip event if devfn not in valid list
            if (!IsSidValid(sid)) {
                // break mathing loop and return nullptr
                return nullptr;
            } else {
                // both vtd_iotlb_cc_update and vtd_iotlb_cc_hit
                // have the same attributes at the same positions
                high = stoul(sm[4], nullptr, 16);
                low = stoul(sm[5], nullptr, 16);
                gen = stoul(sm[6], nullptr, 16);
                did = (high >> 8) & 0xffff;
                // for a valid sid, add did to its respective list
                AddValidDid(did);
            }

            return new CCEvent(CC_EVENT, sid, did, high, low, gen);
        }
    }

    // No matches were found
    return nullptr;
}


// SlpteEventParser
SlpteEventParser::~SlpteEventParser()
{
    vector<regex *>::iterator it;

    for (it = _match_v.begin(); it != _match_v.end(); it++) {
        delete(*it);
    }
}


Event *SlpteEventParser::ParseLine(const string *line)
{
    vector<regex *>::iterator it;
    smatch sm;
    hwaddr high, low, iova, paddr, slpte;
    lvl_t lvl;
    did_t did;


    // Try to match all patterns. Return an event after the first match
    for (it = _match_v.begin(); it != _match_v.end(); it++) {
        if (regex_match(*line, sm, **it)) {
            high = stoul(sm[2], nullptr, 16);
            did = (high >> 8) & 0xffff;
            // Skip event if its Device ID (DID) is not among given ones
            if (!IsDidValid(did)) {
                return nullptr;
            } else {
                low = stoul(sm[3], nullptr, 16);
                iova = stoul(sm[4], nullptr, 16);
                lvl = stoul(sm[5], nullptr, 16);
                paddr = stoul(sm[6], nullptr, 16);
                slpte = stoul(sm[7], nullptr, 16);
            }

            return new SlpteEvent(SLPTE_EVENT, did, high, low,
                                  iova, lvl, paddr, slpte);
        }
    }

    // No matches were found
    return nullptr;
}
