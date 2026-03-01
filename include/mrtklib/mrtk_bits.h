/*------------------------------------------------------------------------------
 * mrtk_bits.h : bit manipulation and CRC functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_bits.h
 * @brief MRTKLIB Bits Module — Binary bit operations and CRC functions.
 *
 * This header provides bit-level extraction/insertion functions
 * (getbitu, getbits, setbitu, setbits), CRC parity computation
 * (rtk_crc32, rtk_crc24q, rtk_crc16), and GPS navigation word
 * decoding (decode_word), extracted from rtkcmn.c with zero
 * algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_bits.c.
 */
#ifndef MRTK_BITS_H
#define MRTK_BITS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * Bit Extraction / Insertion Functions
 *===========================================================================*/

/**
 * @brief Extract unsigned bits from byte data.
 * @param[in] buff  Byte data
 * @param[in] pos   Bit position from start of data (bits)
 * @param[in] len   Bit length (bits) (len<=32)
 * @return Extracted unsigned bits
 */
uint32_t getbitu(const uint8_t *buff, int pos, int len);

/**
 * @brief Extract signed bits from byte data.
 * @param[in] buff  Byte data
 * @param[in] pos   Bit position from start of data (bits)
 * @param[in] len   Bit length (bits) (len<=32)
 * @return Extracted signed bits
 */
int32_t getbits(const uint8_t *buff, int pos, int len);

/**
 * @brief Set unsigned bits in byte data.
 * @param[in,out] buff  Byte data
 * @param[in]     pos   Bit position from start of data (bits)
 * @param[in]     len   Bit length (bits) (len<=32)
 * @param[in]     data  Unsigned data to set
 */
void setbitu(uint8_t *buff, int pos, int len, uint32_t data);

/**
 * @brief Set signed bits in byte data.
 * @param[in,out] buff  Byte data
 * @param[in]     pos   Bit position from start of data (bits)
 * @param[in]     len   Bit length (bits) (len<=32)
 * @param[in]     data  Signed data to set
 */
void setbits(uint8_t *buff, int pos, int len, int32_t data);

/*============================================================================
 * CRC Parity Functions
 *===========================================================================*/

/**
 * @brief Compute CRC-32 parity for NovAtel raw data.
 * @param[in] buff  Data buffer
 * @param[in] len   Data length (bytes)
 * @return CRC-32 parity
 * @note See NovAtel OEMV firmware manual 1.7 32-bit CRC
 */
uint32_t rtk_crc32(const uint8_t *buff, int len);

/**
 * @brief Compute CRC-24Q parity for SBAS and RTCM3.
 * @param[in] buff  Data buffer
 * @param[in] len   Data length (bytes)
 * @return CRC-24Q parity
 */
uint32_t rtk_crc24q(const uint8_t *buff, int len);

/**
 * @brief Compute CRC-16 parity for BINEX and NVS.
 * @param[in] buff  Data buffer
 * @param[in] len   Data length (bytes)
 * @return CRC-16 parity
 */
uint16_t rtk_crc16(const uint8_t *buff, int len);

/*============================================================================
 * Navigation Data Word Decoding
 *===========================================================================*/

/**
 * @brief Check parity and decode GPS navigation data word.
 * @param[in]  word  Navigation data word (2+30 bit)
 *                   (previous word D29*-30* + current word D1-30)
 * @param[out] data  Decoded navigation data without parity (8bit x 3)
 * @return Status (1:ok, 0:parity error)
 * @note See reference [1] 20.3.5.2 user parity algorithm
 */
int decode_word(uint32_t word, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_BITS_H */
