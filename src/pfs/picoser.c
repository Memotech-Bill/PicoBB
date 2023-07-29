/* picoser.c - UART driver for Pico */

#include <string.h>
#include <pico/critical_section.h>
#include <hardware/uart.h>
#include <hardware/structs/uart.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <picoser.h>

#define NDATA   512     // Length of serial receive buffer (must be a power of 2)

typedef struct
    {
    uart_inst_t         *uart;
    critical_section_t  ucs;
    char                data[NDATA];
    int                 rptr;
    int                 wptr;
    } UDATA;

UDATA udata[NUM_UARTS];

static void uart_input (UDATA *pud)
    {
    critical_section_enter_blocking (&pud->ucs);
    int wend = ( pud->rptr - 1 ) & ( NDATA - 1 );
    while ((pud->wptr != wend) && (uart_is_readable (pud->uart)))
        {
        pud->data[pud->wptr] = uart_getc (pud->uart);
        pud->wptr = (++pud->wptr) & ( NDATA - 1 );
        }
    if ( pud->wptr == wend )
        {
        hw_clear_bits (&((uart_hw_t *)pud->uart)->cr, UART_UARTCR_RTS_BITS);
        hw_clear_bits (&((uart_hw_t *)pud->uart)->imsc, UART_UARTIMSC_RXIM_BITS);
        }
    critical_section_exit (&pud->ucs);
    }

static void irq_uart0 (void)
    {
    uart_input (&udata[0]);
    }

static void irq_uart1 (void)
    {
    uart_input (&udata[1]);
    }

int uread (char *ptr, int size, int nmemb, int uid)
    {
    UDATA *pud = &udata[uid];
    int nread = 0;
    uart_input (pud);
    if ( size == 1 )
        {
        while ((nmemb > 0) && (pud->rptr != pud->wptr))
            {
            *ptr = pud->data[pud->rptr];
            pud->rptr = (++pud->rptr) & ( NDATA - 1 );
            ++ptr;
            ++nread;
            --nmemb;
            if ( pud->rptr == pud->wptr ) uart_input (pud);
            }
        }
    else if ( size < NDATA )
        {
        while (nmemb > 0)
            {
            if (((pud->wptr - pud->rptr) & ( NDATA - 1 )) >= size )
                {
                for (int i = 0; i < size; ++i)
                    {
                    *ptr = pud->data[pud->rptr];
                    pud->rptr = (++pud->rptr) & ( NDATA - 1 );
                    ++ptr;
                    }
                ++nread;
                --nmemb;
                uart_input (pud);
                }
            else
                {
                break;
                }
            }
        }
    critical_section_enter_blocking (&pud->ucs);
    hw_set_bits (&((uart_hw_t *)pud->uart)->cr, UART_UARTCR_RTS_BITS);
    hw_set_bits (&((uart_hw_t *)pud->uart)->imsc, UART_UARTIMSC_RXIM_BITS);
    critical_section_exit (&pud->ucs);
    return nread;
    }

int uwrite (const char *ptr, int size, int nmemb, int uid)
    {
    UDATA *pud = &udata[uid];
    uart_write_blocking (pud->uart, ptr, size * nmemb);
    return nmemb;
    }

static bool uart_pin_valid (int uid, int func, int pin)
    {
    if (( pin < 0 ) || ( pin > 29 )) return false;
    pin -= func;
    if ( uid == 0 )
        {
        if (( pin == 0 ) || ( pin == 12 ) || ( pin == 16 ) || ( pin == 28 )) return true;
        }
    else if ( uid == 1 )
        {
        if (( pin == 4 ) || ( pin == 8 ) || ( pin == 20 ) || ( pin == 24 )) return true;
        }
    return false;
    }

bool uopen (int uid, SERIAL_CONFIG *sc)
    {
    if (( uid < 0 ) || ( uid > 1 )) return false;
    UDATA *pud = &udata[uid];
    pud->uart = uart_get_instance (uid);
    pud->rptr = 0;
    pud->wptr = 0;
    critical_section_init (&pud->ucs);
    if ( uart_init (pud->uart, sc->baud) == 0 ) return false;
    if ( sc->tx >= 0 )
        {
        if ( ! uart_pin_valid (uid, 0, sc->tx) ) return false;
        gpio_set_function (sc->tx, GPIO_FUNC_UART);
        }
    if ( sc->rx >= 0 )
        {
        if ( ! uart_pin_valid (uid, 1, sc->rx) ) return false;
        gpio_set_function (sc->rx, GPIO_FUNC_UART);
        }
    if ( sc->cts >= 0 )
        {
        if ( ! uart_pin_valid (uid, 2, sc->cts) ) return false;
        gpio_set_function (sc->cts, GPIO_FUNC_UART);
        uart_set_hw_flow (pud->uart, true, false);
        }
    else
        {
        uart_set_hw_flow (pud->uart, false, false);
        }
    if ( sc->rts >= 0 )
        {
        if ( ! uart_pin_valid (uid, 3, sc->rts) ) return false;
        gpio_set_function (sc->rts, GPIO_FUNC_UART);
        }
    if ( uid == 0 ) irq_set_exclusive_handler(UART0_IRQ, irq_uart0);
    else irq_set_exclusive_handler(UART1_IRQ, irq_uart1);
    hw_clear_bits (&((uart_hw_t *)pud->uart)->ifls, UART_UARTIFLS_RXIFLSEL_BITS);
    hw_set_bits (&((uart_hw_t *)pud->uart)->cr, UART_UARTCR_RTS_BITS);
    hw_set_bits (&((uart_hw_t *)pud->uart)->imsc, UART_UARTIMSC_RXIM_BITS);
    if (( sc->data < 5 ) || ( sc->data > 8 )) return false;
    if (( sc->stop < 1 ) || ( sc->stop > 2 )) return false;
    int iPar;
    if (( sc->parity != UART_PARITY_NONE ) && ( sc->parity != UART_PARITY_EVEN )
        && ( sc->parity != UART_PARITY_ODD )) return false;
    uart_set_format (pud->uart, sc->data, sc->stop, sc->parity);
    uart_set_irq_enables (pud->uart, true, false);
    }

void uclose (int uid)
    {
    if (( uid < 0 ) || ( uid > 1 )) return;
    UDATA *pud = &udata[uid];
    uart_deinit (pud->uart);
    critical_section_deinit (&pud->ucs);
    }

bool parse_sconfig (const char *ps, SERIAL_CONFIG *sc)
    {
    static const char *psPar[] = { "baud", "parity", "data", "stop", "tx", "rx", "cts", "rts"};
    int iPar = 0;
    memset (sc, -1, sizeof (SERIAL_CONFIG));
#if DEBUG
    dbgmsg ("parse_config (%s)\r\n", ps);
#endif
    if ( *ps == '.' ) ++ps;
    while (*ps)
        {
        while (*ps == ' ') ++ps;
        if (( *ps == '\0' ) || ( *ps == '.' )) break;
#if DEBUG
        dbgmsg ("ps = %s\r\n", ps);
#endif
        const char *ps1 = ps;
        while (true)
            {
            if (*ps == '=')
                {
                int n = ps - ps1;
#if DEBUG
                dbgmsg ("n = %d\r\n", n);
#endif
                iPar = -1;
                for (int i = 0; i < sizeof (psPar) / sizeof (psPar[0]); ++i)
                    {
                    if (( n == strlen (psPar[i]) ) && ( ! strncasecmp (ps1, psPar[i], n) ))
                        {
                        iPar = i;
                        break;
                        }
                    }
#if DEBUG
                dbgmsg ("keyword = %d\r\n", iPar);
#endif
                if ( iPar < 0 ) return false;
                ps1 = ps + 1;
                }
            else if ((*ps == '\0' ) || (*ps == ' ') || (*ps == '.'))
                {
#if DEBUG
                dbgmsg ("parameter = %d\r\n", iPar);
#endif
                if ( iPar == 1 )
                    {
                    if (( *ps1 == 'N' ) || ( *ps1 == 'n' )) sc->parity = UART_PARITY_NONE;
                    else if (( *ps1 == 'E' ) || ( *ps1 == 'e' )) sc->parity = UART_PARITY_EVEN;
                    else if (( *ps1 == 'O' ) || ( *ps1 == 'o' )) sc->parity = UART_PARITY_ODD;
                    else return false;
                    }
                else
                    {
                    int n = 0;
                    while (ps1 < ps)
                        {
                        if ((*ps1 < '0') || (*ps1 > '9')) return false;
                        n = 10 * n + *ps1 - '0';
                        ++ps1;
                        }
                    ((int *)(&sc->baud))[iPar] = n;
#if DEBUG
                    dbgmsg ("iPar = %d, n = %d\r\n", iPar, n);
#endif
                    }
                ++iPar;
                ++ps;
                break;
                }
            ++ps;
            }
        }
    if ( sc->data < 0 ) sc->data = 8;
    if ( sc->stop < 0 ) sc->stop = 1;
    if ( sc->parity < 0 ) sc->parity = UART_PARITY_NONE;
#if DEBUG
    dbgmsg ("sconfig (%d, %d, %d, %d, %d, %d, %d, %d)\r\n", sc->baud, sc->parity, sc->data, sc->stop,
        sc->tx, sc->rx, sc->cts, sc->rts);
#endif
    return true;
    }
