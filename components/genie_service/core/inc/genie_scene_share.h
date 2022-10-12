/*
 * Copyright (C) 2019-2022 Alibaba Group Holding Limited
 */
#ifndef _GENIE_SCENE_SHARE_H_
#define _GENIE_SCENE_SHARE_H_

typedef struct _genie_scene_share
{
	char *mac_str;
	uint8_t inited;
	uint8_t custom[16];	  /* custom value */
	uint8_t netkey[16];	  /* netkey */
	uint8_t sharekey[16]; /* sharekey */
} genie_scene_share_t;

int genie_scene_share_msg_encrypt(char *mac_str, struct net_buf_simple *buf);
int genie_scene_share_msg_recv(u8_t ctl_op, struct bt_mesh_net_rx *rx, struct net_buf_simple *buf);

#endif /* _GENIE_SCENE_SHARE_H_ */
