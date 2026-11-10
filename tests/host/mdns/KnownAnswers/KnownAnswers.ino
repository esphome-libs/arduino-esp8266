/*
    Host test for LEAmDNS known answer handling, run with the host emulator in 32-bit mode like
    the ESP8266 (needs g++-10-multilib):

        cd tests/host && make FORCE32=1 mdns/KnownAnswers/KnownAnswers
        bin32/KnownAnswers/KnownAnswers

    It advertises one service, sends itself queries whose known answer list holds other
    instances' records, and checks the heap allocations of answer objects and the replies.
    Exits 0 when every case passes.
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include <cstdlib>
#include <cstring>

static constexpr char INSTANCE[] = "katest";
static constexpr uint16_t MDNS_PORT = 5353;
static constexpr uint16_t CLIENT_PORT = 5354;
static constexpr size_t ANSWER_OBJECT = 256;  // a full known answer object holds 256 B domains
static constexpr uint32_t SETTLE_MS = 6000;   // probing and announcing done
static constexpr uint32_t REPLY_WAIT_MS = 500;
static constexpr uint16_t OTHERS = 20;

// Counts allocations the size of an answer object while a query is processed
static bool counting = false;
static size_t largeAllocations = 0;

static void* countedAlloc(size_t size) {
  if (counting && (ANSWER_OBJECT <= size)) {
    ++largeAllocations;
  }
  void* p = malloc(size ? size : 1);
  if (!p) {
    abort();
  }
  return p;
}

void* operator new(size_t size) {
  return countedAlloc(size);
}
void* operator new[](size_t size) {
  return countedAlloc(size);
}
void operator delete(void* p) noexcept {
  free(p);
}
void operator delete[](void* p) noexcept {
  free(p);
}
void operator delete(void* p, size_t) noexcept {
  free(p);
}
void operator delete[](void* p, size_t) noexcept {
  free(p);
}

struct Packet {
  uint8_t data[1400];
  size_t len = 0;

  void u8(uint8_t v) {
    data[len++] = v;
  }
  void u16(uint16_t v) {
    u8(v >> 8);
    u8(v & 0xff);
  }
  void u32(uint32_t v) {
    u16(v >> 16);
    u16(v & 0xffff);
  }
  void label(const char* s) {
    u8(strlen(s));
    memcpy(&data[len], s, strlen(s));
    len += strlen(s);
  }
  void pointer(uint16_t offset) {
    u16(0xc000 | offset);
  }
};

enum class Ours {
  NONE,
  TTL_LONG,
  TTL_SHORT,
  CACHE_FLUSH,
};

struct Case {
  const char* name;
  const char* service;  // queried service type, "_http" is ours
  bool srv;             // ask for the instance's SRV instead of browsing the type
  Ours ours;
  size_t expectedAllocations;
  bool expectReply;
};

static const Case CASES[] = {
  { "other instances only", "_http", false, Ours::NONE, 0, true },
  { "ours among others", "_http", false, Ours::TTL_LONG, 1, false },
  { "ours with the cache-flush bit", "_http", false, Ours::CACHE_FLUSH, 1, false },
  { "ours with too little TTL left", "_http", false, Ours::TTL_SHORT, 0, true },
  { "a type we do not offer", "_none", false, Ours::NONE, 0, false },
  { "SRV, other instances only", "_http", true, Ours::NONE, 0, true },
  { "SRV, ours among others", "_http", true, Ours::TTL_LONG, 1, false },
};

// A query asking for a unicast reply, plus a second question so it is not handled as a legacy
// query, with known answers for other instances and maybe for us. A browse of <service>._tcp.local
// lists PTRs whose targets alternate between compressed (label + pointer) and spelled out names;
// an SRV query for our instance lists SRVs of other instances.
static void buildQuery(Packet& p, const Case& c) {
  p.len = 0;
  p.u16(0);  // ID
  p.u16(0);  // flags: standard query
  p.u16(2);  // questions
  p.u16(OTHERS + ((Ours::NONE != c.ours) ? 1 : 0));
  p.u16(0);
  p.u16(0);

  const uint16_t instanceOffset = p.len;  // <instance>.<service>._tcp.local for an SRV query
  if (c.srv) {
    p.label(INSTANCE);
  }
  const uint16_t serviceOffset = p.len;  // <service>._tcp.local
  p.label(c.service);
  const uint16_t tcpOffset = p.len;
  p.label("_tcp");
  p.label("local");
  p.u8(0);
  p.u16(c.srv ? 33 : 12);  // SRV or PTR
  p.u16(0x8001);           // IN, unicast reply wanted

  p.label("_other");
  p.pointer(tcpOffset);
  p.u16(12);
  p.u16(1);

  auto answer = [&](const char* instance, uint16_t rrclass, uint32_t ttl, bool compressed) {
    p.pointer(serviceOffset);
    p.u16(12);
    p.u16(rrclass);
    p.u32(ttl);
    const size_t rdLength = p.len;
    p.u16(0);
    p.label(instance);
    if (compressed) {
      p.pointer(serviceOffset);
    } else {
      p.label(c.service);
      p.label("_tcp");
      p.label("local");
      p.u8(0);
    }
    const uint16_t n = p.len - rdLength - 2;
    p.data[rdLength] = n >> 8;
    p.data[rdLength + 1] = n & 0xff;
  };

  // An SRV owned by <instance>.<service>._tcp.local pointing at <instance>.local, port 80
  auto srvAnswer = [&](const char* instance, bool ours) {
    if (ours) {
      p.pointer(instanceOffset);
    } else {
      p.label(instance);
      p.pointer(serviceOffset);
    }
    p.u16(33);
    p.u16(1);
    p.u32(4500);
    p.u16(6 + 1 + strlen(instance) + 1 + 5 + 1);
    p.u16(0);  // priority
    p.u16(0);  // weight
    p.u16(80);
    p.label(instance);
    p.label("local");
    p.u8(0);
  };

  char other[16];
  for (uint16_t i = 0; i < OTHERS; ++i) {
    snprintf(other, sizeof(other), "dev%u", i);
    if (c.srv) {
      srvAnswer(other, false);
    } else {
      answer(other, 1, 4500, (0 == (i % 2)));
    }
    if ((OTHERS / 2 == i) && (Ours::NONE != c.ours))  // ours in the middle of the list
    {
      if (c.srv) {
        srvAnswer(INSTANCE, true);
      } else {
        answer(INSTANCE, (Ours::CACHE_FLUSH == c.ours) ? 0x8001 : 1,
               (Ours::TTL_SHORT == c.ours) ? 100 : 4500, true);
      }
    }
  }
}

static WiFiUDP client;
static size_t caseIndex = 0;
static uint32_t caseStart = 0;
static bool sent = false;
static bool gotReply = false;
static bool replyIsOurs = false;
static int failures = 0;

void setup() {
  Serial.begin(115200);
  MDNS.begin(INSTANCE);
  MDNS.addService(INSTANCE, "http", "tcp", 80);
  client.begin(CLIENT_PORT);
}

void loop() {
  MDNS.update();
  if (millis() < SETTLE_MS) {
    return;
  }
  if (caseIndex >= (sizeof(CASES) / sizeof(CASES[0]))) {
    Serial.printf("%s\n", failures ? "FAILED" : "PASSED");
    exit(failures ? 1 : 0);
  }

  const Case& c = CASES[caseIndex];
  if (!sent) {
    static Packet p;
    buildQuery(p, c);
    largeAllocations = 0;
    gotReply = replyIsOurs = false;
    counting = true;
    client.beginPacket(WiFi.localIP(), MDNS_PORT);
    client.write(p.data, p.len);
    client.endPacket();
    caseStart = millis();
    sent = true;
    return;
  }

  if (int size = client.parsePacket()) {
    uint8_t reply[1400];
    size = client.read(reply, sizeof(reply));
    gotReply = true;
    // Our PTR answer carries our instance name as a label
    for (int i = 0; i + (int)sizeof(INSTANCE) <= size; ++i) {
      if ((sizeof(INSTANCE) - 1 == reply[i])
          && (0 == memcmp(&reply[i + 1], INSTANCE, sizeof(INSTANCE) - 1))) {
        replyIsOurs = true;
      }
    }
  }
  if (millis() - caseStart < REPLY_WAIT_MS) {
    return;
  }

  counting = false;
  // A reply must carry our record; where none is expected, any packet fails
  const bool replyOk = (c.expectReply ? (gotReply && replyIsOurs) : !gotReply);
  const bool ok = ((c.expectedAllocations == largeAllocations) && replyOk);
  Serial.printf("%s: %s: %zu answer allocations (expected %zu), %s (expected %s)\n",
                ok ? "PASS" : "FAIL", c.name, largeAllocations, c.expectedAllocations,
                gotReply ? (replyIsOurs ? "replied" : "a packet without our record") : "no reply",
                c.expectReply ? "a reply" : "none");
  failures += ok ? 0 : 1;
  ++caseIndex;
  sent = false;
}
