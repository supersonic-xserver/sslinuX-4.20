/* SPDX-License-Identifier: GPL-2.0 */
/*
 * XDP (Express Data Path) Metadata Hints (Linux 6.18 Fastpath)
 *
 * Allows network cards to pass "hints" directly to local AI nodes about packet priority.
 * This is used for zero-latency local node AI operations.
 */
#ifndef _XDP_METADATA_H
#define _XDP_METADATA_H

#include <linux/types.h>
#include <linux/skbuff.h>

/**
 * struct xdp_metadata_hint - XDP metadata hints for local AI nodes
 * @priority: packet priority for AI processing (0=normal, 1=high, 2=urgent)
 * @flow_hash: flow hash for AI load balancing
 * @cpu_id: target CPU for AI processing
 * @timestamp: packet timestamp for AI timing
 * @ai_type: type of AI operation (0=none, 1=inference, 2=training)
 * @ai_model_size: size hint for AI model loading
 *
 * Metadata hints passed from the network card to the AI stack.
 */
struct xdp_metadata_hint {
	__u8 priority;
	__u32 flow_hash;
	__u32 cpu_id;
	__u64 timestamp;
	__u8 ai_type;
	__u32 ai_model_size;
};

/* XDP metadata hint flags */
#define XDP_META_HINT_PRIORITY		0x01
#define XDP_META_HINT_FLOW_HASH	0x02
#define XDP_META_HINT_CPU_ID		0x04
#define XDP_META_HINT_TIMESTAMP	0x08
#define XDP_META_HINT_AI_TYPE		0x10

/* AI operation types */
#define XDP_AI_TYPE_NONE		0
#define XDP_AI_TYPE_INFERENCE	1
#define XDP_AI_TYPE_TRAINING	2
#define XDP_AI_TYPE_OFFLOAD	3

/**
 * xdp_set_metadata_hint - Set metadata hints on an skb
 * @skb: socket buffer
 * @hint: metadata hint structure
 *
 * Attaches metadata hints to an skb for AI processing.
 * Returns 0 on success, negative error code otherwise.
 */
int xdp_set_metadata_hint(struct sk_buff *skb, struct xdp_metadata_hint *hint);

/**
 * xdp_get_metadata_hint - Get metadata hints from an skb
 * @skb: socket buffer
 * @hint: metadata hint structure (output)
 *
 * Retrieves metadata hints from an skb.
 * Returns 0 on success, negative error code otherwise.
 */
int xdp_get_metadata_hint(struct sk_buff *skb, struct xdp_metadata_hint *hint);

/**
 * xdp_metadata_hint_valid - Check if hints are valid
 * @skb: socket buffer
 *
 * Returns true if the skb contains valid metadata hints.
 */
bool xdp_metadata_hint_valid(struct sk_buff *skb);

/**
 * xdp_clear_metadata_hint - Clear metadata hints from an skb
 * @skb: socket buffer
 *
 * Removes metadata hints from an skb.
 */
void xdp_clear_metadata_hint(struct sk_buff *skb);

#endif /* _XDP_METADATA_H */