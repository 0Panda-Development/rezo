#pragma once

#include <string>

class RezoWindow {
public:
    void Create(const std::string& startUrl);
    void DestroyWindow();
    bool IsClosing() const { return closing_; }

private:
    bool closing_ = false;
};