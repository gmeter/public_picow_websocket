# public_picow_websocket
An implementation of a websocket on the lwip HTTP server

This websocket implementation works but the rp2040 and wifi 
system seems unable to keep up with even 40ms between writes to the browser.
I don't know if some settings in the lwipopts.h are not set correctly. 


The wifi module supposedly can handle 96Mbps, so writing 8 bytes 50 times per second
should be a walk in the park, yet it seems to struggle.  

Pity, it would have great. It can still be great if someone can fix it, 
but for now it is only suitable for slower use cass.

httpd.c has been altered and renamed httpd_ws.c. The main changes were to add intercept
points for websocket upgrade and response calls in http_recv()

send_data_callback() in handle_ws_frames.c is the main bit of code where you would 
insert your user data which you want sent to the client (browser)
