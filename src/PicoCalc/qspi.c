/* qspi.c - Interface to PicoCalc PSRAM using QSPI interface */

#ifndef PICO_PSRAM_SIO0_PIN
#define PICO_PSRAM_SIO0_PIN     2
#endif
#ifndef PICO_PSRAM_SIO1_PIN
#define PICO_PSRAM_SIO1_PIN     3
#endif
#ifndef PICO_PSRAM_SIO2_PIN
#define PICO_PSRAM_SIO2_PIN     4
#endif
#ifndef PICO_PSRAM_SIO3_PIN
#define PICO_PSRAM_SIO3_PIN     5
#endif
#ifndef PICO_PSRAM_CS_PIN
#define PICO_PSRAM_CS_PIN       20
#endif
#ifndef PICO_PSRAM_CLK_PIN
#define PICO_PSRAM_CLK_PIN      21
#endif

#define USE_DMA     1

#include <stdbool.h>
#include <stdio.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <pico/time.h>
#include "qspi.h"
#include "qspi.pio.h"

uint qspi_dma = 0;
PIO qspi_pio = NULL;
uint qspi_sm = 0;
uint qspi_oset = 0;
const pio_program_t *qspi_pgm = NULL;
bool qspi_qcfg = false;

bool pio_claim_free_sm_for_program (const pio_program_t *program, PIO *pio, uint *sm)
    {
    for (int i = 0; i < 2; ++i)
        {
        *pio = pio_get_instance (i);
        if (pio_can_add_program (*pio, program))
            {
            int nsm = pio_claim_unused_sm (*pio, false);
            if (nsm >= 0)
                {
                *sm = nsm;
                return true;
                }
            }
        }
    return false;
    }

void qspi_wait (void)
    {
    while (! pio_interrupt_get (qspi_pio, 7))
        {
        tight_loop_contents ();
        }
#if USE_DMA
    if (qspi_dma) dma_channel_wait_for_finish_blocking (qspi_dma);
#endif
    }

void qspi_free (void)
    {
    qspi_wait ();
    pio_sm_set_enabled (qspi_pio, qspi_sm, false);
    pio_remove_program_and_unclaim_sm (qspi_pgm, qspi_pio, qspi_sm, qspi_oset);
    qspi_pio = NULL;
#if USE_DMA
    if (qspi_dma) dma_channel_unclaim (qspi_dma);
    qspi_dma = 0;
#endif
    }

bool qspi_cfg_qread (void)
    {
    if (! qspi_qcfg) qspi_qmode ();
    if (! pio_claim_free_sm_for_program (&qread_program, &qspi_pio, &qspi_sm)) return false;
    if (! pio_sm_is_tx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_drain_tx_fifo (qspi_pio, qspi_sm);
    pio_sm_set_pins_with_mask (qspi_pio, qspi_sm, 1 << PICO_PSRAM_CS_PIN, 3 << PICO_PSRAM_CS_PIN);
    pio_sm_set_consecutive_pindirs (qspi_pio, qspi_sm, PICO_PSRAM_CS_PIN, 2, true);
    while (! pio_sm_is_rx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_get (qspi_pio, qspi_sm);
    qspi_oset = pio_add_program (qspi_pio, &qread_program);
    qspi_pgm = &qread_program;
#if USE_DMA
    qspi_dma = dma_claim_unused_channel (true);
#endif
    
    pio_sm_config cfg = qread_program_get_default_config (qspi_oset);
    sm_config_set_in_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_out_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_set_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_sideset_pin_base (&cfg, PICO_PSRAM_CS_PIN);
    
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO0_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO1_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO2_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO3_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CS_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CLK_PIN);
    hw_set_bits (&qspi_pio->input_sync_bypass, 1u << PICO_PSRAM_SIO0_PIN);
    hw_set_bits (&qspi_pio->input_sync_bypass, 1u << PICO_PSRAM_SIO1_PIN);
    hw_set_bits (&qspi_pio->input_sync_bypass, 1u << PICO_PSRAM_SIO2_PIN);
    hw_set_bits (&qspi_pio->input_sync_bypass, 1u << PICO_PSRAM_SIO3_PIN);
    pio_sm_init (qspi_pio, qspi_sm, qspi_oset, &cfg);
    pio_sm_set_enabled (qspi_pio, qspi_sm, true);
    }

void qspi_qread (uint addr, uint len, uint8_t *buffer)
    {
#if USE_DMA
    uint dreq = pio_get_dreq (qspi_pio, qspi_sm, false);
    dma_channel_config_t cfg = dma_channel_get_default_config (qspi_dma);
    channel_config_set_transfer_data_size (&cfg, DMA_SIZE_8);
    channel_config_set_read_increment (&cfg, false);
    channel_config_set_write_increment (&cfg, true);
    channel_config_set_dreq (&cfg, dreq);
    dma_channel_configure (qspi_dma, &cfg, buffer, &qspi_pio->rxf[qspi_sm], len, true);
#endif
    
    pio_interrupt_clear (qspi_pio, 7);
    pio_sm_put_blocking (qspi_pio, qspi_sm, 0xEB000000 | addr);
    pio_sm_put_blocking (qspi_pio, qspi_sm, len - 1);

#if ! USE_DMA
    while (len > 0)
        {
        *buffer = (uint8_t) pio_sm_get_blocking (qspi_pio, qspi_sm);
        ++buffer;
        --len;
        }
#endif
    }

bool qspi_cfg_qwrite (void)
    {
    if (! qspi_qcfg) qspi_qmode ();
    if (! pio_claim_free_sm_for_program (&qwrite_program, &qspi_pio, &qspi_sm)) return false;
    if (! pio_sm_is_tx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_drain_tx_fifo (qspi_pio, qspi_sm);
    pio_sm_set_pins_with_mask (qspi_pio, qspi_sm, 1 << PICO_PSRAM_CS_PIN, 3 << PICO_PSRAM_CS_PIN);
    pio_sm_set_consecutive_pindirs (qspi_pio, qspi_sm, PICO_PSRAM_CS_PIN, 2, true);
    qspi_oset = pio_add_program (qspi_pio, &qwrite_program);
    qspi_pgm = &qwrite_program;
#if USE_DMA
    qspi_dma = dma_claim_unused_channel (true);
#endif
    
    pio_sm_config cfg = qwrite_program_get_default_config (qspi_oset);
    sm_config_set_in_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_out_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_set_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_sideset_pin_base (&cfg, PICO_PSRAM_CS_PIN);
    
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO0_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO1_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO2_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO3_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CS_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CLK_PIN);
    pio_sm_init (qspi_pio, qspi_sm, qspi_oset, &cfg);
    pio_sm_set_enabled (qspi_pio, qspi_sm, true);
    }

void qspi_qwrite (uint addr, uint len, uint8_t *buffer)
    {
#if USE_DMA
    uint dreq = pio_get_dreq (qspi_pio, qspi_sm, true);
    dma_channel_config_t cfg = dma_channel_get_default_config (qspi_dma);
    channel_config_set_transfer_data_size (&cfg, DMA_SIZE_8);
    channel_config_set_read_increment (&cfg, true);
    channel_config_set_write_increment (&cfg, false);
    channel_config_set_dreq (&cfg, dreq);
#endif
    
    pio_interrupt_clear (qspi_pio, 7);
    pio_sm_put_blocking (qspi_pio, qspi_sm, len - 1);
    pio_sm_put_blocking (qspi_pio, qspi_sm, 0x38000000 | addr);
#if USE_DMA
    dma_channel_configure (qspi_dma, &cfg, &qspi_pio->txf[qspi_sm], buffer, len, true);
#else
    while (len > 0)
        {
        pio_sm_put_blocking (qspi_pio, qspi_sm, *buffer);
        ++buffer;
        --len;
        }
    while (! pio_sm_is_tx_fifo_empty (qspi_pio, qspi_sm))
        {
        tight_loop_contents ();
        }
#endif
    }

bool qspi_cfg_scmd (void)
    {
    if (! pio_claim_free_sm_for_program (&scmd_program, &qspi_pio, &qspi_sm)) return false;
    if (! pio_sm_is_tx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_drain_tx_fifo (qspi_pio, qspi_sm);
    pio_sm_set_pins_with_mask (qspi_pio, qspi_sm, 1 << PICO_PSRAM_CS_PIN, 3 << PICO_PSRAM_CS_PIN);
    pio_sm_set_consecutive_pindirs (qspi_pio, qspi_sm, PICO_PSRAM_CS_PIN, 2, true);
    qspi_oset = pio_add_program (qspi_pio, &scmd_program);
    qspi_pgm = &scmd_program;
    
    pio_sm_config cfg = scmd_program_get_default_config (qspi_oset);
    sm_config_set_out_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_set_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_sideset_pin_base (&cfg, PICO_PSRAM_CS_PIN);
    
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO0_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CS_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CLK_PIN);
    hw_set_bits (&qspi_pio->input_sync_bypass, 1u << PICO_PSRAM_SIO1_PIN);
    pio_sm_init (qspi_pio, qspi_sm, qspi_oset, &cfg);
    pio_sm_set_enabled (qspi_pio, qspi_sm, true);
    }

void qspi_cmd (uint8_t data)
    {
    pio_interrupt_clear (qspi_pio, 7);
    pio_sm_put_blocking (qspi_pio, qspi_sm, data);
    while (! pio_interrupt_get (qspi_pio, 7))
        {
        tight_loop_contents ();
        }
    }

void qspi_qmode (void)
    {
    gpio_pull_up (PICO_PSRAM_CS_PIN);
    qspi_cfg_scmd ();
    qspi_cmd (0x35);
    qspi_free ();
    qspi_qcfg = true;
    }

bool qspi_cfg_qcmd (void)
    {
    if (! pio_claim_free_sm_for_program (&qcmd_program, &qspi_pio, &qspi_sm)) return false;
    if (! pio_sm_is_tx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_drain_tx_fifo (qspi_pio, qspi_sm);
    pio_sm_set_pins_with_mask (qspi_pio, qspi_sm, 1 << PICO_PSRAM_CS_PIN, 3 << PICO_PSRAM_CS_PIN);
    pio_sm_set_consecutive_pindirs (qspi_pio, qspi_sm, PICO_PSRAM_CS_PIN, 2, true);
    qspi_oset = pio_add_program (qspi_pio, &qcmd_program);
    qspi_pgm = &qcmd_program;
    
    pio_sm_config cfg = qcmd_program_get_default_config (qspi_oset);
    sm_config_set_in_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_out_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_set_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_sideset_pin_base (&cfg, PICO_PSRAM_CS_PIN);
    
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO0_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO1_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO2_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO3_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CS_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CLK_PIN);
    pio_sm_init (qspi_pio, qspi_sm, qspi_oset, &cfg);
    pio_sm_set_enabled (qspi_pio, qspi_sm, true);
    }

void qspi_smode (void)
    {
    gpio_pull_up (PICO_PSRAM_CS_PIN);
    qspi_cfg_qcmd ();
    qspi_cmd (0xF5);
    qspi_free ();
    qspi_qcfg = false;
    }

bool qspi_cfg_sio (void)
    {
    if (qspi_qcfg) qspi_smode ();
    gpio_init (PICO_PSRAM_CS_PIN);
    gpio_set_dir (PICO_PSRAM_CS_PIN, true);
    gpio_put (PICO_PSRAM_CS_PIN, true);
    if (! pio_claim_free_sm_for_program (&sio_program, &qspi_pio, &qspi_sm)) return false;
    if (! pio_sm_is_tx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_drain_tx_fifo (qspi_pio, qspi_sm);
    pio_sm_set_pins_with_mask (qspi_pio, qspi_sm, 0, 1 << PICO_PSRAM_CLK_PIN);
    pio_sm_set_consecutive_pindirs (qspi_pio, qspi_sm, PICO_PSRAM_CLK_PIN, 1, true);
    while (! pio_sm_is_rx_fifo_empty (qspi_pio, qspi_sm)) pio_sm_get (qspi_pio, qspi_sm);
    qspi_oset = pio_add_program (qspi_pio, &sio_program);
    qspi_pgm = &sio_program;
#if USE_DMA
    qspi_dma = dma_claim_unused_channel (true);
#endif
    
    pio_sm_config cfg = sio_program_get_default_config (qspi_oset);
    sm_config_set_in_pin_base (&cfg, PICO_PSRAM_SIO1_PIN);
    sm_config_set_out_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_set_pin_base (&cfg, PICO_PSRAM_SIO0_PIN);
    sm_config_set_sideset_pin_base (&cfg, PICO_PSRAM_CLK_PIN);
    
    pio_gpio_init (qspi_pio, PICO_PSRAM_SIO0_PIN);
    pio_gpio_init (qspi_pio, PICO_PSRAM_CLK_PIN);
    hw_set_bits (&qspi_pio->input_sync_bypass, 1u << PICO_PSRAM_SIO1_PIN);
    pio_sm_init (qspi_pio, qspi_sm, qspi_oset, &cfg);
    pio_sm_set_enabled (qspi_pio, qspi_sm, true);
    }

void qspi_sread (uint addr, uint len, uint8_t *buffer)
    {
#if USE_DMA
    uint dreq = pio_get_dreq (qspi_pio, qspi_sm, false);
    dma_channel_config_t cfg = dma_channel_get_default_config (qspi_dma);
    channel_config_set_transfer_data_size (&cfg, DMA_SIZE_8);
    channel_config_set_read_increment (&cfg, false);
    channel_config_set_write_increment (&cfg, true);
    channel_config_set_dreq (&cfg, dreq);
#endif

    gpio_put (PICO_PSRAM_CS_PIN, false);
    pio_sm_put_blocking (qspi_pio, qspi_sm, 0x03);
    pio_sm_get_blocking (qspi_pio, qspi_sm);
    pio_sm_put_blocking (qspi_pio, qspi_sm, addr >> 16);
    pio_sm_get_blocking (qspi_pio, qspi_sm);
    pio_sm_put_blocking (qspi_pio, qspi_sm, (addr >> 8) & 0xFF);
    pio_sm_get_blocking (qspi_pio, qspi_sm);
    pio_sm_put_blocking (qspi_pio, qspi_sm, addr & 0xFF);
    pio_sm_get_blocking (qspi_pio, qspi_sm);

#if USE_DMA
    dma_channel_configure (qspi_dma, &cfg, buffer, &qspi_pio->rxf[qspi_sm], len, true);
#endif
    while (len > 0)
        {
        pio_sm_put_blocking (qspi_pio, qspi_sm, 0x00);
#if ! USE_DMA
        *buffer = pio_sm_get_blocking (qspi_pio, qspi_sm);
        ++buffer;
#endif
        --len;
        }
#if USE_DMA
    dma_channel_wait_for_finish_blocking (qspi_dma);
#endif
    gpio_put (PICO_PSRAM_CS_PIN, true);
    }

void qspi_swrite (uint addr, uint len, uint8_t *buffer)
    {
#if USE_DMA
    uint dreq = pio_get_dreq (qspi_pio, qspi_sm, false);
    dma_channel_config_t cfg = dma_channel_get_default_config (qspi_dma);
    channel_config_set_transfer_data_size (&cfg, DMA_SIZE_8);
    channel_config_set_read_increment (&cfg, false);
    channel_config_set_write_increment (&cfg, false);
    channel_config_set_dreq (&cfg, dreq);
#endif

    gpio_put (PICO_PSRAM_CS_PIN, false);
    pio_sm_put_blocking (qspi_pio, qspi_sm, 0x02);
    pio_sm_get_blocking (qspi_pio, qspi_sm);
    pio_sm_put_blocking (qspi_pio, qspi_sm, addr >> 16);
    pio_sm_get_blocking (qspi_pio, qspi_sm);
    pio_sm_put_blocking (qspi_pio, qspi_sm, (addr >> 8) & 0xFF);
    pio_sm_get_blocking (qspi_pio, qspi_sm);
    pio_sm_put_blocking (qspi_pio, qspi_sm, addr & 0xFF);
    pio_sm_get_blocking (qspi_pio, qspi_sm);

#if USE_DMA
    uint32_t dummy;
    dma_channel_configure (qspi_dma, &cfg, &dummy, &qspi_pio->rxf[qspi_sm], len, true);
#endif
    while (len > 0)
        {
        pio_sm_put_blocking (qspi_pio, qspi_sm, *buffer);
        ++buffer;
#if ! USE_DMA
        pio_sm_get_blocking (qspi_pio, qspi_sm);
#endif
        --len;
        }
#if USE_DMA
    dma_channel_wait_for_finish_blocking (qspi_dma);
#endif
    gpio_put (PICO_PSRAM_CS_PIN, true);
    }
