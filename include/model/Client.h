#pragma once

#include <bits/stdc++.h>

#include "model/Node.h"

using namespace std;

namespace pbl {
namespace model {

class Client : public Node {
public:
    Client(int id, string name);

    string get_type() const override;
};

}
}
