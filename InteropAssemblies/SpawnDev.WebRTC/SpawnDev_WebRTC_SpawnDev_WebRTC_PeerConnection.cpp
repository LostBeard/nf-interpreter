//-----------------------------------------------------------------------------
//
//    SpawnDev.WebRTC - libpeer (sepfy) data-channel WebRTC, native binding.
//    (Generated skeleton from the MetadataProcessor; bodies implemented here.
//     The _mshl.cpp + .h are generated and must NOT be edited.)
//
//    Handle-based: a fixed table of libpeer PeerConnection* (libpeer's type is
//    ALSO named PeerConnection, so it is aliased LpPeer to avoid colliding with
//    this interop class). ONE FreeRTOS task pumps peer_connection_loop for every
//    active handle under a mutex; libpeer's callbacks (delivered config.user_data
//    = the slot) fire from inside that loop and stash the local SDP / inbound
//    messages / state into the slot. Managed calls take the same mutex.
//
//-----------------------------------------------------------------------------

#include "SpawnDev_WebRTC.h"
#include "SpawnDev_WebRTC_SpawnDev_WebRTC_PeerConnection.h"

// libpeer's public type is ALSO named PeerConnection, which collides with this interop's
// class. Rename it to LpPeerConn during peer.h parsing so the unqualified name 'PeerConnection'
// stays unambiguously the nf class (via the using-namespace below).
#define PeerConnection LpPeerConn
extern "C"
{
#include "peer.h"
}
#undef PeerConnection
typedef LpPeerConn LpPeer;

// SpawnWear (Phase 7b) DTLS crash localization: libpeer dtls_srtp.c writes this RTC-noinit
// checkpoint (survives the soft-reset reboot). Exposed via GetState(-1) so the managed side can
// read where the DTLS handshake died after a crash. Remove with the dtls_srtp.c checkpoints.
extern "C" volatile uint32_t g_sw_dtls_cp;

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"   // SpawnWear leak probe: free INTERNAL-RAM heap (GetState(-2))
#include <string.h>

using namespace SpawnDev_WebRTC::SpawnDev_WebRTC;

#define SW_MAX_PEERS 2
#define SW_MAX_SDP 4096
#define SW_RX_SLOTS 8
#define SW_RX_MSG_MAX 1024
#define SW_TX_SLOTS 8
#define SW_TX_MSG_MAX 512

struct SwPeerSlot
{
    bool inUse;
    LpPeer *pc;
    PeerConfiguration config; // libpeer copies it, but user_data points back here
    char localSdp[SW_MAX_SDP];
    volatile int localSdpLen; // volatile: read lock-free by GetLocalSdpLength (written last by the pump)
    volatile int state;
    // inbound: single-producer (loop task) / single-consumer (managed TryReceive) ring
    uint8_t rxBuf[SW_RX_SLOTS][SW_RX_MSG_MAX];
    int rxLen[SW_RX_SLOTS];
    volatile int rxHead; // written by loop task
    volatile int rxTail; // written by TryReceive
    // outbound: single-producer (managed Send) / single-consumer (loop task) ring. Send writes here
    // LOCK-FREE (no s_mutex) so it never blocks the cooperatively-scheduled CLR; the pump drains it
    // into libpeer under the mutex it already holds.
    uint8_t txBuf[SW_TX_SLOTS][SW_TX_MSG_MAX];
    int txLen[SW_TX_SLOTS];
    volatile int txHead; // written by Send
    volatile int txTail; // written by loop task
};

static SwPeerSlot s_slots[SW_MAX_PEERS];
static SemaphoreHandle_t s_mutex = NULL;
static TaskHandle_t s_loopTask = NULL;
static bool s_inited = false;

// ---- libpeer callbacks (user_data = the owning slot) ----

static void sw_on_icecandidate(char *sdp, void *ud)
{
    SwPeerSlot *s = (SwPeerSlot *)ud;
    if (s == NULL || sdp == NULL)
        return;
    // nanoPAL.h poisons strlen() (forces safe variants); compute the length manually.
    int n = 0;
    while (sdp[n] != 0)
        n++;
    if (n >= SW_MAX_SDP)
        n = SW_MAX_SDP - 1;
    memcpy(s->localSdp, sdp, n);
    s->localSdp[n] = 0;
    s->localSdpLen = n;
}

static void sw_on_state(PeerConnectionState st, void *ud)
{
    SwPeerSlot *s = (SwPeerSlot *)ud;
    if (s != NULL)
        s->state = (int)st;
}

static void sw_on_message(char *msg, size_t len, void *ud, uint16_t sid)
{
    (void)sid;
    SwPeerSlot *s = (SwPeerSlot *)ud;
    if (s == NULL || msg == NULL)
        return;
    int next = (s->rxHead + 1) % SW_RX_SLOTS;
    if (next == s->rxTail)
        return; // ring full - drop
    int n = (int)len;
    if (n > SW_RX_MSG_MAX)
        n = SW_RX_MSG_MAX;
    memcpy(s->rxBuf[s->rxHead], msg, n);
    s->rxLen[s->rxHead] = n;
    s->rxHead = next;
}

// ---- the single pump task ----

static void sw_loop_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        if (s_mutex != NULL)
            xSemaphoreTake(s_mutex, portMAX_DELAY);
        for (int i = 0; i < SW_MAX_PEERS; i++)
        {
            SwPeerSlot *s = &s_slots[i];
            if (s->inUse && s->pc != NULL)
            {
                peer_connection_loop(s->pc);
                // Drain the managed Send TX ring into libpeer (we hold s_mutex). SPSC: managed Send is
                // the producer (txHead); we are the sole consumer (txTail). This moves the actual
                // libpeer send OFF the managed thread so Send never blocks the CLR.
                while (s->txTail != s->txHead)
                {
                    int idx = s->txTail;
                    peer_connection_datachannel_send(s->pc, (char *)s->txBuf[idx], (size_t)s->txLen[idx]);
                    s->txTail = (idx + 1) % SW_TX_SLOTS;
                }
            }
        }
        if (s_mutex != NULL)
            xSemaphoreGive(s_mutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void sw_ensure_init()
{
    if (s_inited)
        return;
    s_mutex = xSemaphoreCreateMutex();
    peer_init();
    // The pump task runs peer_connection_loop, which drives the mbedTLS DTLS handshake - a deep
    // call chain with large stack frames. 8192 (libpeer's example value) overflowed here because
    // the nanoFramework interop entry frames also live on this stack; 32768 was too big to
    // allocate from internal RAM (xTaskCreate failed -> no pump -> no offer). 16384 both allocates
    // and leaves enough headroom for the handshake.
    xTaskCreate(sw_loop_task, "sw_lp_loop", 16384, NULL, 5, &s_loopTask);
    s_inited = true;
}

static SwPeerSlot *sw_slot(int handle)
{
    if (handle < 0 || handle >= SW_MAX_PEERS)
        return NULL;
    if (!s_slots[handle].inUse)
        return NULL;
    return &s_slots[handle];
}

// ---- interop methods ----

signed int PeerConnection::Create(HRESULT &hr)
{
    (void)hr;
    sw_ensure_init();

    int idx = -1;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < SW_MAX_PEERS; i++)
    {
        if (!s_slots[i].inUse)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
    {
        xSemaphoreGive(s_mutex);
        return -1;
    }

    SwPeerSlot *s = &s_slots[idx];
    memset(s, 0, sizeof(*s));
    s->config.audio_codec = CODEC_NONE;
    s->config.video_codec = CODEC_NONE;
    s->config.datachannel = DATA_CHANNEL_BINARY;
    s->config.ice_servers[0].urls = "stun:hub.spawndev.com:3478";
    s->config.user_data = s;

    s->pc = peer_connection_create(&s->config);
    if (s->pc == NULL)
    {
        memset(s, 0, sizeof(*s));
        xSemaphoreGive(s_mutex);
        return -1;
    }

    peer_connection_oniceconnectionstatechange(s->pc, sw_on_state);
    peer_connection_onicecandidate(s->pc, sw_on_icecandidate);
    peer_connection_ondatachannel(s->pc, sw_on_message, NULL, NULL);

    s->state = (int)PEER_CONNECTION_NEW;
    s->inUse = true;
    xSemaphoreGive(s_mutex);
    return idx;
}

void PeerConnection::CreateDataChannel(signed int param0, const char *param1, HRESULT &hr)
{
    (void)hr;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SwPeerSlot *s = sw_slot(param0);
    if (s != NULL)
        peer_connection_create_datachannel(s->pc, DATA_CHANNEL_RELIABLE, 0, 0,
                                           (char *)(param1 ? param1 : "data"), (char *)"");
    xSemaphoreGive(s_mutex);
}

void PeerConnection::CreateOffer(signed int param0, HRESULT &hr)
{
    (void)hr;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SwPeerSlot *s = sw_slot(param0);
    if (s != NULL)
    {
        s->localSdpLen = 0;
        peer_connection_create_offer(s->pc);
    }
    xSemaphoreGive(s_mutex);
}

void PeerConnection::CreateAnswer(signed int param0, HRESULT &hr)
{
    // libpeer 0.0.3 has no create_answer: the answer SDP is produced automatically
    // by SetRemoteDescription(offer) and delivered via the onicecandidate callback.
    (void)param0;
    (void)hr;
}

void PeerConnection::SetRemoteDescription(signed int param0, const char *param1, signed int param2, HRESULT &hr)
{
    (void)param2; // libpeer infers offer/answer from the SDP itself
    (void)hr;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SwPeerSlot *s = sw_slot(param0);
    if (s != NULL && param1 != NULL)
    {
        s->localSdpLen = 0; // a new local (answer) SDP may follow
        peer_connection_set_remote_description(s->pc, param1);
    }
    xSemaphoreGive(s_mutex);
}

void PeerConnection::AddIceCandidate(signed int param0, const char *param1, HRESULT &hr)
{
    (void)hr;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SwPeerSlot *s = sw_slot(param0);
    if (s != NULL && param1 != NULL)
        peer_connection_add_ice_candidate(s->pc, (char *)param1);
    xSemaphoreGive(s_mutex);
}

signed int PeerConnection::GetLocalSdpLength(signed int param0, HRESULT &hr)
{
    (void)hr;
    // LOCK-FREE (polled). localSdpLen is volatile and is written LAST by sw_on_icecandidate (after
    // the SDP body), so a non-zero read means the SDP is complete. Taking s_mutex here would block
    // the managed thread while the pump holds it across ICE/STUN gathering, freezing the CLR (UI).
    SwPeerSlot *s = sw_slot(param0);
    return (s != NULL) ? s->localSdpLen : 0;
}

void PeerConnection::GetLocalSdp(signed int param0, CLR_RT_TypedArray_UINT8 param1, HRESULT &hr)
{
    (void)hr;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SwPeerSlot *s = sw_slot(param0);
    if (s != NULL)
    {
        int cap = (int)param1.GetSize();
        int n = s->localSdpLen;
        if (n > cap)
            n = cap;
        if (n > 0)
            memcpy(param1.GetBuffer(), s->localSdp, n);
    }
    xSemaphoreGive(s_mutex);
}

signed int PeerConnection::Send(signed int param0, CLR_RT_TypedArray_UINT8 param1, signed int param2, HRESULT &hr)
{
    (void)hr;
    // LOCK-FREE: enqueue into the slot's TX ring (single-producer = this managed thread / single-
    // consumer = the pump). No s_mutex, so a Send NEVER blocks the cooperatively-scheduled CLR - a
    // blocking Send was wedging the UI (touch/watchface) when streaming telemetry. The pump drains the
    // ring into libpeer under the mutex it already holds.
    SwPeerSlot *s = sw_slot(param0);
    if (s == NULL) return -1;
    int len = param2;
    int cap = (int)param1.GetSize();
    if (len > cap) len = cap;
    if (len > SW_TX_MSG_MAX) len = SW_TX_MSG_MAX; // oversize clamped; the bus chunks larger payloads
    int next = (s->txHead + 1) % SW_TX_SLOTS;
    if (next == s->txTail) return -1; // ring full - drop (never block); caller may retry next tick
    if (len > 0) memcpy(s->txBuf[s->txHead], param1.GetBuffer(), len);
    s->txLen[s->txHead] = len;
    s->txHead = next;
    return len;
}

signed int PeerConnection::TryReceive(signed int param0, CLR_RT_TypedArray_UINT8 param1, HRESULT &hr)
{
    (void)hr;
    // LOCK-FREE (polled every ~200 ms by the managed receive loop). The rx ring is single-producer
    // (sw_on_message, from the pump task) / single-consumer (this call); rxHead/rxTail are volatile.
    // Reading without s_mutex is both safe (SPSC) and REQUIRED: the pump holds s_mutex across the whole
    // peer_connection_loop (incl. the blocking DTLS/SCTP recv), so taking it here blocks this managed
    // thread, and a blocking native call freezes nanoFramework's cooperatively-scheduled CLR -> the UI
    // (touch, watchface) hangs until reset. Never block the CLR here.
    SwPeerSlot *s = sw_slot(param0);
    signed int n = 0;
    if (s != NULL && s->rxTail != s->rxHead)
    {
        int idx = s->rxTail;
        n = s->rxLen[idx];
        int cap = (int)param1.GetSize();
        if (n > cap)
            n = cap;
        if (n > 0)
            memcpy(param1.GetBuffer(), s->rxBuf[idx], n);
        s->rxTail = (idx + 1) % SW_RX_SLOTS;
    }
    return n;
}

signed int PeerConnection::GetState(signed int param0, HRESULT &hr)
{
    (void)hr;
    // SpawnWear (Phase 7b) diagnostic: handle -1 returns the RTC-noinit DTLS checkpoint (survives
    // the crash reboot) instead of a slot state - lets the managed side localize the DTLS crash.
    if (param0 == -1)
        return (signed int)g_sw_dtls_cp;
    // SpawnWear generic heap diagnostics: -2 = free INTERNAL-RAM bytes; -3 = largest free INTERNAL block;
    // -4 = free PSRAM bytes; -5 = largest free PSRAM block. Kept as a lightweight always-available memory
    // probe (no libpeer deps). The 2026-06-30 connect-leak hunt used these to PROVE there is no per-session
    // leak (settled cross-session free heap flat in both heaps incl a 120s sustained-telemetry hold).
    if (param0 == -2)
        return (signed int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (param0 == -3)
        return (signed int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (param0 == -4)
        return (signed int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (param0 == -5)
        return (signed int)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    // LOCK-FREE (polled). s->state is volatile, written by sw_on_state from the pump task. Taking
    // s_mutex here would block the managed thread while the pump holds it across the slow DTLS
    // handshake - freezing the cooperatively-scheduled CLR (and the UI) for the entire connect.
    SwPeerSlot *s = sw_slot(param0);
    return (s != NULL) ? s->state : (int)PEER_CONNECTION_CLOSED;
}

void PeerConnection::Close(signed int param0, HRESULT &hr)
{
    (void)hr;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SwPeerSlot *s = sw_slot(param0);
    if (s != NULL)
    {
        if (s->pc != NULL)
        {
            peer_connection_close(s->pc);
            peer_connection_destroy(s->pc);
        }
        memset(s, 0, sizeof(*s));
    }
    xSemaphoreGive(s_mutex);
}
