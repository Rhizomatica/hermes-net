/*
 * UUCP fast-track (uucpd):
 * - Shortcuts the pre-protocol UUCP DLE handshake (Shere/S/ROK/P/U) locally.
 * - Advertises capability using a small on-wire marker (non-DLE bytes).
 * - Falls back to normal pass-through if the peer marker is not seen quickly.
 *
 * NOTE: This is incompatible with login-prompt chat flows (uucpd -l).
 */

#include "uucp_fasttrack.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

#include "circular_buffer.h"

#define UFT_DLE 0x10

/* Hold incoming bytes until uucico has switched from zget_uucp_cmd() to protocol start. */
#define UFT_HOLD_MAX (64 * 1024)

/* Peer capability probe. */
#define UFT_PROBE_TIMEOUT_MS 5000
#define UFT_MARKER_RETX_MS 1000

/* Marker MUST NOT include DLE (0x10) so uucico ignores it during handshake. */
static const uint8_t UFT_MARKER[] = {0x1e, 'U', 'F', 'T', '1', 0x1e};
#define UFT_MARKER_LEN (sizeof(UFT_MARKER))

typedef enum {
    UFT_STATE_OFF = 0,
    UFT_STATE_PROBE,
    UFT_STATE_MASTER_WAIT_S,
    UFT_STATE_MASTER_WAIT_U,
    UFT_STATE_SLAVE_WAIT_SHERE,
    UFT_STATE_SLAVE_WAIT_RP,
    UFT_STATE_DONE,
} uft_state_t;

static bool g_enabled = false;
static uft_role_t g_role = UFT_ROLE_NONE;
static uft_state_t g_state = UFT_STATE_OFF;

static pthread_mutex_t g_hold_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_hold_buf[UFT_HOLD_MAX];
static size_t g_hold_len = 0;
static bool g_hold_incoming = false;

static bool g_slave_seen_r = false;
static bool g_slave_seen_p = false;

/* Stream parser state for uucico->HF direction (DLE commands can span reads). */
static bool g_tx_in_cmd = false;
static char g_tx_cmd[256];
static size_t g_tx_cmd_len = 0;

/* Probe / negotiation state. */
static uint64_t g_probe_start_ms = 0;
static uint64_t g_last_marker_tx_ms = 0;
static size_t g_marker_match = 0; /* streaming matcher over HF->uucico bytes */
static bool g_peer_fasttrack = false;
static bool g_activate_pending = false;
static bool g_fallback_pending = false;
static char g_fallback_reason[64];

#define UFT_PROBE_MAX_CMDS 8
static char g_probe_cmds[UFT_PROBE_MAX_CMDS][256];
static size_t g_probe_cmd_count = 0;

/* Session stats (for field logs). */
static uint64_t g_stat_tx_swallowed_bytes = 0;
static uint64_t g_stat_probe_tx_deferred_bytes = 0;
static uint64_t g_stat_rx_held_bytes = 0;
static uint64_t g_stat_hold_flushed_bytes = 0;
static uint64_t g_stat_injected_cmd_bytes = 0;
static unsigned g_stat_injected_cmds = 0;
static unsigned g_stat_marker_tx = 0;
static unsigned g_stat_marker_rx = 0;
static unsigned g_stat_cmds_swallowed = 0;
static unsigned g_stat_probe_cmds_deferred = 0;

static uint64_t uft_monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void uft_reset_session_state(void)
{
    g_role = UFT_ROLE_NONE;
    g_state = UFT_STATE_OFF;

    pthread_mutex_lock(&g_hold_mutex);
    g_hold_len = 0;
    pthread_mutex_unlock(&g_hold_mutex);
    g_hold_incoming = false;

    g_slave_seen_r = false;
    g_slave_seen_p = false;

    g_tx_in_cmd = false;
    g_tx_cmd_len = 0;

    g_probe_start_ms = 0;
    g_last_marker_tx_ms = 0;
    g_marker_match = 0;
    g_peer_fasttrack = false;
    g_activate_pending = false;
    g_fallback_pending = false;
    g_fallback_reason[0] = 0;

    g_probe_cmd_count = 0;

    g_stat_tx_swallowed_bytes = 0;
    g_stat_probe_tx_deferred_bytes = 0;
    g_stat_rx_held_bytes = 0;
    g_stat_hold_flushed_bytes = 0;
    g_stat_injected_cmd_bytes = 0;
    g_stat_injected_cmds = 0;
    g_stat_marker_tx = 0;
    g_stat_marker_rx = 0;
    g_stat_cmds_swallowed = 0;
    g_stat_probe_cmds_deferred = 0;
}

void uft_set_enabled(bool enabled)
{
    g_enabled = enabled;
    if (!g_enabled)
        uft_reset_session_state();
}

bool uft_is_enabled(void)
{
    return g_enabled;
}

static void uft_log_stats(const char *tag)
{
    const char *role = (g_role == UFT_ROLE_MASTER) ? "master" :
                       (g_role == UFT_ROLE_SLAVE)  ? "slave"  : "none";
    fprintf(stderr,
            "uucp_fasttrack: %s (role=%s, state=%d) "
            "marker_tx=%u marker_rx=%u "
            "swallowed=%" PRIu64 "B deferred=%" PRIu64 "B "
            "held=%" PRIu64 "B flushed=%" PRIu64 "B "
            "injected_cmds=%u injected=%" PRIu64 "B\n",
            tag ? tag : "stats",
            role,
            (int)g_state,
            g_stat_marker_tx,
            g_stat_marker_rx,
            g_stat_tx_swallowed_bytes,
            g_stat_probe_tx_deferred_bytes,
            g_stat_rx_held_bytes,
            g_stat_hold_flushed_bytes,
            g_stat_injected_cmds,
            g_stat_injected_cmd_bytes);
}

static bool uft_write_to_uucico(rhizo_conn *conn, const uint8_t *data, size_t len)
{
    if (!conn || !data || len == 0)
        return false;

    while (circular_buf_free_size(conn->out_buffer) < len)
    {
        if (conn->shutdown == true)
            return false;
        usleep(10000); // 10ms
    }

    circular_buf_put_range(conn->out_buffer, (uint8_t *)data, len);
    return true;
}

static bool uft_write_to_hf(rhizo_conn *conn, const uint8_t *data, size_t len)
{
    if (!conn || !data || len == 0)
        return false;

    while (circular_buf_free_size(conn->in_buffer) < len)
    {
        if (conn->shutdown == true)
            return false;
        usleep(10000); // 10ms
    }

    circular_buf_put_range(conn->in_buffer, (uint8_t *)data, len);
    return true;
}

static bool uft_inject_uucp_cmd(rhizo_conn *conn, const char *cmd)
{
    uint8_t buf[512];
    size_t cmd_len;
    size_t total;

    if (!conn || !cmd)
        return false;

    cmd_len = strlen(cmd);
    total = cmd_len + 2; // DLE + payload + NUL
    if (total > sizeof(buf))
        return false;

    buf[0] = UFT_DLE;
    memcpy(buf + 1, cmd, cmd_len);
    buf[1 + cmd_len] = 0;

    if (!uft_write_to_uucico(conn, buf, total))
        return false;

    g_stat_injected_cmds++;
    g_stat_injected_cmd_bytes += total;
    return true;
}

static void uft_flush_hold_locked(rhizo_conn *conn)
{
    uint8_t *tmp = NULL;
    size_t len = 0;

    pthread_mutex_lock(&g_hold_mutex);
    if (g_hold_len > 0)
    {
        len = g_hold_len;
        tmp = (uint8_t *)malloc(len);
        if (tmp)
            memcpy(tmp, g_hold_buf, len);
        g_hold_len = 0;
    }
    pthread_mutex_unlock(&g_hold_mutex);

    if (!conn || !tmp || len == 0)
    {
        free(tmp);
        return;
    }

    (void)uft_write_to_uucico(conn, tmp, len);
    g_stat_hold_flushed_bytes += len;
    free(tmp);
}

static void uft_mark_done(rhizo_conn *conn, const char *reason)
{
    g_state = UFT_STATE_DONE;
    g_hold_incoming = false;
    fprintf(stderr, "uucp_fasttrack: done (%s), releasing HF RX hold\n", reason ? reason : "ok");
    uft_flush_hold_locked(conn);
    uft_log_stats("done");
}

static void uft_probe_request_fallback(const char *reason)
{
    if (g_fallback_pending)
        return;
    g_fallback_pending = true;
    snprintf(g_fallback_reason, sizeof(g_fallback_reason), "%s", reason ? reason : "fallback");
}

static void uft_probe_request_activate(void)
{
    if (g_activate_pending)
        return;
    g_activate_pending = true;
}

static void uft_probe_queue_marker(rhizo_conn *conn, uint64_t now_ms)
{
    if (!conn)
        return;
    if (now_ms == 0)
        now_ms = uft_monotonic_ms();
    if (g_last_marker_tx_ms > 0 && now_ms - g_last_marker_tx_ms < UFT_MARKER_RETX_MS)
        return;

    if (uft_write_to_hf(conn, UFT_MARKER, UFT_MARKER_LEN))
    {
        g_last_marker_tx_ms = now_ms;
        g_stat_marker_tx++;
    }
}

static void uft_probe_flush_deferred_to_hf(rhizo_conn *conn)
{
    if (!conn || g_probe_cmd_count == 0)
        return;

    for (size_t i = 0; i < g_probe_cmd_count; i++)
    {
        const char *cmd = g_probe_cmds[i];
        if (!cmd[0])
            continue;
        uint8_t frame[512];
        size_t cmd_len = strlen(cmd);
        size_t total = cmd_len + 2;
        if (total > sizeof(frame))
            continue;
        frame[0] = UFT_DLE;
        memcpy(frame + 1, cmd, cmd_len);
        frame[1 + cmd_len] = 0;
        (void)uft_write_to_hf(conn, frame, total);
    }

    g_probe_cmd_count = 0;
    g_stat_probe_cmds_deferred = 0;
    g_stat_probe_tx_deferred_bytes = 0;
}

static void uft_probe_fallback_to_passthrough(rhizo_conn *conn, const char *reason)
{
    fprintf(stderr, "uucp_fasttrack: fallback to pass-through (%s)\n", reason ? reason : "unknown");
    uft_log_stats("fallback");

    uft_probe_flush_deferred_to_hf(conn);

    /* Release any bytes we held while probing so uucico can handshake normally. */
    g_hold_incoming = false;
    uft_flush_hold_locked(conn);

    /* Stop intercepting for this session. */
    uft_reset_session_state();
}

static void uft_handle_master_cmd(rhizo_conn *conn, const char *cmd)
{
    if (!conn || !cmd)
        return;

    if (g_state == UFT_STATE_MASTER_WAIT_S && cmd[0] == 'S')
    {
        /* Reply as if peer accepted introduction + offered only protocol y. */
        (void)uft_inject_uucp_cmd(conn, "ROKN");
        (void)uft_inject_uucp_cmd(conn, "Py");
        g_state = UFT_STATE_MASTER_WAIT_U;
        fprintf(stderr, "uucp_fasttrack: master swallowed S..., injected ROKN/Py\n");
        return;
    }

    if (g_state == UFT_STATE_MASTER_WAIT_U && cmd[0] == 'U')
    {
        fprintf(stderr, "uucp_fasttrack: master swallowed U...\n");
        uft_mark_done(conn, "master saw U");
        return;
    }
}

static void uft_handle_slave_cmd(rhizo_conn *conn, const char *cmd)
{
    if (!conn || !cmd)
        return;

    if (g_state == UFT_STATE_SLAVE_WAIT_SHERE && strncmp(cmd, "Shere", 5) == 0)
    {
        if (conn->remote_call_sign[0] == 0)
        {
            fprintf(stderr, "uucp_fasttrack: slave cannot inject S (remote system unknown)\n");
            return;
        }

        char intro[96];
        snprintf(intro, sizeof(intro), "S%s", conn->remote_call_sign);
        (void)uft_inject_uucp_cmd(conn, intro);
        g_state = UFT_STATE_SLAVE_WAIT_RP;
        fprintf(stderr, "uucp_fasttrack: slave swallowed Shere..., injected %s\n", intro);
        return;
    }

    if (g_state == UFT_STATE_SLAVE_WAIT_RP && cmd[0] == 'R')
    {
        g_slave_seen_r = true;
        fprintf(stderr, "uucp_fasttrack: slave swallowed R...\n");
    }

    if (g_state == UFT_STATE_SLAVE_WAIT_RP && cmd[0] == 'P')
    {
        g_slave_seen_p = true;
        fprintf(stderr, "uucp_fasttrack: slave swallowed P...\n");
    }

    if (g_state == UFT_STATE_SLAVE_WAIT_RP && g_slave_seen_r && g_slave_seen_p)
    {
        (void)uft_inject_uucp_cmd(conn, "Uy");
        fprintf(stderr, "uucp_fasttrack: slave injected Uy\n");
        uft_mark_done(conn, "slave saw R+P");
    }
}

static void uft_probe_activate_fasttrack(rhizo_conn *conn)
{
    if (!conn)
        return;
    if (g_state != UFT_STATE_PROBE)
        return;
    if (!g_peer_fasttrack)
        return;

    fprintf(stderr, "uucp_fasttrack: peer marker seen, activating\n");

    /* Any DLE cmds deferred during probe are now truly swallowed. */
    g_stat_tx_swallowed_bytes += g_stat_probe_tx_deferred_bytes;
    g_stat_probe_tx_deferred_bytes = 0;

    g_slave_seen_r = false;
    g_slave_seen_p = false;

    if (g_role == UFT_ROLE_MASTER)
    {
        if (conn->remote_call_sign[0] == 0)
        {
            fprintf(stderr, "uucp_fasttrack: disabled (remote system unknown)\n");
            uft_reset_session_state();
            return;
        }

        char shere[64];
        snprintf(shere, sizeof(shere), "Shere=%s", conn->remote_call_sign);
        (void)uft_inject_uucp_cmd(conn, shere);
        g_state = UFT_STATE_MASTER_WAIT_S;
        fprintf(stderr, "uucp_fasttrack: master injected %s\n", shere);
    }
    else
    {
        g_state = UFT_STATE_SLAVE_WAIT_SHERE;
        fprintf(stderr, "uucp_fasttrack: slave waiting Shere\n");
    }

    /* If we deferred any local DLE cmds during probe (e.g. slave Shere=...), process them now. */
    for (size_t i = 0; i < g_probe_cmd_count; i++)
    {
        if (g_role == UFT_ROLE_MASTER)
            uft_handle_master_cmd(conn, g_probe_cmds[i]);
        else if (g_role == UFT_ROLE_SLAVE)
            uft_handle_slave_cmd(conn, g_probe_cmds[i]);
    }
    g_probe_cmd_count = 0;
    g_stat_probe_cmds_deferred = 0;

    uft_log_stats("activated");
}

void uft_on_connected(rhizo_conn *conn, bool outgoing)
{
    if (!g_enabled || !conn)
        return;

    uft_reset_session_state();

    /* Login-prompt mode is incompatible with handshake shortcut. */
    if (!outgoing && conn->ask_login)
    {
        fprintf(stderr, "uucp_fasttrack: disabled (incoming login prompt enabled)\n");
        uft_reset_session_state();
        return;
    }

    g_hold_incoming = true;
    g_role = outgoing ? UFT_ROLE_MASTER : UFT_ROLE_SLAVE;
    g_state = UFT_STATE_PROBE;
    g_probe_start_ms = uft_monotonic_ms();

    /* Advertise marker immediately (also retried in uft_poll). */
    uft_probe_queue_marker(conn, g_probe_start_ms);

    fprintf(stderr, "uucp_fasttrack: probe start (role=%s, timeout=%ums)\n",
            g_role == UFT_ROLE_MASTER ? "master" : "slave",
            (unsigned)UFT_PROBE_TIMEOUT_MS);
}

void uft_on_disconnected(void)
{
    if (g_state != UFT_STATE_OFF)
        uft_log_stats("disconnected");
    uft_reset_session_state();
}

void uft_poll(rhizo_conn *conn)
{
    if (!g_enabled || !conn)
        return;
    if (g_state != UFT_STATE_PROBE)
        return;

    uint64_t now_ms = uft_monotonic_ms();

    if (g_fallback_pending)
    {
        uft_probe_fallback_to_passthrough(conn, g_fallback_reason);
        return;
    }

    if (g_peer_fasttrack && g_activate_pending)
    {
        g_activate_pending = false;
        uft_probe_activate_fasttrack(conn);
        return;
    }

    uft_probe_queue_marker(conn, now_ms);

    if (!g_peer_fasttrack &&
        g_probe_start_ms > 0 &&
        now_ms > g_probe_start_ms &&
        now_ms - g_probe_start_ms >= UFT_PROBE_TIMEOUT_MS)
    {
        uft_probe_request_fallback("probe timeout");
        uft_probe_fallback_to_passthrough(conn, g_fallback_reason);
    }
}

size_t uft_filter_uucico_to_hf(rhizo_conn *conn,
                               const uint8_t *in,
                               size_t in_len,
                               uint8_t *out,
                               size_t out_cap)
{
    size_t out_len = 0;

    if (!in || in_len == 0 || !out || out_cap == 0)
        return 0;

    if (!g_enabled || g_state == UFT_STATE_OFF || g_state == UFT_STATE_DONE)
    {
        if (in_len > out_cap)
            in_len = out_cap;
        memcpy(out, in, in_len);
        return in_len;
    }

    for (size_t i = 0; i < in_len; i++)
    {
        uint8_t b = in[i];

        if (!g_tx_in_cmd)
        {
            if (b == UFT_DLE)
            {
                g_tx_in_cmd = true;
                g_tx_cmd_len = 0;
                if (g_state == UFT_STATE_PROBE)
                    g_stat_probe_tx_deferred_bytes++;
                else
                    g_stat_tx_swallowed_bytes++;
                continue; // swallow DLE
            }

            if (out_len < out_cap)
                out[out_len++] = b;
            continue;
        }

        /* In DLE command payload. */
        if (g_state == UFT_STATE_PROBE)
            g_stat_probe_tx_deferred_bytes++;
        else
            g_stat_tx_swallowed_bytes++;

        if (b == 0 || b == '\r' || b == '\n')
        {
            g_tx_cmd[g_tx_cmd_len] = 0;
            g_tx_in_cmd = false;
            g_stat_cmds_swallowed++;

            if (g_state == UFT_STATE_PROBE)
            {
                if (g_probe_cmd_count < UFT_PROBE_MAX_CMDS)
                {
                    snprintf(g_probe_cmds[g_probe_cmd_count],
                             sizeof(g_probe_cmds[g_probe_cmd_count]),
                             "%s", g_tx_cmd);
                    g_probe_cmd_count++;
                    g_stat_probe_cmds_deferred++;
                }
                else
                {
                    uft_probe_request_fallback("probe cmd queue full");
                }
            }
            else
            {
                if (g_role == UFT_ROLE_MASTER)
                    uft_handle_master_cmd(conn, g_tx_cmd);
                else if (g_role == UFT_ROLE_SLAVE)
                    uft_handle_slave_cmd(conn, g_tx_cmd);
            }

            continue; // swallow terminator
        }

        if (g_tx_cmd_len + 1 < sizeof(g_tx_cmd))
        {
            g_tx_cmd[g_tx_cmd_len++] = (char)b;
        }
        else
        {
            /* Unexpected: command too large, fall back to pass-through. */
            fprintf(stderr, "uucp_fasttrack: command overflow, disabling\n");
            uft_probe_request_fallback("command overflow");
            g_tx_in_cmd = false;
        }
    }

    return out_len;
}

static bool uft_hold_append(rhizo_conn *conn, const uint8_t *in, size_t in_len)
{
    if (!conn || !in || in_len == 0)
        return false;

    pthread_mutex_lock(&g_hold_mutex);
    if (g_hold_len + in_len > sizeof(g_hold_buf))
    {
        pthread_mutex_unlock(&g_hold_mutex);
        fprintf(stderr, "uucp_fasttrack: RX hold overflow, disabling\n");
        uft_flush_hold_locked(conn);
        uft_reset_session_state();
        return false;
    }
    memcpy(g_hold_buf + g_hold_len, in, in_len);
    g_hold_len += in_len;
    pthread_mutex_unlock(&g_hold_mutex);
    g_stat_rx_held_bytes += in_len;
    return true;
}

bool uft_consume_hf_to_uucico(rhizo_conn *conn, const uint8_t *in, size_t in_len)
{
    if (!conn || !in || in_len == 0)
        return false;

    if (!g_enabled || !g_hold_incoming || g_state == UFT_STATE_OFF || g_state == UFT_STATE_DONE)
        return false;

    /* Consume bytes while holding; strip capability marker from the stream. */
    for (size_t i = 0; i < in_len; i++)
    {
        uint8_t b = in[i];

        /* Streaming marker matcher (also handles the case where marker bytes are split across recv()). */
        if (g_marker_match > 0)
        {
            if (b == UFT_MARKER[g_marker_match])
            {
                g_marker_match++;
                if (g_marker_match == UFT_MARKER_LEN)
                {
                    g_marker_match = 0;
                    g_peer_fasttrack = true;
                    g_stat_marker_rx++;
                    uft_probe_request_activate();
                }
                continue; // marker bytes are swallowed
            }

            /* Mismatch: the prefix we swallowed was not a marker; append it back to hold. */
            (void)uft_hold_append(conn, UFT_MARKER, g_marker_match);
            g_marker_match = 0;
            /* fallthrough to handle current byte */
        }

        if (b == UFT_MARKER[0])
        {
            g_marker_match = 1;
            continue; // swallow (for now)
        }

        if (g_state == UFT_STATE_PROBE && b == UFT_DLE && !g_peer_fasttrack)
        {
            /* Peer is sending normal UUCP handshake; fast-track is not active on the other end. */
            uft_probe_request_fallback("peer sent DLE");
        }

        (void)uft_hold_append(conn, &b, 1);
    }

    return true;
}

