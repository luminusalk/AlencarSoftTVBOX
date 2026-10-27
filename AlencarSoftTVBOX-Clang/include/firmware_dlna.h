#ifndef ALENCAR_FIRMWARE_DLNA_H
#define ALENCAR_FIRMWARE_DLNA_H

/* Uses the receiver's own DLNA renderer while its firmware owns GXAV. */
int firmware_dlna_start(const char *source, const char *title);
void firmware_dlna_stop(void);
int firmware_dlna_active(void);
/* True when the local MPEG-TS consumer closed its HTTP stream unexpectedly. */
int firmware_dlna_stream_disconnected(void);
int firmware_dlna_stream_started(void);
/* True when our HTTP server has delivered bytes to the renderer recently. */
int firmware_dlna_stream_progress_recent(int seconds);
/* 1 = PLAYING, 0 = stopped/no media, -1 = unavailable/transitioning. */
int firmware_dlna_transport_playing(void);

#endif
