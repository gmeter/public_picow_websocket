
let connected = false;
let needleVal = 0, needleVal0 = 0, lastADCNeedleVal = 0;
var sl_ws; // websocket
var Dcounter = 0;

function initWS() {
    var loc = window.location, new_uri;
    if (loc.protocol === "https:") {
        new_uri = "wss:";
    } else {
        new_uri = "ws:";
    }
    new_uri += "//" + loc.host ;

    //let new_uri = 'ws://192.168.1.12';
    let ADS1256_NEGATIVE_FLAG = 0x00800000;
    let ADS1256_NEGATIVE_PADDING = 0xFF000000;
    console.log("Attempting websocket connection to " + new_uri);
    if (typeof MozWebSocket != "undefined") {
        sl_ws = new MozWebSocket(new_uri,
            "adc-send-protocol");
    } else {
        sl_ws = new WebSocket(new_uri, "adc-send-protocol");
    }
    sl_ws.onopen = function () {
        console.log("Websocket connected! ");
        connected = true;
        if (connected) { sl_ws.send("alive\n"); }// let the server know we are alive
        
        // Send a ping every 5 seconds
        setInterval(function() {            
            if (connected) { sl_ws.send("alive\n"); }
        }, 5000);
        
    };

    sl_ws.binaryType = 'arraybuffer';
    //------------------------------------
    sl_ws.onerror = function () {
        console.log("Connection error");
        connected = false;
    };
    //------------------------------------

    sl_ws.onclose = function (event) {
        console.log("Connection closed");
        console.log(event.code);
        console.log(event.reason);
        connected = false;
    };

    sl_ws.onmessage = function (event) {
        if (event.opcode === 0xA) {
            console.log("pong");
          }
          else
        if (typeof event.data === 'string') {
            var data = event.data;
            console.log("Received data string " + data);
        }
        else {
            var _8Array = new Uint8Array(event.data);
            //console.log(_8Array);
            if (_8Array[0] == 1) // received ADC data
            {
                Dcounter++;
                needleVal = (_8Array[1] << 16) | (_8Array[2] << 8) | _8Array[3];

                if (_8Array[1] > 127) {
                    needleVal |= ADS1256_NEGATIVE_PADDING;
                }
                var tmp = needleVal;
                needleVal = (needleVal + lastADCNeedleVal)>>1;  // low pass filter
                lastADCNeedleVal = tmp;
                // TA
                needleVal0 = (_8Array[5] << 16) | (_8Array[6] << 8) | _8Array[7];
                if (_8Array[5] > 127) {
                    needleVal0 |= ADS1256_NEGATIVE_PADDING;
                }
                //console.log(needleVal);
            }

            else if (_8Array[0] == 2) // received time value
            {
                sl_ws.send("time\n");
                //sl_ws.send("alive\n"); // let the server know we are alive
                //toofarInfo.visible = 0;
            }
            else if (_8Array[0] == 3) // received time too long warning
            {
                sl_ws.send("close\n");
                console.log("too far =======================");
                //toofarInfo.visible = 1;
            }
            else if (_8Array[0] == 8) // received connection count
            {
                connectionCount = (_8Array[1] << 24) | (_8Array[2] << 16) | (_8Array[3] << 8) | _8Array[4];
                //console.log("Received connectionCount "+ connectionCount);
                //ConnectionCountText.innerHTML = connectionCount;
            }

        }
    };

}