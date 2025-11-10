#pragma once
#include <cstddef>
#include <cstdint>

/* Type definitions */
#ifdef ASTRON_128BIT_CHANNELS
#include "util/uint128.h"
typedef uint128_t channel_t;
typedef uint64_t doid_t;
typedef uint64_t zone_t;
#else
typedef uint64_t channel_t;
typedef uint32_t doid_t;
typedef uint32_t zone_t;
#endif

/* Type limits */
constexpr channel_t CHANNEL_MAX = static_cast<channel_t>(-1);
constexpr doid_t DOID_MAX = static_cast<doid_t>(-1);
constexpr zone_t ZONE_MAX = static_cast<zone_t>(-1);
constexpr size_t ZONE_BITS = sizeof(zone_t) * 8;

/* DoId constants */
constexpr doid_t INVALID_DO_ID = 0;

/* Channel constants */
constexpr channel_t INVALID_CHANNEL = 0;
constexpr channel_t CONTROL_MESSAGE = 1;
constexpr channel_t BCHAN_CLIENTS = 10;
constexpr channel_t BCHAN_STATESERVERS = 12;
constexpr channel_t BCHAN_DBSERVERS = 13;
constexpr channel_t PARENT_PREFIX = (channel_t(1) << ZONE_BITS);
constexpr channel_t DATABASE_PREFIX = (channel_t(2) << ZONE_BITS);

/* Channel building methods */
[[nodiscard]] constexpr inline channel_t location_as_channel(doid_t parent, zone_t zone) noexcept
{
    return (channel_t(parent) << ZONE_BITS) | channel_t(zone);
}
[[nodiscard]] constexpr inline channel_t parent_to_children(doid_t parent) noexcept
{
    return PARENT_PREFIX | channel_t(parent);
}
[[nodiscard]] constexpr inline channel_t database_to_object(doid_t object) noexcept
{
    return DATABASE_PREFIX | channel_t(object);
}
