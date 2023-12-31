/* source: xio-vsock.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Author: Stefano Garzarella <sgarzare@redhat.com */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_vsock_h_included
#define __xio_vsock_h_included 1

#if WITH_VSOCK

extern const struct addrdesc xioaddr_vsock_connect;
extern const struct addrdesc xioaddr_vsock_listen;

extern int xiosetsockaddrenv_vsock(int idx, char *namebuff, size_t namelen,
				   char *valuebuff, size_t valuelen,
				   struct sockaddr_vm *sa, int ipproto);

#endif /* WITH_VSOCK */

#endif /* !defined(__xio_vsock_h_included) */
