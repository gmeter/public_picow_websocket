
/*
Licence: Have at it.  Code written by Duncan Gray and gifted to the Raspberry Pi community.
https://github.com/gmeter/public_picow_websocket

If you make improvements, share them so that we can all benefit.

*/


#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/init.h"
#include "httpd_ws.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
//#include "fs.h"
#include "httpd_structs.h"
#include "lwip/def.h"
#include "mbedtls//base64.h"

//#include "altcp.h"
#include "lwip/altcp_tcp.h"
#if HTTPD_ENABLE_HTTPS
#include "lwip/altcp_tls.h"
#endif
#ifdef LWIP_HOOK_FILENAME
#include LWIP_HOOK_FILENAME
#endif

#include <string.h> /* memset */
#include <stdlib.h> /* atoi */
#include <stdio.h>
#include <time.h>
#include "lwipopts.h"

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

uint32_t debug_count = 0;

struct connection_info *head = NULL; // This should be a global variable

void lwip_port_init(void) {
    srand(time(NULL));
}

unsigned int lwip_port_rand(void) {
    return rand();
}



uint8_t connectionCounter = 0;
bool callbacks_have_been_setup = false;
uint8_t loopTime = 20; //ms


struct connection_info {
    struct tcp_pcb *pcb;
    char *data2send;
    uint16_t data2send_len;
    uint8_t loopCounter;
    uint8_t channel, number, alive;
    int32_t data;
    char secKey[26]; // Add your secKey field
    bool isWebsocket; 
    bool canSend;
    char secProtocol[100] ;
    struct connection_info *next; // Next connection in the list
    // any other information you need to keep for each connection
};

//=======================================================================================================================
void send_data_callback(void* arg) ;
void cleanUPconnection(struct connection_info * conn_info);
void send_pong_frame(struct tcp_pcb * pcb);
void send_close_frame(struct tcp_pcb *pcb);
//=======================================================================================================================
//=======================================================================================================================

void sendToWebSocket()
{
  // Iterate over the list of connections
  struct connection_info *conn_info = head;
  while (conn_info != NULL) {
      if (conn_info->isWebsocket && conn_info->canSend) {
          // Call send_data_callback with conn_info
          send_data_callback(conn_info);
          conn_info->canSend = false;
      }
      conn_info = conn_info->next;
  }
}
//=======================================================================================================================

/*
// This is an alternative timer to the one below. It works but I thought the timer might be a problem
// at some point so I switched to the below version which was much the same.
*/
#include "hardware/timer.h"
#include "pico/time.h"

// Global variables to hold the current and previous times
uint64_t previous_time = 0;
uint64_t desired_interval = 40000;  // Desired interval in microseconds (20ms)
volatile bool timer_fired = false;

bool repeating_timer_callback(struct repeating_timer *t) {
    // Get the current time
    uint64_t current_time = time_us_64();

    // Calculate the actual interval since the last call
    uint64_t actual_interval = current_time - previous_time;

    // Adjust the timer interval based on the difference between the desired and actual intervals
    int64_t adjustment = (int64_t)desired_interval - (int64_t)actual_interval;
    //printf("adjusment %lld  t->delay_us %lld\n",adjustment,t->delay_us);
    t->delay_us += adjustment;

    // Iterate over the list of connections
    struct connection_info *conn_info = head;
    while (conn_info != NULL) {
        if (conn_info->isWebsocket) {
            conn_info->canSend = true;
        }
        conn_info = conn_info->next;
    }

    // Update the previous time
    previous_time = current_time;

    return true;
}

struct repeating_timer timer;  // global
bool timerIsSetup = false;
void install_timer_send_data_callback()
{
    if(timerIsSetup)
        return;
    printf("\n====================================================\ninstall_timer_send_data_callback\n\n");
    // Get the current time
    previous_time = time_us_64();

    // Initialize the timer with the desired interval
    bool result = add_repeating_timer_us(desired_interval, repeating_timer_callback, NULL, &timer);

    if (result) {
        printf("Timer successfully created\n");
        timerIsSetup = true;
    } else {
        printf("Could not create timer\n");
    }
}
/*

// Yeah I know. What are includes doing here. I tend to keep them where they are needed during dev
// because if I decide to remove this code I can also easily remove the headers it needed.
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/regs/rtc.h"


typedef struct {
    uint64_t previous_time;
    uint64_t interval_us;
    bool timerIsSetup;
} timer_data_t;

// Global variables
//timer_data_t timer_data = {0};
uint64_t desired_interval = 40000;  // Desired interval in microseconds (40ms)
timer_data_t timer_data = {.interval_us = 40000, .timerIsSetup = false};


int64_t timer_callback(alarm_id_t id, void *user_data) {
    // Get the current time
    uint64_t current_time = time_us_64();

    // Calculate the actual interval since the last call
    uint64_t actual_interval = current_time - timer_data.previous_time;

    // Adjust the timer interval based on the difference between the desired and actual intervals
    int64_t adjustment = (int64_t)timer_data.interval_us - (int64_t)actual_interval;
    timer_data.interval_us += adjustment;
    printf("adjusment %lld  t->interval_us %lld\n", adjustment, timer_data.interval_us);
    if(timer_data.interval_us < 0)
      timer_data.interval_us = 1;

    // Iterate over the list of connections
    struct connection_info *conn_info = head;
    while (conn_info != NULL) {
        if (conn_info->isWebsocket) {
            // Call send_data_callback with conn_info
            send_data_callback(conn_info);
        }
        conn_info = conn_info->next;
    }

    // Update the previous time
    timer_data.previous_time = current_time;

    return timer_data.interval_us;
}

// We want to send our voltage readings at a regular 50 times per sec
void install_timer_send_data_callback()
{
    if(timer_data.timerIsSetup)
        return; // Already done so go away

    // Get the current time
    timer_data.previous_time = time_us_64();

    // Initialize the timer with the desired interval
    alarm_pool_add_alarm_in_us(alarm_pool_get_default(), timer_data.interval_us, timer_callback, NULL, false);

    timer_data.timerIsSetup = true;
}

*/



//============================================================================================
void removeFromList(struct connection_info *conn_info_gone) {
    // Special case: the first node is the one to remove
    if (head == conn_info_gone) {
        head = head->next;
        free(conn_info_gone);
        return;
    }

    // Iterate over the list of connections
    struct connection_info *conn_info = head;
    while (conn_info != NULL && conn_info->next != NULL) {
        if (conn_info->next == conn_info_gone) {
            // Remove conn_info_gone from the list
            conn_info->next = conn_info_gone->next;
            free(conn_info_gone);
            return;
        }
        conn_info = conn_info->next;
    }
}

//=======================================================================================================================
int findOpenChannel(struct connection_info *conn_info)
{
// Originally intended for an 8 channel ADC1256. As a browser connects, find an open ADC channel
// which is not in use.
	return -1;
}
//=======================================================================================================================

bool send_data(struct connection_info *conn_info, const char* data, uint16_t length);

err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct connection_info *conn_info = (struct connection_info *)arg;
    if(conn_info->data2send != NULL) {
        send_data(conn_info, conn_info->data2send, conn_info->data2send_len);
    }
    return true;
}

// Once we have a websocket connection, store all required session data here.
struct connection_info *setup_callbacks(struct tcp_pcb *pcb)
{
    struct connection_info *conn_info = malloc(sizeof(struct connection_info));
    tcp_arg(pcb, conn_info); // set the pcb callback argumrnt to conn_info
    findOpenChannel(conn_info);
    conn_info->pcb = pcb;
    conn_info->data2send = NULL;
    conn_info->data2send_len = 0;
    conn_info->loopCounter = 0;
    conn_info->alive = 0;
    // Add the new connection to the head of the list
    conn_info->next = head;
    head = conn_info;
    
    conn_info->isWebsocket = true; 
    tcp_sent(pcb, sent_callback);
    return conn_info;
}

//------------------------------------------------------------------------
/*
I was trying to debug the slow sending and apparant buffer filling up somewhere
in the system. So I tried various method of handeling and throtteling the socket writes.
To no avail. The problem seems to be deeper in the system.
*/


/*
bool send_data(struct connection_info *conn_info, const char* data, uint16_t length) {
    if (tcp_sndbuf(conn_info->pcb) >= length) {
        err_t result = tcp_write(conn_info->pcb, data, length, TCP_WRITE_FLAG_COPY);
        if (result == ERR_OK) {
            err_t send_result = tcp_output(conn_info->pcb);
            if (send_result != ERR_OK) {
                // Handle sending error
                LWIP_DEBUGF(HTTPD_DEBUG,("Error sending data: %d\n", send_result));
                return false;
            }
        } else if(result == ERR_MEM){
          //  check tcp_sndbuf again to see how much data can actually be sent, and then call tcp_write with that amount of data.
          return send_data(conn_info, data, tcp_sndbuf(conn_info->pcb));
        }        
        else {
            // Handle tcp_write error
            LWIP_DEBUGF(HTTPD_DEBUG,("Error writing data to TCP send buffer: %d\n", result));
            return false;
        }
    } else {        
        // Handle insufficient space in the send buffer
        if (conn_info->data2send != NULL) {
            free(conn_info->data2send);
        }
        conn_info->data2send = malloc(length);
        if (conn_info->data2send == NULL) {
            // Handle malloc error
            LWIP_DEBUGF(HTTPD_DEBUG,("Error allocating memory for data2send\n"));
            return false;
        }
        memcpy(conn_info->data2send, data, length);
        conn_info->data2send_len = length;
    }
    // All good if you got this far
    if(conn_info->data2send){
      free(conn_info->data2send);
      conn_info->data2send = NULL;
    }
    return true;
}
*/

// Yet another try

/*
bool send_data(struct connection_info *conn_info, const char* data, uint16_t length) {
    if (tcp_sndbuf(conn_info->pcb) >= length) {
        err_t result = tcp_write(conn_info->pcb, data, length, TCP_WRITE_FLAG_COPY);
        if (result == ERR_OK) {
            err_t send_result = tcp_output(conn_info->pcb);
            if (send_result != ERR_OK) {
                // Handle sending error
                LWIP_DEBUGF(HTTPD_DEBUG,("Error sending data: %d\n", send_result));
                return false;
            }
        } else if(result == ERR_MEM){
            // Check tcp_sndbuf again to see how much data can actually be sent, and then call tcp_write with that amount of data.
            return send_data(conn_info, data, tcp_sndbuf(conn_info->pcb));
        } else {
            // Handle tcp_write error
            LWIP_DEBUGF(HTTPD_DEBUG,("Error writing data to TCP send buffer: %d\n", result));
            return false;
        }
    } else {        
        // Handle insufficient space in the send buffer. Here we introduce a delay to give the buffer time to free up.
        sleep_ms(10); // Adjust this value based on your specific needs.
        return send_data(conn_info, data, length);
    }
    // All good if you got this far
    if(conn_info->data2send){
        free(conn_info->data2send);
        conn_info->data2send = NULL;
    }
    return true;
}
*/
bool send_data(struct connection_info *conn_info, const char* data, uint16_t length) {
    err_t err;
    uint16_t len = length;
    //int attempt = 0;

    do {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Trying to send %d bytes, attempt #%d\n", len, ++attempt));
        err = tcp_write(conn_info->pcb, data, len, TCP_WRITE_FLAG_COPY);
        if (err == ERR_MEM) {
            if ((tcp_sndbuf(conn_info->pcb) == 0) || (tcp_sndqueuelen(conn_info->pcb) >= TCP_SND_QUEUELEN)) {
                /* no need to try smaller sizes */
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Buffer full or queue length exceeded. Waiting...\n"));
                sleep_ms(500);
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Woke up from sleep. Retry...\n"));
            } else {
                len /= 2;
            }
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed, trying less (%d bytes)\n", len));
        }
    } while (err == ERR_MEM && len > 1);

    if (err == ERR_OK) {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Sent %d bytes\n", len));
        err_t send_result = tcp_output(conn_info->pcb);
        if (send_result != ERR_OK) {
            // Handle sending error
            LWIP_DEBUGF(HTTPD_DEBUG,("Error sending data: %d\n", send_result));
            return false;
        }
    } else {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed with err %d (\"%s\")\n", err, lwip_strerr(err)));
        return false;
    }

    // All good if you got this far
    if(conn_info->data2send){
        free(conn_info->data2send);
        conn_info->data2send = NULL;
    }
    return true;
}

//======================================================================================================================
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

void send_websocket_upgrade_response(struct tcp_pcb *pcb) {
  
    
    //struct connection_info *conn_info = setup_callbacks(pcb);
    struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;

    mbedtls_sha1_context sha1_ctx;
    unsigned char sha1_result[20];
    unsigned char client_key_and_guid[60]; // Length of client_key (24) + length of WS_GUID (36)
    unsigned char base64_result[30];
    size_t base64_len;
    
    // Concatenate the client_key and WS_GUID
    strcpy((char*)client_key_and_guid, conn_info->secKey);
    strcat((char*)client_key_and_guid, WS_GUID);

    // Compute SHA-1 of the concatenated string
    mbedtls_sha1_init(&sha1_ctx);
    mbedtls_sha1_starts(&sha1_ctx);
    mbedtls_sha1_update(&sha1_ctx, client_key_and_guid, strlen((const char*)client_key_and_guid));
    mbedtls_sha1_finish(&sha1_ctx, sha1_result);
    mbedtls_sha1_free(&sha1_ctx);

    // Base64 encode the SHA-1 result
    mbedtls_base64_encode(base64_result, sizeof(base64_result), &base64_len, sha1_result, sizeof(sha1_result));

    //printf("conn_info->secProtocol = %s and client key = %s\n",conn_info->secProtocol,conn_info->secKey);
    // Send the WebSocket upgrade response
    char response[256];
// Check if the client requested a subprotocol
if (conn_info->secProtocol != NULL) {
    // Check if the server supports the requested subprotocol
    if (strcmp(conn_info->secProtocol, "adc-send-protocol") == 0) {
        // Include the Sec-WebSocket-Protocol header in the response
        snprintf(response, sizeof(response),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "Sec-WebSocket-Protocol: adc-send-protocol\r\n"
            "\r\n",
            base64_result
        );
    } else {
        // Handle unsupported subprotocol
    }
} else {
    // The client did not request a subprotocol
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        base64_result
    );
}

//printf("...\n%s\n...\n",response);
sleep_ms(120); // it seems to need this

    //struct connection_info *conn_info = setup_callbacks(pcb);
    // Send the response.
    if(send_data(conn_info, response, strlen(response)))
    {
      tcp_output(pcb);
      connectionCounter++;  // assume we have a connection...
      
			conn_info->number = connectionCounter;
      printf("Got a ws connection %d\n", connectionCounter);
      install_timer_send_data_callback();
    }
    else free(conn_info);
}



//==================================================================================================================

void send_text_frame(struct tcp_pcb *pcb, const char *text) {
  
  struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;
  uint64_t len = strlen(text);

  // Construct the WebSocket frame header
  uint8_t header[10] = {0};
  header[0] = 0x81; // FIN bit set and opcode for text frame
  if (len <= 125) {
    header[1] = len;
  } else if (len <= 0xFFFF) {
    header[1] = 126;
    header[2] = len >> 8;
    header[3] = len & 0xFF;
  } else {
    header[1] = 127;
    // Fill in the next 8 bytes with the length in big-endian order
    for (int i = 0; i < 8; i++) {
      header[9 - i] = len & 0xFF;
      len >>= 8;
    }
  }

  if (send_data(conn_info, (const char *)header, sizeof(header)))
  {
    // Send the payload data
    send_data(conn_info, text, strlen(text));
  }
}

//==================================================================================================================
/*
void send_binary_frame(struct tcp_pcb *pcb, const uint8_t *data, uint64_t len) {

  struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;
  // Construct the WebSocket frame header
  uint8_t header[10] = {0};
  header[0] = 0x82; // FIN bit set and opcode for binary frame
  if (len <= 125) {
    header[1] = len;
  } else if (len <= 0xFFFF) {
    header[1] = 126;
    header[2] = len >> 8;
    header[3] = len & 0xFF;
  } else {
    header[1] = 127;
    // Fill in the next 8 bytes with the length in big-endian order
    for (int i = 0; i < 8; i++) {
      header[9 - i] = len & 0xFF;
      len >>= 8;
    }
  }

  if (send_data(conn_info, (const char *)header, sizeof(header)))
  {
    // Send the payload data
    send_data(conn_info, (const char *)data, len);
  }
}
*/
void send_binary_frame(struct tcp_pcb *pcb, const uint8_t *data, uint64_t len)
{

  struct connection_info *conn_info = (struct connection_info *)pcb->callback_arg;

  uint8_t *header = NULL;
  size_t header_size = 2;

  // Calculate the size of the header based on the length of the frame payload.
  if (len > 125)
  {
    header_size += 1;
    if (len > 65535)
    {
      header_size += 7;
    }
  }

  // Allocate memory for the header.
  header = malloc(header_size);

  memset(header, 0, header_size);

  header[0] = 0x82; // FIN bit set and opcode for binary frame
  if (len <= 125)
  {
    header[1] = len;
  }
  else if (len <= 0xFFFF)
  {
    header[1] = 126;
    header[2] = len >> 8;
    header[3] = len & 0xFF;
  }
  else
  {
    header[1] = 127;
    // Fill in the next 8 bytes with the length in big-endian order
    for (int i = 0; i < 8; i++)
    {
      header[9 - i] = len & 0xFF;
      len >>= 8;
    }
  }

  if (send_data(conn_info, (const char *)header, header_size))
  {
    uint8_t *payload = malloc(len);
    if (payload == NULL)
    {
      // Handle malloc error
      LWIP_DEBUGF(HTTPD_DEBUG, ("Error allocating memory for payload\n"));
      free(header);
      return;
    }
    memcpy(payload, data, len);

    // Send the payload data
    send_data(conn_info, (const char *)payload, len);
    // printf("p len %d",len);
    // sleep_ms(10);
    /*
    printf("->");
    for (uint64_t i = 0; i < len; i++) {
        printf("%02x", (unsigned char)payload[i]);
    }
    printf("|%lld|", len);
    for (uint64_t i = 0; i < len; i++) {
        printf("%02x", (unsigned char)data[i]);
    }
    printf("\n");
    */
    free(payload); // Free the payload memory as well
  }
  free(header); // Free the header memory regardless of send_data result
}
//=======================================================================================================================
    uint8_t * convert32bitIntTo24Bit2sCompliment(uint32_t I, uint8_t* byteArray)
    {
        // assumes that in C, ints are already stored in 2's compliment
        uint8_t out[3];
        uint8_t tmp;

        tmp = (I >> 24)& 0xFF;
        out[0] = (I >> 16)& 0xFF;
        out[1] = (I >> 8)& 0xFF;
        out[2] = I & 0xFF;
        if((tmp & 0x8) == 0x8)
        out[0] |= 0x8;
        memcpy(byteArray,&out[0],3);
        return byteArray;
    }
//====================================================================
#include <math.h>
#include <time.h>
//#include <hardware/time.h>
void calcSineWave(struct connection_info *conn_info)
 {

  // Calculate the current time in seconds.
  //double currentTime = time_get_millisecond_count() / 1000.0;
  double currentTime = time_us_64() / 1000000.0;

  // Calculate the value of the sine wave at the current time.
  double sineWaveValue = sin(2 * 3.142 * currentTime * 0.3) *500;

  // Load the value of the sine wave into the conn_info->channel variable.
  conn_info->data = sineWaveValue;
}
//========================================================
//========================================================
//========================================================
//========================================================
//========================================================
//========================================================
void send_data_callback(void* arg) {

  struct connection_info *conn_info = (struct connection_info *) arg;
  uint8_t p[10], n;

  // Normally you would get data from an ADC at this point
  // but for now we just calculate a sine wave and send it
  // to the browser
  calcSineWave(conn_info);  
/*
    // send the number of connections periodically
			if( conn_info->loopCounter++ > 50)
			{
				memset(p, 0, 6);
				conn_info->loopCounter = 0;

				p[1] = (unsigned char)((connectionCounter >> 24) & 0xFF);
				p[2] = (unsigned char)((connectionCounter >> 16) & 0xFF);
				p[3] = (unsigned char)((connectionCounter >> 8) & 0xFF);
				p[4] = (unsigned char)((connectionCounter) & 0xFF);
        n = 4;
        //printf ("send_data_callback: number of connections\n");
        send_binary_frame(conn_info->pcb, p, n);
        send_pong_frame((struct tcp_pcb *)conn_info->pcb);
			}
			else
      */
			{
        // This is the main 'send' function that gets measured in the browser for fps
				memset(p, 0, 10);

				//readshm(&p[1],&p[5], conn_info->channel );
				n = 8;
				p[0] = 1; // tell javascript this is a meter reading
        // p[1] contains the main data read
        convert32bitIntTo24Bit2sCompliment(conn_info->data, &p[1]);
        // p[5] contains the TA data
				//p[8] = conn_info->number;    // connection count    
				send_binary_frame(conn_info->pcb, p, n); // if i comment this out then the program works without freezing after 30 sec
        //sleep_ms(10);
        //printf ("%lu  %c %c %c    %d\n",conn_info->data, p[4], p[5], p[6], n);
  
      }

     // conn_info->alive++;
      if (conn_info->alive > 300)
      {
        printf("conn_info->alive = %d\n",conn_info->alive);
        LWIP_DEBUGF(HTTPD_DEBUG, ("send_data_callback: closing because client has gone away %d\n", conn_info->alive));
        send_close_frame(conn_info->pcb);
        
      }
      sys_check_timeouts();
}
//==================================================================================================================


void decode_websocket_header(uint8_t *frame_data, uint8_t *fin, uint8_t *opcode, uint8_t *mask, uint64_t *payload_len, uint8_t *masking_key) {
  // Decode the header
  *fin = (frame_data[0] & 0x80) >> 7;
  *opcode = frame_data[0] & 0x0F;
  *mask = (frame_data[1] & 0x80) >> 7;
  *payload_len = frame_data[1] & 0x7F;
  //  if the payload length is 126 or 127, indicating an extended payload length.
  // If the MASK bit is set, get the masking key
  if (*mask) {
    memcpy(masking_key, &frame_data[2], 4);
  }
}

//==================================================================================================================

void handle_text_frame(struct tcp_pcb *pcb, uint8_t *data, uint64_t len)
{

  struct connection_info *conn_info = (struct connection_info *)pcb->callback_arg;

  // Convert the data to a string..... might not need this.
  /*
  char *text = (char *)malloc(len + 1);
  memcpy(text, data, len);
  text[len] = '\0';

  // Do something with the text
  printf("Received text: %s\n", text);
  // Send a response
  //send_text_frame(pcb, "Thanks for the message!");
  */
 //printf("Received text:\n"); 
  if (strncmp((const char *)data, "alive\n", 6) == 0)
  {
    //printf("alive\n");
    conn_info->alive = 1; // this is our watch-dog counter, reset by the client on every draw frame
  }

  // Free the text string
  // free(text);
}

//==================================================================================================================

void handle_binary_frame(struct tcp_pcb *pcb, uint8_t *data, uint64_t len) {
  
  //struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;

  // Convert the data to a string
  uint8_t *bdata = (uint8_t *)malloc(len + 1);
  memcpy(bdata, data, len);

  // Do something with the text
  printf("Received binary data: \n");

  // Free the text string
  free(bdata);
}
//==============================================================================================================
void send_close_frame(struct tcp_pcb *pcb)
{
  struct connection_info *conn_info = (struct connection_info *)pcb->callback_arg;

  // Create a Close frame with status code 1000 (normal closure)
  uint8_t close_frame[4] = {0x88, 0x02, 0x03, 0xE8}; // 0x88 = FIN + Close opcode, 0x02 = payload length, 0x03E8 = status code 1000

  // Send the Close frame
  err_t err = tcp_write(pcb, close_frame, sizeof(close_frame), TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK)
  {
    printf("Error sending Close frame: %s\n", lwip_strerr(err));
  }
  // close the connection
  err_t error = tcp_close(pcb);
  if (error != ERR_OK)
  {
    LWIP_DEBUGF(HTTPD_DEBUG, ("send_data_callback: error closing socket\n"));
  }
  cleanUPconnection(conn_info);
  printf("close\n");
}

//==================================================================================================================
void send_pong_frame(struct tcp_pcb *pcb) {
  // Create a Pong frame
  uint8_t pong_frame[2] = {0x8A, 0x00};  // 0x8A = FIN + Pong opcode, 0x00 = payload length

  // Send the Pong frame
  err_t err = tcp_write(pcb, pong_frame, sizeof(pong_frame), TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    printf("Error sending Pong frame: %s\n", lwip_strerr(err));
  }
  printf("pong\n");
}
//==================================================================================================================
void handle_ping_frame(struct tcp_pcb *pcb) {
  // A Ping frame has been received, so we need to send a Pong frame in response
  send_pong_frame(pcb);
  printf("ping pong\n");
}

//==================================================================================================================
void handle_pong_frame(struct tcp_pcb *pcb) {
  // A Pong frame has been received, indicating that the connection is still alive
  // No action is required
}

//==================================================================================================================
void cleanUPconnection(struct connection_info *conn_info)
{
  printf("closing connection\n");
  if (conn_info->data2send != NULL)
  {
    free(conn_info->data2send);
  }
  //conn_info->pcb->callback_arg = NULL;
  tcp_arg(conn_info->pcb, NULL); // Clear the callback arg to avoid dangling pointer

  removeFromList(conn_info); // removes this conn_info from the linked list of conn_info's

  if (conn_info != NULL)
  {
    free(conn_info);
    conn_info = NULL;
  }
}
//==================================================================================================================
void handle_close_frame(struct tcp_pcb *pcb) {
  connectionCounter--;
	//shared_memory->channelInUse[conn_info->channel] = FALSE;
  struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;
  cleanUPconnection(conn_info);
}

//==================================================================================================================

void handle_websocket_frame(struct pbuf *p, struct tcp_pcb *pcb) {
  // Get the raw frame data and length
  uint8_t *frame_data = (uint8_t *)p->payload;
  //uint16_t frame_len = p->len;

  //printf("handle_websocket_frame\n");
  // Decode the header
  uint8_t fin, opcode, mask;
  uint64_t payload_len;
  uint8_t masking_key[4] = {0};
  decode_websocket_header(frame_data, &fin, &opcode, &mask, &payload_len, masking_key);

  // Unmask the payload if necessary
  if (mask) {
    //printf("got a mask\n");
    for (uint64_t i = 0; i < payload_len; i++) {
      frame_data[i + 6] ^= masking_key[i % 4];
    }
  }

  //printf("handle_websocket_frame opcode = %d\n", opcode);

  // Handle the opcode
  switch (opcode) {
    case 0x1: // Text frame
      // Handle a text frame
      handle_text_frame(pcb, frame_data + 6, payload_len); // similar to LWS_CALLBACK_RECEIVE 
      break;

    case 0x2: // Binary frame
      // Handle a binary frame
      handle_binary_frame(pcb, frame_data + 6, payload_len); // similar to LWS_CALLBACK_RECEIVE
      break;

    case 0x8: // Close frame
      // Handle a close frame
      handle_close_frame(pcb);  // similar to LWS_CALLBACK_CLOSED 
      break;

    case 0x9: // Ping frame
      // Handle a ping frame
      handle_ping_frame(pcb);
      break;

    case 0xA: // Pong frame
      // Handle a pong frame
      handle_pong_frame(pcb);
      break;

    default:
      // Unknown opcode
      LWIP_DEBUGF(HTTPD_DEBUG, ("Error, handle_websocket_frame: unknown opcode %d\n", opcode));
      break;
  }
}

//======================================================================================================================
char* get_requested_subprotocol(char* http_request) {
    char* protocol_header_start = strstr(http_request, "Sec-WebSocket-Protocol: ");
    if (protocol_header_start != NULL) {
        protocol_header_start += strlen("Sec-WebSocket-Protocol: ");
        char* protocol_header_end = strstr(protocol_header_start, "\r\n");
        if (protocol_header_end != NULL) {
            size_t protocol_length = protocol_header_end - protocol_header_start;
            char* requested_subprotocol = malloc(protocol_length + 1);
            strncpy(requested_subprotocol, protocol_header_start, protocol_length);
            requested_subprotocol[protocol_length] = '\0';
            return requested_subprotocol;
        }
    }
    return NULL;
}
//==================================================================================================================
// Assuming pbuf data is a null-terminated string

#define SEC_WEBSOCKET_KEY_HEADER "Sec-WebSocket-Key: "

bool is_websocket_upgrade_request(struct pbuf *p, struct tcp_pcb *pcb) { 
  //struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;
    const char *hdr = (char *)p->payload;
    uint32_t data_len = p->len;

    if (p->len != p->tot_len) {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Warning: incomplete header due to chained pbufs\n"));
    }

    if (p->len < 15) {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Warning: too short to be a upgrade request\n"));
    }

    // Check if the Upgrade: websocket header exists
    if (lwip_strnstr(hdr, "Upgrade: websocket", data_len) == NULL) {
        return false;
    }

    // Check if the Connection: Upgrade header exists
    if (lwip_strnstr(hdr, "Connection: Upgrade", data_len) == NULL) {
        return false;
    }

  
  
  
    printf("Checking for Sec-WebSocket-Key\n");
    // Check if the Sec-WebSocket-Key header exists
    char* key_header_start = lwip_strnstr(hdr, SEC_WEBSOCKET_KEY_HEADER, data_len);
    if (key_header_start == NULL) {
        printf("Not found Sec-WebSocket-Key\n");
        return false;
    } 
    
    struct connection_info *conn_info = setup_callbacks(pcb);
    bool bad = false;
     {
        char* key_start = key_header_start + strlen(SEC_WEBSOCKET_KEY_HEADER);
        char* key_end = strchr(key_start, '\r'); // Headers are separated by "\r\n"
        //printf("key_start = %s, key_end = %s",key_start,key_end);
        if (key_end != NULL) {
            size_t key_len = key_end - key_start;
            //printf("key len = %d and buffer size = %d\n", key_len, sizeof(conn_info->secKey));
            if (key_len < sizeof(conn_info->secKey)) { // make sure the key fits into the buffer
                strncpy(conn_info->secKey, key_start, key_len);
                conn_info->secKey[key_len] = '\0'; // Null-terminate the key string
                //printf("Sec-WebSocket-Key = %s\n", conn_info->secKey);
            } else {
              bad = true;
                // Handle error: the key is too long to fit into the buffer
                LWIP_DEBUGF(HTTPD_DEBUG, ("is_websocket_upgrade_request: the key is too long to fit into the buffer\n%s\n",hdr));
            }
        } else {
          bad = true;
            LWIP_DEBUGF(HTTPD_DEBUG, ("is_websocket_upgrade_request: the key is not properly terminated\n%s\n",hdr));
        }
    }
    
    // Check if the Sec-WebSocket-Protocol header exists
    char* protocol_header_start = lwip_strnstr(hdr, "Sec-WebSocket-Protocol: ", data_len);
    if (protocol_header_start != NULL) {
        protocol_header_start += strlen("Sec-WebSocket-Protocol: ");
        char* protocol_header_end = strchr(protocol_header_start, '\r'); // Headers are separated by "\r\n"

        if (protocol_header_end != NULL) {
            size_t protocol_length = protocol_header_end - protocol_header_start;
            if (protocol_length < sizeof(conn_info->secProtocol)) { // make sure the protocol fits into the buffer
                strncpy(conn_info->secProtocol, protocol_header_start, protocol_length);
                conn_info->secProtocol[protocol_length] = '\0'; // Null-terminate the protocol string
            } else {
              bad = true;
                // Handle error: the protocol is too long to fit into the buffer
                LWIP_DEBUGF(HTTPD_DEBUG, ("is_websocket_upgrade_request: the protocol is too long to fit into the buffer\n%s\n",hdr));
            }
        } else {
          bad = true;
            LWIP_DEBUGF(HTTPD_DEBUG, ("is_websocket_upgrade_request: the protocol is not properly terminated\n%s\n",hdr));
        }
    }

if(bad){
  free(conn_info);
  return false;
}
//
    LWIP_DEBUGF(HTTPD_DEBUG, ("valid upgrade request\n"));
    // If all headers are present, this is a WebSocket upgrade request
    return true;
}
//=========================================================================================
bool isWebSocket(struct tcp_pcb *pcb)
{  
  struct connection_info *conn_info = (struct connection_info *) pcb->callback_arg;
  if(conn_info)
  {
    return conn_info->isWebsocket;
  }
  return false;
}
