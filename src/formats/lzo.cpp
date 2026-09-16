#include "formats/lzo.h"

namespace ra2yr::formats {

std::size_t lzo1x_decompress(const std::uint8_t* in, std::size_t in_len,
                             std::uint8_t* out, std::size_t out_capacity) {
    const std::uint8_t* ip = in;
    const std::uint8_t* const ip_end = in + in_len;
    std::uint8_t* op = out;
    std::uint8_t* const op_end = out + out_capacity;
    std::size_t t = 0;
    const std::uint8_t* m_pos = nullptr;

    if (in_len == 0) {
        return 0;
    }

    if (*ip > 17) {
        t = static_cast<std::size_t>(*ip++) - 17;
        goto first_literal_run;
    }

    for (;;) {
        if (ip >= ip_end) {
            return 0;
        }
        t = *ip++;
        if (t >= 16) {
            goto match;
        }
        // A literal run.
        if (t == 0) {
            t = 15;
            while (ip < ip_end && *ip == 0) {
                t += 255;
                ip++;
            }
            if (ip >= ip_end) {
                return 0;
            }
            t += *ip++;
        }
        if (ip + 3 > ip_end || op + 3 > op_end) {
            return 0;
        }
        *op++ = *ip++;
        *op++ = *ip++;
        *op++ = *ip++;
    first_literal_run:
        if (ip + t > ip_end || op + t > op_end) {
            return 0;
        }
        do {
            *op++ = *ip++;
        } while (--t > 0);

        if (ip >= ip_end) {
            return 0;
        }
        t = *ip++;

        if (t >= 16) {
            goto match;
        }
        m_pos = op - 1 - 0x800;
        m_pos -= t >> 2;
        m_pos -= static_cast<std::size_t>(*ip++) << 2;
        if (m_pos < out || op + 3 > op_end) {
            return 0;
        }
        *op++ = *m_pos++;
        *op++ = *m_pos++;
        *op++ = *m_pos;
        goto match_done;

        for (;;) {
            if (t < 16) {  // A M1 match.
                m_pos = op - 1;
                m_pos -= t >> 2;
                m_pos -= static_cast<std::size_t>(*ip++) << 2;
                if (m_pos < out || op + 2 > op_end) {
                    return 0;
                }
                *op++ = *m_pos++;
                *op++ = *m_pos;
            } else {
            match:
                if (t >= 64) {  // A M2 match.
                    m_pos = op - 1;
                    m_pos -= (t >> 2) & 7;
                    m_pos -= static_cast<std::size_t>(*ip++) << 3;
                    t = (t >> 5) - 1;
                } else if (t >= 32) {  // A M3 match.
                    t &= 31;
                    if (t == 0) {
                        t = 31;
                        while (ip < ip_end && *ip == 0) {
                            t += 255;
                            ip++;
                        }
                        if (ip >= ip_end) {
                            return 0;
                        }
                        t += *ip++;
                    }
                    m_pos = op - 1;
                    m_pos -= *ip++ >> 2;
                    m_pos -= static_cast<std::size_t>(*ip++) << 6;
                } else {  // A M4 match.
                    m_pos = op;
                    m_pos -= (t & 8) << 11;
                    t &= 7;
                    if (t == 0) {
                        t = 7;
                        while (ip < ip_end && *ip == 0) {
                            t += 255;
                            ip++;
                        }
                        if (ip >= ip_end) {
                            return 0;
                        }
                        t += *ip++;
                    }
                    m_pos -= *ip++ >> 2;
                    m_pos -= static_cast<std::size_t>(*ip++) << 6;
                    if (m_pos == op) {
                        goto eof_found;
                    }
                    m_pos -= 0x4000;
                }
                if (m_pos < out || op + 2 > op_end) {
                    return 0;
                }
                *op++ = *m_pos++;
                *op++ = *m_pos++;
                if (op + t > op_end) {
                    return 0;
                }
                do {
                    *op++ = *m_pos++;
                } while (--t > 0);
            }

        match_done:
            t = ip[-2] & 3;
            if (t == 0) {
                break;
            }
            if (ip + t > ip_end || op + t > op_end) {
                return 0;
            }
            do {
                *op++ = *ip++;
            } while (--t > 0);
            if (ip >= ip_end) {
                return 0;
            }
            t = *ip++;
        }
    }

eof_found:
    return static_cast<std::size_t>(op - out);
}

}  // namespace ra2yr::formats
