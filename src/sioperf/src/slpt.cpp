#include "slpt.h"
#include "config.h"

hwaddr addrLvlOffset(hwaddr addr, lvl_t lvl)
{
    hwaddr index = (addr >> LEVEL_SHIFT(lvl)) & (((hwaddr)1 << PAGE_LEVEL_SHIFT)-1);
    return index * SLPTE_SIZE;
}