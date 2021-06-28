//
// Author:   Jonathan Blow
// Version:  1
// Date:     31 August, 2018
//
// This code is released under the MIT license, which you can find at
//
//          https://opensource.org/licenses/MIT
//
//
//
// See the comments for how to use this library just below the includes.
//

// See https://gist.github.com/andrewrk/ffb272748448174e6cdb4958dae9f3d8#file-microsoft_craziness-h for the
// full source

#pragma once

// Example: defer { ReleaseResources(); };

#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

template <typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda) : lambda(lambda) {}
    ~ExitScope() { lambda(); }
    ExitScope(const ExitScope &);

  private:
    ExitScope &operator=(const ExitScope &);
};

class ExitScopeHelp {
  public:
    template <typename T>
    ExitScope<T> operator+(T t) {
        return t;
    }
};

#define defer const auto &CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()