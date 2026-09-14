#include <iostream>
#include <vector>
#include <queue>
#include <climits>
#include <string>
using namespace std;
 
struct CanhKe {
    int denNode;
    int chiPhi;
};

class TimDuongToiUu {
private:
    int soNode;
    vector<vector<CanhKe>> doThi;
 
public:
    TimDuongToiUu(int n) {
        soNode = n;
        doThi.resize(n);
    }
    void themKetNoi(int a, int b, int chiPhi) {
        doThi[a].push_back({b,chiPhi});
        doThi[b].push_back({a,chiPhi});
    }
    vector<int> dijkstra(int nguon) {
        vector<int> chiPhiThapNhat(soNode,INT_MAX);
        chiPhiThapNhat[nguon]=0;
        priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> hangDoi;
        hangDoi.push({0,nguon});
        while (!hangDoi.empty()) {
            int chiPhiHienTai=hangDoi.top().first;
            int nodeHienTai=hangDoi.top().second;
            hangDoi.pop();
            if (chiPhiHienTai>chiPhiThapNhat[nodeHienTai]) {
                continue;
            }
            for (CanhKe &canh:doThi[nodeHienTai]) {
                int chiPhiMoi=chiPhiHienTai+canh.chiPhi;
                if (chiPhiMoi<chiPhiThapNhat[canh.denNode]) {
                    chiPhiThapNhat[canh.denNode] = chiPhiMoi;
                    hangDoi.push({chiPhiMoi, canh.denNode});
                }
            }
        }
        return chiPhiThapNhat;
    }
};
 
int main() {
    TimDuongToiUu mangLuoi(4);
    mangLuoi.themKetNoi(0, 1, 2);
    mangLuoi.themKetNoi(0, 2, 5);
    mangLuoi.themKetNoi(1, 2, 1);
    mangLuoi.themKetNoi(2, 3, 4);
    vector<int> ketQua = mangLuoi.dijkstra(0);
    string tenNode[4] = {"RouterA", "RouterB", "RouterC", "ClientD"};
    for (int i = 0; i < (int)ketQua.size(); i++) {
        cout << "Duong di re nhat tu RouterA toi " << tenNode[i] << " la: " << ketQua[i] << endl;
    }
    return 0;
}
