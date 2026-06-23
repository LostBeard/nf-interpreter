# libpeer + mbedTLS DTLS patches (SpawnWear Phase 7b)

**Files live in `$IDF_PATH/components/libpeer/` and `$IDF_PATH/components/mbedtls/mbedtls/library/`**
(`C:/Espressif/frameworks/esp-idf-v5.5.4/...`). The libpeer component is **fetched fresh** by the
IDF component manager and is NOT tracked in any repo; the mbedTLS library is the upstream IDF copy.
Both are lost on a clean re-fetch / IDF reinstall. **Re-apply everything here after a fresh fetch.**

Read via the `SpawnDev.WebRTC` interop `GetState(-1)` -> returns the `g_sw_dtls_cp` checkpoint
(also surfaced over the watch test server at `GET /webrtc-checkpoint`).

---

## STATE AS OF 2026-06-23 (pick-up point)

The watch <-> SipSorcery DTLS handshake is **bidirectional** (watch receives the ClientHello and
responds). It then fails: the watch's mbedTLS returns `MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE` (-0x6E00)
and sends `handshake_failure(40)`. **Key facts established:**

- It is NOT the TLS-1.2 cipher / curve / renegotiation-info / sig-alg checks - all instrumented,
  none fire (`g_sw_dtls_cp` stays 1 = "handshake entered", then the outer capture writes
  `0x16E00` = `0x10000 | 0x6E00`). So cipher+curve+sig-alg all PASS; failure is later/elsewhere.
- Both ends use ECDSA (watch cert = EC P-256 / SHA-256; SipSorcery defaults to ECDSA P-256), so
  it is NOT an RSA-vs-ECDSA cipher-list mismatch.
- **TLS 1.3 was never actually disabled until now.** The sdkconfig had a later
  `CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=y` that overrode an earlier `=n` (Kconfig: last line wins).
  Now genuinely `=n`. **TOMORROW'S FIRST TEST: rebuild (delete the stale generated `sdkconfig`
  first) + reflash + `/webrtc-connect` + read `/webrtc-checkpoint`.** If 1.3-off completes the
  handshake -> done. If not, the remaining uninstrumented `HANDSHAKE_FAILURE` sites are the
  client-key-exchange parse (`ssl_tls12_server.c` ~3728/3735/3744/3937) and `ssl_tls.c`
  8529/9107/9121 - instrument those next, OR capture the mbedTLS handshake **state** at failure
  (`#define MBEDTLS_ALLOW_PRIVATE_ACCESS` + read `ssl.MBEDTLS_PRIVATE(state)` in
  `dtls_srtp_do_handshake` on fatal ret -> names the exact failing step in one shot).
- The clean route (mbedTLS debug) is BLOCKED: `CONFIG_MBEDTLS_DEBUG=y` collides with nf's own
  crypto mbedtls config (`MBEDTLS_SSL_IN_CONTENT_LEN` etc. "redefined" -Werror). Do not re-enable.

---

## A. libpeer `src/peer_connection.c` (THE real DTLS recv + fail transition)

libpeer's actual DTLS recv is wired here (lines ~330-331:
`pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;`), NOT the `dtls_srtp_udp_recv` in
dtls_srtp.c (that is dead code in this flow). `agent_recv` demuxes STUN/DTLS and keeps ICE alive.

1. **Make `peer_connection_dtls_srtp_recv` non-blocking** (~line 62): return WANT_READ instead of
   blocking, so a stalled handshake can't freeze the (mutex-holding) pump:
   ```c
   ret = agent_recv(&pc->agent, buf, len);
   if (ret > 0) { return ret; }
   return MBEDTLS_ERR_SSL_WANT_READ;
   ```
2. **CONNECTED case (~line 428): transition to FAILED on a fatal handshake error** (else the failed
   handshake retries forever and the watch wedges):
   ```c
   case PEER_CONNECTION_CONNECTED: {
     int hs = dtls_srtp_handshake(&pc->dtls_srtp, NULL);
     if (hs == 0) { /* ...SCTP create... */ STATE_CHANGED(pc, PEER_CONNECTION_COMPLETED); }
     else if (hs != MBEDTLS_ERR_SSL_WANT_READ && hs != MBEDTLS_ERR_SSL_WANT_WRITE) {
       STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
     }
     break;
   }
   ```

## B. libpeer `src/dtls_srtp.c`

1. **Checkpoint global** (near top, RTC-noinit so it survives soft reset):
   `#include "esp_attr.h"` + `RTC_NOINIT_ATTR volatile uint32_t g_sw_dtls_cp;`
   Set to `1` on entering `dtls_srtp_handshake`. Read via `GetState(-1)`.
2. **set_bio back to NULL f_recv_timeout** (the recv-timeout approach below was ABANDONED):
   `mbedtls_ssl_set_bio(&dtls_srtp->ssl, dtls_srtp, dtls_srtp->udp_send, dtls_srtp->udp_recv, NULL);`
3. **Non-blocking handshake:**
   - `dtls_srtp_do_handshake`: single `ret = mbedtls_ssl_handshake(&dtls_srtp->ssl); return ret;`
     (no `do/while`; the `static` DTLS timer persists across calls).
   - `dtls_srtp_handshake_server`: body is just `return dtls_srtp_do_handshake(dtls_srtp);`
     (drop the `while(1)`+`mbedtls_ssl_session_reset`+`set_client_transport_id`).
   - **Disable DTLS cookies** in `dtls_srtp_init`:
     `mbedtls_ssl_conf_dtls_cookies(&dtls_srtp->conf, NULL, NULL, NULL);` (removes the HelloVerify
     session-reset retry that made the server handshake un-non-blockable).
4. **Outer `dtls_srtp_handshake` early-return + error capture** (BEFORE the `get_peer_cert`
   fingerprint check - that cert only exists once the handshake is DONE):
   ```c
   if (ret != 0) {
     if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && g_sw_dtls_cp < 0x30000u) {
       g_sw_dtls_cp = 0x10000u | ((unsigned int)(-ret) & 0xFFFFu);  // 0x10000|err unless mbedtls instrumentation set a 0x3xxxx reason
     }
     return ret;
   }
   ```
5. **`dtls_srtp_debug` keyword coding** (inside the existing `#if CONFIG_MBEDTLS_DEBUG`, currently
   DEAD because that config breaks nf's build): encodes the failure message into `g_sw_dtls_cp`
   (`0x2000X`). Harmless dead code; keep or drop.

## C. mbedTLS library instrumentation (TEMP DIAGNOSTIC - remove once DTLS works)

Each adds `extern volatile unsigned int g_sw_dtls_cp;` (after the `#include "ssl_misc.h"` /
`<string.h>`) and sets a distinct reason code right before a `HANDSHAKE_FAILURE` return:

- `library/ssl_tls12_server.c`: `0x30001` renegotiation-info, `0x30002` handshake_failure flag
  (version/compression/ext), `0x30003` "no ciphersuites in common", `0x30004` "no matching curve
  for ECDHE", `0x30005` "elliptic curve not supported".  **(none of these fired)**
- `library/ssl_tls.c`: `0x30010` "no signature algorithm in common".  **(did not fire)**
- `library/ssl_tls13_server.c`: `0x30020` TLS1.3 no ciphersuite, `0x30021` TLS1.3 no named group.
  **(did not fire)**

## D. sdkconfig (`targets/ESP32/_IDF/sdkconfig.default_octal_ble_qspi.esp32s3` - TRACKED in nf repo)

Phase 7b additions: `CONFIG_LWIP_IPV6=y` (libpeer hard-requires it), `CONFIG_MBEDTLS_SSL_PROTO_DTLS=y`,
`CONFIG_MBEDTLS_PEM_WRITE_C=y` (self-signed cert PEM), `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` +
`CONFIG_PTHREAD_TASK_STACK_SIZE_DEFAULT=8192` (DTLS handshake mem/stack), and now
**`CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=n`** (force DTLS 1.2; the prior `=y` silently won until 2026-06-23).
NOTE: the IDF feature components (sepfy/libpeer registry add) are in `CMakeLists`/component lists -
see `feedback-nf-registry-component-integration-gotchas` memory.

## E. native interop (`InteropAssemblies/SpawnDev.WebRTC/...PeerConnection.cpp` - TRACKED in nf repo)

`extern "C" volatile uint32_t g_sw_dtls_cp;` + `GetState(-1)` returns it. `xTaskCreate` stack 16384
(8192 too small for DTLS, 32768 fails to allocate from internal RAM). Checksum 0xD3101139.

## Build/flash loop
1. Edit the IDF files above (re-apply from this doc if re-fetched).
2. Delete the stale generated sdkconfig: `rm nf-interpreter/sdkconfig` (so the `.default` change takes).
3. `tools\nf-build-py313.bat ESP32_S3_BLE_QSPI`
4. BOOT dance (COM6) -> `tools\nf-flash-py313.bat COM6` -> power-cycle -> run mode (COM3/COM9).
5. Peer: `SpawnWear.Bridge.Desktop answerroom <room>`; watch: `GET /webrtc-connect` then
   `GET /webrtc-checkpoint`.
