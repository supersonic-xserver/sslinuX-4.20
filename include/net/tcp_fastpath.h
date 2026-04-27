/* SPDX-License-Identifier: GPL-2.0 */
/* sslinuX-4.20: TCP Fastpath header
 * Linux 6.18 TCP Fastpath backport - optimized local node paths
 * for zero-copy AI inference traffic
 */
#ifndef _NET_TCP_FASTPATH_H
#define _NET_TCP_FASTPATH_H

/* TCP Fastpath flags for Zero-Copy AI paths */
#define TCP_FASTPATH_PRIORITY_AI	0x01	/* Local AI inference traffic */
#define TCP_FASTPATH_ENABLED	0x02	/* Fastpath enabled for socket */
#define TCP_FASTPATH_ZEROCOPY	0x04	/* Zero-copy path active */

/* sysctl_tcp_fastpath - enable TCP fastpath optimizations
 * 0: disabled
 * 1: enabled for loopback only  
 * 2: enabled for all local traffic
 */
extern int sysctl_tcp_fastpath;

/* sysctl_tcp_zero_copy - enable TCP zero-copy receive
 * 0: disabled
 * 1: enabled for capable sockets
 */
extern int sysctl_tcp_zero_copy;


/* Check if socket should use fastpath */
static inline bool tcp_fastpath_should_use(struct sock *sk)
{
	if (!sysctl_tcp_fastpath)
		return false;
	
	/* Check for AI priority traffic using sk_buff priority field */
	if (inet_sk(sk)->pktoptions && 
	    (inet_sk(sk)->pktoptions->priority == TCP_FASTPATH_PRIORITY_AI))
		return true;
	
	/* Check for loopback (fastpath=1 allows loopback only) */
	return sysctl_tcp_fastpath == 1 && 
	       (sk->sk_route_caps & NETIF_F_LOOPBACK);
}

#endif /* _NET_TCP_FASTPATH_H */