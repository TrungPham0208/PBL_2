#pragma once
#include<string>
using namespace std;

namespace pbl {
namespace model {

class Node {
private:
    int id_;
    string name_;
    bool active_;
protected:
    void set_identity(int id, string name);
public:
    Node();
    Node(int id, string name);
    virtual ~Node();

    int get_id() const;
    string get_name() const;
    bool is_active() const;
    bool is_failed() const;

    void set_active(bool active);
    void fail();
    void recover();

    virtual string get_type() const;
};

}
}
