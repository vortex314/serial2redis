# ws2redis

Web-socket to redis. Acts as a gateway to redis with minimal overhead. 
It accepts the same protocol as the serial or UDP gateway.

It also can serve as a static content server for serving pages. 

No need for node.js server and redis library limitations. 

It's a single thread application based on the excellent C++ seasocks library.

Sending a redis command is straight forward
```
    ws = new WebSocket('ws://' + document.location.host + '/redis', ['string', 'foo']);
    ws.send(JSON.stringify(["HELLO", "3"]))
```
