#ifndef __GENIE_MESH_BT_H__
#define __GENIE_MESH_BT_H__

#include <core/inc/genie_event.h>
#include <core/inc/genie_crypto.h>
#include <core/inc/genie_ais.h>
#include <core/inc/genie_ota.h>
#include <core/inc/genie_reset.h>

bool genie_transport_tx_in_progress(void);
void genie_mesh_load_group_addr(void);

#endif
