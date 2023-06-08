/*
This probably is not required because we just include the handle_es_frames.c file into httpd_ws.c

*/


#ifndef ___handle_websocket_frame___
#define ___handle_websocket_frame___

void handle_websocket_frame(struct pbuf *p, struct tcp_pcb *pcb);
void send_websocket_upgrade_response(struct tcp_pcb *pcb, struct http_state *hs) ;
//bool is_websocket_upgrade_request(struct pbuf *p, struct http_state *hs);

#endif