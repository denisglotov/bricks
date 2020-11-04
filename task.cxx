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


// Simple producer-consumer queue implementation.
template <typename T>
class ProducerConsumerQueue {
    deque<T> que;
    mutex mu;
    condition_variable cv;

public:
    void enqueue(T id) {
        {
            unique_lock<mutex> lock(mu);
            que.push_back(id);
        }
        cv.notify_one();
    }

    deque<T> dequeue() {
        unique_lock<mutex> lock(mu);
        while (que.empty()) cv.wait(lock);
        deque<T> res;
        res.swap(que);
        return res;
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


// Common space for worker threads.
struct Context {
    ProducerConsumerQueue<CellId> ready_cells;
    int stat_total_jobs;
    unordered_set<thread::id> stat_job_ids;
    mutex stat_mu;

    void add_stat() {
        unique_lock<mutex> lock(stat_mu);
        stat_job_ids.insert(this_thread::get_id());
        ++stat_total_jobs;
    }
};


// Data for every cell.
struct CellData {
    int ind;
    int value;
    int epoch;
    vector<int> args;
    int remote_result;

    CellData() : ind(0), value(0), epoch(0) {}

    void eval(vector<int> local_args, CellId id, Context *ctx) {
        remote_result = accumulate(local_args.begin(), local_args.end(), 0);
        ctx->add_stat();
        ctx->ready_cells.enqueue(id);
    }
};


int main(int argc, char *argv[]) {
    unordered_map<CellId, CellData> values;
    unordered_map<CellId, unordered_set<CellId>> deps;

    // Parse input file.
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
    ThreadPool pool(thread::hardware_concurrency());  // -1
    int jobs = 0;
    Context ctx;
    queue<CellId> que;
    for (auto &data: values) if (data.second.ind == 0) que.push(data.first);

    while (true) {
        while (!que.empty()) {
            CellId cell = que.front();
            que.pop();
            for (CellId dep: deps[cell]) {
                CellData &data = values[dep];
                data.args.push_back(values[cell].value);
                if (--data.ind == 0) {
                    ++jobs;
                    pool.enqueue([dep, &data, &ctx] { data.eval(data.args, dep, &ctx); });
                }
            }
            deps.erase(cell);
        }
        if (!jobs) break;
        for (CellId cell: ctx.ready_cells.dequeue()) {
            --jobs;
            CellData &data = values[cell];
            // TODO: if (data.epoch)
            data.value = data.remote_result;
            que.push(cell);
        }
    };

    if (deps.size()) {
        cerr << "Warning: the following are unresolved: ";
        for (auto &d: deps) cerr << decode(d.first).c_str() << " ";
        cerr << endl;
    }

    // Print results.
    for (auto &val: map<CellId, CellData>(values.begin(), values.end())) {
        if (val.second.ind == 0) cout << decode(val.first).c_str() << " = " << val.second.value << "\n";
    }
    cerr << "Total jobs executed: " << ctx.stat_total_jobs
         << ", current thread: " << this_thread::get_id() << ".\n"
         << "Threads used: ";
    for (auto t: ctx.stat_job_ids) cerr << t << " ";
    cerr << "\n";
}

// Local Variables:
// compile-command: "g++ -x c++ -std=gnu++17 -O2 -Wall -pedantic -pthread task.cxx"
// End:
