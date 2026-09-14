/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Paul Sokolovsky
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "py/runtime.h"
#include "py/stream.h"
#include "extmod/modwebsocket.h"

#if MICROPY_PY_WEBSOCKET

enum { FRAME_HEADER, FRAME_OPT, PAYLOAD, CONTROL };

enum { BLOCKING_WRITE = 0x80 };

// A blocking write waits at most this long for send-buffer space. lwIP only
// waits once the peer has fallen a full window behind, so a live client on
// a lossy link still gets one retransmit recovery, while a client that
// stopped reading costs the event loop one second before it is dropped.
#define WEBSOCKET_WRITE_TIMEOUT_S (1)

typedef struct _mp_obj_websocket_t {
    mp_obj_base_t base;
    mp_obj_t sock;
    uint32_t msg_sz;
    byte mask[4];
    byte state;
    byte to_recv;
    byte mask_pos;
    byte buf_pos;
    byte buf[6];
    byte opts;
    // Copy of last data frame flags
    byte ws_flags;
    // Copy of current frame flags
    byte last_flags;
    // Set once the peer closed (socket EOF or a CLOSE frame). A closed
    // socket reports readable-EOF forever; without suppressing that in
    // POLL an asyncio reader (WebREPL via aiorepl's stdin) spins on it and
    // starves the event loop.
    byte closed;
    // One payload byte fetched by MP_STREAM_POLL and held for the next
    // read, so "readable" is only ever reported with a byte in hand.
    byte has_peek;
    byte peek;
} mp_obj_websocket_t;

static mp_uint_t websocket_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode);
static mp_uint_t websocket_write_raw(mp_obj_t self_in, const byte *header, int hdr_sz, const void *buf, mp_uint_t size, int *errcode);

static mp_obj_t websocket_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 2, false);
    mp_get_stream_raise(args[0], MP_STREAM_OP_READ | MP_STREAM_OP_WRITE | MP_STREAM_OP_IOCTL);
    mp_obj_websocket_t *o = mp_obj_malloc(mp_obj_websocket_t, type);
    o->sock = args[0];
    o->state = FRAME_HEADER;
    o->to_recv = 2;
    o->mask_pos = 0;
    o->buf_pos = 0;
    o->opts = FRAME_TXT;
    o->closed = 0;
    o->has_peek = 0;
    if (n_args > 1 && args[1] == mp_const_true) {
        o->opts |= BLOCKING_WRITE;
    }
    return MP_OBJ_FROM_PTR(o);
}

static mp_uint_t websocket_read_frames(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode);

static mp_uint_t websocket_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_websocket_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->closed && !self->has_peek) {
        // The socket may already be closed (write timeout drop): report EOF
        // without touching it.
        return 0;
    }
    if (!self->has_peek) {
        return websocket_read_frames(self_in, buf, size, errcode);
    }
    ((byte *)buf)[0] = self->peek;
    self->has_peek = 0;
    if (size == 1) {
        return 1;
    }
    int err = 0;
    mp_uint_t more = websocket_read_frames(self_in, (byte *)buf + 1, size - 1, &err);
    if (more == 0 || more == MP_STREAM_ERROR) {
        return 1;
    }
    return 1 + more;
}

static mp_uint_t websocket_read_frames(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_websocket_t *self = MP_OBJ_TO_PTR(self_in);

    const mp_stream_p_t *stream_p = mp_get_stream(self->sock);
    while (1) {
        if (self->to_recv != 0) {
            mp_uint_t out_sz = stream_p->read(self->sock, self->buf + self->buf_pos, self->to_recv, errcode);
            if (out_sz == 0) {
                self->closed = 1;
                return out_sz;
            }
            if (out_sz == MP_STREAM_ERROR) {
                return out_sz;
            }
            self->buf_pos += out_sz;
            self->to_recv -= out_sz;
            if (self->to_recv != 0) {
                *errcode = MP_EAGAIN;
                return MP_STREAM_ERROR;
            }
        }

        switch (self->state) {
            case FRAME_HEADER: {
                // "Control frames MAY be injected in the middle of a fragmented message."
                // So, they must be processed before data frames (and not alter
                // self->ws_flags)
                byte frame_type = self->buf[0];
                self->last_flags = frame_type;
                frame_type &= FRAME_OPCODE_MASK;

                if ((self->buf[0] & FRAME_OPCODE_MASK) == FRAME_CONT) {
                    // Preserve previous frame type
                    self->ws_flags = (self->ws_flags & FRAME_OPCODE_MASK) | (self->buf[0] & ~FRAME_OPCODE_MASK);
                } else {
                    self->ws_flags = self->buf[0];
                }

                // Reset mask in case someone will use "simplified" protocol
                // without masks.
                memset(self->mask, 0, sizeof(self->mask));

                int to_recv = 0;
                size_t sz = self->buf[1] & 0x7f;
                if (sz == 126) {
                    // Msg size is next 2 bytes
                    to_recv += 2;
                } else if (sz == 127) {
                    // Msg size is next 8 bytes (unsupported, no way to recover)
                    mp_stream_close(self->sock);
                    *errcode = MP_EIO;
                    return MP_STREAM_ERROR;
                }
                if (self->buf[1] & 0x80) {
                    // Next 4 bytes is mask
                    to_recv += 4;
                }
                self->buf_pos = 0;
                self->to_recv = to_recv;
                self->msg_sz = sz; // May be overridden by FRAME_OPT
                if (to_recv != 0) {
                    self->state = FRAME_OPT;
                } else {
                    if (frame_type >= FRAME_CLOSE) {
                        self->state = CONTROL;
                    } else {
                        self->state = PAYLOAD;
                    }
                }
                continue;
            }

            case FRAME_OPT: {
                if (self->buf_pos & 2) { // to_recv was 2 or 6
                    assert(self->buf_pos == 2 || self->buf_pos == 6);
                    // First two bytes are message length. Technically the size must be at least 126 per RFC6455
                    // but MicroPython skips checking that.
                    self->msg_sz = (self->buf[0] << 8) | self->buf[1];
                }
                if (self->buf_pos & 4) {
                    // Last 4 bytes is mask
                    memcpy(self->mask, self->buf + self->buf_pos - 4, 4);
                }
                self->buf_pos = 0;
                if ((self->last_flags & FRAME_OPCODE_MASK) >= FRAME_CLOSE) {
                    self->state = CONTROL;
                } else {
                    self->state = PAYLOAD;
                }
                continue;
            }

            case PAYLOAD:
            case CONTROL: {
                mp_uint_t out_sz = 0;
                if (self->msg_sz == 0) {
                    // In case message had zero payload
                    goto no_payload;
                }

                size_t sz = MIN(size, self->msg_sz);
                out_sz = stream_p->read(self->sock, buf, sz, errcode);
                if (out_sz == 0) {
                    self->closed = 1;
                    return out_sz;
                }
                if (out_sz == MP_STREAM_ERROR) {
                    return out_sz;
                }

                sz = out_sz;
                for (byte *p = buf; sz--; p++) {
                    *p ^= self->mask[self->mask_pos++ & 3];
                }

                self->msg_sz -= out_sz;
                if (self->msg_sz == 0) {
                    byte last_state;
                no_payload:
                    last_state = self->state;
                    self->state = FRAME_HEADER;
                    self->to_recv = 2;
                    self->mask_pos = 0;
                    self->buf_pos = 0;

                    // Handle control frame
                    if (last_state == CONTROL) {
                        byte frame_type = self->last_flags & FRAME_OPCODE_MASK;
                        if (frame_type == FRAME_CLOSE) {
                            static const byte close_resp[2] = {0x88, 0};
                            int err;
                            websocket_write_raw(self_in, close_resp, sizeof(close_resp), close_resp, 0, &err);
                            self->closed = 1;
                            return 0;
                        }

                        // DEBUG_printf("Finished receiving ctrl message %x, ignoring\n", self->last_flags);
                        continue;
                    }
                }

                if (out_sz != 0) {
                    return out_sz;
                }
                // Empty (data) frame received is not EOF
                continue;
            }

        }
    }
}

static mp_uint_t websocket_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_websocket_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->closed) {
        // Peer is gone. With BLOCKING_WRITE the socket write would block
        // until TCP gives up (seconds), stalling the event loop, and an
        // error would crash the aiorepl task writing the prompt. Discard
        // silently so the caller neither blocks nor errors; the reader
        // parks via the POLL suppression above.
        return size;
    }
    if (size >= 0x10000) {
        *errcode = MP_ENOBUFS;
        return MP_STREAM_ERROR;
    }
    byte header[4] = {0x80 | (self->opts & FRAME_OPCODE_MASK)};
    int hdr_sz;
    // "Note that in all cases, the minimal number of bytes MUST be used to
    // encode the length, for example, the length of a 124-byte-long string
    // can't be encoded as the sequence 126, 0, 124."
    //   -- https://www.rfc-editor.org/rfc/rfc6455.html
    if (size < 126) {
        header[1] = size;
        hdr_sz = 2;
    } else {
        header[1] = 126;
        header[2] = size >> 8;
        header[3] = size & 0xff;
        hdr_sz = 4;
    }

    return websocket_write_raw(self_in, header, hdr_sz, buf, size, errcode);
}
static mp_uint_t websocket_write_raw(mp_obj_t self_in, const byte *header, int hdr_sz, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_websocket_t *self = MP_OBJ_TO_PTR(self_in);

    mp_obj_t dest[3];
    if (self->opts & BLOCKING_WRITE) {
        mp_load_method(self->sock, MP_QSTR_settimeout, dest);
        dest[2] = MP_OBJ_NEW_SMALL_INT(WEBSOCKET_WRITE_TIMEOUT_S);
        mp_call_method_n_kw(1, 0, dest);
    }

    // Send header and payload in one call when they fit, and in the
    // blocking mode never retry a partial write: each socket call waits up
    // to the timeout on its own, so a peer that frees a few bytes at a time
    // would otherwise stretch one frame over many timeouts. lwIP only
    // writes short once the peer is a full send buffer behind, which is a
    // stalled peer, not a slow one.
    byte flags = MP_STREAM_RW_WRITE | ((self->opts & BLOCKING_WRITE) ? MP_STREAM_RW_ONCE : 0);
    byte joined[512];
    mp_uint_t total = hdr_sz + size;
    mp_uint_t done;
    if (total <= sizeof(joined)) {
        memcpy(joined, header, hdr_sz);
        memcpy(joined + hdr_sz, buf, size);
        done = mp_stream_rw(self->sock, joined, total, errcode, flags);
    } else {
        done = mp_stream_rw(self->sock, (void *)header, hdr_sz, errcode, flags);
        if (*errcode == 0 && done == (mp_uint_t)hdr_sz) {
            done += mp_stream_rw(self->sock, (void *)buf, size, errcode, flags);
        }
    }
    bool complete = *errcode == 0 && done == total;

    if (self->opts & BLOCKING_WRITE) {
        if (!complete) {
            // Timed out on a peer that stopped reading, or the peer reset.
            // A websocket frame cannot resume after a partial send, and a
            // blocking caller (dupterm) must neither hang nor see an error,
            // so drop the client: the reader parks, later writes are
            // discarded, and closing the socket lets the Python webrepl
            // supervisor detach it on its next tick.
            self->closed = 1;
            mp_stream_close(self->sock);
            *errcode = 0;
            return size;
        }
        mp_load_method(self->sock, MP_QSTR_setblocking, dest);
        dest[2] = mp_const_false;
        mp_call_method_n_kw(1, 0, dest);
    }

    if (*errcode != 0) {
        return MP_STREAM_ERROR;
    }
    return done > (mp_uint_t)hdr_sz ? done - hdr_sz : 0;
}

static mp_uint_t websocket_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    mp_obj_websocket_t *self = MP_OBJ_TO_PTR(self_in);
    switch (request) {
        case MP_STREAM_CLOSE:
            // TODO: Send close signaling to the other side, otherwise it's
            // abrupt close (connection abort).
            mp_stream_close(self->sock);
            return 0;
        case MP_STREAM_GET_DATA_OPTS:
            return self->ws_flags & FRAME_OPCODE_MASK;
        case MP_STREAM_SET_DATA_OPTS: {
            int cur = self->opts & FRAME_OPCODE_MASK;
            self->opts = (self->opts & ~FRAME_OPCODE_MASK) | (arg & FRAME_OPCODE_MASK);
            return cur;
        }
        case MP_STREAM_POLL: {
            // Forward poll to the underlying socket so poll/select and
            // dupterm consumers (WebREPL through aiorepl's stdin wait) wake
            // on incoming frames.
            //
            // lwIP reports a peer-closed socket as readable and a reset as
            // HUP. A dupterm reader woken by either reads no byte and then
            // sits in mp_hal_stdin_rx_chr, which loops until some stdin
            // source yields a char: the asyncio loop and the watchdog feed
            // stop with it and the board resets 20 s later. So "readable"
            // is only reported with a payload byte in hand: run the frame
            // parser here and hold the byte for the next read. EOF, a
            // CLOSE frame and a socket error mark the peer gone; from then
            // on the reader parks and the Python webrepl supervisor /
            // reconnect reclaim detaches dupterm.
            if (self->closed) {
                // Nothing will ever be readable again, and the socket may
                // already be closed: polling it would report NVAL, which
                // wakes an asyncio reader just like RD does.
                return 0;
            }
            const mp_stream_p_t *stream_p = mp_get_stream(self->sock);
            mp_uint_t ret = stream_p->ioctl(self->sock, request, arg, errcode);
            if (ret == MP_STREAM_ERROR) {
                return ret;
            }
            const mp_uint_t rd_flags = MP_STREAM_POLL_RD | MP_STREAM_POLL_HUP | MP_STREAM_POLL_ERR;
            if (!self->closed && !self->has_peek && (ret & rd_flags)) {
                int err = 0;
                mp_uint_t got = websocket_read_frames(self_in, &self->peek, 1, &err);
                if (got == 1) {
                    self->has_peek = 1;
                } else if (got == 0 || !mp_is_nonblocking_error(err)) {
                    self->closed = 1;
                }
                // A nonblocking error is a partial frame: wait for the rest.
            }
            ret &= ~rd_flags;
            if (self->has_peek) {
                ret |= arg & MP_STREAM_POLL_RD;
            }
            return ret;
        }
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

static const mp_rom_map_elem_t websocket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&mp_stream_ioctl_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
};
static MP_DEFINE_CONST_DICT(websocket_locals_dict, websocket_locals_dict_table);

static const mp_stream_p_t websocket_stream_p = {
    .read = websocket_read,
    .write = websocket_write,
    .ioctl = websocket_ioctl,
};

static MP_DEFINE_CONST_OBJ_TYPE(
    websocket_type,
    MP_QSTR_websocket,
    MP_TYPE_FLAG_NONE,
    make_new, websocket_make_new,
    protocol, &websocket_stream_p,
    locals_dict, &websocket_locals_dict
    );

static const mp_rom_map_elem_t websocket_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_websocket) },
    { MP_ROM_QSTR(MP_QSTR_websocket), MP_ROM_PTR(&websocket_type) },
};

static MP_DEFINE_CONST_DICT(websocket_module_globals, websocket_module_globals_table);

const mp_obj_module_t mp_module_websocket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&websocket_module_globals,
};

// This module should not be extensible (as it is not a CPython standard
// library nor is it necessary to override from the filesystem), however it
// has previously been known as `uwebsocket`, so by making it extensible the
// `uwebsocket` alias will continue to work.
MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_websocket, mp_module_websocket);

#endif // MICROPY_PY_WEBSOCKET
