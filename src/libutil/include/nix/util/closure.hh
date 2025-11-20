#pragma once
///@file

#include <set>
#include <future>
#include "nix/util/sync.hh"
#include "nix/util/thread-pool.hh"

using std::set;

namespace nix {

template<typename T>
using GetEdgesAsync = std::function<void(const T &, std::function<void(std::promise<set<T>> &)>)>;

template<typename T>
void computeClosure(const set<T> startElts, set<T> & res, GetEdgesAsync<T> getEdgesAsync)
{
    struct State
    {
        set<T> & res;
        std::exception_ptr exc;
    };

    Sync<State> state_(State{res, 0});

    ThreadPool pool(0);

    auto enqueue = [&](this auto & enqueue, const T & current) -> void {
        {
            auto state(state_.lock());
            if (state->exc)
                return;
            if (!state->res.insert(current).second)
                return;
        }
        pool.enqueue([&, current] {
            getEdgesAsync(current, [&](std::promise<set<T>> & prom) {
                try {
                    auto children = prom.get_future().get();
                    for (auto & child : children)
                        enqueue(child);
                } catch (...) {
                    auto state(state_.lock());
                    if (!state->exc)
                        state->exc = std::current_exception();
                };
            });
        });
    };

    for (auto & startElt : startElts)
        enqueue(startElt);

    pool.process();
}

} // namespace nix
