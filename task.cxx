#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>


using namespace std;
using CellId = uint32_t;


// Parse the string `str` beginning with position `right`.
bool parse(const string &str, size_t &left, size_t &right) {
    left = right;
    while (left != str.size() && !isalnum(str[left])) ++left;
    right = left;
    while (right != str.size() && isalnum(str[right])) ++right;
    return right > left;
}

bool isCell(const string &str, size_t pos = 0) {
    return isalpha(str[pos]);
}

CellId encode(const string &str, size_t beg = 0) {
    assert(isCell(str, beg));
    return ((str[beg] - 'A') << 24) + atoi(str.c_str() + beg + 1);
}

string decode(CellId cell) {
    return string(1, (cell >> 24) + 'A') + to_string(cell & 0xFFFFFF);
}


int main(int argc, char *argv[]) {
    map<CellId, pair<int, int>> values;
    unordered_map<CellId, unordered_set<CellId>> deps;
    queue<CellId> que;
    ifstream input(argc > 1? argv[1] : "input.txt");
    string str;
    while (getline(input, str)) {
        size_t left = 0, right = 0;
        bool ok = parse(str, left, right);
        if (!ok) continue;
        CellId lval = encode(str, left);

        ok = parse(str, left, right);
        assert(ok);
        if (isCell(str, left)) {
            int cnt = 0;
            do {
                ++cnt;
                CellId rval = encode(str, left);
                if (deps[lval].count(rval)) {
                    cerr << "Warning: " << decode(lval).c_str()
                         << " already depends on " << decode(rval).c_str()
                         << ", parsing '" << str.c_str() << "', ignored." << endl;
                }
                deps[rval].insert(lval);
            } while (parse(str, left, right));
            values[lval] = {cnt, 0};
        } else {
            values[lval] = {0, atoi(str.c_str() + left)};
            que.push(lval);
        }
    }

    while (!que.empty()) {
        CellId cell = que.front();
        que.pop();
        for (CellId d: deps[cell]) {
            if (--values[d].first == 0) que.push(d);
            values[d].second += values[cell].second;
        }
        deps.erase(cell);
    }
    if (deps.size()) {
        cerr << "Warning: the following are unresolved: ";
        for (auto &d: deps) cerr << decode(d.first).c_str() << " ";
        cerr << endl;
    }

    for (auto &val: values) {
        if (val.second.first == 0) cout << decode(val.first).c_str() << " = " << val.second.second << "\n";
        //else cout << decode(val.first).c_str() << " =: " << val.second.first << "\n";
    }
    // for (auto &dep: deps) {
    //     cout << decode(dep.first).c_str() << ": ";
    //     for (auto cell: dep.second) cout << decode(cell).c_str() << " ";
    //     cout << "\n";
    // }
}


// Local Variables:
// compile-command: "g++ -x c++ -std=gnu++17 -O2 task.cxx && time ./a.out"
// End:
