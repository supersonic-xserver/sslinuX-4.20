/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TCP Zero-Copy Receive API (Linux 6.18 Fastpath)
 *
 * This enables direct mapping of packet data into userspace memory,
 * bypassing the CPU-heavy copy_to_user calls for low-latency networking.
 */
#ifndef _TCP_ZEROCOPY_H
#define _TCP_ZEROCOPY_H

#include <linux/types.h>
#include <linux/socket.h>

/**
 * struct tcp_zerocopy_rectifier - TCP zero-copy receive control structure
 * @address: userspace address for zero-copy mapping
 * @length: length of the mapping
 * @offset: offset into the receive window
 * @copybuf_address: fallback copy buffer (if needed)
 * @copybuf_len: fallback copy buffer length
 * @flags: control flags
 *
 * Used to setup zero-copy receive for a TCP socket.
 */
struct tcp_zerocopy_rectifier {
	__u64 address;
	__u32 length;
	__u32 offset;
	__u64 copybuf_address;
	__u32 copybuf_len;
	__u32 flags;
};

/* TCP zero-copy flags */
#define TCP_ZEROCOPY_RECV_MADVISE		0x1	/* madvise for HugeTLB */
#define TCP_ZEROCOPY_RECV_COPY		0x2	/* use copy fallback */

/**
 * TCP_ZEROCOPY_RECV - Enable TCP zero-copy receive
 * @sk: socket pointer
 * @zc: zero-copy rectifier structure
 *
 * Enable zero-copy receive on a TCP socket, allowing the kernel
 * to map incoming packet data directly into userspace memory.
 *
 * Returns 0 on success, negative error code otherwise.
 */
int TCP_ZEROCOPY_RECV(struct socket *sk, struct tcp_zerocopy_rectifier *zc);

/**
 * tcp_zerocopy_rectify_finished - Signal zero-copy rectifying complete
 * @sk: socket pointer
 * @length: bytes received
 * @offset: receive offset
 *
 * Called after processing zero-copied data to release references
 * and update the receive window.
 */
void tcp_zerocopy_rectify_finished(struct socket *sk, unsigned int length,
				 unsigned int offset);

/**
 * tcp_zerocopy_can_zerocopy - Check if socket can use zero-copy
 * @sk: socket pointer
 *
 * Returns true if the socket supports zero-copy receive.
 */
bool tcp_zerocopy_can_zerocopy(struct socket *sk);

/* Fast-path priority hints for gaming/AI traffic */
#ifndef TCP_FASTPATH_PRIORITY_GAMING
#define TCP_FASTPATH_PRIORITY_GAMING	0x01
#endif
#ifndef TCP_FASTPATH_PRIORITY_AI
#define TCP_FASTPATH_PRIORITY_AI	0x01
#endif

/**
 * tcp_set_fastpath_priority - Set fast-path priority hint
 * @sk: socket pointer
 * @priority: priority hint (TCP_FASTPATH_PRIORITY_*)
 *
 * Marks the socket for fast-path processing of gaming or AI traffic.
 * This prioritizes packet processing over complex slow-path operations.
 */
void tcp_set_fastpath_priority(struct socket *sk, unsigned int priority);

/**
 * tcp_get_fastpath_priority - Get fast-path priority hint
 * @sk: socket pointer
 *
 * Returns the current fast-path priority hint.
 */
unsigned int tcp_get_fastpath_priority(struct socket *sk);

#endif /* _TCP_ZEROCOPY_H */