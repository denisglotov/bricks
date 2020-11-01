#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>
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

struct CellData {
    int ind;
    int value;
    int epoch;
    vector<int> args;

    CellData() : ind(0), value(0), epoch(0) {}

    int eval() {
        int result = accumulate(args.begin(), args.end(), 0);
        args.clear();
        return result;
    }
};


int main(int argc, char *argv[]) {
    unordered_map<CellId, CellData> values;
    unordered_map<CellId, unordered_set<CellId>> deps;

    ifstream input(argc > 1? argv[1] : "input.txt");
    string str;
    while (getline(input, str)) {
        size_t left = 0, right = 0;
        bool ok = parse(str, left, right);
        if (!ok) continue;
        CellId lval = encode(str, left);
        if (values.count(lval)) {
            cerr << "Warning: " << decode(lval).c_str() << " is redefined.\n";
        }

        ok = parse(str, left, right);
        assert(ok);
        if (isCell(str, left)) {
            int ind = 0;
            do {
                ++ind;
                CellId rval = encode(str, left);
                if (deps[lval].count(rval)) {
                    cerr << "Warning: " << decode(lval).c_str()
                         << " already depends on " << decode(rval).c_str()
                         << ", parsing '" << str.c_str() << "', ignored." << endl;
                }
                deps[rval].insert(lval);
            } while (parse(str, left, right));
            values[lval].ind = ind;
        } else {
            values[lval].value = atoi(str.c_str() + left);
        }
    }

    // Evaluate cells by sorting them.
    queue<CellId> que;
    for (auto &data: values) if (data.second.ind == 0) que.push(data.first);

    while (!que.empty()) {
        CellId cell = que.front();
        que.pop();
        for (CellId d: deps[cell]) {
            CellData &data = values[d];
            data.args.push_back(values[cell].value);
            if (--data.ind == 0) {
                data.value = data.eval();
                que.push(d);
            }
        }
        deps.erase(cell);
    }
    if (deps.size()) {
        cerr << "Warning: the following are unresolved: ";
        for (auto &d: deps) cerr << decode(d.first).c_str() << " ";
        cerr << endl;
    }

    for (auto &val: map<CellId, CellData>(values.begin(), values.end())) {
        if (val.second.ind == 0) cout << decode(val.first).c_str() << " = " << val.second.value << "\n";
    }
    // for (auto &dep: deps) {
    //     cout << decode(dep.first).c_str() << ": ";
    //     for (auto cell: dep.second) cout << decode(cell).c_str() << " ";
    //     cout << "\n";
    // }
}


// Local Variables:
// compile-command: "g++ -x c++ -std=gnu++17 -O2 -Wall -pedantic -pthread task.cxx && time ./a.out"
// End:
