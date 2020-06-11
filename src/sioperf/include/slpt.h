#ifndef __SLPT_H__
#define __SLPT_H__

#include "defines.h"

hwaddr addrLvlOffset(hwaddr addr, lvl_t lvl);

class SLPT_Page {
public:
    SLPT_Page();
    ~SLPT_Page();
private:
    hwaddr _base, _offset, _next_page;
    did_t _did;
    lvl_t _lvl;
};

#endif