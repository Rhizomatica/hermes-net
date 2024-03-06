// C++ example for interacting with the sBitx radio daemon

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "radio_cmds.h"
#include "sbitx_io.h"
#include "shm_utils.h"

int main(int argc, char *argv[])
{
    // ================== INITIALIZATION ==================
    // This should be called just one, in the beginnig of any software
    controller_conn *connector = NULL;

    if (shm_is_created(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn)) == false)
    {
        fprintf(stderr, "Connector SHM not created. Is sbitx_controller running?\n");
        return EXIT_FAILURE;
    }

    connector = (controller_conn *) shm_attach(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));
    // ================== INITIALIZATION ENDS =============



    // Variables used to place the radio command and for its response
    uint8_t srv_cmd[5];
    uint8_t response[5];
    bool cmd_resp;

    memset(srv_cmd, 0, 5);
    memset(response, 0, 5);

    // ================== KEY ON ==================
    srv_cmd[4] = CMD_PTT_ON;

    cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (cmd_resp == false)
        fprintf(stderr, "ERROR\n");
    // ================== KEY ON ENDS =============

    sleep(5);

    // ================== KEY OFF =================
    srv_cmd[4] = CMD_PTT_OFF;

    cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (cmd_resp == false)
        fprintf(stderr, "ERROR\n");
    // ================== KEY OFF ENDS ============

    sleep(5);

    // =============== GET T/R INFORMATION ========
    srv_cmd[4] = CMD_GET_TXRX_STATUS;

    cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (cmd_resp == false)
        fprintf(stderr, "ERROR\n");

    if (response[0] == CMD_RESP_GET_TXRX_INTX)
        printf("TX\n");

    if (response[0] == CMD_RESP_GET_TXRX_INRX)
        printf("RX\n");
    // =============== GET T/R INFORMATION ENDS ===

    sleep(5);

    // ================== GET FREQUENCY ===========
    srv_cmd[4] = CMD_GET_FREQ;

    cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (cmd_resp == false)
        fprintf(stderr, "ERROR\n");

    uint32_t freq;
    memcpy (&freq, response+1, 4);
    printf("%u\n", freq);
    // ================== GET FREQUENCY ENDS ======

    sleep(5);

    // ================== GET FWD =================
    srv_cmd[4] = CMD_GET_FWD;

    cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (cmd_resp == false)
        fprintf(stderr, "ERROR\n");

    uint16_t fwd;
    memcpy (&fwd, response+1, 2);
    printf("%hu\n", fwd); // Power in Watt * 10 (eg. 213 == 21.3W)
    // ================== GET FWD END =============

    sleep(5);

    // ================== GET SWR =================
    srv_cmd[4] = CMD_GET_REF;

    cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (cmd_resp == false)
        fprintf(stderr, "ERROR\n");

    uint16_t swr;
    memcpy (&swr, response+1, 2);
    printf("%hu\n", swr); // VSWR * 10 (eg. 14 == 1.4:1)
    // ================== GET SWR END =============


    return EXIT_SUCCESS;
}
