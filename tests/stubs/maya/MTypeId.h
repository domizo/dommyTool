#pragma once

class MTypeId
{
public:
    explicit MTypeId(unsigned int id = 0) : id_(id) {}
    unsigned int id() const { return id_; }

private:
    unsigned int id_;
};
