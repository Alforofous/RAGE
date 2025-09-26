#pragma once
#include <string_view>
#include <tuple>

/**
 * Helper to define an event type with its data type
 * Usage: EventType<eventName, DataType>
 */
template<const char* Name, typename DataType>
struct EventType {
    static constexpr const char* name = Name;
    using data_type = DataType;
};

/**
 * Helper to create event map from multiple event types
 * Usage: EventMap<EventType<name1, Type1>, EventType<name2, Type2>, ...>
 */
template<typename... Events>
struct EventMap {
    using event_types = std::tuple<Events...>;
};

/**
 * Template to check if an event name exists in the event map
 */
template<typename EventMap, const char* EventName>
struct has_event;

template<const char* EventName, typename... Events>
struct has_event<EventMap<Events...>, EventName> {
    static constexpr bool value = ((std::string_view(Events::name) == std::string_view(EventName)) || ...);
};

/**
 * Template to get data type for an event name
 */
template<typename EventMap, const char* EventName>
struct get_event_data_type;

template<const char* EventName, typename... Events>
struct get_event_data_type<EventMap<Events...>, EventName> {
private:
    template<typename Event>
    static typename Event::data_type get_type_impl() {
        if constexpr (std::string_view(Event::name) == std::string_view(EventName)) {
            return typename Event::data_type{};
        }
    }
    
public:
    using type = decltype((get_type_impl<Events>(), ...));
};

/**
 * Macro to help define event names as compile-time constants
 * Usage: DEFINE_EVENT_NAME(nodeAdded) creates a constant you can use in EventType
 */
#define DEFINE_EVENT_NAME(name) \
    constexpr char (name##_name)[] = #name; \
    constexpr const char* (name) = (name##_name);
