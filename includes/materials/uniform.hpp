#pragma once
#include <cstring>
#include <stdexcept>

/**
 * Base class for type-erased uniform storage.
 */
class UniformBase {
public:
    virtual ~UniformBase() = default;
    virtual size_t getSize() const = 0;
    virtual const void *getDataPtr() const = 0;
    virtual void copyTo(void *buffer, size_t bufferSize) const = 0;
};

/**
 * Template class for type-safe uniform storage.
 * Stores a single uniform value of type T.
 */
template<typename T>
class Uniform : public UniformBase {
public:
    Uniform() = default;

    explicit Uniform(const T &value) : data(value) {}

    void setValue(const T &value) { this->data = value; }

    const T& getValue() const { return this->data; }

    const void *getDataPtr() const override { return &this->data; }

    static constexpr size_t getSizeStatic() { return sizeof(T); }

    size_t getSize() const override { return sizeof(T); }

    void copyTo(void *buffer, size_t bufferSize) const override {
        if (bufferSize < sizeof(T)) {
            throw std::runtime_error("Buffer too small for uniform data");
        }
        std::memcpy(buffer, &this->data, sizeof(T));
    }

private:
    T data{};
};