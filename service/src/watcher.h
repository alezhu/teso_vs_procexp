//
// Created by Alexandr Zhuravlev on 04.11.2024.
//

#ifndef WATCHER_H
#define WATCHER_H
#include <memory>

class Watcher {
public:

    Watcher();

    ~Watcher();

    [[nodiscard]] bool start() const;

    void stop() const;

private:
    class WatcherPrivate;
    std::unique_ptr<WatcherPrivate> d;

};
#endif //WATCHER_H
