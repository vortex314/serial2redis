# Serial to redis Protocol
The serial or udp endpoint is sending frames that contain a JSON array and receives as a response JSON Variant ( can be any variuable integer,bool,double,string,array or object )

```
["PUBLISH","X","3.5"]
["PSUBSCRIBE","dst/myNode/*"]
["XADD","logs","*","node",myNode,"message",message]
// in C++ code to send 
auto array = doc.to<JsonArray>();
array.add("XADD");
array.add("str:pos");
array.add("*");
array.add("lon");
array.add("50.9543");
array.add("lat");
array.add("4.9563");
std::string cmd;
serializeJson(doc,cmd);
["TS.ADD","ts:temperature","*","34.5"]
["SET","x","5665"]
["HELLO","3"]
```
can return 
```
["pmessage","dst/myNode/*","key","value"]
```

First parameter :
- 0 : no reply
- 1 : ACK
- 2 : full redisreply
- other numbers are read as tokens with FNV hash
````
[-1,"src/myNode/object/property","3.54"]
[-2,767,"3.54"]
[-3,"object/property","3.54"]
````