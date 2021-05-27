/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 */

#include "uv-compat.h"
#include <unistd.h>

#include <isc/util.h>

#include "netmgr-int.h"

#if UV_VERSION_HEX < UV_VERSION(1, 27, 0)
int
isc_uv_udp_connect(uv_udp_t *handle, const struct sockaddr *addr) {
	int err = 0;

	do {
		int addrlen = (addr->sa_family == AF_INET)
				      ? sizeof(struct sockaddr_in)
				      : sizeof(struct sockaddr_in6);
#ifdef WIN32
		err = connect(handle->socket, addr, addrlen);
#else  /* WIN32 */
		err = connect(handle->io_watcher.fd, addr, addrlen);
#endif /* WIN32 */
	} while (err == -1 && errno == EINTR);

	if (err) {
#ifdef WIN32
		return (uv_translate_sys_error(err));
#else /* WIN32 */
#ifdef HAVE_UV_TRANSLATE_SYS_ERROR
		return (uv_translate_sys_error(errno));
#else
		return (-errno);
#endif /* HAVE_UV_TRANSLATE_SYS_ERROR */
#endif /* WIN32 */
	}

	return (0);
}
#endif /* UV_VERSION_HEX < UV_VERSION(1, 27, 0) */

int
isc_uv_udp_freebind(uv_udp_t *handle, const struct sockaddr *addr,
		    unsigned int flags) {
	int r;
	uv_os_sock_t fd;

	r = uv_fileno((const uv_handle_t *)handle, (uv_os_fd_t *)&fd);
	if (r < 0) {
		return (r);
	}

#if defined(WIN32)
	REQUIRE(fd != INVALID_SOCKET);
#endif

	r = uv_udp_bind(handle, addr, flags);
	if (r == UV_EADDRNOTAVAIL &&
	    isc__nm_socket_freebind(fd, addr->sa_family) == ISC_R_SUCCESS)
	{
		/*
		 * Retry binding with IP_FREEBIND (or equivalent option) if the
		 * address is not available. This helps with IPv6 tentative
		 * addresses which are reported by the route socket, although
		 * named is not yet able to properly bind to them.
		 */
		r = uv_udp_bind(handle, addr, flags);
	}

	return (r);
}

static int
isc__uv_tcp_bind_now(uv_tcp_t *handle, const struct sockaddr *addr,
		     unsigned int flags) {
	int r;
	struct sockaddr_storage sname;
	int snamelen = sizeof(sname);

	r = uv_tcp_bind(handle, addr, flags);
	if (r < 0) {
		return (r);
	}

	/*
	 * uv_tcp_bind() uses a delayed error, initially returning
	 * success even if bind() fails. By calling uv_tcp_getsockname()
	 * here we can find out whether the bind() call was successful.
	 */
	r = uv_tcp_getsockname(handle, (struct sockaddr *)&sname, &snamelen);
	if (r < 0) {
		return (r);
	}

	return (0);
}

int
isc_uv_tcp_freebind(uv_tcp_t *handle, const struct sockaddr *addr,
		    unsigned int flags) {
	int r;
	uv_os_sock_t fd;

	r = uv_fileno((const uv_handle_t *)handle, (uv_os_fd_t *)&fd);
	if (r < 0) {
		return (r);
	}

	r = isc__uv_tcp_bind_now(handle, addr, flags);
	if (r == UV_EADDRNOTAVAIL &&
	    isc__nm_socket_freebind(fd, addr->sa_family) == ISC_R_SUCCESS)
	{
		/*
		 * Retry binding with IP_FREEBIND (or equivalent option) if the
		 * address is not available. This helps with IPv6 tentative
		 * addresses which are reported by the route socket, although
		 * named is not yet able to properly bind to them.
		 */
		r = isc__uv_tcp_bind_now(handle, addr, flags);
	}

	return (r);
}
