#ifndef __EVENTPARSER_H__
#define __EVENTPARSER_H__

#include <regex>
#include <vector>
#include <string>
#include <unordered_set>
#include "event.h"
#include "defines.h"

class EventParser
{
public:
    EventParser();
    virtual ~EventParser() = 0;
    void AddMatch(const char *);
    void SetValidSid(std::vector<sid_t> sid_v);
    void ResetValidDid();
    virtual Event *ParseLine(const std::string *) = 0;
protected:
    std::vector<std::regex *> _match_v;
    std::vector<sid_t> _valid_sid_v;
    // shared between all derived classes
    // constructed dynamically
    static std::unordered_set<did_t> _valid_did_set;
    static bool IsDidValid(did_t did);
    static void AddValidDid(did_t did);
    bool IsSidValid(sid_t sid);
};


class TranslateEventParser : public EventParser
{
public:
    virtual ~TranslateEventParser();
    virtual Event *ParseLine(const std::string *);
};

class CCEventParser : public EventParser
{
public:
    virtual ~CCEventParser();
    virtual Event *ParseLine(const std::string *);
};

class SlpteEventParser : public EventParser
{
public:
    virtual ~SlpteEventParser();
    virtual Event *ParseLine(const std::string *);
};

#endif