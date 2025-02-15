//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17

// <memory>

// template <class T, class ...Args>
// constexpr T* construct_at(T* location, Args&& ...args);

#include <memory>
#include <cassert>
#include <utility>

#include "test_iterators.h"

struct Foo {
    constexpr Foo() { }
    constexpr Foo(int a, char b, double c) : a_(a), b_(b), c_(c) { }
    constexpr Foo(int a, char b, double c, int* count) : Foo(a, b, c) { *count += 1; }
    constexpr bool operator==(Foo const& other) const {
        return a_ == other.a_ && b_ == other.b_ && c_ == other.c_;
    }

private:
    int a_;
    char b_;
    double c_;
};

struct Counted {
    int& count_;
    constexpr Counted(int& count) : count_(count) { ++count; }
    constexpr Counted(Counted const& that) : count_(that.count_) { ++count_; }
    constexpr ~Counted() { --count_; }
};

union union_t {
    int first{42};
    double second;
};

struct NotAssignable {
  NotAssignable() = default;
  NotAssignable(const NotAssignable&) = default;
  NotAssignable(NotAssignable&&) = default;

  NotAssignable& operator=(const NotAssignable&) = delete;
  NotAssignable& operator=(NotAssignable&&) = delete;
};

constexpr bool move_assignment_called = false;
struct Always_false {
    constexpr Always_false(const bool val) noexcept { assert(val);}
};

struct WithSpecialMoveAssignment {
  WithSpecialMoveAssignment() = default;
  WithSpecialMoveAssignment(const WithSpecialMoveAssignment&) = default;
  WithSpecialMoveAssignment(WithSpecialMoveAssignment&&) = default;
  WithSpecialMoveAssignment& operator=(const WithSpecialMoveAssignment&) = default;
  constexpr WithSpecialMoveAssignment& operator=(WithSpecialMoveAssignment&&) noexcept {
    [[maybe_unused]] Always_false invalid{move_assignment_called};
    return *this;
  };
};
static_assert(std::is_trivially_constructible_v<WithSpecialMoveAssignment>);

constexpr bool test()
{
    {
        int i = 99;
        int* res = std::construct_at(&i);
        assert(res == &i);
        assert(*res == 0);
    }

    {
        int i = 0;
        int* res = std::construct_at(&i, 42);
        assert(res == &i);
        assert(*res == 42);
    }

    {
        Foo foo = {};
        int count = 0;
        Foo* res = std::construct_at(&foo, 42, 'x', 123.89, &count);
        assert(res == &foo);
        assert(*res == Foo(42, 'x', 123.89));
        assert(count == 1);
    }

    // switching of the active member of a union must work
    {
        union_t with_int{};
        double* res = std::construct_at(&with_int.second, 123.89);
        assert(res == &with_int.second);
        assert(*res == 123.89);
    }

    // ensure that we can construct trivially constructible types with a deleted move assignment
    {
        NotAssignable not_assignable{};
        NotAssignable* res = std::construct_at(&not_assignable);
        assert(res == &not_assignable);
    }

    // ensure that we can construct trivially constructible types with a nefarious move assignment
    {
        WithSpecialMoveAssignment with_special_move_assignment{};
        WithSpecialMoveAssignment* res = std::construct_at(&with_special_move_assignment);
        assert(res == &with_special_move_assignment);
    }

    if (!std::is_constant_evaluated()) // constexpr allocator is not implemented yet
    {
        std::allocator<Counted> a;
        Counted* p = a.allocate(2);
        int count = 0;
        std::construct_at(p, count);
        assert(count == 1);
        std::construct_at(p+1, count);
        assert(count == 2);
        (p+1)->~Counted();
        assert(count == 1);
        p->~Counted();
        assert(count == 0);
        a.deallocate(p, 2);
    }

    if (!std::is_constant_evaluated()) // constexpr allocator is not implemented yet
    {
        std::allocator<Counted const> a;
        Counted const* p = a.allocate(2);
        int count = 0;
        std::construct_at(p, count);
        assert(count == 1);
        std::construct_at(p+1, count);
        assert(count == 2);
        (p+1)->~Counted();
        assert(count == 1);
        p->~Counted();
        assert(count == 0);
        a.deallocate(p, 2);
    }

    return true;
}

template <class ...Args, class = decltype(std::construct_at(std::declval<Args>()...))>
constexpr bool can_construct_at(Args&&...) { return true; }

template <class ...Args>
constexpr bool can_construct_at(...) { return false; }

// Check that SFINAE works.
static_assert( can_construct_at((int*)nullptr, 42));
static_assert( can_construct_at((Foo*)nullptr, 1, '2', 3.0));
static_assert(!can_construct_at((Foo*)nullptr, 1, '2'));
static_assert(!can_construct_at((Foo*)nullptr, 1, '2', 3.0, 4));
static_assert(!can_construct_at(nullptr, 1, '2', 3.0));
static_assert(!can_construct_at((int*)nullptr, 1, '2', 3.0));
//static_assert(!can_construct_at(contiguous_iterator<Foo*>(), 1, '2', 3.0));
// Can't construct function pointers.
static_assert(!can_construct_at((int(*)())nullptr));
static_assert(!can_construct_at((int(*)())nullptr, nullptr));

int main(int, char**)
{
    test();
    static_assert(test());
    return 0;
}
