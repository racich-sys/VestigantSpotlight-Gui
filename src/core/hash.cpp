#include "core/hash.h"
#include "core/path_utils.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace vestigant::spotlight {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr std::array<u32, 64> K{
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
inline u32 rotr(u32 x, u32 n) { return (x >> n) | (x << (32 - n)); }
inline u32 ch(u32 x, u32 y, u32 z) { return (x & y) ^ (~x & z); }
inline u32 maj(u32 x, u32 y, u32 z) { return (x & y) ^ (x & z) ^ (y & z); }
inline u32 bsig0(u32 x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline u32 bsig1(u32 x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline u32 ssig0(u32 x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline u32 ssig1(u32 x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

class Sha256Context {
public:
    Sha256Context() { reset(); }

    void reset() {
        h_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        buffer_len_ = 0;
        bit_len_ = 0;
        finalized_ = false;
    }

    void update(const unsigned char* data, std::size_t len) {
        if (finalized_) throw std::runtime_error("SHA256 context already finalized");
        if (!data && len != 0) throw std::runtime_error("SHA256 update received null data");
        bit_len_ += static_cast<u64>(len) * 8u;
        std::size_t pos = 0;
        while (pos < len) {
            const std::size_t space = 64 - buffer_len_;
            const std::size_t take = (len - pos < space) ? (len - pos) : space;
            std::memcpy(buffer_.data() + buffer_len_, data + pos, take);
            buffer_len_ += take;
            pos += take;
            if (buffer_len_ == 64) {
                transform(buffer_.data());
                buffer_len_ = 0;
            }
        }
    }

    std::string finalHex() {
        if (!finalized_) finalize();
        std::ostringstream os;
        os << std::hex << std::setfill('0');
        for (u32 h : h_) os << std::setw(8) << h;
        return os.str();
    }

private:
    void transform(const unsigned char* block) {
        std::array<u32, 64> w{};
        for (int i = 0; i < 16; ++i) {
            const auto j = static_cast<std::size_t>(i) * 4;
            w[static_cast<std::size_t>(i)] =
                (static_cast<u32>(block[j]) << 24) |
                (static_cast<u32>(block[j + 1]) << 16) |
                (static_cast<u32>(block[j + 2]) << 8) |
                static_cast<u32>(block[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            w[static_cast<std::size_t>(i)] = ssig1(w[static_cast<std::size_t>(i - 2)]) +
                                            w[static_cast<std::size_t>(i - 7)] +
                                            ssig0(w[static_cast<std::size_t>(i - 15)]) +
                                            w[static_cast<std::size_t>(i - 16)];
        }
        u32 a=h_[0], b=h_[1], c=h_[2], d=h_[3], e=h_[4], f=h_[5], g=h_[6], h=h_[7];
        for (int i = 0; i < 64; ++i) {
            const u32 t1 = h + bsig1(e) + ch(e,f,g) + K[static_cast<std::size_t>(i)] + w[static_cast<std::size_t>(i)];
            const u32 t2 = bsig0(a) + maj(a,b,c);
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
        h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }

    void finalize() {
        buffer_[buffer_len_++] = 0x80u;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) buffer_[buffer_len_++] = 0;
            transform(buffer_.data());
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) buffer_[buffer_len_++] = 0;
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_len_++] = static_cast<unsigned char>((bit_len_ >> (i * 8)) & 0xffu);
        }
        transform(buffer_.data());
        buffer_len_ = 0;
        finalized_ = true;
    }

    std::array<u32, 8> h_{};
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffer_len_ = 0;
    u64 bit_len_ = 0;
    bool finalized_ = false;
};

std::string sha256Stream(std::istream& in, const std::function<void(std::uintmax_t)>& progressCallback) {
    Sha256Context ctx;
    std::vector<unsigned char> buffer(1024 * 1024);
    std::uintmax_t bytesRead = 0;
    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) {
            ctx.update(buffer.data(), static_cast<std::size_t>(got));
            bytesRead += static_cast<std::uintmax_t>(got);
            if (progressCallback) progressCallback(bytesRead);
        }
    }
    if (in.bad()) throw std::runtime_error("Read error while hashing file");
    return ctx.finalHex();
}

#ifdef _WIN32
std::string winErrorMessage(DWORD code) {
    LPWSTR raw = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
    std::wstring ws;
    if (n && raw) ws.assign(raw, raw + n);
    if (raw) LocalFree(raw);
    while (!ws.empty() && (ws.back() == L'\r' || ws.back() == L'\n' || ws.back() == L' ' || ws.back() == L'\t')) ws.pop_back();
    if (ws.empty()) {
        std::ostringstream os;
        os << "Windows error " << code;
        return os.str();
    }
    std::string out;
    out.reserve(ws.size());
    for (wchar_t wc : ws) {
        out.push_back((wc >= 0 && wc <= 0x7f) ? static_cast<char>(wc) : '?');
    }
    return out;
}

std::string sha256FileWindows(const fs::path& file, const std::function<void(std::uintmax_t)>& progressCallback) {
    HANDLE h = CreateFileW(file.wstring().c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        throw std::runtime_error("Unable to open file for SHA256: " + pathString(file) + " (" + winErrorMessage(err) + ")");
    }
    try {
        Sha256Context ctx;
        std::vector<unsigned char> buffer(4 * 1024 * 1024);
        std::uintmax_t bytesRead = 0;
        for (;;) {
            DWORD got = 0;
            if (!ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr)) {
                const DWORD err = GetLastError();
                throw std::runtime_error("Read error while hashing file: " + pathString(file) + " (" + winErrorMessage(err) + ")");
            }
            if (got == 0) break;
            ctx.update(buffer.data(), static_cast<std::size_t>(got));
            bytesRead += static_cast<std::uintmax_t>(got);
            if (progressCallback) progressCallback(bytesRead);
        }
        CloseHandle(h);
        return ctx.finalHex();
    } catch (...) {
        CloseHandle(h);
        throw;
    }
}

#endif
}

std::string sha256Bytes(const unsigned char* data, std::size_t len) {
    Sha256Context ctx;
    ctx.update(data, len);
    return ctx.finalHex();
}

std::string sha256FileWithProgress(const fs::path& file, const std::function<void(std::uintmax_t)>& progressCallback) {
#ifdef _WIN32
    return sha256FileWindows(file, progressCallback);
#else
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Unable to open file for SHA256: " + pathString(file));
    return sha256Stream(in, progressCallback);
#endif
}

std::string sha256File(const fs::path& file) {
    return sha256FileWithProgress(file, {});
}

} // namespace vestigant::spotlight
