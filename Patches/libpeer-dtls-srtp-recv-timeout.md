# libpeer patch: DTLS recv timeout (Phase 7b)

**File:** `$IDF_PATH/components/libpeer/src/dtls_srtp.c` (sepfy/libpeer registry component).
This component is **fetched fresh** by the IDF component manager and is NOT tracked in any of
our repos, so this change is lost on a clean re-fetch. Re-apply it after fetching libpeer.

## Why
libpeer configures a DTLS retransmission timer (`mbedtls_ssl_set_timer_cb`) but then hands
mbedTLS a **forever-blocking** `f_recv` (`dtls_srtp_udp_recv` spins `while (recvfrom <= 0)`)
and passes `f_recv_timeout = NULL`. So mbedTLS can never time out to retransmit a lost flight,
and `peer_connection_loop` never returns - on the SpawnWear watch this **froze** the device
(the caller holds a mutex across the loop). Giving mbedTLS a real timeout-aware recv fixes it.

## Change 1 - bound the blocking `dtls_srtp_udp_recv` spin (safety cap)
```c
int dtls_srtp_udp_recv(void* ctx, uint8_t* buf, size_t len) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  UdpSocket* udp_socket = (UdpSocket*)dtls_srtp->user_data;

  int ret;
  int spins = 0;                                   // <-- ADDED

  while ((ret = udp_socket_recvfrom(udp_socket, &udp_socket->bind_addr, buf, len)) <= 0) {
    ports_sleep_ms(1);
    if (++spins >= 2000) {                         // <-- ADDED (~2s safety cap)
      return MBEDTLS_ERR_SSL_TIMEOUT;              // <-- ADDED
    }                                              // <-- ADDED
  }

  LOGD("dtls_srtp_udp_recv (%d)", ret);
  return ret;
}
```

## Change 2 - add a timeout-aware recv (immediately after `dtls_srtp_udp_recv`)
```c
int dtls_srtp_udp_recv_timeout(void* ctx, uint8_t* buf, size_t len, uint32_t timeout) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  UdpSocket* udp_socket = (UdpSocket*)dtls_srtp->user_data;

  int ret;
  uint32_t waited = 0;
  if (timeout == 0) {
    timeout = 1000;
  }

  while ((ret = udp_socket_recvfrom(udp_socket, &udp_socket->bind_addr, buf, len)) <= 0) {
    ports_sleep_ms(5);
    waited += 5;
    if (waited >= timeout) {
      return MBEDTLS_ERR_SSL_TIMEOUT;
    }
  }

  return ret;
}
```

## Change 3 - wire it into the BIO (in `dtls_srtp_do_handshake`, the `mbedtls_ssl_set_bio` call)
```c
// was: mbedtls_ssl_set_bio(&dtls_srtp->ssl, dtls_srtp, dtls_srtp->udp_send, dtls_srtp->udp_recv, NULL);
mbedtls_ssl_set_bio(&dtls_srtp->ssl, dtls_srtp, dtls_srtp->udp_send, dtls_srtp->udp_recv, dtls_srtp_udp_recv_timeout);
```

## Status
This fix removed the **freeze**. A separate native crash (the watch reboots the instant the
DTLS handshake starts on the pump task) is still under investigation - see the SpawnWear
project memory and `Plans/phase7b-libpeer-integration.md`.
