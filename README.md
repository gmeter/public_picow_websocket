# public_picow_websocket
An implementation of a websocket on the lwip HTTP server

This websocket implementation works but the rp2040 and wifi 
system seems unable to keep up with even 40ms between writes to the browser.
I don't know if some settings in the lwipopts.h are not set correctly. 

The wifi module supposedly can handle 96Mbps, so writing 8 bytes 50 times per second
should be a walk in the park, yet it seems to struggle.  

The demo imitates an ADC receiving sinewave data and sending it to the 
browser which displays and old style analog meter which moves back and forth
with the sine wave.  It only runs for a few secs before the write buffer chokes,
so don't miss it.

Pity, it would have great. It can still be great if someone can fix it, 
but for now it is only suitable for slower use cass.

httpd.c has been altered and renamed httpd_ws.c. The main changes were to add intercept
points for websocket upgrade and response calls in http_recv()

send_data_callback() in handle_ws_frames.c is the main bit of code where you would 
insert your user data which you want sent to the client (browser)

#Installing

Make a build folder in the project root.

I made shell scripts for build, cmake and the perl script which takes the fs data and puts it into
fsdata.c  Your work flow might be different but I use vscode and when I open its terminal
it drops me in the project root direcory so its easier to just do everything from there.

If you change anything in the fs folder, add images, change html code etc then you
need to run runPerl.sh followed by runMake.sh. The uf2 file will be in build/src

When you upload the compiled uf2 file to the pico, it will wait 20 seconds before 
connecting to your wifi and printing the IP address to put in your browser.
That's enough time for you to get your terminal up and catch the data.
