#include "model/Client.h"

using namespace std;

namespace pbl {
namespace model {

Client::Client(int id, string name) {
    set_identity(id, name);
}

string Client::get_type() const {
    return "Client";
}

}
}
