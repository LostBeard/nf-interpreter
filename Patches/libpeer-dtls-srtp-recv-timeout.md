# libpeer + mbedTLS WebRTC patches (SpawnWear Phase 7b)

**Files live in `$IDF_PATH/components/libpeer/` and `$IDF_PATH/components/mbedtls/mbedtls/library/`**
(`C:/Espressif/frameworks/esp-idf-v5.5.4/...`). The libpeer component is a **vendored copy** (no
live `.git`) fetched by the IDF component manager; the mbedTLS library is the upstream IDF copy.
Both are lost on a clean re-fetch / IDF reinstall. **Re-apply everything here after a fresh fetch.**

> **Long-term TODO:** fork libpeer (`LostBeard/libpeer`) and wire it as a git submodule like the
> SipSorcery / ILGPU forks, so these become tracked commits instead of a re-apply doc. Until then,
> this doc is the source of truth - keep it in sync with the IDF files.

Checkpoint read path: `SpawnDev.WebRTC` interop `GetState(-1)` -> `g_sw_dtls_cp` (also
`GET http://192.168.1.170:8080/webrtc-checkpoint`).

---

## STATE AS OF 2026-06-23 PM: FULL WebRTC TRANSPORT WORKS END-TO-END ✅

watch(libpeer) <-> SipSorcery(SpawnDev.RTC desktop fork): **ICE -> DTLS -> SCTP -> DCEP data
channel** all complete; the watch's "ping from watch" bytes were delivered to SipSorcery
(`OnDataChannel FIRED label='data' state=open`, `RECV BINARY 15B: 'ping from watch'`).

Two root causes were found+fixed this session (both verified with instrumentation, not guessed):

1. **DTLS `handshake_failure(40)`/-0x6E00** - libpeer generated an **RSA** DTLS cert
   (`CONFIG_DTLS_USE_ECDSA` defaulted 0). SipSorcery offers ONLY ECDHE-**ECDSA** suites, so the
   watch (DTLS server) found the common suite but `mbedtls_pk_can_do(RSA_key, ECDSA)` failed in
   `ssl_pick_cert` -> "no usable ciphersuite" -> fail at `ssl_parse_client_hello` (**state=1**,
   before ServerHello). Diagnostic leaf code was `0x30041`. The earlier theory in this doc
   ("bidirectional / both ECDSA / fails later / TLS 1.3") was ALL WRONG - TLS 1.3 was a red herring.
   **FIX = section F.**

2. **Data channel never opened** - after SCTP connected, SipSorcery warned
   `no channel found for stream ID 0`. The watch (DTLS server) was opening its DCEP channel + sending
   data on **even SCTP stream 0**, but RFC 8832 §6 requires the DTLS **server** to use **ODD**
   streams. This build forces `-DCONFIG_USE_USRSCTP=0` (manual SCTP) + `-DCONFIG_DATA_BUFFER_SIZE=102400`
   (buffered send) in `libpeer/CMakeLists.txt:14`, so the LIVE send path was the buffered data-drain
   + manual chunk builder, both hardcoding sid 0. Also the watch never sent a DCEP `DATA_CHANNEL_OPEN`
   because the managed `CreateDataChannel` ran before SCTP was up (no-op). **FIX = section G.**

Known follow-ups: ICE is flaky ~50% (answerroom 5-PC outbound-offer swarm + stale room candidates
cause a transient checking->failed->connected that can kill the SCTP handshake; re-run succeeds -
NOT a transport-correctness bug). mbedTLS diagnostics in section C are TEMP - remove once stable.

---

## A. libpeer `src/peer_connection.c` - non-blocking DTLS recv + FAILED transition

libpeer's real DTLS recv is `peer_connection_dtls_srtp_recv` (~line 62), wired at
`pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;` (NOT `dtls_srtp_udp_recv`, dead here).
`agent_recv` demuxes STUN/DTLS and keeps ICE alive.

1. **`peer_connection_dtls_srtp_recv` non-blocking** (~line 62): `agent_recv`; if `ret>0` return it,
   else `return MBEDTLS_ERR_SSL_WANT_READ;` (so a stalled handshake can't freeze the mutex-holding pump).
2. **CONNECTED case -> FAILED on fatal handshake error** (~line 432): `int hs = dtls_srtp_handshake(...)`;
   `hs==0` -> SCTP create + `STATE_CHANGED(..., PEER_CONNECTION_COMPLETED)`; else if
   `hs != WANT_READ && hs != WANT_WRITE` -> `STATE_CHANGED(..., PEER_CONNECTION_FAILED)`.

## B. libpeer `src/dtls_srtp.c` - checkpoint + non-blocking handshake

1. **Checkpoint global** (after the early includes): `#include "esp_attr.h"` +
   `RTC_NOINIT_ATTR volatile uint32_t g_sw_dtls_cp;` set `=1` on entering `dtls_srtp_handshake`.
2. `set_bio` with NULL `f_recv_timeout` (recv-timeout approach abandoned).
3. **Non-blocking handshake:** `dtls_srtp_do_handshake` = single `ret = mbedtls_ssl_handshake(&ssl); return ret;`
   (no do/while; static DTLS timer persists). `dtls_srtp_handshake_server` body =
   `return dtls_srtp_do_handshake(dtls_srtp);` (drop while(1)+session_reset). **Disable DTLS cookies**
   in `dtls_srtp_init`: `mbedtls_ssl_conf_dtls_cookies(&conf, NULL, NULL, NULL);`.
4. **Outer `dtls_srtp_handshake` early-return + error capture** before the get_peer_cert check:
   `if (ret != WANT_READ && ret != WANT_WRITE && g_sw_dtls_cp < 0x30000u) g_sw_dtls_cp = 0x10000u | ((-ret) & 0xFFFF);`
5. **TEMP state-capture** (added 2026-06-23, remove later): `#define MBEDTLS_ALLOW_PRIVATE_ACCESS`
   before `#include "mbedtls/ssl.h"`; in the capture above, pack the handshake state:
   `g_sw_dtls_cp = (0x40u<<24) | ((ssl.MBEDTLS_PRIVATE(state)&0xFF)<<16) | ((-ret)&0xFFFF);` (0x40_SS_EEEE).

## C. mbedTLS library instrumentation (TEMP DIAGNOSTIC - remove once stable)

Each adds `extern volatile unsigned int g_sw_dtls_cp;` and sets a distinct code before a
`HANDSHAKE_FAILURE`/reject. `library/ssl_tls12_server.c`: 0x30001 renegotiation-info, 0x30002
hs-failure flag, 0x30003 no common suite, 0x30004/5 curve. **2026-06-23 ADDED** (the ones that
localized bug #1): in `ssl_ciphersuite_match` 0x30030 version / 0x30031 no-common-EC-curve /
0x30033 no-hash; in `ssl_pick_cert` 0x30040 no-cert / **0x30041 cert-key-cant-do-pk_alg (FIRED)** /
0x30042 keyUsage / 0x30043 cert-EC-curve; in `ssl_parse_client_hello` 0x30006 renego-SCSV /
0x30007 common-but-unusable. **ALL of section C is temporary - strip when closing the WebRTC work.**

## D. sdkconfig (`targets/ESP32/_IDF/sdkconfig.default_octal_ble_qspi.esp32s3` - TRACKED in nf repo)

Phase 7b: `CONFIG_LWIP_IPV6=y`, `CONFIG_MBEDTLS_SSL_PROTO_DTLS=y`, `CONFIG_MBEDTLS_PEM_WRITE_C=y`,
`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`, `CONFIG_PTHREAD_TASK_STACK_SIZE_DEFAULT=8192`,
`CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=n` (single line - confirm no later `=y` wins; delete the generated
`nf-interpreter/sdkconfig` so the `.default` re-derives). TLS 1.3 off did NOT fix the handshake
(red herring) but is correct to keep (force clean DTLS 1.2).

## E. native interop (`InteropAssemblies/SpawnDev.WebRTC/...PeerConnection.cpp` - TRACKED in nf repo)

`extern "C" volatile uint32_t g_sw_dtls_cp;` + `GetState(-1)` returns it. DTLS `xTaskCreate` stack
16384. `CreateDataChannel` -> `peer_connection_create_datachannel(...,"data",...)`; `Send` ->
`peer_connection_datachannel_send`.

## F. libpeer `src/config.h` - ENABLE ECDSA (fix #1, DTLS handshake_failure)

Line 5 was `// #define CONFIG_DTLS_USE_ECDSA 1` (commented) and lines 28-29 default it to 0 ->
`dtls_srtp_selfsign_cert` took the `#else` RSA branch. **Uncomment so it is 1:**
```c
#define CONFIG_DTLS_USE_ECDSA 1
```
-> watch presents ECDSA P-256 (WebRTC standard; matches SipSorcery's ECDSA-only suite offer).

## G. DCEP SCTP stream-id parity (fix #2, data channel) - RFC 8832 §6 server=ODD, client=EVEN

### G.1 libpeer `src/peer_connection.c`
Add a role->sid helper just above `peer_connection_datachannel_send`:
```c
static inline uint16_t peer_connection_default_dc_sid(PeerConnection* pc) {
  return pc->dtls_srtp.role == DTLS_SRTP_ROLE_SERVER ? 1 : 0;
}
```
Use it in THREE places that hardcoded 0:
- `peer_connection_datachannel_send` -> `peer_connection_datachannel_send_sid(pc, message, len, peer_connection_default_dc_sid(pc));`
- `peer_connection_create_datachannel` -> `..._sid(..., peer_connection_default_dc_sid(pc));`
- **THE LIVE PATH** - the data-ring-buffer drain in `peer_connection_loop` (COMPLETED state, the
  `#if (CONFIG_DATA_BUFFER_SIZE) > 0` block, ~line 480): replace both
  `sctp_outgoing_data(&pc->sctp, (char*)data, bytes, PPID_*, 0)` with
  `... , peer_connection_default_dc_sid(pc))` (compute `uint16_t dc_sid` once).

### G.2 libpeer `src/sctp.c` (manual path, since CONFIG_USE_USRSCTP=0)
In `sctp_outgoing_data` (the `#else` non-usrsctp branch, ~line 127) the DATA chunk hardcoded
`chunk->sid = htons(0)`; change to honor the param: `chunk->sid = htons(sid);`.

### G.3 managed `SpawnWear/Program.cs` `WebRtcConnectRun` (TRACKED in SpawnWear repo)
`peer_connection_create_datachannel` no-ops when SCTP isn't connected, so the early
`CreateDataChannel(h, "data")` sent NO DCEP OPEN. **Move it to AFTER `StateCompleted`** (the
`if (connected)` block, +~500ms) so the OPEN is sent on the live SCTP association. The offer's
`m=application` line comes from the PC config, not that call, so the SDP is unaffected.

## H. libpeer `src/sctp.c` - deliver BINARY data-channel messages on RECEIVE (fix #3, Phase 7c)

The manual SCTP receive (`sctp_handle_sctp_packet`, the `SCTP_DATA` case ~line 278) only delivered
`onmessage` for the `DATA_CHANNEL_PPID_DOMSTRING` ppid - every **BINARY** data-channel message was
silently dropped on RECEIVE (the watch had only ever SENT before, so this was never hit). The
Ed25519 challenge nonces/responses (and all app data) are binary. Deliver BINARY + the partial
variants too (mirrors usrsctp's `sctp_handle_incoming_data`):
```c
} else if (ntohl(data_chunk->ppid) == DATA_CHANNEL_PPID_DOMSTRING ||
           ntohl(data_chunk->ppid) == DATA_CHANNEL_PPID_BINARY ||
           ntohl(data_chunk->ppid) == DATA_CHANNEL_PPID_DOMSTRING_PARTIAL ||
           ntohl(data_chunk->ppid) == DATA_CHANNEL_PPID_BINARY_PARTIAL) {
  if (sctp->onmessage) { sctp->onmessage(...); }
}
```
With this, the watch-side Ed25519 mutual challenge over the data channel completes (answerroom logs
"CONNECTED + verified"). Phase 7c done.

## I. Watch is the DTLS CLIENT, not server (fix #4, Phase 7c browser interop)

Chrome/Firefox send a LARGE DTLS ClientHello that exceeds the DTLS MTU and is FRAGMENTED across
handshake records. mbedTLS's SERVER refuses to reassemble the initial ClientHello
(`ssl_tls12_server.c` ~line 1111: "ClientHello fragmentation not supported" ->
`MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE` = -0x7080, captured as `g_sw_dtls_cp` state=1 err=0x7080).
SipSorcery worked only because its ClientHello is small (one fragment).

FIX: make the watch (the OFFERER) the DTLS CLIENT instead of server. As the client, the watch sends
its own SMALL ClientHello, and mbedTLS's CLIENT reads the peer's flight via the normal read_record
path which DOES reassemble fragments. The answerer (browser/SipSorcery) becomes the DTLS server
(setup:passive). No mbedTLS surgery; works with any browser, no flags.
- `peer_connection.c` ~line 426 (PEER_CONNECTION_NEW, the offerer): `DTLS_SRTP_ROLE_SERVER` ->
  `DTLS_SRTP_ROLE_CLIENT`. The offer SDP then advertises `a=setup:active`.
- `dtls_srtp.c` `dtls_srtp_handshake_client`: only LOGE on a real error, not WANT_READ/WANT_WRITE
  (non-blocking client steps the handshake across many loop calls).
- The role-based SCTP stream-id (section G.1) auto-switches the data channel to EVEN stream 0
  (correct for the DTLS client per RFC 8832 §6); the answerer-server accepts the client's even stream.
- mbedTLS server cert-verify limitation is now MOOT for the watch (it no longer parses ClientHellos).
  The mbedTLS diagnostics in section C can be stripped any time.
- `dtls_srtp.c` `dtls_srtp_init` after `mbedtls_ssl_setup`: add
  `mbedtls_ssl_set_hostname(&dtls_srtp->ssl, NULL);`. As the DTLS CLIENT, mbedTLS 3.6+ refuses to
  verify the server cert unless set_hostname was called explicitly
  (`MBEDTLS_ERR_SSL_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME` = -0x5D80 at SERVER_CERTIFICATE).
  WebRTC has no hostname (identity = cert fingerprint), so opt out with NULL.

## Build/flash loop
1. Re-apply A-G to the IDF files if re-fetched. 2. `rm nf-interpreter/sdkconfig`.
3. `tools\nf-build-py313.bat ESP32_S3_BLE_QSPI`. 4. BOOT dance (COM6) ->
`tools\nf-flash-py313.bat COM6` -> power-cycle -> runtime. 5. Managed changes deploy over COM3 with
`dotnet run tools/nf-deploy.cs "SpawnWear\bin\Debug" COM3 <secs>` (NO BOOT dance; reboots the watch).
6. Test: `SpawnWear.Bridge.Desktop dcdiag <room>` (or `answerroom`), then `GET /webrtc-connect`;
watch `dcdiag` for `*** OnDataChannel FIRED ***` + `RECV BINARY ... 'ping from watch'`.
