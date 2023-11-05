/* source: xio-rawip.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of raw IP type */

#include "xiosysincludes.h"

#if (WITH_IP4 || WITH_IP6) && WITH_RAWIP

#include "xioopen.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-ip6.h"
#include "xio-tcpwrap.h"

#include "xio-rawip.h"


static
int xioopen_rawip_sendto(int argc, const char *argv[], struct opt *opts,
			 int xioflags, xiofile_t *fd, groups_t groups, int pf,
			 int dummy2, int dummy3);
static
int xioopen_rawip_datagram(int argc, const char *argv[], struct opt *opts,
			 int xioflags, xiofile_t *fd, groups_t groups, int pf,
			 int dummy2, int dummy3);
static
int xioopen_rawip_recvfrom(int argc, const char *argv[], struct opt *opts,
			 int xioflags, xiofile_t *xfd, groups_t groups,
			 int pf, int socktype, int dummy3);
static
int xioopen_rawip_recv(int argc, const char *argv[], struct opt *opts,
		       int xioflags, xiofile_t *xfd, groups_t groups,
		       int pf, int socktype, int ipproto);

static
int _xioopen_rawip_sendto(const char *hostname, const char *protname,
			  struct opt *opts, int xioflags,
			  xiofile_t *xxfd, groups_t groups, int *pf);

const struct addrdesc xioaddr_rawip_sendto  = { "IP-SENDTO",      3, xioopen_rawip_sendto,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6, PF_UNSPEC, 0, 0 HELP(":<host>:<protocol>") };
const struct addrdesc xioaddr_rawip_datagram= { "IP-DATAGRAM",    3, xioopen_rawip_datagram, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_RANGE, PF_UNSPEC, 0, 0 HELP(":<host>:<protocol>") };
const struct addrdesc xioaddr_rawip_recvfrom= { "IP-RECVFROM",    3, xioopen_rawip_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_CHILD|GROUP_RANGE, PF_UNSPEC, SOCK_RAW, 0 HELP(":<protocol>") };
const struct addrdesc xioaddr_rawip_recv    = { "IP-RECV",        1, xioopen_rawip_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_RANGE,             PF_UNSPEC, SOCK_RAW, 0 HELP(":<protocol>") };

#if WITH_IP4
const struct addrdesc xioaddr_rawip4_sendto  = { "IP4-SENDTO",     3, xioopen_rawip_sendto,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4, PF_INET, 0, 0 HELP(":<host>:<protocol>") };
const struct addrdesc xioaddr_rawip4_datagram= { "IP4-DATAGRAM",   3, xioopen_rawip_datagram, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_RANGE, PF_INET, 0, 0 HELP(":<host>:<protocol>") };
const struct addrdesc xioaddr_rawip4_recvfrom= { "IP4-RECVFROM",   3, xioopen_rawip_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_CHILD|GROUP_RANGE, PF_INET,  SOCK_RAW, 0 HELP(":<protocol>") };
const struct addrdesc xioaddr_rawip4_recv    = { "IP4-RECV",       1, xioopen_rawip_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_RANGE,             PF_INET,  SOCK_RAW, 0 HELP(":<protocol>") };
#endif

#if WITH_IP6
const struct addrdesc xioaddr_rawip6_sendto  = { "IP6-SENDTO",     3, xioopen_rawip_sendto,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6, PF_INET6, 0, 0 HELP(":<host>:<protocol>") };
const struct addrdesc xioaddr_rawip6_datagram= { "IP6-DATAGRAM",   3, xioopen_rawip_datagram, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_RANGE, PF_INET6, 0, 0 HELP(":<host>:<protocol>") };
const struct addrdesc xioaddr_rawip6_recvfrom= { "IP6-RECVFROM",   3, xioopen_rawip_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_CHILD|GROUP_RANGE, PF_INET6, SOCK_RAW, 0 HELP(":<protocol>") };
const struct addrdesc xioaddr_rawip6_recv    = { "IP6-RECV",       1, xioopen_rawip_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_RANGE,             PF_INET6, SOCK_RAW, 0 HELP(":<protocol>") };
#endif


/* we expect the form: host:protocol */
/* struct sockaddr_in sa;*/
/* socklen_t salen;*/
static
int xioopen_rawip_sendto(int argc, const char *argv[], struct opt *opts,
			 int xioflags, xiofile_t *xxfd, groups_t groups,
			 int pf, int dummy2, int dummy3) {
   int result;

   if (argc != 3) {
      Error2("%s: wrong number of parameters (%d instead of 2)",
	     argv[0], argc-1);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.preferred_ip);
   if ((result = _xioopen_rawip_sendto(argv[1], argv[2], opts, xioflags, xxfd,
				       groups, &pf)) != STAT_OK) {
      return result;
   }
   _xio_openlate(&xxfd->stream, opts);
   return STAT_OK;
}

/*
   applies and consumes the following options:
   PH_PASTSOCKET, PH_FD, PH_PREBIND, PH_BIND, PH_PASTBIND, PH_CONNECTED, PH_LATE
   OFUNC_OFFSET
   OPT_PROTOCOL_FAMILY, OPT_BIND, OPT_SO_TYPE, OPT_SO_PROTOTYPE, OPT_USER,
   OPT_GROUP, OPT_CLOEXEC
 */
static
int _xioopen_rawip_sendto(const char *hostname, const char *protname,
			  struct opt *opts, int xioflags, xiofile_t *xxfd,
			  groups_t groups, int *pf) {
   char *garbage;
   xiosingle_t *xfd = &xxfd->stream;
   union sockaddr_union us;
   socklen_t uslen;
   int feats = 1;	/* option bind supports only address, not port */
   int socktype = SOCK_RAW;
   int ipproto;
   bool needbind = false;
   int result;

   if ((ipproto = strtoul(protname, &garbage, 0)) >= 256) {
      Error3("xioopen_rawip_sendto(\"%s:%s\",,): protocol number exceeds 255 (%u)",
	     hostname, protname, ipproto);
      return STAT_NORETRY;
   } else if (*garbage) {
      Warn2("xioopen_rawip_sendto(\"%s:%s\",,): trailing garbage in protocol specification",
	     hostname, protname);
      /*return STAT_NORETRY;*/
   }

   xfd->howtoend = END_SHUTDOWN;
   retropt_int(opts, OPT_PROTOCOL_FAMILY, pf);

   /* ...res_opts[] */
   if (applyopts_single(xfd, opts, PH_INIT) < 0)  return -1;
   applyopts(-1, opts, PH_INIT);

   xfd->salen = sizeof(xfd->peersa);
   if ((result =
	xioresolve(hostname, NULL, *pf, socktype, ipproto,
		       &xfd->peersa, &xfd->salen,
		       xfd->para.socket.ip.ai_flags,
		       xfd->para.socket.ip.res_opts))
       != STAT_OK) {
      return result;
   }
   if (*pf == PF_UNSPEC) {
      *pf = xfd->peersa.soa.sa_family;
   }

   uslen = socket_init(*pf, &us);

   xfd->dtype = XIODATA_RECVFROM_SKIPIP;

   if (retropt_bind(opts, *pf, socktype, ipproto, &us.soa, &uslen, feats,
		    xfd->para.socket.ip.ai_flags,
		    xfd->para.socket.ip.res_opts)
       != STAT_NOACTION) {
      needbind = true;
   }
   return
      _xioopen_dgram_sendto(needbind?&us:NULL, uslen,
			    opts, xioflags, xfd, groups, *pf, socktype, ipproto, 0);
}


/* we expect the form: address:protocol */
static
int xioopen_rawip_datagram(int argc, const char *argv[], struct opt *opts,
			 int xioflags, xiofile_t *xxfd, groups_t groups,
			 int pf, int dummy2, int dummy3) {
   xiosingle_t *xfd = &xxfd->stream;
   char *rangename;
   int result;

   if (argc != 3) {
      Error2("%s: wrong number of parameters (%d instead of 2)",
	     argv[0], argc-1);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.preferred_ip);
   if ((result =
	_xioopen_rawip_sendto(argv[1], argv[2], opts, xioflags, xxfd,
				groups, &pf)) != STAT_OK) {
      return result;
   }

   xfd->dtype = XIOREAD_RECV|XIOWRITE_SENDTO;
   if (pf == PF_INET) {
      xfd->dtype |= XIOREAD_RECV_SKIPIP;
   }

   xfd->para.socket.la.soa.sa_family = xfd->peersa.soa.sa_family;

   /* which reply packets will be accepted - determine by range option */
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, pf, &xfd->para.socket.range,
			xfd->para.socket.ip.ai_flags,
			xfd->para.socket.ip.res_opts)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      xfd->para.socket.dorange = true;
      xfd->dtype |= XIOREAD_RECV_CHECKRANGE;
      free(rangename);
   }

#if WITH_LIBWRAP
   xio_retropt_tcpwrap(xfd, opts);
#endif /* WITH_LIBWRAP */

   _xio_openlate(xfd, opts);
   return STAT_OK;
}


static
int xioopen_rawip_recvfrom(int argc, const char *argv[], struct opt *opts,
		     int xioflags, xiofile_t *xfd, groups_t groups,
		     int pf, int socktype, int dummy3) {
   const char *protname = argv[1];
   char *garbage;
   union sockaddr_union us;
   socklen_t uslen = sizeof(us);
   int ipproto;
   bool needbind = false;
   int result;

   if (argc != 2) {
      Error2("%s: wrong number of parameters (%d instead of 1)",
	     argv[0], argc-1);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   if ((ipproto = strtoul(protname, &garbage, 0)) >= 256) {
      Error2("xioopen_rawip_recvfrom(\"%s\",,): protocol number exceeds 255 (%u)",
	     protname, ipproto);
      return STAT_NORETRY;
   } else if (*garbage) {
      Warn1("xioopen_rawip_recvfrom(\"%s\",,): trailing garbage in protocol specification",
	     protname);
      /*return STAT_NORETRY;*/
   }
   xfd->stream.howtoend = END_NONE;

   retropt_socket_pf(opts, &pf);
   if (pf == PF_UNSPEC) {
#if WITH_IP4 && WITH_IP6
      switch (xioparms.default_ip) {
      case '4': pf = PF_INET; break;
      case '6': pf = PF_INET6; break;
      default: break;		/* includes \0 */
      }
#elif WITH_IP6
      pf = PF_INET6;
#else
      pf = PF_INET;
#endif
   }

   if (retropt_bind(opts, pf, socktype, ipproto, &us.soa, &uslen, 1,
		    xfd->stream.para.socket.ip.ai_flags,
		    xfd->stream.para.socket.ip.res_opts)
       != STAT_NOACTION) {
      needbind = true;
   }

   xfd->stream.dtype = XIODATA_RECVFROM_SKIPIP_ONE;
   if ((result =
	_xioopen_dgram_recvfrom(&xfd->stream, xioflags, needbind?&us.soa:NULL,
				uslen, opts, pf, socktype, ipproto, E_ERROR))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(&xfd->stream, opts);
   return STAT_OK;
}


static
int xioopen_rawip_recv(int argc, const char *argv[], struct opt *opts,
		       int xioflags, xiofile_t *xfd, groups_t groups,
		       int pf, int socktype, int dummy3) {
   const char *protname = argv[1];
   char *garbage;
   bool needbind = false;
   union sockaddr_union us;
   socklen_t uslen = sizeof(us);
   int ipproto;
   int result;

   if (argc != 2) {
      Error2("%s: wrong number of parameters (%d instead of 1)",
	     argv[0], argc-1);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   if ((ipproto = strtoul(protname, &garbage, 0)) >= 256) {
      Error2("xioopen_rawip_recv(\"%s\",,): protocol number exceeds 255 (%u)",
	     protname, ipproto);
      return STAT_NORETRY;
   } else if (*garbage) {
      Warn1("xioopen_rawip_recv(\"%s\",,): trailing garbage in protocol specification",
	     protname);
      /*return STAT_NORETRY;*/
   }

   retropt_socket_pf(opts, &pf);
   if (pf == PF_UNSPEC) {
#if WITH_IP4 && WITH_IP6
      pf = xioparms.default_ip=='6'?PF_INET6:PF_INET;
#elif WITH_IP6
      pf = PF_INET6;
#else
      pf = PF_INET;
#endif
   }

   if (retropt_bind(opts, pf, socktype, ipproto,
		    &/*us.soa*/xfd->stream.para.socket.la.soa, &uslen, 1,
		    xfd->stream.para.socket.ip.ai_flags,
		    xfd->stream.para.socket.ip.res_opts)
       == STAT_OK) {
      needbind = true;
   } else {
      /* pf is required during xioread checks */
      xfd->stream.para.socket.la.soa.sa_family = pf;
   }

   xfd->stream.dtype = XIODATA_RECV_SKIPIP;
   result =
      _xioopen_dgram_recv(&xfd->stream, xioflags,
			  needbind?&/*us.soa*/xfd->stream.para.socket.la.soa:NULL,
			  uslen,
			  opts, pf, socktype, ipproto, E_ERROR);
   _xio_openlate(&xfd->stream, opts);
   return result;
}

#endif /* (WITH_IP4 || WITH_IP6) && WITH_RAWIP */
