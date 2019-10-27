#ifndef SENTRY_BACKENDS_INPROC_HPP_INCLUDED
#define SENTRY_BACKENDS_INPROC_HPP_INCLUDED
#ifdef SENTRY_WITH_INPROC_BACKEND

#include <signal.h>
#include "../internal.hpp"
#include "../scope.hpp"
#include "base_backend.hpp"

namespace sentry {
namespace backends {

class InprocBackend : public Backend {
   public:
    InprocBackend();
    ~InprocBackend();
    void start();
    void flush_scope_state(const sentry::Scope &scope);
    void add_breadcrumb(sentry::Value breadcrumb);

    static const size_t SIGNAL_COUNT = 6;
};
}  // namespace backends
}  // namespace sentry

#endif
#endif
