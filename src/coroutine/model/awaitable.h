//
// Created by Ricardo Evans on 2023/6/6.
//

#ifndef NS3_COROUTINE_OPERATION_AWAITABLE_H
#define NS3_COROUTINE_OPERATION_AWAITABLE_H

#include <functional>
#include <utility>

#include "operation-trait.h"
#include "operation-type.h"

namespace ns3 {
    class ConditionalAwaitable {
    private:
        const bool condition;
    public:
        constexpr ConditionalAwaitable(bool condition) noexcept: condition(condition) {}

        constexpr bool await_ready() const noexcept {
            return condition;
        }

        constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

        constexpr void await_resume() const noexcept {}

        ~ConditionalAwaitable() = default;
    };

    template<typename R>
    class CoroutineOperationAwaitable {
    public:
        constexpr explicit CoroutineOperationAwaitable(CoroutineOperation<R> &&operation) noexcept(std::is_nothrow_move_constructible_v<CoroutineOperation<R>>): operation(std::move(operation)) {}

        constexpr explicit CoroutineOperationAwaitable(const CoroutineOperation<R> &operation) noexcept(std::is_nothrow_copy_constructible_v<CoroutineOperation<R>>): operation(operation) {}

        constexpr CoroutineOperationAwaitable(CoroutineOperationAwaitable &&awaitable) noexcept(std::is_nothrow_move_constructible_v<CoroutineOperation<R>>) = default;

        CoroutineOperationAwaitable(const CoroutineOperationAwaitable &awaitable) noexcept(std::is_nothrow_copy_constructible_v<CoroutineOperation<R>>) = default;

        constexpr CoroutineOperationAwaitable &operator=(CoroutineOperationAwaitable &&awaitable) noexcept(std::is_nothrow_move_assignable_v<CoroutineOperation<R>>) = default;

        CoroutineOperationAwaitable &operator=(const CoroutineOperationAwaitable &awaitable) noexcept(std::is_nothrow_copy_assignable_v<CoroutineOperation<R>>) = default;

        constexpr bool await_ready() const noexcept {
            return operation.done();
        }

        void await_suspend(std::coroutine_handle<> h) {
            if constexpr (std::is_void_v<R>) {
                operation.onComplete([h](auto &) { h.resume(); });
            } else {
                operation.onComplete([h](auto &, auto &) { h.resume(); });
            }
        }

        decltype(auto) await_resume() noexcept(false) {
            return operation.result();
        }

        decltype(auto) await_resume() const noexcept(false) {
            return operation.result();
        }

        ~CoroutineOperationAwaitable() = default;

    private:
        CoroutineOperation<R> operation;
    };
}

#endif //NS3_COROUTINE_OPERATION_AWAITABLE_H
