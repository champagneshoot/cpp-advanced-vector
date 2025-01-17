#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    //RawMemory(const RawMemory&) = delete;
    //RawMemory& operator=(const RawMemory& rhs) = delete;

    //RawMemory(RawMemory&& other) noexcept { ... }
    //RawMemory& operator=(RawMemory&& rhs) noexcept { ... }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
    {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs)
    {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity())
            {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else
            {
                if (size_ > rhs.size_)
                {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else
                {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);

                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept
    {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        DestroyN(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }


    void Resize(size_t new_size)
    {
        if (size_ > new_size)
        {
            DestroyN(data_.GetAddress() + new_size, size_ - new_size);
        }
        else
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }
    void PushBack(const T& value)
    {
        EmplaceBack(value);
    }

    void PushBack(T&& value)
    {
        EmplaceBack(std::move(value));
    }

    void PopBack() /* noexcept */
    {
        if (size_ > 0)
        {
            DestroyN(data_.GetAddress() + size_ - 1, 1);
            --size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        /*
           Реализация похожа на PushBack, только вместо копирования или перемещения
           переданного элемента, он конструируется путём передачи параметров метода конструктору T
        */
        T* res = nullptr;
        if (size_ == Capacity())
        {

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            res = new (new_data + size_) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else
            {
                try
                {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());

                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), size_);
                    throw;
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {

            res = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *res;
    }

    ~Vector()
    {
        DestroyN(data_.GetAddress(), size_);
    }


    iterator begin() noexcept
    {
        return data_.GetAddress();
    }
    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept
    {
        return const_cast<const_iterator>(data_.GetAddress());
    }
    const_iterator end() const noexcept
    {
        return const_cast<const_iterator>(data_.GetAddress() + size_);
    }
    const_iterator cbegin() const noexcept
    {
        return begin();
    }
    const_iterator cend() const noexcept
    {
        return end();
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value)
    {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        assert(pos >= begin() && pos <= end());
        int position = pos - begin();

        if (data_.Capacity() <= size_)
        {

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            new (new_data.GetAddress() + position) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), position, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + position, size_ - position, new_data.GetAddress() + position + 1);

            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), position, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + position, size_ - position, new_data.GetAddress() + position + 1);
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

        }
        else {

            try {

                if (pos != end()) {

                    T new_s(std::forward<Args>(args)...);
                    new (end()) T(std::forward<T>(data_[size_ - 1]));

                    std::move_backward(begin() + position, end() - 1, end());
                    *(begin() + position) = std::forward<T>(new_s);

                }
                else {
                    new (end()) T(std::forward<Args>(args)...);
                }

            }
            catch (...) {
                operator delete (end());
                throw;
            }
        }

        size_++;
        return begin() + position;
    }
    iterator Erase(const_iterator pos)
    {
        size_t step = pos - begin();
        std::move(begin() + step + 1, end(), begin() + step);
        PopBack();
        return begin() + step;
    }

private:

    RawMemory<T> data_;
    size_t size_ = 0;

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};
