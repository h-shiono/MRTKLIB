/*------------------------------------------------------------------------------
 * mrtk_stream.h : stream I/O type definitions and functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_stream.h
 * @brief MRTKLIB Stream Module — stream I/O abstraction layer.
 *
 * This header provides the stream_t type and related types (strconv_t,
 * strsvr_t) for managing various I/O streams (file, serial, TCP, NTRIP,
 * UDP, FTP/HTTP, memory buffer).
 *
 * @note Functions declared here are implemented in mrtk_stream.c.
 */
#ifndef MRTK_STREAM_H
#define MRTK_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Stream Type Constants
 *===========================================================================*/

#define STR_NONE 0     /* stream type: none */
#define STR_SERIAL 1   /* stream type: serial */
#define STR_FILE 2     /* stream type: file */
#define STR_TCPSVR 3   /* stream type: TCP server */
#define STR_TCPCLI 4   /* stream type: TCP client */
#define STR_NTRIPSVR 5 /* stream type: NTRIP server */
#define STR_NTRIPCLI 6 /* stream type: NTRIP client */
#define STR_FTP 7      /* stream type: ftp */
#define STR_HTTP 8     /* stream type: http */
#define STR_NTRIPCAS 9 /* stream type: NTRIP caster */
#define STR_UDPSVR 10  /* stream type: UDP server */
#define STR_UDPCLI 11  /* stream type: UDP client */
#define STR_MEMBUF 12  /* stream type: memory buffer */

#define MAXRCVFMT 12 /* max number of receiver format */

#define STR_MODE_R 0x1  /* stream mode: read */
#define STR_MODE_W 0x2  /* stream mode: write */
#define STR_MODE_RW 0x3 /* stream mode: read/write */

#define MSG_DISCONN "$_DISCONNECT\r\n" /* disconnect message */

/*============================================================================
 * Stream Types
 *===========================================================================*/

typedef struct stream_tag { /* stream type */
    int type;               /* type (STR_???) */
    int mode;               /* mode (STR_MODE_?) */
    int state;              /* state (-1:error,0:close,1:open) */
    uint32_t inb, inr;      /* input bytes/rate */
    uint32_t outb, outr;    /* output bytes/rate */
    uint32_t tick_i;        /* input tick tick */
    uint32_t tick_o;        /* output tick */
    uint32_t tact;          /* active tick */
    uint32_t inbt, outbt;   /* input/output bytes at tick */
    rtk_lock_t lock;        /* lock flag */
    void* port;             /* type dependent port control struct */
    char path[MAXSTRPATH];  /* stream path */
    char msg[MAXSTRMSG];    /* stream message */
} stream_t;

typedef struct {       /* stream converter type */
    int itype, otype;  /* input and output stream type */
    int nmsg;          /* number of output messages */
    int msgs[32];      /* output message types */
    double tint[32];   /* output message intervals (s) */
    uint32_t tick[32]; /* cycle tick of output message */
    int ephsat[32];    /* satellites of output ephemeris */
    int stasel;        /* station info selection (0:remote,1:local) */
    rtcm_t rtcm;       /* rtcm input data buffer */
    raw_t raw;         /* raw  input data buffer */
    rtcm_t out;        /* rtcm output data buffer */
} strconv_t;

typedef struct {                       /* stream server type */
    int state;                         /* server state (0:stop,1:running) */
    int cycle;                         /* server cycle (ms) */
    int buffsize;                      /* input/monitor buffer size (bytes) */
    int nmeacycle;                     /* NMEA request cycle (ms) (0:no) */
    int relayback;                     /* relay back of output streams (0:no) */
    int nstr;                          /* number of streams (1 input + (nstr-1) outputs */
    int npb;                           /* data length in peek buffer (bytes) */
    char cmds_periodic[16][MAXRCVCMD]; /* periodic commands */
    double nmeapos[3];                 /* NMEA request position (ecef) (m) */
    uint8_t* buff;                     /* input buffers */
    uint8_t* pbuf;                     /* peek buffer */
    uint32_t tick;                     /* start tick */
    stream_t stream[16];               /* input/output streams */
    stream_t strlog[16];               /* return log streams */
    strconv_t* conv[16];               /* stream converter */
    rtk_thread_t thread;               /* server thread */
    rtk_lock_t lock;                   /* lock flag */
} strsvr_t;

/*============================================================================
 * Global Variables
 *===========================================================================*/

extern const char* formatstrs[]; /* stream format strings */

/*============================================================================
 * Stream Functions
 *===========================================================================*/

/**
 * @brief Initialize stream communication system.
 */
void strinitcom(void);

/**
 * @brief Initialize stream structure.
 * @param[in,out] stream  stream struct
 */
void strinit(stream_t* stream);

/**
 * @brief Lock stream.
 * @param[in,out] stream  stream struct
 */
void strlock(stream_t* stream);

/**
 * @brief Unlock stream.
 * @param[in,out] stream  stream struct
 */
void strunlock(stream_t* stream);

/**
 * @brief Open stream.
 * @param[in,out] stream  stream struct
 * @param[in]     type    stream type (STR_???)
 * @param[in]     mode    stream mode (STR_MODE_?)
 * @param[in]     path    stream path
 * @return status (0:error,1:ok)
 */
int stropen(stream_t* stream, int type, int mode, const char* path);

/**
 * @brief Close stream.
 * @param[in,out] stream  stream struct
 */
void strclose(stream_t* stream);

/**
 * @brief Read data from stream.
 * @param[in,out] stream  stream struct
 * @param[out]    buff    data buffer
 * @param[in]     n       max data length (bytes)
 * @return data length (bytes)
 */
int strread(stream_t* stream, uint8_t* buff, int n);

/**
 * @brief Write data to stream.
 * @param[in,out] stream  stream struct
 * @param[in]     buff    data buffer
 * @param[in]     n       data length (bytes)
 * @return status (0:error,1:ok)
 */
int strwrite(stream_t* stream, uint8_t* buff, int n);

/**
 * @brief Sync streams.
 * @param[in,out] stream1  stream struct 1
 * @param[in,out] stream2  stream struct 2
 */
void strsync(stream_t* stream1, stream_t* stream2);

/**
 * @brief Get stream status.
 * @param[in]  stream  stream struct
 * @param[out] msg     status message (NULL: no output)
 * @return stream status (-1:error,0:close,1:wait,2:connect,3:active)
 */
int strstat(stream_t* stream, char* msg);

/**
 * @brief Get stream extended status.
 * @param[in]  stream  stream struct
 * @param[out] msg     status message (NULL: no output)
 * @return stream status
 */
int strstatx(stream_t* stream, char* msg);

/**
 * @brief Get stream statistics summary.
 * @param[in]  stream  stream struct
 * @param[out] inb     input bytes
 * @param[out] inr     input rate (bps)
 * @param[out] outb    output bytes
 * @param[out] outr    output rate (bps)
 */
void strsum(stream_t* stream, int* inb, int* inr, int* outb, int* outr);

/**
 * @brief Set stream options.
 * @param[in] opt  stream options (timeout/reconnect/rate/buffsize/fswapmargin)
 */
void strsetopt(const int* opt);

/**
 * @brief Get stream time.
 * @param[in] stream  stream struct
 * @return stream time
 */
gtime_t strgettime(stream_t* stream);

/**
 * @brief Send NMEA request to stream.
 * @param[in,out] stream  stream struct
 * @param[in]     sol     solution data
 */
void strsendnmea(stream_t* stream, const sol_t* sol);

/**
 * @brief Send receiver command to stream.
 * @param[in,out] stream  stream struct
 * @param[in]     cmd     receiver command
 */
void strsendcmd(stream_t* stream, const char* cmd);

/**
 * @brief Set stream timeout.
 * @param[in,out] stream   stream struct
 * @param[in]     toinact  inactive timeout (ms) (0:no timeout)
 * @param[in]     tirecon  reconnect interval (ms)
 */
void strsettimeout(stream_t* stream, int toinact, int tirecon);

/**
 * @brief Set FTP/HTTP local directory.
 * @param[in] dir  local directory
 */
void strsetdir(const char* dir);

/**
 * @brief Set HTTP/NTRIP proxy address.
 * @param[in] addr  proxy address
 */
void strsetproxy(const char* addr);

/**
 * @brief Set default NTRIP version for new connections.
 * @param[in] ver  NTRIP version (0:auto, 1:v1, 2:v2)
 *
 * When set to auto (0), the client tries NTRIP v2 first and falls back to v1
 * if the caster does not support it.  Per-stream override is available via
 * the ?ver=N query parameter in the stream path.
 */
void strsetntripver(int ver);

/*============================================================================
 * Stream Server Functions
 *===========================================================================*/

void strsvrinit(strsvr_t* svr, int nout);
int strsvrstart(strsvr_t* svr, int* opts, int* strs, char** paths, char** logs, strconv_t** conv, char** cmds,
                char** cmds_priodic, const double* nmeapos);
void strsvrstop(strsvr_t* svr, char** cmds);
void strsvrstat(strsvr_t* svr, int* stat, int* log_stat, int* byte, int* bps, char* msg);
strconv_t* strconvnew(int itype, int otype, const char* msgs, int staid, int stasel, const char* opt);
void strconvfree(strconv_t* conv);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_STREAM_H */
