/*
 * UUCP fast-track (uucpd):
 * - Intercepts the pre-protocol UUCP handshake framed with DLE (0x10): Shere/S/ROK/P/U.
 * - Generates the minimum set of peer commands locally so uucico can enter protocol 'y'
 *   without sending the handshake over the HF byte stream.
 *
 * IMPORTANT:
 * - This requires BOTH ends to run uucpd with -F enabled.
 * - Incompatible with login-prompt chat flows (uucpd -l).
 */

#include "uucp_fasttrack.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "circular_buffer.h"

#define UFT_DLE 0x10

/* Hold incoming bytes until uucico has switched from zget_uucp_cmd() to protocol start. */
#define UFT_HOLD_MAX (64 * 1024)

typedef enum {
    UFT_STATE_OFF = 0,
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

/* Session stats (for field logs). */
static uint64_t g_stat_tx_swallowed_bytes = 0;
static uint64_t g_stat_rx_held_bytes = 0;
static uint64_t g_stat_hold_flushed_bytes = 0;
static uint64_t g_stat_injected_cmd_bytes = 0;
static unsigned g_stat_injected_cmds = 0;
static unsigned g_stat_cmds_swallowed = 0;

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

    g_stat_tx_swallowed_bytes = 0;
    g_stat_rx_held_bytes = 0;
    g_stat_hold_flushed_bytes = 0;
    g_stat_injected_cmd_bytes = 0;
    g_stat_injected_cmds = 0;
    g_stat_cmds_swallowed = 0;
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
            "uucp_fasttrack: %s (role=%s, state=%d) swallowed=%" PRIu64 "B cmds=%u "
            "held=%" PRIu64 "B flushed=%" PRIu64 "B injected_cmds=%u injected=%" PRIu64 "B\n",
            tag ? tag : "stats",
            role,
            (int)g_state,
            g_stat_tx_swallowed_bytes,
            g_stat_cmds_swallowed,
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
}

void uft_on_disconnected(void)
{
    if (g_state != UFT_STATE_OFF)
        uft_log_stats("disconnected");
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
            fprintf(stderr, "uucp_fasttrack: disabled (remote system unknown)\n");
            uft_reset_session_state();
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
                g_stat_tx_swallowed_bytes++;
                continue; // swallow DLE
            }

            if (out_len < out_cap)
                out[out_len++] = b;
            continue;
        }

        /* In DLE command payload. */
        g_stat_tx_swallowed_bytes++;

        if (b == 0 || b == '\r' || b == '\n')
        {
            g_tx_cmd[g_tx_cmd_len] = 0;
            g_tx_in_cmd = false;
            g_stat_cmds_swallowed++;

            if (g_role == UFT_ROLE_MASTER)
                uft_handle_master_cmd(conn, g_tx_cmd);
            else if (g_role == UFT_ROLE_SLAVE)
                uft_handle_slave_cmd(conn, g_tx_cmd);

            /* If we just switched to DONE, stop intercepting and pass-through remaining bytes. */
            if (g_state == UFT_STATE_DONE)
            {
                size_t remaining = in_len - (i + 1);
                size_t cap_remaining = out_cap - out_len;
                if (remaining > cap_remaining)
                    remaining = cap_remaining;
                if (remaining > 0)
                {
                    memcpy(out + out_len, in + i + 1, remaining);
                    out_len += remaining;
                }
                return out_len;
            }

            continue; // swallow terminator
        }

        if (g_tx_cmd_len + 1 < sizeof(g_tx_cmd))
        {
            g_tx_cmd[g_tx_cmd_len++] = (char)b;
        }
        else
        {
            fprintf(stderr, "uucp_fasttrack: command overflow, disabling\n");
            uft_log_stats("overflow");
            uft_reset_session_state();
            if (out_len < out_cap)
                out[out_len++] = b;
            g_tx_in_cmd = false;
        }
    }

    return out_len;
}

bool uft_consume_hf_to_uucico(rhizo_conn *conn, const uint8_t *in, size_t in_len)
{
    if (!conn || !in || in_len == 0)
        return false;

    if (!g_enabled || !g_hold_incoming || g_state == UFT_STATE_OFF || g_state == UFT_STATE_DONE)
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

