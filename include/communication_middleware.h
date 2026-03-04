/**
 * @file   communication_middleware.h
 * @brief  C declarations for the communication-middleware layer implemented
 *         in Rust (rust/src/communication_middleware.rs → libhselib_rust.a).
 *
 * This header covers all UART-level communication between the HSE-side firmware
 * and the application processor.  It is intentionally kept separate from the
 * HSE security-service API (hse_rust.h / hse_host.h).
 *
 * Hardware channel assignments (S32K344):
 *   LPUART instance  3  — default send/receive backend
 *   FlexIO TX channel 0 — optional TX backend (send_to_ap2 / recv_from_ap2)
 *   FlexIO RX channel 1 — optional RX backend (send_to_ap2 / recv_from_ap2)
 *
 * All functions return true on success and false on any of the following:
 *   - NULL pointer argument
 *   - len == 0 or len > COMM_MAX_TRANSFER_LEN
 *   - underlying UART driver error
 *   - busy-poll timeout
 */

#ifndef COMMUNICATION_MIDDLEWARE_H
#define COMMUNICATION_MIDDLEWARE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------- */

/**
 * @brief  Maximum number of bytes accepted by a single send or receive call.
 *
 * Matches MAX_TRANSFER_LEN defined in rust/src/communication_middleware.rs.
 */
#define COMM_MAX_TRANSFER_LEN  256U

/* -------------------------------------------------------------------------
 * Send functions
 * ------------------------------------------------------------------------- */

/**
 * @brief  Transmit @p len bytes from @p data to the application processor
 *         over LPUART (instance 3).
 *
 * @param  data  Pointer to the data buffer to send (must not be NULL).
 * @param  len   Number of bytes to send (1 .. COMM_MAX_TRANSFER_LEN).
 * @return true on success, false on error.
 */
bool send_to_ap(const uint8_t *data, uint32_t len);

/**
 * @brief  Transmit @p len bytes from @p data to the application processor,
 *         with explicit backend selection.
 *
 * @param  data        Pointer to the data buffer to send (must not be NULL).
 * @param  len         Number of bytes to send (1 .. COMM_MAX_TRANSFER_LEN).
 * @param  use_flexio  false → LPUART instance 3;
 *                     true  → FlexIO UART TX channel 0.
 * @return true on success, false on error.
 */
bool send_to_ap2(const uint8_t *data, uint32_t len, bool use_flexio);

/* -------------------------------------------------------------------------
 * Receive functions
 * ------------------------------------------------------------------------- */

/**
 * @brief  Receive @p len bytes from the application processor into @p buf
 *         over LPUART (instance 3).
 *
 * @param  buf  Destination buffer (must not be NULL).
 * @param  len  Number of bytes to receive (1 .. COMM_MAX_TRANSFER_LEN).
 * @return true on success, false on error.
 */
bool recv_from_ap(uint8_t *buf, uint32_t len);

/**
 * @brief  Receive @p len bytes from the application processor into @p buf,
 *         with explicit backend selection.
 *
 * @param  buf         Destination buffer (must not be NULL).
 * @param  len         Number of bytes to receive (1 .. COMM_MAX_TRANSFER_LEN).
 * @param  use_flexio  false → LPUART instance 3;
 *                     true  → FlexIO UART RX channel 1.
 * @return true on success, false on error.
 */
bool recv_from_ap2(uint8_t *buf, uint32_t len, bool use_flexio);

/* -------------------------------------------------------------------------
 * Persistent-data request functions
 * ------------------------------------------------------------------------- */

/**
 * @brief  Request that the application processor persists @p len bytes of
 *         @p data.
 *
 * Currently implemented as a raw LPUART send.  Protocol-level framing
 * (command byte, acknowledgement, etc.) will be added in a future revision.
 *
 * @param  data  Pointer to the data to persist (must not be NULL).
 * @param  len   Number of bytes (1 .. COMM_MAX_TRANSFER_LEN).
 * @return true on success, false on error.
 */
bool req_save_data(const uint8_t *data, uint32_t len);

/**
 * @brief  Request that the application processor returns previously saved
 *         data into @p buf.
 *
 * Currently implemented as a raw LPUART receive.  Protocol-level framing
 * will be added in a future revision.
 *
 * @param  buf  Destination buffer (must not be NULL).
 * @param  len  Number of bytes to receive (1 .. COMM_MAX_TRANSFER_LEN).
 * @return true on success, false on error.
 */
bool req_saved_data(uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* COMMUNICATION_MIDDLEWARE_H */
