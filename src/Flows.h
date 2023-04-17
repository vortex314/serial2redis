#include <ArduinoJson.h>
#include <CborDeserializer.h>
#include <CborDump.h>
#include <CborSerializer.h>
#include <ConfigFile.h>
#include <Frame.h>
#include <Framing.h>
#include <Log.h>
#include <StringUtility.h>
#include <cborjson.h>
#include <limero.h>
#include <stdio.h>

#include <thread>
#include <unordered_map>
#include <utility>

Json cborToJson(const Bytes &);

Flow<Bytes, Json> *cborToRequest();
Flow<Json, Bytes> *responseToCbor();

Flow<Bytes, Json> *crlfCrcToRequest();
Flow<Json, Bytes> *responseToCrlfCrc();

Flow<Bytes, Json> *bytesToRequest();
Flow<Json, Bytes> *responseToBytes();

Flow<std::string, Json> *stringToRequest();
Flow<Json, std::string> *responseToString();

Flow<Bytes,std::string> *bytesToString();
Flow<std::string,Bytes> *stringToBytes();

