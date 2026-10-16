#include "GBA2/gba2.h"

int Port;

void mGcgba_Init(void) {
}

void mGcgba_InitVar(void) {
}

void mGcgba_EndComm(void) {
}

int mGcgba_ConnectEnabled(void) {
    return FALSE;
}

int mGcgba_Boot(u8* data_buf, size_t buf_size) {
    return GBA2_GBA_STATE_ERROR;
}

int mGcgba_IsEditor(void) {
    return FALSE;
}

int mGcgba_IsIsland(void) {
    return FALSE;
}

int mGcgba_Recv(u8* recv_bufp, size_t recv_buf_size) {
    return GBA2_GBA_STATE_ERROR;
}

int mGcgba_Read(u8* read_buf, size_t read_buf_size, u8 prog_type) {
    return GBA2_GBA_STATE_ERROR;
}

int mGcgba_CheckRecv(u8* recv_bufp, size_t recv_buf_size) {
    return GBA2_GBA_STATE_ERROR;
}

int mGcgba_Send(u8* send_bufp, size_t send_buf_size) {
    return GBA2_GBA_STATE_ERROR;
}

int mGcgba_send_eAppri(u8* eappli_p, size_t eappli_size) {
    return GBA2_EAPPLI_FAILURE_NO_GBA;
}
