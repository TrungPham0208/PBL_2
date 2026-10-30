#pragma once

#include <bits/stdc++.h>

#include "model/Node.h"

using namespace std;

namespace pbl {
namespace model {

class Router : public Node {
public:
    Router(int id, string name);
    string get_type() const override;
};

}
}
