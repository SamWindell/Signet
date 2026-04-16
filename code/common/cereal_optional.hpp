#pragma once

#include <optional>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>

// JSON-archive overrides for std::optional<T>.
//
// Cereal upstream (cereal/types/optional.hpp) serializes std::optional as a
// tagged union: {"nullopt": true} or {"nullopt": false, "data": value}. That
// keeps a single implementation working for binary archives (which have no
// native null and no peek-ahead), but produces awkward JSON. Here we
// specialize for JSON only:
//   empty   -> null
//   present -> the value, inlined directly
//
// Include this header instead of <cereal/types/optional.hpp> wherever
// std::optional is serialized via JSON archives.

namespace cereal {

// Don't introduce a wrapping object/array node for the optional itself; the
// contained value's own prologue handles its node, or we write a bare null.
template <class T>
inline void prologue(JSONOutputArchive &, std::optional<T> const &) {}
template <class T>
inline void epilogue(JSONOutputArchive &, std::optional<T> const &) {}
template <class T>
inline void prologue(JSONInputArchive &, std::optional<T> const &) {}
template <class T>
inline void epilogue(JSONInputArchive &, std::optional<T> const &) {}

template <class T>
inline void save(JSONOutputArchive &ar, std::optional<T> const &opt) {
    if (opt)
        ar(*opt);
    else
        ar(nullptr);
}

template <class T>
inline void load(JSONInputArchive &ar, std::optional<T> &opt) {
    std::nullptr_t n;
    try {
        // Probe for null. loadValue(nullptr_t) calls search() (which positions
        // the iterator and consumes the pending NVP name) and then asserts
        // IsNull(). The assertion throws RapidJSONException without advancing
        // past the value, so on a non-null value we fall through and load T
        // from the same position.
        ar.loadValue(n);
        opt = std::nullopt;
    } catch (const RapidJSONException &) {
        T value;
        ar(value);
        opt = std::move(value);
    }
}

} // namespace cereal
