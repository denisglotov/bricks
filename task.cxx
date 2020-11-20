#include <cassert>
#include <cctype>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>


using namespace std;
using CellId = uint32_t;


bool parse(const string &str, size_t &left, size_t &right, char &last_punct) {
    left = right;
    while (left != str.size() && !isalnum(str[left])) {
        if (ispunct(str[left])) last_punct = str[left];
        ++left;
    }
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


// Simple producer-consumer queue implementation.
template <typename T>
class ProducerConsumerQueue {
    deque<T> que;
    mutex mu;
    condition_variable cv;

public:
    // Worker enqueues id of its calculated cell.
    void enqueue(T id) {
        {
            unique_lock<mutex> lock(mu);
            que.push_back(id);
        }
        cv.notify_one();
    }

    // Main thread dequeues array of calculated cell ids.
    deque<T> dequeue() {
        unique_lock<mutex> lock(mu);
        cv.wait(lock, [this] { return !que.empty(); });
        return move(que);
    }
};


// Simple thread pool.
class ThreadPool {
    vector<thread> workers;
    queue<function<void()>> tasks;
    bool stop;
    mutex mu;
    condition_variable cv;

public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back(thread(&ThreadPool::worker, this));
        }
    }

    void worker(void) {
        function<void()> task;
        while (true) {
            {
                unique_lock<mutex> lock(mu);
                cv.wait(lock, [this] { return stop || !tasks.empty(); });
                if (stop) return;
                task = move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

    void enqueue(function<void()>&& task) {
        {
            unique_lock<mutex> lock(mu);
            tasks.emplace(task);
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> lock(mu);
            stop = true;
        }
        cv.notify_all();
        for(thread &worker: workers) {
            worker.join();
        }
    }
};


// Formula of every cell. In this implementation it is just a linear
// combination of dependant cells.
using Formula = unordered_map<CellId, int>;  // cellId -> coefficient
using FormulaData = vector<pair<int, int>>;  // cell value, coefficient


// Data for every cell on a page.
struct CellData {
    int value;          // current value
    int indir;          // indirection - on how many cells does this one depend
    int epoch;          // epoch of this cell, updated when user rewrites it
    FormulaData args;   // arguments needed to calculate this cell
    int remote_result;  // result of calculation by worker thread

    CellData() : value(0), indir(0), epoch(0) {}
};


// Common space for worker threads.
struct WorkersContext {
    ProducerConsumerQueue<CellId> ready_cells;
    int stat_total_jobs;
    unordered_set<thread::id> stat_job_ids;
    mutex stat_mu;

    void eval(const FormulaData &local_args, CellId cell, CellData &data) {
        if (rand() % 10 == 0) this_thread::sleep_for(500us);  // simulate random delay
        data.remote_result = accumulate(local_args.begin(), local_args.end(), 0,
                                        [] (int acc, auto &pair) { return acc + pair.first * pair.second; } );
        add_stat();
        ready_cells.enqueue(cell);
    }

    void add_stat() {
        unique_lock<mutex> lock(stat_mu);
        stat_job_ids.insert(this_thread::get_id());
        ++stat_total_jobs;
    }
};


int main(int argc, char *argv[]) {
    unordered_map<CellId, CellData> page;               // all cell data on this page
    unordered_map<CellId, unordered_set<CellId>> deps;  // set of cells that depend on this
    unordered_map<CellId, Formula> formulae;            // set of formulae for each cell

    // Parse input file.
    ifstream input(argc > 1? argv[1] : "input.txt");
    string str;
    char last_punct;
    while (getline(input, str)) {
        size_t left = 0, right = 0;
        bool ok = parse(str, left, right, last_punct);
        if (!ok) continue;
        CellId left_cell = encode(str, left);
        if (page.count(left_cell)) {
            cerr << "Warning: " << decode(left_cell) << " is redefined.\n";
        }

        ok = parse(str, left, right, last_punct);
        assert(ok);
        if (isCell(str, left)) {
            int indir = 0;
            do {
                ++indir;
                CellId right_cell = encode(str, left);
                if (deps[left_cell].count(right_cell)) {
                    cerr << "Warning: " << decode(left_cell)
                         << " already depends on " << decode(right_cell)
                         << ", parsing '" << str << "', ignored." << endl;
                }
                deps[right_cell].insert(left_cell);
                formulae[left_cell][right_cell] = (last_punct == '-')? -1 : 1;
            } while (parse(str, left, right, last_punct));
            page[left_cell].indir = indir;
        } else {
            page[left_cell].value = atoi(str.c_str() + left);
        }
    }

    // Evaluate cells by sorting them.
    WorkersContext ctx;
    ThreadPool pool(thread::hardware_concurrency());
    int running_jobs = 0;
    queue<CellId> que;
    for (auto &data: page) if (data.second.indir == 0) que.push(data.first);

    while (true) {
        while (!que.empty()) {
            CellId cur_cell = que.front();
            que.pop();
            for (CellId dep_cell: deps[cur_cell]) {
                CellData &data = page[dep_cell];
                data.args.emplace_back(page[cur_cell].value, formulae[dep_cell][cur_cell]);
                if (--data.indir == 0) {
                    ++running_jobs;
                    pool.enqueue([dep_cell, args = move(data.args), &data, &ctx]
                                 { ctx.eval(args, dep_cell, data); });
                }
            }
            deps.erase(cur_cell);
        }
        if (!running_jobs) break;
        for (CellId cell: ctx.ready_cells.dequeue()) {
            --running_jobs;
            CellData &data = page[cell];
            data.value = data.remote_result;
            que.push(cell);
        }
    };

    if (deps.size()) {
        cerr << "Warning: the following cells are left unresolved: ";
        for (auto &d: deps) cerr << decode(d.first) << " ";
        cerr << endl;
    }

    // Print results.
    for (auto &val: map<CellId, CellData>(page.begin(), page.end())) {
        if (val.second.indir == 0) cout << decode(val.first) << " = " << val.second.value << "\n";
    }
    cerr << "Total jobs executed: " << ctx.stat_total_jobs << ", "
         << "main thread id: " << this_thread::get_id() << ".\n"
         << "Threads used: ";
    for (auto t: ctx.stat_job_ids) cerr << t << " ";
    cerr << "\n";
}

// Local Variables:
// compile-command: "g++ -x c++ -std=gnu++17 -O2 -Wall -pedantic -pthread task.cxx"
// End:
