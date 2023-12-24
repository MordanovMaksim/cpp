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

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
    {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
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
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
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

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

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
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (data_.Capacity() < rhs.size_) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                int i = 0;
                for (; i < rhs.size_ && i < size_; ++i) {
                    data_[i] = rhs.data_[i];
                }

                std::uninitialized_copy_n(rhs.data_.GetAddress() + i, rhs.size_ - i, data_.GetAddress() + i);
                std::destroy_n(data_.GetAddress() + i, size_ - i);
                size_ = rhs.size_;
            }
        }

        return *this;
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(Vector&& other) noexcept {
        Swap(other);
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size > data_.Capacity()) {
            Reserve(new_size);
        }

        if (new_size > size_) {
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }

        size_ = new_size;
    }

    template <typename Type>
    void PushBack(Type&& value) {
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(std::forward<Type>(value));
        }
        else {
            RawMemory<T> new_data(data_.Capacity() == 0 ? 1 : data_.Capacity() * 2);
            new (new_data + size_) T(std::forward<Type>(value));

            if constexpr (std::is_nothrow_move_constructible_v<T>
                || !std::is_copy_constructible_v<T>
                ) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
    }
    
    void PopBack() noexcept {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }
    
    template<typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        else {
            RawMemory<T> new_data(data_.Capacity() == 0 ? 1 : data_.Capacity() * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T>
                || !std::is_copy_constructible_v<T>
                ) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;

        return data_[size_ - 1];
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return const_cast<Vector*>(this)->begin();
    }

    const_iterator end() const noexcept {
        return const_cast<Vector*>(this)->end();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/{
        size_t pos_number = pos - begin();

        if constexpr (std::is_nothrow_move_constructible_v<T>
            || !std::is_copy_constructible_v<T>) {
            std::move(data_.GetAddress() + pos_number + 1, end(), data_.GetAddress() + pos_number);
        }
        else {
            std::copy(data_.GetAddress() + pos_number + 1, end(), data_.GetAddress() + pos_number);
        }
        
        PopBack();
        return &data_[pos_number];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t pos_number = pos - begin();
        if (size_ < data_.Capacity()) {
            if (size_ > 0) {
                T new_el(std::forward<Args>(args)...);

                if constexpr (std::is_nothrow_move_constructible_v<T>
                    || !std::is_copy_constructible_v<T>) {
                    new (&data_[size_]) T(std::move(data_[size_ - 1]));
                    std::move_backward(begin() + pos_number, end() - 1, end());
                }
                else {
                    new (&data_[size_]) T(data_[size_ - 1]);
                    std::copy_backward(begin() + pos_number, end() - 1, end());
                }

                data_[pos_number] = std::move(new_el);
            }
            else {
                new (end()) T(std::forward<Args>(args)...);
            }
        }
        else {
            RawMemory<T> new_data(data_.Capacity() ? data_.Capacity() * 2 : 1);
            new (new_data.GetAddress() + pos_number) T(std::forward<Args>(args)...);

            if (size_ > 0) {
                if constexpr (std::is_nothrow_move_constructible_v<T>
                    || !std::is_copy_constructible_v<T>)
                {
                    std::uninitialized_move(begin(), begin() + pos_number, new_data.GetAddress());
                    std::uninitialized_move(begin() + pos_number, end(), new_data.GetAddress() + pos_number + 1);
                }
                else {
                    std::uninitialized_copy(begin(), begin() + pos_number, new_data.GetAddress());
                    std::uninitialized_copy(begin() + pos_number, end(), new_data.GetAddress() + pos_number + 1);
                }

                std::destroy_n(begin(), size_);
            }

            data_.Swap(new_data);
        }

        ++size_;
        return &data_[pos_number];
    }
        
private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
};