#include "includes/htmodloader.h"

static const HTSignature sig_Lua_debugDoString = {
  .offset = 0,
  .indirect = HT_SCAN_DIRECT,
  .name = "Lua::debugDoString",
  .sig = 
    "55 56 57 53 48 81 EC 38 09 00 00 48 8D AC 24 80 "
    "00 00 00 48 89 D6 48 89 CB E8 ?  ?  ?  ?  85 C0 "
};

static const HTSignature sig_Client_checkChangeLevel = {
  .offset = 0,
  .indirect = HT_SCAN_DIRECT,
  .name = "Client::checkChangeLevel",
  .sig = 
    "55 41 57 41 56 56 57 53 48 81 EC ?  ?  ?  ?  48 "
    "8D AC 24 ?  ?  ?  ?  48 83 E4 E0 48 89 CE E8 ?  "
    "?  ?  ?  C5 FB 11 86 ?  ?  ?  ?  48 8B 8E ?  ?  "
    "?  ?  E8 ?  ?  ?  ?  48 8B 8E"
};

 
