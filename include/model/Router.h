#pragma once

#include <bits/stdc++.h>

#include "model/Node.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 2: a router is a forwarding node with the same base state as a client.
class Router : public Node {
public:
    Router(int id, string name);

    string get_type() const override;
};

}
}
