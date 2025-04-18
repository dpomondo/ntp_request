#ifndef _NTP_REQUEST_H
#define _NTP_REQUEST_H

/********** defines **********/
#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define MOUNTAIN_STANDARD_OFFSET    (7 * 60 * 60)
#define DAYLIGHT_SAVINGS_OFFSET     (1 * 60 * 60)

/********** structs **********/
typedef struct _dns_obj DNS_t;

/********** prototypes **********/

// make the magic happen
extern int set_rtc_using_ntp_request_blocking(void);

//called by alarm set when dns request is sent
static int64_t dns_failed_callback(alarm_id_t id, void *arg);

// make the NTP request via an alarm, allow for re-tries
static int64_t ntp_request_via_alarm(alarm_id_t id, void *arg);

// called when dns server sends a response
static void dns_request_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);

// called when receiving a udp packet; set by udp_recv
static void udp_received_func(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

// utilities to change tm (from time.h) to datetime (from pico/time.h)
struct tm local_datetime_to_tm(datetime_t* t);
datetime_t local_tm_to_datetime(struct tm* t);
int approx_epoch(datetime_t * t_ptr);

#endif // !_NTP_REQUEST_H
