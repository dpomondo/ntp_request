#include "pico/stdlib.h"
#include <time.h>
#include "hardware/rtc.h"
#include "pico/time.h"
#include "pico/util/datetime.h"

#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "ntp_request.h"

typedef struct _dns_obj
{
    ip_addr_t       dns_host_addr;
    struct udp_pcb  *packet_control_block;
    alarm_id_t      dns_resend_alarm;
    alarm_id_t      ntp_resend_alarm;
    uint            resend_time_in_ms;
    bool            ntp_request_sent;
    bool            rtc_clock_set;
    datetime_t      time_returned;
} DNS_t;

int set_rtc_using_ntp_request_blocking(void)
{
    // set up data structures and such
    // calloc = allocate memory and fill it with zeros
    DNS_t *attempt = (DNS_t*)calloc(1, sizeof(DNS_t));
    if (!attempt)
    {
        printf("Failed to allocate data structure\r\n");
        //return 1;
    }
    attempt->resend_time_in_ms = 10 * 1000;
    attempt->ntp_request_sent = false;
    attempt->rtc_clock_set = false;
    attempt->packet_control_block = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!attempt->packet_control_block)
    {
        printf("***** failed to create pcb *****\r\n");
        free(attempt);
        //return 1;
    } // end if
    udp_recv(attempt->packet_control_block,
            udp_received_func,
            attempt);

    // get the DNS
    attempt->dns_resend_alarm = add_alarm_in_ms(attempt->resend_time_in_ms, 
            dns_failed_callback, 
            attempt, 
            true);
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(NTP_SERVER, &attempt->dns_host_addr, dns_request_callback, attempt);
    cyw43_arch_lwip_end();

    switch (err) 
    {
        case ERR_OK:
            printf("\tCached result: %s\r\n", attempt->dns_host_addr);
            break;
        case ERR_INPROGRESS:
            printf("\tRequest queued, awaiting callback\r\n");
            break;
        case ERR_ARG:
            printf("\tDNS request failed, errors present?!?\r\n");
            break;
        default:
            printf("\tDNS request has done something very strange, should not be here\r\n");
            break;
    } // end switch...err
    while ( attempt->rtc_clock_set == false )
    {
        tight_loop_contents();
    }
    rtc_init();
    rtc_set_datetime(&attempt->time_returned);
    // clk_sys is >2000x faster than clk_rtc, so datetime is not updated immediately when rtc_get_datetime() is called.
    // tbe delay is up to 3 RTC clock cycles (which is 64us with the default clock settings)
    busy_wait_us(64);
    // if you love something you've allocated, set it free!
    cancel_alarm(attempt->dns_resend_alarm);
    cancel_alarm(attempt->ntp_resend_alarm);
    free(attempt);
    return EXIT_SUCCESS;
} // end set_rtc_using_ntp_request_blocking

static int64_t ntp_request_via_alarm(alarm_id_t id, void *arg)
{
    static uint num_calls = 0;
    DNS_t* attempt_ptr = (DNS_t*)arg;
    printf("Sending request to %s\trequest number: %d\r\n", 
           ipaddr_ntoa(&attempt_ptr->dns_host_addr),
           ++num_calls);
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *) p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(attempt_ptr->packet_control_block, 
            p, 
            &attempt_ptr->dns_host_addr, 
            NTP_PORT);
    pbuf_free(p);
    attempt_ptr->ntp_request_sent = true;
    cyw43_arch_lwip_end();

    attempt_ptr->ntp_resend_alarm = id;
    return attempt_ptr->resend_time_in_ms * 1000;
} // end ntp_request_via_alarm

static int64_t dns_failed_callback(alarm_id_t id, void *arg)
{
    static uint num_calls = 0;
    printf("hello from inside the <dns_failed_callback> func\twe've been here %d time[s]\r\n", ++num_calls);
    DNS_t* attempt_ptr = (DNS_t*)arg;
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(NTP_SERVER, &attempt_ptr->dns_host_addr, dns_request_callback, attempt_ptr);
    cyw43_arch_lwip_end();

    switch (err) 
    {
        case ERR_OK:
            printf("\tCached result: %s\r\n", attempt_ptr->dns_host_addr);
            break;
        case ERR_INPROGRESS:
            printf("\tRequest queued, awaiting callback\r\n");
            break;
        case ERR_ARG:
            printf("\tDNS request failed, errors present?!?\r\n");
            break;
        default:
            printf("\tDNS request has done something very strange, should not be here\r\n");
            break;
    } // end switch...err

    // reset alarm
    attempt_ptr->dns_resend_alarm = id;
    return attempt_ptr->resend_time_in_ms * 1000; // expects microseconds
} // end dns_failed_callback

static void dns_request_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    printf("Got a response to DNS request, hello from inside callback!\r\n");
    DNS_t* attempt_ptr = (DNS_t*)callback_arg;
    if (ipaddr)
    {
        attempt_ptr->dns_host_addr = *ipaddr;
        printf("ntp address %s for %s\r\n", 
                ipaddr_ntoa(&attempt_ptr->dns_host_addr),
                *name);
        cancel_alarm(attempt_ptr->dns_resend_alarm);

        if (attempt_ptr->rtc_clock_set == false)
        {
            // printf("Sending request to %s\r\n", ipaddr_ntoa(&attempt_ptr->dns_host_addr));
            // cyw43_arch_lwip_begin();
            // struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
            // uint8_t *req = (uint8_t *) p->payload;
            // memset(req, 0, NTP_MSG_LEN);
            // req[0] = 0x1b;
            // udp_sendto(attempt_ptr->packet_control_block, 
            //         p, 
            //         &attempt_ptr->dns_host_addr, 
            //         NTP_PORT);
            // pbuf_free(p);
            // attempt_ptr->ntp_request_sent = true;
            // cyw43_arch_lwip_end();
            attempt_ptr->ntp_resend_alarm = add_alarm_in_ms(1, 
                    ntp_request_via_alarm, 
                    attempt_ptr, 
                    true);
        }
    } 
    else 
    {
        printf("DNS request went south...\r\n");    
    } 
} // end dns_request_callback

static void udp_received_func(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    printf("something happened, UDP packet returned\r\n");

    DNS_t* attempt_ptr = (DNS_t*)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    if (ip_addr_cmp(addr, &attempt_ptr->dns_host_addr) &&
            port == NTP_PORT &&
            p->tot_len == NTP_MSG_LEN) 
    {
        printf("all the address & port stuff is proper...\r\n");
    }
    if (mode == 0x4 && stratum != 0)
    {
        printf(" and mode & stratum are good.\r\n");
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);

        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        // account for mountain standard time
        time_t epoch = seconds_since_1970 - MOUNTAIN_STANDARD_OFFSET + DAYLIGHT_SAVINGS_OFFSET; 
        struct tm *utc = localtime(&epoch);

        datetime_t t = tm_to_datetime(utc);
        cancel_alarm(attempt_ptr->ntp_resend_alarm);
        attempt_ptr->rtc_clock_set = true;
        attempt_ptr->time_returned = t;

        char datetime_buf[256];
        char *datetime_str = &datetime_buf[0];

        datetime_to_str(datetime_str, sizeof(datetime_buf), &t);
        printf("time returned from internet was:\r\n%s      ", datetime_str);
    }
    pbuf_free(p);
} // end udp_received_func

/* Convert a pointer to tm (from stdlib time.h) to 
*  datetime_t (from pico/util/datetime.h) in order 
*  to feed the real time clock
*  */
datetime_t tm_to_datetime(struct tm* t)
{
    datetime_t dt = {
        .year  = t->tm_year + 1900,
        .month = t->tm_mon + 1,
        .day   = t->tm_mday,
        .dotw  = t->tm_wday, 
        .hour  = t->tm_hour,
        .min   = t->tm_min,
        .sec   = t->tm_sec
    };
    return dt;
} // end tm_to_datetime_ptr

/* Convert a datetime_t (from pico/util/datetime.h) 
*  pointer into a tm struct (from stdlib time.h)
*  */
struct tm datetime_to_tm(datetime_t* t)
{
    struct tm reverse_time = {
            .tm_year = t->year - 1900,
            .tm_mon =  t->month - 1,
            .tm_mday = t->day,   
            .tm_wday = t->dotw,
            .tm_hour = t->hour,
            .tm_min =  t->min,
            .tm_sec =  t->sec,
            .tm_isdst = 1
        };
    return reverse_time;
} // end tm_to_datetime_ptr

/* Convert a pointer to a datetime_t struct back into (approx)
*  the current unix epoch, a number useful for timestamps!
*  */
int approx_epoch(datetime_t * t_ptr)
{
    struct tm reverse_time = datetime_to_tm(t_ptr);
    return (int)mktime(&reverse_time) + MOUNTAIN_STANDARD_OFFSET - DAYLIGHT_SAVINGS_OFFSET;
}
