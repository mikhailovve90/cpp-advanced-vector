#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <iostream>
/*Шаблонный класс RawMemory будет отвечать за хранение буфера,
который вмещает заданное количество элементов, и предоставлять
доступ к элементам по индексу*/
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
    RawMemory& operator=(const RawMemory& rhs) = delete;


    RawMemory(RawMemory&& other) noexcept {
        this->Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(std::move(rhs));
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
    /*Если размер выделяемого массива нулевой, функция Allocate не выделяет память,
    а просто возвращает nullptr. Такая оптимизация направлена на экономию
    памяти — нет смысла выделять динамическую память под хранение пустого массива и
    зря расходовать память под служебные данные.*/
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
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (size_ == Capacity() && size_ > 0) {
            // Создаю копию принятого значения дя вставки, что бы не потерять при реаалокации;
            RawMemory<T> new_data(size_ * 2);
            const auto index_position = std::distance(cbegin(), pos);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                new (new_data  + index_position) T(std::forward<Args>(args)...);
                std::uninitialized_move_n(data_.GetAddress(), index_position, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + index_position, size_ - index_position,
                                          (new_data.GetAddress() + index_position + 1));
            } else {
                new (new_data  + index_position) T(std::forward<Args>(args)...);
                std::uninitialized_copy_n(data_.GetAddress(), index_position, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + index_position, size_ - index_position,
                                          (new_data.GetAddress() + index_position + 1));
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            size_ += 1;
            return begin() + index_position;
        } else if (size_ > 0) {
            const auto index_position = std::distance(cbegin(), pos);
            T copy_elem(std::forward<Args>(args)...);
            new (data_  + size_) T(std::move(data_[size_ -1]));
            std::move_backward(begin() + index_position, end() - 1, end());
            data_[index_position] = std::move(copy_elem);
            size_ += 1;
            return begin() + index_position;
        } else {
            EmplaceBack(std::forward<Args>(args)...);
            return data_.GetAddress();
        }
    }

    iterator Erase(const_iterator pos) noexcept {
        if(size_ > 0) {
            const auto index_position = std::distance(cbegin(), pos);
            if constexpr(std::is_nothrow_move_assignable_v<T>) {
                std::move(begin() + index_position + 1, end(), begin() + index_position);
            }
            std::destroy_n(end() - 1, 1);
            size_ -= 1;
            if(size_ > 0) {
                return begin() + index_position;
            }
            return end();
        }
        return end();
    }


    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, std::move(value));
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos,std::forward<T>(value));
    }

    Vector() = default;

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)) {
        std::swap(this->size_, other.size_);
    };

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /*применяю copy-and-swap только когда вместимости вектора-приёмника не хватает,
                чтобы вместить все элементы вектора-источника:*/
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if(size_ > rhs.size_) {
                    /* Размер вектора-источника меньше размера вектора-приёмника.
                    Тогда нужно скопировать имеющиеся элементы из источника в приёмник,
                    а избыточные элементы вектора-приёмника разрушить:*/
                    for(size_t i = 0; i < rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                    size_ = rhs.size_;
                } else {
                    /*Размер вектора-источника больше или равен размеру вектора-приёмника.
                    Тогда нужно присвоить существующим элементам приёмника значения соответствующих
                    элементов источника, а оставшиеся скопировать в свободную область,
                    используя функцию uninitialized_copy или uninitialized_copy_n.*/
                    for(size_t i = 0; i < size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                    size_ = rhs.size_;
                }

            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept  {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /*применяю copy-and-swap только когда вместимости вектора-приёмника не хватает,
                чтобы вместить все элементы вектора-источника:*/
                Vector rhs_copy(std::move(rhs));
                Swap(rhs_copy);
            }
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        this->data_.Swap(other.data_);
        std::swap(this->size_, other.size_);
    };

    /*Этот конструктор сначала выделяет в сырой памяти буфер, достаточный для
    хранения  элементов в количестве, равном size. Затем конструирует в сырой
    памяти элементы массива. Для этого он вызывает их конструктор по умолчанию,
    используя размещающий оператор new.*/
    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    /*Чтобы создать копию контейнера Vector, выделим память под нужное количество
    элементов, а затем сконструируем в ней копию элементов оригинального контейнера,
    используя функцию CopyConstruct*/
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    /*Cначала необходимо вызвать деструкторы у size_ элементов массива, используя функцию DestroyN. Затем
    нужно освободить выделенную динамическую память, используя функцию Deallocate.*/
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }
    /*В константном операторе [] используется оператор  const_cast, чтобы снять
    константность с ссылки на текущий объект и вызвать неконстантную версию
    оператора [].*/
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    /*Метод Reserve предназначен для заблаговременного резервирования памяти под
    элементы вектора,когда известно их примерное количество.
    Если требуемая вместимость больше текущей, Reserve выделяет нужный объём сырой
    памяти. На следующем шаге из массива data_ копируются значения в только что
    выделенную область памяти.*/
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if(new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }

        if(new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    /*Допустим, вы сначала переместили существующие элементы вектора в новую область памяти.
    Если value ссылался на существующий элемент, в конец вектора будет вставлена копия объекта,
    значение из которого перемещено. Поэтому сначала надо создать копию объекта в позиции size_,
    а потом переместить существующие элементы. Стандартный
    vector использует подобную стратегию вставки элемента.*/
    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data  + size_) T({value});
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            size_ += 1;
        } else {
            new (data_ + size_) T(value);
            size_ += 1;
        }
    }

    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data  + size_) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            size_ += 1;
        } else {
            new (data_ + size_) T(std::move(value));
            size_ += 1;
        }
    }
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data  + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            size_ += 1;
            return data_[size_ - 1];
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
            size_ += 1;
            return data_[size_ - 1];
        }
    }





    void PopBack() noexcept {
        if(size_ > 0) {
            std::destroy_n(data_.GetAddress() + (size_ - 1), 1);
            --size_;
        }
    }

  private:
    RawMemory<T> data_;
    size_t size_ = 0;

};

