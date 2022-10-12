#ifndef __GENIE_INTERNAL_H__
#define __GENIE_INTERNAL_H__

#include "genie_event.h"
#include "genie_storage.h"
#include "genie_cli.h"
#include "genie_at.h"
#include "genie_bin_cmds.h"

#include "genie_triple.h"

#ifdef CONFIG_GENIE_RHYTHM
#include "genie_rhythm.h"
#endif

#ifdef CONFIG_BT_MESH_NPS_OPT
#include "genie_nps_config.h"
#endif

#ifdef CONFIG_GENIE_MESH_SCENE_SHARE
#include "genie_scene_share.h"
#endif

#ifdef CONFIG_GENIE_ULTRA_PROV
#include "genie_ultra_prov.h"
#endif

#endif
