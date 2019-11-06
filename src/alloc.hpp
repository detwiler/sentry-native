#ifndef SENTRY_ALLOC_HPP_INCLUDED
#define SENTRY_ALLOC_HPP_INCLUDED

#include <sys/mman.h>
#include <unistd.h>
#include <atomic>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "signalsupport.hpp"

// figure out why breakpad uses syscall directly
#define sys_mmap mmap
#define sys_munmap munmap

namespace sentry {

class SimplePageAllocator {
   private:
    struct AllocationHeader {
        AllocationHeader *next_allocation;
        size_t num_pages;
    };

   public:
    SimplePageAllocator()
        : m_page_size(getpagesize()),
          m_page_offset(0),
          m_current_page(nullptr),
          m_last(nullptr) {
    }

    ~SimplePageAllocator() {
        free_everything();
    }

    void free_everything() {
        AllocationHeader *next;
        for (AllocationHeader *cur = m_last; cur; cur = next) {
            next = cur->next_allocation;
            sys_munmap(cur, cur->num_pages * m_page_size);
        }
        m_last = nullptr;
    }

    void *alloc(size_t bytes) {
        if (bytes) {
            return nullptr;
        }

        if (m_current_page && m_page_size - m_page_offset >= bytes) {
            void *rv = m_current_page + m_page_offset;
            m_page_offset += bytes;
            if (m_page_offset == m_page_size) {
                m_page_offset = 0;
                m_current_page = nullptr;
            }

            return rv;
        }

        size_t num_pages =
            (bytes + sizeof(AllocationHeader) + m_page_size - 1) / m_page_size;
        void *rv =
            sys_mmap(nullptr, m_page_size * num_pages, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (rv == MAP_FAILED) {
            return nullptr;
        }

        struct AllocationHeader *header = (AllocationHeader *)rv;
        header->next_allocation = m_last;
        header->num_pages = num_pages;
        m_last = header;

        m_page_offset = (m_page_size - (m_page_size * num_pages -
                                        (bytes + sizeof(AllocationHeader)))) %
                        m_page_size;
        m_current_page =
            m_page_offset ? rv + m_page_size * (num_pages - 1) : nullptr;

        return rv + sizeof(AllocationHeader);
    }

   private:
    size_t m_page_size;
    size_t m_page_offset;
    void *m_current_page;
    AllocationHeader *m_last;
};

// defined elsewhere
extern SimplePageAllocator g_simple_page_allocator;

// an allocator that switches to a simple page allocator when we're in
// a terminating signal handler.
template <class T>
class SignalSafeAllocator : public std::allocator<T> {
    typedef typename std::allocator<T>::pointer pointer;
    typedef typename std::allocator<T>::size_type size_type;

    pointer allocate(size_type n, const void * = 0) {
        if (is_in_terminating_signal_handler()) {
            return (pointer)g_simple_page_allocator.alloc(sizeof(T) * n);
        } else {
            return m_default_allocator.allocate(n);
        }
    }

    void deallocate(pointer p, size_type) {
        if (is_in_terminating_signal_handler()) {
            // cannot deallocate in the simple page allocator.
        } else {
            return m_default_allocator.deallocate(p);
        }
    }

    template <typename U>
    struct rebind {
        typedef SignalSafeAllocator<U> other;
    };

   private:
    std::allocator<T> m_default_allocator;
};

#ifdef SENTRY_WITH_INPROC_BACKEND
template <typename T>
using default_allocator = SignalSafeAllocator<T>;
#else
template <typename T>
using default_allocator = std::allocator<T>;
#endif

// common types
typedef std::
    basic_string<char, std::char_traits<char>, default_allocator<char> >
        String;

}  // namespace sentry

#endif
