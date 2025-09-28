#pragma once
#include <unordered_map>
#include <vector>
#include <functional>
#include <string>
#include <type_traits>
#include "event_type.hpp"

/**
 * Type-safe EventDispatcher that only allows predefined event types
 */

template<typename EventMapType>
class EventDispatcher {
private:
    template<const char *EventName>
    using EventDataType = typename get_event_data_type<EventMapType, EventName>::type;

    std::unordered_map<std::string, std::vector<std::function<void(const void *)> > > listeners;

public:
    virtual ~EventDispatcher() = default;

    /** Add event listener - only works for defined event types */
    template<const char *EventName>
    void addEventListener(std::function<void(EventDataType<EventName>)> listener) {
        static_assert(has_event<EventMapType, EventName>::value, "Event type not defined in EventMap");

        auto wrapper = [listener](const void *data) {
                           if constexpr (std::is_same_v<EventDataType<EventName>, void>) {
                               listener();
                           }
                           else {
                               listener(*static_cast<const EventDataType<EventName> *>(data));
                           }
                       };

        this->listeners[EventName].push_back(wrapper);
    }

    /** Remove event listener - only works for defined event types */
    template<const char *EventName>
    void removeEventListener() {
        static_assert(has_event<EventMapType, EventName>::value, "Event type not defined in EventMap");
        this->listeners.erase(EventName);
    }

    /** Remove all event listeners */
    void removeAllEventListeners() {
        this->listeners.clear();
    }

    /** Dispatch event - only works for defined event types with correct data type */
    template<const char *EventName>
    void dispatchEvent(const EventDataType<EventName> &data) {
        static_assert(has_event<EventMapType, EventName>::value, "Event type not defined in EventMap");

        auto it = this->listeners.find(EventName);
        if (it != this->listeners.end()) {
            for (const auto &listener : it->second) {
                if constexpr (std::is_same_v<EventDataType<EventName>, void>) {
                    listener(nullptr);
                }
                else {
                    listener(&data);
                }
            }
        }
    }


    /** Check if event listener is registered */
    template<const char *EventName>
    bool hasEventListener() const {
        static_assert(has_event<EventMapType, EventName>::value, "Event type not defined in EventMap");
        auto it = this->listeners.find(EventName);

        return it != this->listeners.end() && !it->second.empty();
    }
};