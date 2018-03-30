/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "util.h"

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif // HAVE_ARPA_INET_H

#include <chrono>
#include <array>

namespace ngtcp2 {

namespace util {

namespace {
constexpr char LOWER_XDIGITS[] = "0123456789abcdef";
} // namespace

std::string format_hex(uint8_t c) {
  std::string s;
  s.resize(2);

  s[0] = LOWER_XDIGITS[c >> 4];
  s[1] = LOWER_XDIGITS[c & 0xf];

  return s;
}

std::string format_hex(const uint8_t *s, size_t len) {
  std::string res;
  res.resize(len * 2);

  for (size_t i = 0; i < len; ++i) {
    auto c = s[i];

    res[i * 2] = LOWER_XDIGITS[c >> 4];
    res[i * 2 + 1] = LOWER_XDIGITS[c & 0x0f];
  }
  return res;
}

namespace {
// format_fraction2 formats |n| as fraction part of integer.  |n| is
// considered as fraction, and its precision is 3 digits.  The last
// digit is ignored.  The precision of the resulting fraction is 2
// digits.
std::string format_fraction2(uint32_t n) {
  n /= 10;

  if (n < 10) {
    return {'.', '0', static_cast<char>('0' + n)};
  }
  return {'.', static_cast<char>('0' + n / 10),
          static_cast<char>('0' + (n % 10))};
}
} // namespace

namespace {
// round2even rounds the last digit of |n| so that the n / 10 becomes
// even.
uint64_t round2even(uint64_t n) {
  if (n % 10 == 5) {
    if ((n / 10) & 1) {
      n += 10;
    }
  } else {
    n += 5;
  }
  return n;
}
} // namespace

std::string format_duration(uint64_t ns) {
  static constexpr const char *units[] = {"us", "ms", "s"};
  if (ns < 1000) {
    return std::to_string(ns) + "ns";
  }
  auto unit = 0;
  if (ns < 1000000) {
    // do nothing
  } else if (ns < 1000000000) {
    ns /= 1000;
    unit = 1;
  } else {
    ns /= 1000000;
    unit = 2;
  }

  ns = round2even(ns);

  if (ns / 1000 >= 1000 && unit < 2) {
    ns /= 1000;
    ++unit;
  }

  return std::to_string(ns / 1000) + format_fraction2(ns % 1000) + units[unit];
}

std::mt19937 make_mt19937() {
  std::random_device rd;
  return std::mt19937(rd());
}

ngtcp2_tstamp timestamp() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

ngtcp2_tstamp timestamp(struct ev_loop *loop) {
  return ev_now(loop) * 1000000000;
}

bool numeric_host(const char *hostname) {
  return numeric_host(hostname, AF_INET) || numeric_host(hostname, AF_INET6);
}

bool numeric_host(const char *hostname, int family) {
  int rv;
  std::array<uint8_t, sizeof(struct in6_addr)> dst;

  rv = inet_pton(family, hostname, dst.data());

  return rv == 1;
}

namespace {
void hexdump8(FILE *out, const uint8_t *first, const uint8_t *last) {
  auto stop = std::min(first + 8, last);
  for (auto k = first; k != stop; ++k) {
    fprintf(out, "%02x ", *k);
  }
  // each byte needs 3 spaces (2 hex value and space)
  for (; stop != first + 8; ++stop) {
    fputs("   ", out);
  }
  // we have extra space after 8 bytes
  fputc(' ', out);
}
} // namespace

void hexdump(FILE *out, const uint8_t *src, size_t len) {
  if (len == 0) {
    return;
  }
  size_t buflen = 0;
  auto repeated = false;
  std::array<uint8_t, 16> buf{};
  auto end = src + len;
  auto i = src;
  for (;;) {
    auto nextlen =
        std::min(static_cast<size_t>(16), static_cast<size_t>(end - i));
    if (nextlen == buflen &&
        std::equal(std::begin(buf), std::begin(buf) + buflen, i)) {
      // as long as adjacent 16 bytes block are the same, we just
      // print single '*'.
      if (!repeated) {
        repeated = true;
        fputs("*\n", out);
      }
      i += nextlen;
      continue;
    }
    repeated = false;
    fprintf(out, "%08lx", static_cast<unsigned long>(i - src));
    if (i == end) {
      fputc('\n', out);
      break;
    }
    fputs("  ", out);
    hexdump8(out, i, end);
    hexdump8(out, i + 8, std::max(i + 8, end));
    fputc('|', out);
    auto stop = std::min(i + 16, end);
    buflen = stop - i;
    auto p = buf.data();
    for (; i != stop; ++i) {
      *p++ = *i;
      if (0x20 <= *i && *i <= 0x7e) {
        fputc(*i, out);
      } else {
        fputc('.', out);
      }
    }
    fputs("|\n", out);
  }
}

} // namespace util

} // namespace ngtcp2
