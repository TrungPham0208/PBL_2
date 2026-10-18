#pragma once

#include <bits/stdc++.h>

using namespace std;

namespace pbl {
namespace model {

// Milestone 2: common identity and availability state for network nodes.
class Node {
public:
    Node();
    Node(int id, string name);
    // Virtual cleanup lets derived nodes be safely handled through Node later.
    virtual ~Node();

    int get_id() const;
    string get_name() const;
    bool is_active() const;
    bool is_failed() const;

    void set_active(bool active);
    void fail();
    void recover();

    // Derived classes provide a readable model type for logs and tests.
    virtual string get_type() const;

protected:
    void set_identity(int id, string name);

private:
    int id_;
    string name_;
    bool active_;
};

}
}
