#include "model/Router.h"

using namespace std;

namespace pbl {
namespace model {

Router::Router(int id, string name) {
    set_identity(id, name);
}

string Router::get_type() const {
    return "Router";
}

}
}
