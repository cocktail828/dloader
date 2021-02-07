/*
 * @Author: sinpo828
 * @Date: 2021-02-07 13:18:21
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-07 13:18:21
 * @Description: file content
 */
#ifndef __SCOPEGUARD__
#define __SCOPEGUARD__

namespace ScopeGuard
{
    template <typename func>
    class Scope
    {
    private:
        func defer_func;
        bool active;

    public:
        Scope() = delete;
        Scope(const func &f) : defer_func(f), active(true) {}
        Scope(func &&f) : defer_func(std::forward<func>(f)), active(true) {}
        Scope &operator=(const Scope &) = delete;
        Scope(const Scope &s) = delete;
        Scope(Scope &&s) : defer_func(std::move(s.defer_func)), active(s.active) { s.active = false; }

        ~Scope()
        {
            if (active)
                defer_func();
        }
    };

    enum class ScopeEnumHelper
    {
    };

    template <typename func>
    inline Scope<func> operator+(ScopeEnumHelper, func &&f)
    {
        return Scope<func>(std::forward<func>(f));
    }
}; // namespace ScopeGuard

#define __SCOPEGUARD_HELPER_IMPL(a, b) a##_##b
#define __SCOPEGUARD_HELPER(a, b) __SCOPEGUARD_HELPER_IMPL(a, b)
#define ON_SCOPE_EXIT auto __SCOPEGUARD_HELPER(RAII_AT_EXIT, __LINE__) = ScopeGuard::ScopeEnumHelper() + [&]()

#endif //__SCOPEGUARD__