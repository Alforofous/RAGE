#pragma once

template<typename T>
class IPositionable {
public:
    virtual ~IPositionable() = default;
    virtual T getPosition() const = 0;
};

template<typename T>
class ISizable {
public:
    virtual ~ISizable() = default;
    virtual T getSize() const = 0;
};

template<typename T>
class IColorable {
public:
    virtual ~IColorable() = default;
    virtual T getColor() const = 0;
};