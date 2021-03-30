#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory&) = delete;

	RawMemory(RawMemory&& other) noexcept {
		Swap(other);
	}

	~RawMemory() {
		Deallocate(buffer_);
	}

	RawMemory& operator=(const RawMemory& rhs) = delete;

	RawMemory& operator=(RawMemory&& rhs) noexcept {
		Swap(rhs);

		return *this;
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
	T* buffer_ = nullptr;
	size_t capacity_ = 0;

	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}
};

template <typename T>
class Vector {
public:
	using iterator       = T*;
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
		std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
	}

	Vector(Vector&& other) noexcept {
		Swap(other);
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
	}

	Vector& operator=(const Vector& rhs) {
		if (this != &rhs) {
			if (rhs.size_ > data_.Capacity()) {
				Vector rhs_copy(rhs);
				Swap(rhs_copy);
			} else {
				if (size_ > rhs.size_) {
					CopyN(rhs.data_.GetAddress(), data_.GetAddress(), rhs.size_);
					std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
				} else {
					CopyN(rhs.data_.GetAddress(), data_.GetAddress(), size_);
					std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
				}
				size_ = rhs.size_;
			}
		}

		return *this;
	}

	Vector& operator=(Vector&& rhs) noexcept {
		Swap(rhs);

		return *this;
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<Vector&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < size_);

		return data_[index];
	}

	iterator begin() noexcept {
		return data_.GetAddress();
	}

	iterator end() noexcept {
		return data_ + size_;
	}

	const_iterator begin() const noexcept {
		return data_.GetAddress();
	}

	const_iterator end() const noexcept {
		return data_ + size_;
	}

	const_iterator cbegin() const noexcept {
		return data_.GetAddress();
	}

	const_iterator cend() const noexcept {
		return data_ + size_;
	}

	void Resize(size_t new_size) {
		if (size_ > new_size) {
			std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
		} else if (size_ < new_size) {
			Reserve(new_size);
			std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
		}
		size_ = new_size;
	}

	template <typename... Args>
	T& EmplaceBack(Args&&... args) {
		if (size_ == Capacity()) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data + size_) T(std::forward<Args>(args)...);

			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
			} else {
				std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
			}
			std::destroy_n(data_.GetAddress(), size_);

			data_.Swap(new_data);
		} else {
			new (data_ + size_) T(std::forward<Args>(args)...);
		}
		++size_;

		return data_[size_ - 1];
	}

	template<typename Type>
	void PushBack(Type&& value) {
		EmplaceBack(std::forward<Type>(value));
	}

	void PopBack() {
		std::destroy_n(data_ + size_ - 1, 1u);
		--size_;
	}

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		iterator res_pos = begin();
		if (pos == cend()) {
			res_pos = &EmplaceBack(std::forward<Args>(args)...);
		} else if (size_ == data_.Capacity()) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			const size_t dis_pos = (pos - begin());
			new (new_data + dis_pos) T(std::forward<Args>(args)...);
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress(), dis_pos, new_data.GetAddress());
			} else {
				try {
					std::uninitialized_copy_n(data_.GetAddress(), dis_pos, new_data.GetAddress());
				} catch (...) {
					std::destroy_n(new_data + dis_pos, 1u);
				}
			}

			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress() + dis_pos, size_ - dis_pos, new_data.GetAddress() + dis_pos + 1);
			} else {
				try {
					std::uninitialized_copy_n(data_.GetAddress() + dis_pos, size_ - dis_pos, new_data.GetAddress() + dis_pos + 1);
				} catch (...) {
					std::destroy_n(new_data.GetAddress(), dis_pos + 1);
				}
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
			res_pos = begin() + dis_pos;
			++size_;
		} else {
			T new_t(std::forward<Args>(args)...);
			new (data_ + size_) T(std::move(data_[size_ - 1u]));
			res_pos = begin() + (pos - begin());
			std::move_backward(res_pos, end() - 1, end());
			*res_pos = std::move(new_t);
			++size_;
		}

		return res_pos;
	}

	iterator Erase(const_iterator pos) {
		iterator res_pos = begin() + (pos - begin());
		std::move(res_pos + 1, end(), res_pos);
		std::destroy_n(end() - 1, 1u);
		--size_;

		return res_pos;
	}

	template <typename Type>
	iterator Insert(const_iterator pos, Type&& value) {
		return Emplace(pos, std::forward<Type>(value));
	}

	void Swap(Vector& other) noexcept {
		data_.Swap(other.data_);
		std::swap(size_, other.size_);
	}

	void Reserve(const size_t new_capacity) {
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

	size_t Size() const noexcept {
		return size_;
	}

	size_t Capacity() const noexcept {
		return data_.Capacity();
	}

private:
	RawMemory<T> data_;
	size_t size_ = 0;

	void CopyN(const T* from, T* to, const size_t n) {
		for (size_t i = 0; i < n; ++i) {
			*(to + i) = *(from + i);
		}
	}
};