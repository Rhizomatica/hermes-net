/*
 * UUCP fast-track: shortcut the pre-protocol UUCP DLE handshake (Shere/S/ROK/P/U)
 * to reduce over-the-air roundtrips on high-latency/half-duplex links.
 *
 * This is intentionally conservative and OFF by default.
 */

#ifndef UUCP_FASTTRACK_H__
#define UUCP_FASTTRACK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uucpd.h"

typedef enum {
    UFT_ROLE_NONE = 0,
    UFT_ROLE_MASTER,
    UFT_ROLE_SLAVE,
} uft_role_t;

void uft_set_enabled(bool enabled);
bool uft_is_enabled(void);

void uft_on_connected(rhizo_conn *conn, bool outgoing);
void uft_on_disconnected(void);

/* Filter bytes flowing from local uucico toward HF. Returns number of bytes to actually TX. */
size_t uft_filter_uucico_to_hf(rhizo_conn *conn,
                               const uint8_t *in,
                               size_t in_len,
                               uint8_t *out,
                               size_t out_cap);

/* Returns true if bytes were consumed/held by fast-track (caller should not deliver them to uucico). */
bool uft_consume_hf_to_uucico(rhizo_conn *conn, const uint8_t *in, size_t in_len);

#endif /* UUCP_FASTTRACK_H__ */
