/* Included by the staged USB driver after its SIE helpers. EP3 OUT is shared
 * with the optional CDC build, which this board target disables. */
#include "usb_pcm.h"

#define PLAYBACK_EP 3u
#define PLAYBACK_IF 3u
static uint8_t playback_data[USB_PCM_PACKET + 4] __attribute__((aligned(4)));
static ZiviUsbPcm playback_pcm;
static volatile uint8_t playback_alt;
static volatile unsigned playback_gain = 64;

static void playback_stream(unsigned alt)
{
    pcm_reset(&playback_pcm);
    playback_alt = (uint8_t)alt;
    sie_wr(S_INDEX, PLAYBACK_EP);
    if (alt) {
        fm1_usb_ep_rxbuf(PLAYBACK_EP, playback_data);
        sie_wr(S_RXMAXP, USB_PCM_PACKET / 8);
        sie_wr(S_RXCSR1, 0x10);
        sie_wr(S_RXCSR2, 0x40);  /* ISO */
    } else {
        sie_wr(S_RXCSR1, 0x10);
        sie_wr(S_RXCSR2, 0);
    }
    sie_wr(S_INDEX, 0);
}

static void playback_receive(void)
{
    if (!playback_alt || usb.suspended) return;
    sie_wr(S_INDEX, PLAYBACK_EP);
    unsigned status = sie_rd(S_RXCSR1);
    if (status & 1u) {
        unsigned bytes = sie_rd(S_RXCOUNT1) | sie_rd(S_RXCOUNT2) << 8;
        fm1_usb_rx_sync();
        pcm_receive(&playback_pcm, playback_data, bytes);
        sie_wr(S_RXCSR1, 0);
    }
}
