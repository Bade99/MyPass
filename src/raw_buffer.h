#pragma once
#include "platform.h"

using raw_buffer = std::string; //TODO(fran): this does have the pointless null termination when using .data()

template <typename T>
concept Trivial = std::is_trivially_copyable_v<T>;

//TODO(fran): do we want to support 32bit builds? if so then we need to stop using size_t, there are two possible solutions:
// - stop using std lib containers that use it and make our own that are fixed to 64bit for sizes
// - limit all size_t saves to 32bit and validate it doesnt go over the limit, if it does then refuse to save the container

namespace detail {
    constexpr std::size_t get_alignment_padding(std::size_t offset, std::size_t alignment) {
        if (alignment <= 1) return 0;
        return (alignment - (offset % alignment)) % alignment;
    }

    void append_bytes(raw_buffer& out, const void* src, std::size_t bytes) {
        if (bytes) {
            const auto old = out.size();

            //TODO(fran): verify behaviour when the string buffer has become large: will a request to resize just resize the array a little bit more than requested, eg going from 512 to 530, or will it be more efficient and go from 512 to 768 or more, for a small resize request of lets say 10 bytes?

            out.resize_and_overwrite(old + bytes, [&](char* data, std::size_t new_size) {
                std::memcpy(data + old, src, bytes); // WARNING: this is system endianness dependant
                return new_size;
            });
        }
    }

    void append_zero_bytes(raw_buffer& out, std::size_t bytes) {
        if (bytes) {
            const auto old = out.size();
            out.resize_and_overwrite(old + bytes, [&](char* data, std::size_t new_size) {
                std::memset(data + old, 0, bytes);
                return new_size;
            });
        }
    }
}

// 1) trivially-copyable scalars/flat structs
//TODO(fran): even for trivial structs different compilers may generate different padding within the struct, review that, we may need to require all serialized structs to have a user set alignment, or change the way we do the serialization to be invariant to different padding
template <Trivial T> 
void append_to_buffer(raw_buffer& out, const T& v) {
    detail::append_bytes(out, &v, sizeof(T));
}

// 2) strings (null-terminated format)
template <typename CharT, typename Traits, typename Alloc>
void append_to_buffer(raw_buffer& out, const std::basic_string<CharT, Traits, Alloc>& s) {
    // Keep string code units aligned so read_view can return zero-copy views safely for UTF-16/UTF-32/wchar_t.
    const auto padding = detail::get_alignment_padding(out.size(), alignof(CharT));
    detail::append_zero_bytes(out, padding);
    detail::append_bytes(out, s.data(), (s.size() + 1) * sizeof(CharT)); // includes terminator
}

// 3) vectors/containers: encode size, then elements recursively
template <typename T, typename Alloc>
void append_to_buffer(raw_buffer& out, const std::vector<T, Alloc>& v) {
    const auto n = (u64)(v.size()); // always store as 64bit number
    append_to_buffer(out, n);

    if constexpr (Trivial<T>) detail::append_bytes(out, v.data(), v.size() * sizeof(T));
    else for (const auto& e : v) append_to_buffer(out, e);
}


template <class CharT>
concept StringCodeUnit =
    std::same_as<CharT, char> || std::same_as<CharT, char8_t> ||
    std::same_as<CharT, wchar_t> || std::same_as<CharT, char16_t> ||
    std::same_as<CharT, char32_t>;

struct raw_buffer_reader {
    std::span<const u8> bytes{};
    std::size_t pos{};

    raw_buffer_reader(const void* data, std::size_t sz_bytes) : bytes((const u8*)data, sz_bytes) { 
        Assert((std::uintptr_t)data % 4 == 0); 
    }

    bool read_bytes(void* out, std::size_t n) {
        if (pos + n > bytes.size()) return false;
        std::memcpy(out, bytes.data() + pos, n);
        pos += n;
        return true;
    }

    bool cut_bytes_from_end(void* out, std::size_t n) {
        if (n > bytes.size() || bytes.size() - n < pos) return false;
        std::memcpy(out, bytes.data() + bytes.size() - n, n);
        bytes = bytes.first(bytes.size() - n);
        return true;
    }

    template <Trivial T>
    bool read(T& out) {
        return read_bytes(&out, sizeof(T));
    }

    // views: view type entities are read in a fashion similar to a vector, but without performing any copies over the data
    template <StringCodeUnit CharT>
    bool read(std::basic_string_view<CharT>& out) {
        //out = {};

        // Writer inserts zero padding so each string starts at alignof(CharT).
        const auto padding = detail::get_alignment_padding(pos, alignof(CharT));
        #ifdef _DEBUG
        if (padding > remaining())
            for (std::size_t i = 0; i < padding; i++)
                Assert(bytes[pos + i] == 0);
        #endif
        if (!skip(padding)) return false;

        const auto rem_bytes = remaining();
        if (rem_bytes < sizeof(CharT)) return false;

        const auto* start = bytes.data() + pos;
        // Debug check for proper pointer alignment since bytes.data() itself could be unaligned
        // If it is unaligned we do a best effort load hoping the arch can do unaligned access
        Assert((std::uintptr_t)start % alignof(CharT) == 0);

        const auto unit_count = rem_bytes / sizeof(CharT);
        const auto* chars = (const CharT*)start;

        for (std::size_t i = 0; i < unit_count; i++) {
            if (chars[i] == CharT{}) {
                out = std::basic_string_view<CharT>(chars, i);
                pos += (i + 1) * sizeof(CharT); // consume terminator too
                return true;
            }
        }

        return false;
    }

    template <Trivial T>
    bool cut_from_end(T& out) {
        return cut_bytes_from_end(&out, sizeof(T));
    }

    bool skip(std::size_t n) {
        if (pos + n > bytes.size()) return false;
        pos += n;
        return true;
    }

    std::span<const u8> get_current_subspan() const {
        return bytes.subspan(pos);
    }

    std::size_t remaining() const { return bytes.size() - pos; }

    void restart_position() { pos = {}; }
};
