#pragma once

#ifndef FOCUS_MEMORY_HPP
#define FOCUS_MEMORY_HPP

#include <functional>
#include <memory>

template <typename T, template <typename> class SharedPtr = std::shared_ptr, template <typename> class WeakPtr = std::weak_ptr> class FocusMemory {
  public:
    using Shared = SharedPtr<T>;
    using Weak   = WeakPtr<T>;

    void remember(const Shared& item) {
        if (item)
            m_last = item;
    }

    Shared recall(const std::function<bool(const Shared&)>& validator) const {
        auto locked = m_last.lock();
        if (!locked)
            return nullptr;
        if (validator && !validator(locked))
            return nullptr;
        return locked;
    }

    void forget(const Shared& item) {
        if (!item)
            return;
        auto locked = m_last.lock();
        if (locked && locked == item)
            m_last.reset();
    }

    void reset() { m_last.reset(); }

  private:
    Weak m_last;
};

#endif
