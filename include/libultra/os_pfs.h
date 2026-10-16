#ifndef OS_PFS_H
#define OS_PFS_H

#include "types.h"
#include <libultraship/libultra/pfs.h>

#ifndef PFS_ERR_NOPACK
#define PFS_ERR_NOPACK 1
#endif
#ifndef PFS_ERR_NEW_PACK
#define PFS_ERR_NEW_PACK 2
#endif
#ifndef PFS_ERR_INCONSISTENT
#define PFS_ERR_INCONSISTENT 3
#endif
#ifndef PFS_ERR_CONTRFAIL
#define PFS_ERR_CONTRFAIL CONT_OVERRUN_ERROR
#endif
#ifndef PFS_ERR_INVALID
#define PFS_ERR_INVALID 5
#endif
#ifndef PFS_ERR_BAD_DATA
#define PFS_ERR_BAD_DATA 6
#endif
#ifndef PFS_DATA_FULL
#define PFS_DATA_FULL 7
#endif
#ifndef PFS_DIR_FULL
#define PFS_DIR_FULL 8
#endif
#ifndef PFS_ERR_EXIST
#define PFS_ERR_EXIST 9
#endif
#ifndef PFS_ERR_ID_FATAL
#define PFS_ERR_ID_FATAL 10
#endif
#ifndef PFS_ERR_DEVICE
#define PFS_ERR_DEVICE 11
#endif
#ifndef PFS_ERR_NO_GBCART
#define PFS_ERR_NO_GBCART 12
#endif
#ifndef PFS_ERR_NEW_GBCART
#define PFS_ERR_NEW_GBCART 13
#endif

#endif
