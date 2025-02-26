////////////////////////////////////////////////////////////////////////////////////////
// Supporting code for blog post "C++ Coroutines: Understanding the Compiler Transform"
//
// https://lewissbaker.github.io/2022/08/27/understanding-the-compiler-transform
//
// By Lewis Baker
//
// This code sample shows the lowering of the following simple coroutine into equivalent
// non-coroutine C++ code.
//
//   task f(int x) {
//     co_return x;
//   }
//
//   task g(int x) {
//     int fx = co_await f(x);
//     co_return fx * fx;
//   }
//
///////////////////////////////////////////////////////////////////////////////////////

// Uncomment the following line to see the compilation with visibility of
// task coroutine type method definitions. This should allow you to see more
// closely what the final generated code would look like whereas with this
// commented out it is easier to see where the compiler is inserting calls
// to these methods as they won't be inlined.

#include<cstddef>
#include<concepts>
#include<type_traits>
#include<memory>
#include<utility>

//////////////////////////////////////////////////
// <coroutine> header definitions
//
// This section contains definitions of the parts of <coroutine> header needed by
// the lowering and implementation below.

struct __coroutine_state
{
    using  __resume_fn = __coroutine_state *(__coroutine_state *);
    using __destroy_fn =              void  (__coroutine_state *);

     __resume_fn *__resume;
    __destroy_fn *__destroy;

    static const __coroutine_state __noop_coroutine;

    static __coroutine_state * __noop_resume(__coroutine_state *__state) noexcept
    {
        return __state;
    }

    static void __noop_destroy(__coroutine_state *) noexcept
    {
    }
};

inline const __coroutine_state __coroutine_state::__noop_coroutine
{
    &__coroutine_state::__noop_resume,
    &__coroutine_state::__noop_destroy
};

template<typename Promise> struct __coroutine_state_with_promise: __coroutine_state
{
    union
    {
        Promise __promise;
    };

    /**/  __coroutine_state_with_promise() noexcept {}
    /**/ ~__coroutine_state_with_promise()          {}
};

namespace std
{
    template<typename RetObj, typename... Args> struct coroutine_traits
    {
        using promise_type = typename std::remove_cvref_t<RetObj>::promise_type;
    };

    template<typename Promise = void> class coroutine_handle;
    template<> class coroutine_handle<void>
    {
        private:
            __coroutine_state * state_ = nullptr;

        public:
            coroutine_handle            (                        ) noexcept = default;
            coroutine_handle            (const coroutine_handle &) noexcept = default;
            coroutine_handle & operator=(const coroutine_handle &) noexcept = default;

        public:
            void * address() const
            {
                return static_cast<void *>(state_);
            }

            static coroutine_handle from_address(void *ptr)
            {
                coroutine_handle h;
                h.state_ = static_cast<__coroutine_state *>(ptr);
                return h;
            }

            explicit operator bool() noexcept
            {
                return state_ != nullptr;
            }

            friend bool operator==(coroutine_handle a, coroutine_handle b) noexcept
            {
                return a.state_ == b.state_;
            }

            void resume() const
            {
                __coroutine_state *s = state_;
                do{
                    s = s->__resume(s);
                }
                while(s != &__coroutine_state::__noop_coroutine);
            }

            void destroy() const
            {
                state_->__destroy(state_);
            }

            bool done() const
            {
                return state_->__resume == nullptr;
            }
    };

    template<typename Promise> class coroutine_handle
    {
        private:
            using state_t = __coroutine_state_with_promise<Promise>;

        private:
            state_t * state_;

        public:
            coroutine_handle            (                        ) noexcept = default;
            coroutine_handle            (const coroutine_handle &) noexcept = default;
            coroutine_handle & operator=(const coroutine_handle &) noexcept = default;

        public:
            operator coroutine_handle<void>() const noexcept
            {
                return coroutine_handle<void>::from_address(address());
            }

            explicit operator bool() const noexcept
            {
                return state_ != nullptr;
            }

            friend bool operator==(coroutine_handle a, coroutine_handle b) noexcept
            {
                return a.state_ == b.state_;
            }

            void * address() const
            {
                return static_cast<void *>(static_cast<__coroutine_state *>(state_));
            }

            static coroutine_handle from_address(void * ptr)
            {
                coroutine_handle h;
                h.state_ = static_cast<state_t *>(static_cast<__coroutine_state *>(ptr));
                return h;
            }

            Promise & promise() const
            {
                return state_->__promise;
            }

            static coroutine_handle from_promise(Promise & promise)
            {
                coroutine_handle h;

                // We know the address of the __promise member
                // so calculate the address of the coroutine-state by subtracting the offset of the __promise field from this address
                h.state_ = reinterpret_cast<state_t *>(reinterpret_cast<unsigned char *>(std::addressof(promise)) - offsetof(state_t, __promise));
                return h;
            }

            // Define these in terms of their `coroutine_handle<void>` implementations

            void resume() const
            {
                static_cast<coroutine_handle<void>>(*this).resume();
            }

            void destroy() const
            {
                static_cast<coroutine_handle<void>>(*this).destroy();
            }

            bool done() const
            {
                return static_cast<coroutine_handle<void>>(*this).done();
            }
    };

    struct noop_coroutine_promise {};
    using  noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

    noop_coroutine_handle noop_coroutine() noexcept;

    template<> class coroutine_handle<noop_coroutine_promise>
    {
        public:
            constexpr coroutine_handle            (const coroutine_handle &) noexcept = default;
            constexpr coroutine_handle & operator=(const coroutine_handle &) noexcept = default;

        public:
            constexpr explicit operator bool() noexcept
            {
                return true;
            }

            constexpr friend bool operator==(coroutine_handle, coroutine_handle) noexcept
            {
                return true;
            }

            operator coroutine_handle<void>() const noexcept
            {
                return coroutine_handle<void>::from_address(address());
            }

            noop_coroutine_promise & promise() const noexcept
            {
                static noop_coroutine_promise promise;
                return promise;
            }

            constexpr void resume () const noexcept {}
            constexpr void destroy() const noexcept {}
            constexpr bool done   () const noexcept { return false; }

        public:
            constexpr void * address() const noexcept
            {
                return const_cast<__coroutine_state *>(std::addressof(__coroutine_state::__noop_coroutine));
            }

        private:
            constexpr coroutine_handle() noexcept = default;

        private:
            friend noop_coroutine_handle noop_coroutine() noexcept
            {
                return {};
            }
    };

    struct suspend_always
    {
        constexpr suspend_always() noexcept = default;

        constexpr bool await_ready  ()                   const noexcept { return false; }
        constexpr void await_suspend(coroutine_handle<>) const noexcept {}
        constexpr void await_resume ()                   const noexcept {}
    };
}

////////////////////////////////////////////////////////////////////////
// Definition of the 'task' coroutine type used by this example

#include<variant>
#include<exception>

class task
{
    public:
        struct awaiter;

    public:
        class promise_type
        {
            private:
                friend task::awaiter;
                std::coroutine_handle<> continuation_;
                std::variant<std::monostate, int, std::exception_ptr> result_;

            public:
                /**/  promise_type() noexcept {}
                /**/ ~promise_type()          {}

            public:
                struct final_awaiter
                {
                    bool                    await_ready  ()                                      noexcept { return false; }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept { return h.promise().continuation_; }
                    void                    await_resume ()                                      noexcept {}
                };

                task get_return_object() noexcept
                {
                    return task{std::coroutine_handle<task::promise_type>::from_promise(*this)};
                }

                std::suspend_always initial_suspend() noexcept { return {}; }
                final_awaiter         final_suspend() noexcept { return {}; }

                void return_value       (int result) noexcept { result_ = result;                   }
                void unhandled_exception()           noexcept { result_ = std::current_exception(); }
        };

    private:
        std::coroutine_handle<promise_type> coro_;

    public:
        task(task && t) noexcept
            : coro_(std::exchange(t.coro_, {}))
        {}

        ~task()
        {
            if(coro_){
                coro_.destroy();
            }
        }

        task & operator=(task && t) noexcept
        {
            task tmp = std::move(t);
            using std::swap;

            swap(coro_, tmp.coro_);
            return *this;
        }

        struct awaiter
        {
            private:
                std::coroutine_handle<promise_type> coro_;

            public:
                explicit awaiter(std::coroutine_handle<promise_type> h) noexcept
                    : coro_(h)
                {}

                bool await_ready() noexcept
                {
                    return false;
                }

                std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> h) noexcept
                {
                    coro_.promise().continuation_ = h;
                    return coro_;
                }

                int await_resume()
                {
                    if(coro_.promise().result_.index() == 2){
                        std::rethrow_exception(std::get<2>(std::move(coro_.promise().result_)));
                    }
                    else{
                        return std::get<1>(coro_.promise().result_);
                    }
                }
        };

        awaiter operator co_await() && noexcept
        {
            return awaiter{coro_};
        }

    private:
        explicit task(std::coroutine_handle<promise_type> h) noexcept
            : coro_(h)
        {}

    public:
        int execute()
        {
            // add this member function to access result from a non-coroutine
            // need to setup continuation_ to describe what to do after task finished, it's noop_coroutine since execute() is not a coroutine

            awaiter{coro_}.await_suspend(std::noop_coroutine());
            coro_.resume();
            return awaiter{coro_}.await_resume();
        }
};

//////////////////////
// Helpers used by Coroutine Lowering

template<typename T> struct manual_lifetime
{
    private:
        alignas(T) std::byte storage[sizeof(T)];

    public:
        /**/  manual_lifetime() noexcept = default;
        /**/ ~manual_lifetime()          = default;

        manual_lifetime            (const manual_lifetime  &) = delete;
        manual_lifetime            (      manual_lifetime &&) = delete;
        manual_lifetime & operator=(const manual_lifetime  &) = delete;
        manual_lifetime & operator=(      manual_lifetime &&) = delete;

        template<typename Factory>
            requires std::invocable<Factory &> &&
                     std::same_as<std::invoke_result_t<Factory &>, T>
        T & construct_from(Factory factory) noexcept(std::is_nothrow_invocable_v<Factory &>)
        {
            return *::new (static_cast<void *>(&storage)) T(factory());
        }

        void destroy() noexcept(std::is_nothrow_destructible_v<T>)
        {
            std::destroy_at(std::launder(reinterpret_cast<T *>(&storage)));
        }

        T & get() & noexcept
        {
            return *std::launder(reinterpret_cast<T *>(&storage));
        }
};

template<typename T> struct destructor_guard
{
    private:
        manual_lifetime<T> * ptr_;

    public:
        explicit destructor_guard(manual_lifetime<T> & obj) noexcept
            : ptr_(std::addressof(obj))
        {}

        ~destructor_guard() noexcept(std::is_nothrow_destructible_v<T>)
        {
            if (ptr_ != nullptr) {
                ptr_->destroy();
            }
        }

        destructor_guard            (destructor_guard &&) = delete;
        destructor_guard & operator=(destructor_guard &&) = delete;

    public:
        void cancel() noexcept
        {
            ptr_ = nullptr;
        }
};

// Parital specialisation for types that don't need their destructors called.
template<typename T>
    requires std::is_trivially_destructible_v<T>
struct destructor_guard<T>
{
    explicit destructor_guard(manual_lifetime<T> &) noexcept {}
    void cancel() noexcept {}
};

// Class-template argument deduction to simplify usage
template<typename T> destructor_guard(manual_lifetime<T>& obj) -> destructor_guard<T>;

template<typename Promise, typename... Params> Promise construct_promise([[maybe_unused]] Params &... params)
{
    if constexpr (std::constructible_from<Promise, Params &...>){
        return Promise(params...);
    }
    else{
        return Promise();
    }
}


// Forward declaration of a function called by the function we are lowering.
task f(int x);
task g(int x);
