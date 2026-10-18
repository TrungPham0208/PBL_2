#include "model/Node.h"

using namespace std;

namespace pbl {
namespace model {

Node::Node() {
    id_ = 0;
    name_ = "node";
    active_ = true;
}

Node::Node(int id, string name) {
    set_identity(id, name);
    active_ = true;
}

void Node::set_identity(int id, string name) {
    if (id < 0) {
        throw invalid_argument("node id must not be negative");
    }
    if (name.empty()) {
        throw invalid_argument("node name must not be empty");
    }
    id_ = id;
    name_ = name;
}

Node::~Node() {
}

int Node::get_id() const {
    return id_;
}

string Node::get_name() const {
    return name_;
}

bool Node::is_active() const {
    return active_;
}

bool Node::is_failed() const {
    return !active_;
}

void Node::set_active(bool active) {
    active_ = active;
}

void Node::fail() {
    active_ = false;
}

void Node::recover() {
    active_ = true;
}

string Node::get_type() const {
    return "Node";
}

}
}
