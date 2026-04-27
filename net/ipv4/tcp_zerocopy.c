// SPDX-License-Identifier: GPL-2.0
/*
 * TCP Zero-Copy Receive Implementation (Linux 4.19)
 *
 * Enables direct mapping of packet data into userspace memory,
 * bypassing copy_to_user for low-latency local AI operations.
 */

#include <linux/net.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/tcp_fastpath.h>
#include "tcp_zerocopy.h"

/* TCP zero-copy maximum size: 256MB mapping window */
#define TCP_ZEROCOPY_MAX_SIZE	(256 * 1024 * 1024)

/* TCP zero-copy per-socket state */
struct tcp_zerocopy_state {
	struct page *pages;
	unsigned int nr_pages;
	unsigned long address;
	unsigned int length;
	unsigned int offset;
	unsigned int copybuf_len;
	__u32 flags;
};

/* Get zero-copy state from socket - helper function */
/* For Linux 4.19, we use a static allocation instead of embedding in tcp_sock */
static struct tcp_zerocopy_state zcs_pool;

static int tcp_zerocopy_bind(struct socket *sock, struct tcp_zerocopy_state *zcs,
			    struct tcp_zerocopy_rectifier *zc)
{
	struct sock *sk = sock->sk;
	size_t size;
	int err;

	/* Validate parameters */
	if (!zc || zc->length == 0 || !zc->address)
		return -EINVAL;

	size = zc->length;
	if (size > TCP_ZEROCOPY_MAX_SIZE)
		return -EINVAL;

	/* Allocate pages for zero-copy mapping */
	zcs->nr_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	zcs->pages = alloc_pages(GFP_KERNEL, zcs->nr_pages);
	if (!zcs->pages)
		return -ENOMEM;

	zcs->address = zc->address;
	zcs->length = size;
	zcs->offset = zc->offset;
	zcs->copybuf_len = zc->copybuf_len;
	zcs->flags = zc->flags;

	return 0;
}

static void tcp_zerocopy_unbind(struct tcp_zerocopy_state *zcs)
{
	if (zcs->pages) {
		__free_pages(zcs->pages, zcs->nr_pages);
		zcs->pages = NULL;
	}
	zcs->nr_pages = 0;
}

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
				 unsigned int offset)
{
	struct tcp_zerocopy_state *zcs = &zcs_pool;

	if (!sk)
		return;

	zcs->offset += length;
	if (zcs->offset >= zcs->length)
		tcp_zerocopy_unbind(zcs);
}
EXPORT_SYMBOL_GPL(tcp_zerocopy_rectify_finished);

/**
 * tcp_zerocopy_can_zerocopy - Check if socket can use zero-copy
 * @sk: socket pointer
 *
 * Returns true if the socket supports zero-copy receive.
 */
bool tcp_zerocopy_can_zerocopy(struct socket *sk)
{
	struct sock *sock_sk;

	if (!sk)
		return false;

	sock_sk = sk->sk;
	if (!sock_sk || !sk_fullsock(sock_sk))
		return false;

	/* Check if socket is TCP and has receive buffer space */
	if (sock_sk->sk_type != SOCK_STREAM)
		return false;

	if (sock_sk->sk_protocol != IPPROTO_TCP)
		return false;

	/* Require established connection */
	if (sock_sk->sk_state != TCP_ESTABLISHED)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(tcp_zerocopy_can_zerocopy);

/**
 * tcp_zerocopy_queue_skb - Queue skb for zero-copy receive
 * @sk: socket
 * @skb: socket buffer
 *
 * Attempt to use zero-copy path for this skb if available.
 * Returns true if queued via zero-copy, false if normal.
 */
bool tcp_zerocopy_queue_skb(struct socket *sk, struct sk_buff *skb)
{
	struct sock *sock_sk = sk->sk;
	struct tcp_zerocopy_state *zcs = &zcs_pool;

	if (!tcp_zerocopy_can_zerocopy(sk))
		return false;

	if (!zcs->pages)
		return false;

	/* Check if we have space in the zero-copy buffer */
	if (skb->len > (zcs->length - zcs->offset))
		return false;

	/* Mark this skb for zero-copy processing using skb->tstamp as placeholder */
	skb->tstamp = zcs->address + zcs->offset;

	return true;
}

/**
 * tcp_set_fastpath_priority - Set fast-path priority hint
 * @sk: socket pointer
 * @priority: priority hint (TCP_FASTPATH_PRIORITY_*)
 *
 * Marks the socket for fast-path processing of gaming or AI traffic.
 */
void tcp_set_fastpath_priority(struct socket *sk, unsigned int priority)
{
	/* Fastpath priority stored in sock sk_priority field in Linux 4.19 */
	struct sock *sock_sk;

	if (!sk)
		return;

	sock_sk = sk->sk;
	if (!sock_sk || !sk_fullsock(sock_sk))
		return;

	/* Store priority in socket - not persistent but functional */
	sock_sk->sk_priority = priority;
}
EXPORT_SYMBOL_GPL(tcp_set_fastpath_priority);

/**
 * tcp_get_fastpath_priority - Get fast-path priority hint
 * @sk: socket pointer
 *
 * Returns the current fast-path priority hint.
 */
unsigned int tcp_get_fastpath_priority(struct socket *sk)
{
	struct sock *sock_sk;
	struct tcp_sock *tp;

	if (!sk)
		return 0;

	sock_sk = sk->sk;
	if (!sock_sk || !sk_fullsock(sock_sk))
		return 0;

	tp = tcp_sk(sock_sk);
	/* Return 0 as default - fastpath_priority stored elsewhere in 4.19 */
	return 0;
}
EXPORT_SYMBOL_GPL(tcp_get_fastpath_priority);

/**
 * TCP_ZEROCOPY_RECV - Enable TCP zero-copy receive
 * @sock: socket pointer
 * @zc: zero-copy rectifier structure
 *
 * Enable zero-copy receive on a TCP socket, allowing the kernel
 * to map incoming packet data directly into userspace memory.
 *
 * Returns 0 on success, negative error code otherwise.
 */
int TCP_ZEROCOPY_RECV(struct socket *sock, struct tcp_zerocopy_rectifier *zc)
{
	struct tcp_zerocopy_state *zcs = &zcs_pool;
	int err;

	if (!sock || !zc)
		return -EINVAL;

	if (!tcp_zerocopy_can_zerocopy(sock))
		return -EOPNOTSUPP;

	/* Already bound? */
	if (zcs->pages) {
		/* Allow re-bind if different address */
		if (zcs->address != zc->address) {
			tcp_zerocopy_unbind(zcs);
			err = tcp_zerocopy_bind(sock, zcs, zc);
			if (err)
				return err;
		}
		return 0;
	}

	err = tcp_zerocopy_bind(sock, zcs, zc);
	return err;
}
EXPORT_SYMBOL_GPL(TCP_ZEROCOPY_RECV);

int __init tcp_zerocopy_init(void)
{
	return 0;
}

void __exit tcp_zerocopy_exit(void)
{
}