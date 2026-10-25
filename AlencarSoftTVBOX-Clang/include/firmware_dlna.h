#ifndef ALENCAR_FIRMWARE_DLNA_H
#define ALENCAR_FIRMWARE_DLNA_H

/* Uses the receiver's own DLNA renderer while its firmware owns GXAV. */
int firmware_dlna_start(const char *source, const char *title);
void firmware_dlna_stop(void);
int firmware_dlna_active(void);

#endif
